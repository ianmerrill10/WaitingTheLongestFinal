import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
  logApiRequest,
} from "@/lib/shelter-api/auth";
import {
  checkApiKeyRateLimit,
  rateLimitHeaders,
} from "@/lib/shelter-api/rate-limiter";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/v1/dogs — List shelter's own dogs (paginated, filterable)
 */
export async function GET(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    await logApiRequest({
      shelter_id: auth.shelter_id!,
      api_key_id: auth.key!.id,
      method: "GET",
      path: "/api/v1/dogs",
      status_code: 429,
      response_time_ms: Date.now() - startTime,
      rate_limited: true,
    });
    return NextResponse.json(
      { error: "Rate limit exceeded", code: "RATE_LIMITED" },
      { status: 429, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!hasScope(auth.key!, "read")) return scopeError("read");

  const url = new URL(request.url);
  const page = parseInt(url.searchParams.get("page") || "1");
  const limit = Math.min(parseInt(url.searchParams.get("limit") || "50"), 100);
  const status = url.searchParams.get("status");
  const breed = url.searchParams.get("breed");
  const search = url.searchParams.get("search");
  const sort = url.searchParams.get("sort") || "created_at";
  const order = url.searchParams.get("order") === "asc" ? true : false;
  const offset = (page - 1) * limit;

  const supabase = createAdminClient();
  let query = supabase
    .from("dogs")
    .select("*", { count: "exact" })
    .eq("shelter_id", auth.shelter_id!)
    .range(offset, offset + limit - 1)
    .order(sort, { ascending: order });

  if (status === "available") query = query.eq("is_available", true);
  else if (status === "adopted") query = query.eq("is_available", false);
  if (breed) query = query.ilike("breed_primary", `%${breed}%`);
  if (search) query = query.or(`name.ilike.%${search}%,breed_primary.ilike.%${search}%`);

  const { data, error, count } = await query;

  const responseTimeMs = Date.now() - startTime;
  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "GET",
    path: "/api/v1/dogs",
    query_params: { page, limit, status, breed, search },
    status_code: error ? 500 : 200,
    response_time_ms: responseTimeMs,
    error_message: error?.message,
  });

  if (error) {
    return NextResponse.json(
      { error: "Failed to fetch dogs", code: "QUERY_ERROR" },
      { status: 500, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json(
    {
      data: data || [],
      pagination: {
        page,
        limit,
        total: count || 0,
        total_pages: Math.ceil((count || 0) / limit),
      },
    },
    { headers: rateLimitHeaders(rateLimit) }
  );
}

/**
 * POST /api/v1/dogs — Create a new dog listing
 */
export async function POST(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json(
      { error: "Rate limit exceeded", code: "RATE_LIMITED" },
      { status: 429, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!hasScope(auth.key!, "write")) return scopeError("write");

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid JSON body", code: "INVALID_BODY" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Required fields
  if (!body.name || typeof body.name !== "string") {
    return NextResponse.json(
      { error: "name is required", code: "VALIDATION_ERROR" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  const supabase = createAdminClient();
  const hasProvidedIntakeDate = typeof body.intake_date === "string" && body.intake_date.trim().length > 0;
  const intakeDate = hasProvidedIntakeDate ? body.intake_date : new Date().toISOString();
  const dogData = {
    name: body.name.trim(),
    shelter_id: auth.shelter_id,
    breed_primary: body.breed || body.breed_primary || null,
    breed_secondary: body.breed_secondary || null,
    breed_mixed: body.breed_mixed ?? null,
    age_text: body.age_text || body.age || null,
    age_months: body.age_months || null,
    sex: body.sex || null,
    size_general: body.size || body.size_general || null,
    size_current_lbs: body.weight || body.size_current_lbs || null,
    color_primary: body.color || body.color_primary || null,
    description: body.description || null,
    description_plain: body.description?.replace(/<[^>]*>/g, "") || null,
    photo_url: body.photo_url || null,
    photo_urls: body.photos || body.photo_urls || null,
    is_available: true,
    status: body.status || "available",
    intake_date: intakeDate,
    adoption_fee: body.adoption_fee || null,
    is_house_trained: body.is_house_trained ?? null,
    good_with_kids: body.good_with_kids ?? null,
    good_with_dogs: body.good_with_dogs ?? null,
    good_with_cats: body.good_with_cats ?? null,
    special_needs: body.special_needs || null,
    external_id: body.external_id || null,
    external_url: body.external_url || null,
    external_source: "api_direct",
    date_confidence: hasProvidedIntakeDate ? "verified" : "low",
    date_source: hasProvidedIntakeDate ? "api_submission" : "api_submission_missing_intake_date",
    original_intake_date: intakeDate,
    ranking_eligible: hasProvidedIntakeDate,
    intake_date_observation_count: 1,
    source_extraction_method: "partner_api_direct",
  };

  const { data, error } = await supabase
    .from("dogs")
    .insert(dogData)
    .select()
    .single();

  const responseTimeMs = Date.now() - startTime;
  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "POST",
    path: "/api/v1/dogs",
    status_code: error ? 500 : 201,
    response_time_ms: responseTimeMs,
    error_message: error?.message,
  });

  if (error) {
    return NextResponse.json(
      { error: "Failed to create dog", code: "INSERT_ERROR", detail: error.message },
      { status: 500, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json(
    { data, message: "Dog created successfully" },
    { status: 201, headers: rateLimitHeaders(rateLimit) }
  );
}

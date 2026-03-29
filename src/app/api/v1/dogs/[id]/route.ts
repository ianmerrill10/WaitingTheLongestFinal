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
 * GET /api/v1/dogs/:id — Get single dog detail
 */
export async function GET(
  request: Request,
  { params }: { params: { id: string } }
) {
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

  if (!hasScope(auth.key!, "read")) return scopeError("read");

  const supabase = createAdminClient();
  const { data, error } = await supabase
    .from("dogs")
    .select("*")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "GET",
    path: `/api/v1/dogs/${params.id}`,
    status_code: error ? (data === null ? 404 : 500) : 200,
    response_time_ms: Date.now() - startTime,
    error_message: error?.message,
  });

  if (error || !data) {
    return NextResponse.json(
      { error: "Dog not found", code: "NOT_FOUND" },
      { status: 404, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json({ data }, { headers: rateLimitHeaders(rateLimit) });
}

/**
 * PUT /api/v1/dogs/:id — Update dog information
 */
export async function PUT(
  request: Request,
  { params }: { params: { id: string } }
) {
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

  // Verify dog belongs to shelter
  const supabase = createAdminClient();
  const { data: existing } = await supabase
    .from("dogs")
    .select("id, intake_date, original_intake_date")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  if (!existing) {
    return NextResponse.json(
      { error: "Dog not found", code: "NOT_FOUND" },
      { status: 404, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Build update object (only allow valid fields)
  const allowedFields = [
    "name", "breed_primary", "breed_secondary", "breed_mixed",
    "age_text", "age_months", "sex", "size_general", "size_current_lbs",
    "color_primary", "description", "description_plain",
    "photo_url", "photo_urls", "is_available", "status",
    "intake_date", "euthanasia_date", "adoption_fee",
    "is_house_trained", "good_with_kids", "good_with_dogs", "good_with_cats",
    "special_needs", "external_id", "external_url",
  ];

  const update: Record<string, unknown> = {};
  for (const field of allowedFields) {
    if (body[field] !== undefined) {
      update[field] = body[field];
    }
  }

  // Handle field aliases
  if (body.breed !== undefined) update.breed_primary = body.breed;
  if (body.weight !== undefined) update.size_current_lbs = body.weight;
  if (body.color !== undefined) update.color_primary = body.color;
  if (body.size !== undefined) update.size_general = body.size;
  if (body.photos !== undefined) update.photo_urls = body.photos;

  if (body.intake_date !== undefined) {
    update.date_confidence = "verified";
    update.date_source = "api_submission";
    update.original_intake_date = existing.original_intake_date || existing.intake_date;
    update.ranking_eligible = true;
    update.intake_date_observation_count = 1;
  }

  if (Object.keys(update).length === 0) {
    return NextResponse.json(
      { error: "No valid fields to update", code: "VALIDATION_ERROR" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  const { data, error } = await supabase
    .from("dogs")
    .update(update)
    .eq("id", params.id)
    .select()
    .single();

  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "PUT",
    path: `/api/v1/dogs/${params.id}`,
    status_code: error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
    error_message: error?.message,
  });

  if (error) {
    return NextResponse.json(
      { error: "Failed to update dog", code: "UPDATE_ERROR" },
      { status: 500, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json(
    { data, message: "Dog updated successfully" },
    { headers: rateLimitHeaders(rateLimit) }
  );
}

/**
 * DELETE /api/v1/dogs/:id — Mark dog as adopted/removed
 */
export async function DELETE(
  request: Request,
  { params }: { params: { id: string } }
) {
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

  if (!hasScope(auth.key!, "delete")) return scopeError("delete");

  const supabase = createAdminClient();

  // Verify ownership
  const { data: existing } = await supabase
    .from("dogs")
    .select("id")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  if (!existing) {
    return NextResponse.json(
      { error: "Dog not found", code: "NOT_FOUND" },
      { status: 404, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Soft delete: mark as unavailable
  const { error } = await supabase
    .from("dogs")
    .update({
      is_available: false,
      status: "adopted",
    })
    .eq("id", params.id);

  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "DELETE",
    path: `/api/v1/dogs/${params.id}`,
    status_code: error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
    error_message: error?.message,
  });

  if (error) {
    return NextResponse.json(
      { error: "Failed to remove dog", code: "DELETE_ERROR" },
      { status: 500, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json(
    { message: "Dog removed successfully" },
    { headers: rateLimitHeaders(rateLimit) }
  );
}

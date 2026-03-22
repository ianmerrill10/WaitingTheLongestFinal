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
 * GET /api/v1/account — Shelter profile, stats, integration status
 */
export async function GET(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "read")) return scopeError("read");

  const supabase = createAdminClient();

  const [shelterResult, dogCountResult, activeKeysResult] = await Promise.all([
    supabase.from("shelters").select("*").eq("id", auth.shelter_id!).single(),
    supabase.from("dogs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).eq("is_available", true),
    supabase.from("shelter_api_keys").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).eq("is_active", true),
  ]);

  await logApiRequest({
    shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
    method: "GET", path: "/api/v1/account",
    status_code: shelterResult.error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
  });

  if (shelterResult.error || !shelterResult.data) {
    return NextResponse.json({ error: "Shelter not found" }, { status: 404 });
  }

  const shelter = shelterResult.data;
  return NextResponse.json({
    data: {
      id: shelter.id,
      name: shelter.name,
      city: shelter.city,
      state_code: shelter.state_code,
      partner_status: shelter.partner_status,
      partner_tier: shelter.partner_tier,
      partner_since: shelter.partner_since,
      integration_type: shelter.integration_type,
      total_active_dogs: dogCountResult.count || 0,
      total_active_keys: activeKeysResult.count || 0,
      website: shelter.website,
      email: shelter.email,
      phone: shelter.phone,
    },
  }, { headers: rateLimitHeaders(rateLimit) });
}

/**
 * PUT /api/v1/account — Update shelter profile
 */
export async function PUT(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "write")) return scopeError("write");

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  const allowedFields = [
    "name", "address", "city", "state_code", "zip_code",
    "phone", "email", "website", "description",
    "social_facebook", "social_instagram", "social_tiktok", "social_youtube",
    "feed_url",
  ];

  const update: Record<string, unknown> = {};
  for (const field of allowedFields) {
    if (body[field] !== undefined) update[field] = body[field];
  }

  if (Object.keys(update).length === 0) {
    return NextResponse.json({ error: "No valid fields to update" }, { status: 400 });
  }

  const supabase = createAdminClient();
  const { data, error } = await supabase
    .from("shelters")
    .update(update)
    .eq("id", auth.shelter_id!)
    .select("id, name, city, state_code, email, website, phone")
    .single();

  await logApiRequest({
    shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
    method: "PUT", path: "/api/v1/account",
    status_code: error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
  });

  if (error) {
    return NextResponse.json({ error: "Failed to update profile" }, { status: 500 });
  }

  return NextResponse.json({ data, message: "Profile updated" }, { headers: rateLimitHeaders(rateLimit) });
}

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

const VALID_STATUSES = ["available", "adopted", "transferred", "hold", "pending", "rescue_only"];

/**
 * PATCH /api/v1/dogs/:id/status — Quick status change
 */
export async function PATCH(
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

  if (!body.status || !VALID_STATUSES.includes(body.status)) {
    return NextResponse.json(
      {
        error: `Invalid status. Must be one of: ${VALID_STATUSES.join(", ")}`,
        code: "VALIDATION_ERROR",
      },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

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

  const update: Record<string, unknown> = {
    status: body.status,
    is_available: body.status === "available",
  };

  if (body.status === "adopted") {
    update.adopted_date = new Date().toISOString();
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
    method: "PATCH",
    path: `/api/v1/dogs/${params.id}/status`,
    status_code: error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
    error_message: error?.message,
  });

  if (error) {
    return NextResponse.json(
      { error: "Failed to update status", code: "UPDATE_ERROR" },
      { status: 500, headers: rateLimitHeaders(rateLimit) }
    );
  }

  return NextResponse.json(
    { data, message: `Status updated to ${body.status}` },
    { headers: rateLimitHeaders(rateLimit) }
  );
}

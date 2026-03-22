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
 * GET /api/v1/webhooks/:id — Endpoint details + recent deliveries
 */
export async function GET(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  const supabase = createAdminClient();
  const { data: endpoint } = await supabase
    .from("webhook_endpoints")
    .select("*")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  if (!endpoint) {
    return NextResponse.json({ error: "Webhook not found" }, { status: 404, headers: rateLimitHeaders(rateLimit) });
  }

  // Get recent deliveries
  const { data: deliveries } = await supabase
    .from("webhook_deliveries")
    .select("id, event_type, status, response_status, response_time_ms, attempt_number, created_at")
    .eq("webhook_endpoint_id", params.id)
    .order("created_at", { ascending: false })
    .limit(20);

  // Hide signing secret
  const { signing_secret: _, ...safeEndpoint } = endpoint;

  return NextResponse.json(
    { data: { ...safeEndpoint, recent_deliveries: deliveries || [] } },
    { headers: rateLimitHeaders(rateLimit) }
  );
}

/**
 * PUT /api/v1/webhooks/:id — Update endpoint
 */
export async function PUT(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  const supabase = createAdminClient();
  const { data: existing } = await supabase
    .from("webhook_endpoints")
    .select("id")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  if (!existing) {
    return NextResponse.json({ error: "Webhook not found" }, { status: 404 });
  }

  const update: Record<string, unknown> = {};
  if (body.url) update.url = body.url;
  if (body.description !== undefined) update.description = body.description;
  if (body.events) update.events = body.events;
  if (body.headers !== undefined) update.headers = body.headers;
  if (body.is_active !== undefined) update.is_active = body.is_active;

  const { data, error } = await supabase
    .from("webhook_endpoints")
    .update(update)
    .eq("id", params.id)
    .select("id, url, description, events, is_active")
    .single();

  await logApiRequest({
    shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
    method: "PUT", path: `/api/v1/webhooks/${params.id}`,
    status_code: error ? 500 : 200,
    response_time_ms: 0,
  });

  if (error) {
    return NextResponse.json({ error: "Failed to update webhook" }, { status: 500 });
  }

  return NextResponse.json({ data }, { headers: rateLimitHeaders(rateLimit) });
}

/**
 * DELETE /api/v1/webhooks/:id — Remove endpoint
 */
export async function DELETE(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  const supabase = createAdminClient();
  const { error } = await supabase
    .from("webhook_endpoints")
    .delete()
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!);

  if (error) {
    return NextResponse.json({ error: "Failed to delete webhook" }, { status: 500 });
  }

  return NextResponse.json({ message: "Webhook deleted" });
}

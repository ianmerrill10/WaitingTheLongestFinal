import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { randomBytes } from "crypto";
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

const VALID_EVENTS = [
  "dog.created", "dog.updated", "dog.adopted", "dog.urgent",
  "dog.viewed", "dog.inquiry_received", "dog.shared", "account.updated",
];

/**
 * GET /api/v1/webhooks — List registered endpoints
 */
export async function GET(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json(
      { error: "Rate limit exceeded" },
      { status: 429, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  const supabase = createAdminClient();
  const { data, error } = await supabase
    .from("webhook_endpoints")
    .select("id, url, description, events, is_active, is_verified, consecutive_failures, total_deliveries, total_successes, total_failures, last_delivery_at, last_success_at, created_at")
    .eq("shelter_id", auth.shelter_id!)
    .order("created_at", { ascending: false });

  await logApiRequest({
    shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
    method: "GET", path: "/api/v1/webhooks",
    status_code: error ? 500 : 200,
    response_time_ms: Date.now() - startTime,
  });

  if (error) {
    return NextResponse.json({ error: "Failed to fetch webhooks" }, { status: 500 });
  }

  return NextResponse.json({ data: data || [] }, { headers: rateLimitHeaders(rateLimit) });
}

/**
 * POST /api/v1/webhooks — Register new endpoint
 */
export async function POST(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json(
      { error: "Rate limit exceeded" },
      { status: 429, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json({ error: "Invalid JSON" }, { status: 400 });
  }

  if (!body.url || typeof body.url !== "string") {
    return NextResponse.json(
      { error: "url is required and must be a valid HTTPS URL" },
      { status: 400 }
    );
  }

  // Validate URL format
  try {
    const parsed = new URL(body.url);
    if (parsed.protocol !== "https:") {
      return NextResponse.json(
        { error: "Webhook URL must use HTTPS" },
        { status: 400 }
      );
    }
  } catch {
    return NextResponse.json(
      { error: "Invalid URL format" },
      { status: 400 }
    );
  }

  // Validate events
  const events = body.events || VALID_EVENTS.slice(0, 4); // Default events
  for (const event of events) {
    if (!VALID_EVENTS.includes(event)) {
      return NextResponse.json(
        { error: `Invalid event: ${event}. Valid: ${VALID_EVENTS.join(", ")}` },
        { status: 400 }
      );
    }
  }

  // Generate signing secret
  const signingSecret = randomBytes(32).toString("hex");

  const supabase = createAdminClient();
  const { data, error } = await supabase
    .from("webhook_endpoints")
    .insert({
      shelter_id: auth.shelter_id,
      url: body.url,
      description: body.description || null,
      events,
      signing_secret: signingSecret,
      headers: body.headers || {},
    })
    .select("id, url, description, events, is_active, created_at")
    .single();

  await logApiRequest({
    shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
    method: "POST", path: "/api/v1/webhooks",
    status_code: error ? 500 : 201,
    response_time_ms: Date.now() - startTime,
  });

  if (error) {
    return NextResponse.json({ error: "Failed to create webhook" }, { status: 500 });
  }

  return NextResponse.json(
    {
      data,
      signing_secret: signingSecret,
      message: "Webhook created. Save the signing_secret — it will not be shown again.",
    },
    { status: 201, headers: rateLimitHeaders(rateLimit) }
  );
}

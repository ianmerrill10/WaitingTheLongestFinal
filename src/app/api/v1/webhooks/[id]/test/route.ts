import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
} from "@/lib/shelter-api/auth";
import { signPayload } from "@/lib/shelter-api/webhook-sender";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/v1/webhooks/:id/test — Send a test event
 */
export async function POST(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);
  if (!hasScope(auth.key!, "webhook_manage")) return scopeError("webhook_manage");

  const supabase = createAdminClient();
  const { data: endpoint } = await supabase
    .from("webhook_endpoints")
    .select("*")
    .eq("id", params.id)
    .eq("shelter_id", auth.shelter_id!)
    .single();

  if (!endpoint) {
    return NextResponse.json({ error: "Webhook not found" }, { status: 404 });
  }

  // Send test event
  const testPayload = {
    event: "test",
    data: {
      message: "This is a test webhook delivery from WaitingTheLongest.com",
      timestamp: new Date().toISOString(),
      webhook_id: endpoint.id,
    },
  };

  const payloadStr = JSON.stringify(testPayload);
  const signature = signPayload(payloadStr, endpoint.signing_secret);

  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15000);

    const response = await fetch(endpoint.url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "X-WTL-Signature": `sha256=${signature}`,
        "X-WTL-Event": "test",
        "X-WTL-Event-ID": `test_${Date.now()}`,
        "User-Agent": "WaitingTheLongest-Webhook/1.0",
        ...(endpoint.headers || {}),
      },
      body: payloadStr,
      signal: controller.signal,
    });

    clearTimeout(timeout);
    const responseBody = await response.text().catch(() => "");

    return NextResponse.json({
      success: response.ok,
      status_code: response.status,
      response_body: responseBody.substring(0, 500),
      response_time_ms: Date.now(),
      message: response.ok
        ? "Test delivery successful"
        : `Test delivery returned HTTP ${response.status}`,
    });
  } catch (err) {
    return NextResponse.json({
      success: false,
      error: err instanceof Error ? err.message : "Delivery failed",
      message: "Test delivery failed — check your endpoint URL and ensure it accepts POST requests",
    });
  }
}

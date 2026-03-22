import { createAdminClient } from "@/lib/supabase/admin";
import { createHmac } from "crypto";
import type { WebhookEndpoint } from "@/types/shelter-crm";

const MAX_RETRY_ATTEMPTS = 5;
const AUTO_DISABLE_THRESHOLD = 10; // consecutive failures

// Exponential backoff delays in milliseconds
const RETRY_DELAYS = [
  60 * 1000,        // 1 minute
  5 * 60 * 1000,    // 5 minutes
  30 * 60 * 1000,   // 30 minutes
  2 * 60 * 60 * 1000, // 2 hours
  24 * 60 * 60 * 1000, // 24 hours
];

/**
 * Sign a payload with HMAC-SHA256 using the endpoint's signing secret.
 */
export function signPayload(payload: string, secret: string): string {
  return createHmac("sha256", secret).update(payload).digest("hex");
}

/**
 * Deliver a webhook event to all matching endpoints for a shelter.
 */
export async function deliverWebhookEvent(
  shelterId: string,
  eventType: string,
  payload: Record<string, unknown>
): Promise<{ delivered: number; failed: number }> {
  const supabase = createAdminClient();

  // Find active endpoints for this shelter that subscribe to this event
  const { data: endpoints } = await supabase
    .from("webhook_endpoints")
    .select("*")
    .eq("shelter_id", shelterId)
    .eq("is_active", true);

  if (!endpoints?.length) return { delivered: 0, failed: 0 };

  let delivered = 0;
  let failed = 0;

  for (const endpoint of endpoints as WebhookEndpoint[]) {
    // Check if endpoint subscribes to this event
    if (!endpoint.events.includes(eventType as never)) continue;

    const result = await deliverToEndpoint(endpoint, eventType, payload);
    if (result.success) {
      delivered++;
    } else {
      failed++;
    }
  }

  return { delivered, failed };
}

/**
 * Deliver a webhook to a specific endpoint and log the result.
 */
async function deliverToEndpoint(
  endpoint: WebhookEndpoint,
  eventType: string,
  payload: Record<string, unknown>,
  attemptNumber = 1
): Promise<{ success: boolean }> {
  const supabase = createAdminClient();
  const payloadStr = JSON.stringify(payload);
  const signature = signPayload(payloadStr, endpoint.signing_secret);
  const eventId = `evt_${Date.now()}_${Math.random().toString(36).substring(2, 10)}`;

  const startTime = Date.now();
  let responseStatus: number | null = null;
  let responseBody: string | null = null;
  let responseTimeMs: number | null = null;
  let success = false;

  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 30000); // 30s timeout

    const headers: Record<string, string> = {
      "Content-Type": "application/json",
      "X-WTL-Signature": `sha256=${signature}`,
      "X-WTL-Event": eventType,
      "X-WTL-Event-ID": eventId,
      "X-WTL-Delivery-ID": crypto.randomUUID(),
      "User-Agent": "WaitingTheLongest-Webhook/1.0",
      ...(endpoint.headers || {}),
    };

    const response = await fetch(endpoint.url, {
      method: "POST",
      headers,
      body: payloadStr,
      signal: controller.signal,
    });

    clearTimeout(timeout);
    responseStatus = response.status;
    responseTimeMs = Date.now() - startTime;

    try {
      const body = await response.text();
      responseBody = body.substring(0, 2000); // Cap at 2000 chars
    } catch {
      responseBody = null;
    }

    success = responseStatus >= 200 && responseStatus < 300;
  } catch (err) {
    responseTimeMs = Date.now() - startTime;
    responseBody = err instanceof Error ? err.message : "Request failed";
    success = false;
  }

  // Log the delivery attempt
  const deliveryStatus = success
    ? "success"
    : attemptNumber < MAX_RETRY_ATTEMPTS
      ? "retrying"
      : "failed";

  const nextRetryAt =
    !success && attemptNumber < MAX_RETRY_ATTEMPTS
      ? new Date(Date.now() + RETRY_DELAYS[attemptNumber - 1]).toISOString()
      : null;

  await supabase.from("webhook_deliveries").insert({
    webhook_endpoint_id: endpoint.id,
    shelter_id: endpoint.shelter_id,
    event_type: eventType,
    event_id: eventId,
    payload,
    response_status: responseStatus,
    response_body: responseBody,
    response_time_ms: responseTimeMs,
    attempt_number: attemptNumber,
    max_attempts: MAX_RETRY_ATTEMPTS,
    status: deliveryStatus,
    next_retry_at: nextRetryAt,
    delivered_at: success ? new Date().toISOString() : null,
  });

  // Update endpoint stats
  if (success) {
    await supabase
      .from("webhook_endpoints")
      .update({
        last_delivery_at: new Date().toISOString(),
        last_success_at: new Date().toISOString(),
        consecutive_failures: 0,
        total_deliveries: (endpoint.total_deliveries || 0) + 1,
        total_successes: (endpoint.total_successes || 0) + 1,
      })
      .eq("id", endpoint.id);
  } else {
    const newFailures = (endpoint.consecutive_failures || 0) + 1;
    const update: Record<string, unknown> = {
      last_delivery_at: new Date().toISOString(),
      last_failure_at: new Date().toISOString(),
      consecutive_failures: newFailures,
      total_deliveries: (endpoint.total_deliveries || 0) + 1,
      total_failures: (endpoint.total_failures || 0) + 1,
    };

    // Auto-disable after threshold
    if (newFailures >= AUTO_DISABLE_THRESHOLD) {
      update.is_active = false;
      update.disabled_at = new Date().toISOString();
      update.disable_reason = `Auto-disabled after ${newFailures} consecutive delivery failures`;
    }

    await supabase
      .from("webhook_endpoints")
      .update(update)
      .eq("id", endpoint.id);
  }

  return { success };
}

/**
 * Process pending webhook retries.
 * Called by cron to retry failed deliveries with exponential backoff.
 */
export async function processWebhookRetries(): Promise<{
  retried: number;
  succeeded: number;
  failed: number;
}> {
  const supabase = createAdminClient();

  const { data: pendingRetries } = await supabase
    .from("webhook_deliveries")
    .select("*, webhook_endpoints(*)")
    .in("status", ["retrying"])
    .lte("next_retry_at", new Date().toISOString())
    .order("next_retry_at", { ascending: true })
    .limit(50);

  if (!pendingRetries?.length) {
    return { retried: 0, succeeded: 0, failed: 0 };
  }

  let succeeded = 0;
  let failed = 0;

  for (const delivery of pendingRetries) {
    const endpoint = delivery.webhook_endpoints as unknown as WebhookEndpoint;
    if (!endpoint || !endpoint.is_active) continue;

    const result = await deliverToEndpoint(
      endpoint,
      delivery.event_type,
      delivery.payload,
      delivery.attempt_number + 1
    );

    if (result.success) succeeded++;
    else failed++;
  }

  return {
    retried: pendingRetries.length,
    succeeded,
    failed,
  };
}

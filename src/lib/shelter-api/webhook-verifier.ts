import { createHmac, timingSafeEqual } from "crypto";

/**
 * Verify an incoming webhook signature using HMAC-SHA256.
 * Expects header format: "sha256=<hex_digest>"
 */
export function verifyWebhookSignature(
  payload: string,
  signature: string,
  secret: string
): boolean {
  if (!signature || !secret || !payload) return false;

  // Parse "sha256=..." format
  const parts = signature.split("=");
  if (parts.length !== 2 || parts[0] !== "sha256") return false;

  const expectedSignature = parts[1];
  const computedSignature = createHmac("sha256", secret)
    .update(payload)
    .digest("hex");

  // Timing-safe comparison to prevent timing attacks
  try {
    const a = Buffer.from(expectedSignature, "hex");
    const b = Buffer.from(computedSignature, "hex");
    if (a.length !== b.length) return false;
    return timingSafeEqual(a, b);
  } catch {
    return false;
  }
}

/**
 * Validate incoming webhook payload structure.
 */
export function validateWebhookPayload(
  body: unknown
): { valid: boolean; error?: string } {
  if (!body || typeof body !== "object") {
    return { valid: false, error: "Request body must be a JSON object" };
  }

  const payload = body as Record<string, unknown>;

  if (!payload.event || typeof payload.event !== "string") {
    return { valid: false, error: "Missing or invalid 'event' field" };
  }

  const validEvents = [
    "dog.created",
    "dog.updated",
    "dog.adopted",
    "dog.transferred",
    "dog.euthanasia_scheduled",
  ];

  if (!validEvents.includes(payload.event)) {
    return {
      valid: false,
      error: `Invalid event type: ${payload.event}. Valid events: ${validEvents.join(", ")}`,
    };
  }

  if (!payload.data || typeof payload.data !== "object") {
    return { valid: false, error: "Missing or invalid 'data' field" };
  }

  return { valid: true };
}

import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { verifyWebhookSignature, validateWebhookPayload } from "@/lib/shelter-api/webhook-verifier";
import { rateLimit } from "@/lib/utils/rate-limit";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/webhooks/incoming/:shelterId — Receive dog events from shelters
 */
export async function POST(
  request: Request,
  { params }: { params: { shelterId: string } }
) {
  const ip = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() || "unknown";

  // Rate limit: 120/min per shelter
  const rl = rateLimit(`webhook:${params.shelterId}`, 120, 60000);
  if (!rl.allowed) {
    return NextResponse.json(
      { error: "Rate limit exceeded" },
      { status: 429, headers: { "Retry-After": "60" } }
    );
  }

  const supabase = createAdminClient();

  // Verify shelter exists and has an API key
  const { data: shelter } = await supabase
    .from("shelters")
    .select("id, name")
    .eq("id", params.shelterId)
    .single();

  if (!shelter) {
    return NextResponse.json({ error: "Shelter not found" }, { status: 404 });
  }

  // Get the shelter's webhook signing secret (from their registered endpoint pointing at us)
  const { data: apiKey } = await supabase
    .from("shelter_api_keys")
    .select("id, key_hash")
    .eq("shelter_id", params.shelterId)
    .eq("is_active", true)
    .limit(1)
    .single();

  // Read body as text for signature verification
  const bodyText = await request.text();

  // Verify HMAC signature if provided
  const signature = request.headers.get("x-wtl-signature");
  if (signature && apiKey) {
    // Use the key_hash as the shared secret for incoming webhooks
    const valid = verifyWebhookSignature(bodyText, signature, apiKey.key_hash);
    if (!valid) {
      return NextResponse.json(
        { error: "Invalid webhook signature" },
        { status: 401 }
      );
    }
  }

  // Check for idempotency
  const eventId = request.headers.get("x-wtl-event-id");
  if (eventId) {
    const { data: existing } = await supabase
      .from("webhook_deliveries")
      .select("id")
      .eq("event_id", eventId)
      .limit(1)
      .single();

    if (existing) {
      return NextResponse.json(
        { message: "Event already processed", event_id: eventId },
        { status: 200 }
      );
    }
  }

  // Parse and validate payload
  let payload;
  try {
    payload = JSON.parse(bodyText);
  } catch {
    return NextResponse.json({ error: "Invalid JSON body" }, { status: 400 });
  }

  const validation = validateWebhookPayload(payload);
  if (!validation.valid) {
    return NextResponse.json(
      { error: validation.error },
      { status: 400 }
    );
  }

  const event = payload.event as string;
  const data = payload.data as Record<string, unknown>;

  // Process the event
  try {
    switch (event) {
      case "dog.created":
        await processDogCreated(supabase, params.shelterId, data);
        break;
      case "dog.updated":
        await processDogUpdated(supabase, params.shelterId, data);
        break;
      case "dog.adopted":
        await processDogAdopted(supabase, params.shelterId, data);
        break;
      case "dog.transferred":
        await processDogAdopted(supabase, params.shelterId, data); // Same logic
        break;
      case "dog.euthanasia_scheduled":
        await processEuthanasiaScheduled(supabase, params.shelterId, data);
        break;
    }

    // Log successful delivery
    await supabase.from("webhook_deliveries").insert({
      webhook_endpoint_id: null, // Incoming, not tied to our outgoing endpoints
      shelter_id: params.shelterId,
      event_type: event,
      event_id: eventId || null,
      payload,
      response_status: 200,
      status: "success",
      delivered_at: new Date().toISOString(),
    });

    return NextResponse.json({
      message: `Event ${event} processed successfully`,
      event_id: eventId,
    });
  } catch (err) {
    console.error(`Webhook processing error for ${params.shelterId}:`, err);

    await supabase.from("webhook_deliveries").insert({
      webhook_endpoint_id: null,
      shelter_id: params.shelterId,
      event_type: event,
      event_id: eventId || null,
      payload,
      response_status: 500,
      response_body: err instanceof Error ? err.message : "Processing error",
      status: "failed",
    });

    return NextResponse.json(
      { error: "Failed to process event" },
      { status: 500 }
    );
  }
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function processDogCreated(supabase: any, shelterId: string, data: Record<string, unknown>) {
  await supabase.from("dogs").insert({
    name: data.name || "Unknown",
    shelter_id: shelterId,
    breed_primary: data.breed || null,
    age_text: data.age || null,
    sex: data.sex || null,
    size_general: data.size || null,
    description: data.description || null,
    photo_url: data.photo_url || null,
    is_available: true,
    status: "available",
    intake_date: data.intake_date || new Date().toISOString(),
    external_id: data.external_id || null,
    external_source: "webhook",
    date_confidence: data.intake_date ? "verified" : "high",
    date_source: "webhook",
  });
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function processDogUpdated(supabase: any, shelterId: string, data: Record<string, unknown>) {
  if (!data.external_id && !data.id) return;

  const query = data.id
    ? supabase.from("dogs").update(data).eq("id", data.id).eq("shelter_id", shelterId)
    : supabase.from("dogs").update(data).eq("external_id", data.external_id).eq("shelter_id", shelterId);

  await query;
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function processDogAdopted(supabase: any, shelterId: string, data: Record<string, unknown>) {
  const identifier = data.id || data.external_id;
  if (!identifier) return;

  const field = data.id ? "id" : "external_id";
  await supabase
    .from("dogs")
    .update({ is_available: false, status: "adopted" })
    .eq(field, identifier)
    .eq("shelter_id", shelterId);
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function processEuthanasiaScheduled(supabase: any, shelterId: string, data: Record<string, unknown>) {
  const identifier = data.id || data.external_id;
  if (!identifier || !data.euthanasia_date) return;

  const field = data.id ? "id" : "external_id";
  await supabase
    .from("dogs")
    .update({
      euthanasia_date: data.euthanasia_date,
      urgency_level: "critical",
    })
    .eq(field, identifier)
    .eq("shelter_id", shelterId);
}

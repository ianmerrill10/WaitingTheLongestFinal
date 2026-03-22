import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { randomBytes } from "crypto";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

function getShelterId(request: NextRequest): string | null {
  return request.cookies.get("wtl_shelter_id")?.value || null;
}

function getSession(request: NextRequest): string | null {
  return request.cookies.get("wtl_session")?.value || null;
}

const VALID_EVENTS = [
  "dog.created",
  "dog.updated",
  "dog.adopted",
  "dog.urgent",
  "dog.viewed",
  "dog.inquiry_received",
  "dog.shared",
  "account.updated",
];

/**
 * POST /api/partners/webhooks — Register a new webhook endpoint
 */
export async function POST(request: NextRequest) {
  const shelterId = getShelterId(request);
  const session = getSession(request);
  if (!shelterId || !session) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json({ error: "Invalid request body" }, { status: 400 });
  }

  const url = String(body.url || "").trim();
  if (!url || !url.startsWith("https://")) {
    return NextResponse.json(
      { error: "Webhook URL must start with https://" },
      { status: 400 }
    );
  }

  const events = Array.isArray(body.events)
    ? body.events.filter((e: string) => VALID_EVENTS.includes(e))
    : VALID_EVENTS;

  if (events.length === 0) {
    return NextResponse.json(
      { error: "At least one valid event is required" },
      { status: 400 }
    );
  }

  const supabase = createAdminClient();

  // Check endpoint limit (max 5 per shelter)
  const { count } = await supabase
    .from("webhook_endpoints")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .eq("is_active", true);

  if ((count || 0) >= 5) {
    return NextResponse.json(
      { error: "Maximum 5 webhook endpoints per shelter" },
      { status: 400 }
    );
  }

  const secret = "whsec_" + randomBytes(24).toString("hex");

  const { data, error } = await supabase
    .from("webhook_endpoints")
    .insert({
      shelter_id: shelterId,
      url,
      events,
      secret,
      is_active: true,
    })
    .select("id")
    .single();

  if (error) {
    console.error("Failed to create webhook:", error);
    return NextResponse.json(
      { error: "Failed to create webhook endpoint" },
      { status: 500 }
    );
  }

  return NextResponse.json({
    success: true,
    id: data.id,
    secret,
    message: "Save the webhook secret now — it will not be shown again.",
  });
}

/**
 * DELETE /api/partners/webhooks — Remove a webhook endpoint
 */
export async function DELETE(request: NextRequest) {
  const shelterId = getShelterId(request);
  const session = getSession(request);
  if (!shelterId || !session) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const webhookId = request.nextUrl.searchParams.get("id");
  if (!webhookId) {
    return NextResponse.json({ error: "Webhook ID required" }, { status: 400 });
  }

  const supabase = createAdminClient();

  const { error } = await supabase
    .from("webhook_endpoints")
    .update({ is_active: false })
    .eq("id", webhookId)
    .eq("shelter_id", shelterId);

  if (error) {
    return NextResponse.json(
      { error: "Failed to delete webhook" },
      { status: 500 }
    );
  }

  return NextResponse.json({ success: true });
}

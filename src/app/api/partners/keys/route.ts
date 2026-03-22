import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { createHash, randomBytes } from "crypto";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const KEY_PREFIX = "wtl_sk_";

function getShelterId(request: NextRequest): string | null {
  return request.cookies.get("wtl_shelter_id")?.value || null;
}

function getSession(request: NextRequest): string | null {
  return request.cookies.get("wtl_session")?.value || null;
}

/**
 * POST /api/partners/keys — Generate a new API key
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

  const label = String(body.label || "Default Key").slice(0, 100);
  const environment = body.environment === "production" ? "production" : "sandbox";
  const scopes = Array.isArray(body.scopes) ? body.scopes : ["dogs:read"];

  const supabase = createAdminClient();

  // Check key limit (max 5 per shelter)
  const { count } = await supabase
    .from("shelter_api_keys")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .eq("is_active", true);

  if ((count || 0) >= 5) {
    return NextResponse.json(
      { error: "Maximum 5 active API keys per shelter" },
      { status: 400 }
    );
  }

  // Generate key
  const rawKey = KEY_PREFIX + randomBytes(32).toString("hex");
  const keyHash = createHash("sha256").update(rawKey).digest("hex");
  const keyPrefix = rawKey.substring(0, 14);

  const { error } = await supabase.from("shelter_api_keys").insert({
    shelter_id: shelterId,
    key_hash: keyHash,
    key_prefix: keyPrefix,
    label,
    environment,
    scopes,
    is_active: true,
    rate_limit_per_minute: 60,
    rate_limit_per_hour: 1000,
    rate_limit_per_day: 10000,
  });

  if (error) {
    console.error("Failed to create API key:", error);
    return NextResponse.json(
      { error: "Failed to create API key" },
      { status: 500 }
    );
  }

  return NextResponse.json({
    success: true,
    api_key: rawKey,
    key_prefix: keyPrefix,
    label,
    environment,
    scopes,
    message: "Save this key now — it will not be shown again.",
  });
}

/**
 * DELETE /api/partners/keys — Revoke an API key
 */
export async function DELETE(request: NextRequest) {
  const shelterId = getShelterId(request);
  const session = getSession(request);
  if (!shelterId || !session) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const keyId = request.nextUrl.searchParams.get("id");
  if (!keyId) {
    return NextResponse.json({ error: "Key ID required" }, { status: 400 });
  }

  const supabase = createAdminClient();

  const { error } = await supabase
    .from("shelter_api_keys")
    .update({ is_active: false, revoked_at: new Date().toISOString() })
    .eq("id", keyId)
    .eq("shelter_id", shelterId);

  if (error) {
    return NextResponse.json({ error: "Failed to revoke key" }, { status: 500 });
  }

  return NextResponse.json({ success: true });
}

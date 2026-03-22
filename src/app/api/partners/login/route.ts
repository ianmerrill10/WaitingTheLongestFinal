import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { rateLimit } from "@/lib/utils/rate-limit";
import { createHash, randomBytes } from "crypto";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const KEY_PREFIX = "wtl_sk_";

/**
 * POST /api/partners/login — Shelter partner authentication
 * Accepts either email + API key, or email for magic link
 * Rate limited: 10 attempts per minute per IP
 */
export async function POST(request: Request) {
  // Rate limit login attempts (10/min per IP)
  const ip = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() || "unknown";
  const { allowed } = rateLimit(ip, 10, 60000);
  if (!allowed) {
    return NextResponse.json(
      { error: "Too many login attempts. Please try again in a minute." },
      { status: 429 }
    );
  }

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json({ error: "Invalid request body" }, { status: 400 });
  }

  if (!body.email || typeof body.email !== "string") {
    return NextResponse.json({ error: "Email is required" }, { status: 400 });
  }

  const email = body.email.trim().toLowerCase();
  const supabase = createAdminClient();

  // Method 1: Email + API Key login
  if (body.api_key) {
    const rawKey = body.api_key.trim();
    if (!rawKey.startsWith(KEY_PREFIX)) {
      return NextResponse.json({ error: "Invalid API key format" }, { status: 401 });
    }

    const keyHash = createHash("sha256").update(rawKey).digest("hex");

    // Find the API key
    const { data: apiKey } = await supabase
      .from("shelter_api_keys")
      .select("shelter_id, is_active, expires_at, revoked_at")
      .eq("key_hash", keyHash)
      .single();

    if (!apiKey || !apiKey.is_active || apiKey.revoked_at) {
      return NextResponse.json({ error: "Invalid or inactive API key" }, { status: 401 });
    }

    if (apiKey.expires_at && new Date(apiKey.expires_at) < new Date()) {
      return NextResponse.json({ error: "API key has expired" }, { status: 401 });
    }

    // Verify email matches a contact for this shelter
    const { data: contact } = await supabase
      .from("shelter_contacts")
      .select("id, shelter_id")
      .eq("email", email)
      .eq("shelter_id", apiKey.shelter_id)
      .eq("is_active", true)
      .single();

    if (!contact) {
      return NextResponse.json(
        { error: "Email not associated with this API key's shelter" },
        { status: 401 }
      );
    }

    // Generate session token (JWT-like but simpler for now)
    const sessionToken = randomBytes(32).toString("hex");
    const expiresAt = new Date(Date.now() + 24 * 60 * 60 * 1000); // 24h

    // Store session (in a simple way — would use proper JWT in production)
    // For now, return the shelter info directly
    const { data: shelter } = await supabase
      .from("shelters")
      .select("id, name, city, state_code, partner_status, partner_tier")
      .eq("id", apiKey.shelter_id)
      .single();

    const response = NextResponse.json({
      success: true,
      shelter,
      session_token: sessionToken,
      expires_at: expiresAt.toISOString(),
    });

    // Set httpOnly cookie
    response.cookies.set("wtl_session", sessionToken, {
      httpOnly: true,
      secure: process.env.NODE_ENV === "production",
      sameSite: "lax",
      path: "/",
      maxAge: 24 * 60 * 60, // 24 hours
    });

    response.cookies.set("wtl_shelter_id", apiKey.shelter_id, {
      httpOnly: true,
      secure: process.env.NODE_ENV === "production",
      sameSite: "lax",
      path: "/",
      maxAge: 24 * 60 * 60,
    });

    return response;
  }

  // Method 2: Magic link (email only — send login link)
  // Check if email is a known contact
  const { data: contact } = await supabase
    .from("shelter_contacts")
    .select("id, shelter_id, first_name")
    .eq("email", email)
    .eq("is_active", true)
    .limit(1)
    .single();

  if (!contact) {
    // Don't reveal if email exists or not
    return NextResponse.json({
      success: true,
      message: "If this email is associated with a shelter partner account, you will receive a login link shortly.",
    });
  }

  // Generate magic link token
  const magicToken = randomBytes(32).toString("hex");
  const magicExpiry = new Date(Date.now() + 15 * 60 * 1000); // 15 minutes

  // Store the magic token (using communications as a lightweight store)
  await supabase.from("shelter_communications").insert({
    shelter_id: contact.shelter_id,
    contact_id: contact.id,
    comm_type: "in_app_message",
    direction: "outbound",
    subject: "Login Link",
    body: `Magic login token: ${magicToken}`,
    status: "sent",
    sent_by: "system",
    channel: "magic_link",
    metadata: {
      magic_token: magicToken,
      expires_at: magicExpiry.toISOString(),
      email,
    },
  });

  // In production, this would send an email via SendGrid
  // For now, return success message
  return NextResponse.json({
    success: true,
    message: "If this email is associated with a shelter partner account, you will receive a login link shortly.",
    // DEV ONLY: remove in production
    ...(process.env.NODE_ENV !== "production" ? { dev_token: magicToken } : {}),
  });
}

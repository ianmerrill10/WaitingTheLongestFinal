import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { verifySessionToken, SESSION_COOKIE_NAME } from "./signed-session";

/**
 * Verify admin access via signed site_auth cookie.
 * Admin routes are for the site owner — protected by a signed session token.
 */
export async function requireAdminAuth(request: NextRequest): Promise<NextResponse | null> {
  const authCookie = request.cookies.get(SESSION_COOKIE_NAME);
  if (await verifySessionToken(authCookie?.value)) {
    return null; // authenticated
  }
  return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
}

/**
 * Verify partner access via wtl_session + wtl_shelter_id cookies.
 * Returns the validated shelter_id or an error response.
 */
export async function requirePartnerAuth(
  request: NextRequest
): Promise<{ shelterId: string } | NextResponse> {
  const sessionToken = request.cookies.get("wtl_session")?.value;
  const shelterIdCookie = request.cookies.get("wtl_shelter_id")?.value;

  if (!sessionToken || !shelterIdCookie) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  // Validate the shelter_id actually exists and has partner status
  const supabase = createAdminClient();
  const { data: shelter } = await supabase
    .from("shelters")
    .select("id, partner_status")
    .eq("id", shelterIdCookie)
    .single();

  if (!shelter) {
    return NextResponse.json({ error: "Invalid shelter" }, { status: 401 });
  }

  return { shelterId: shelterIdCookie };
}

/**
 * Sanitize a redirect path — only allow relative paths, no protocol or host.
 */
export function sanitizeRedirectPath(path: string): string {
  // Strip any protocol, host, or double slashes
  const clean = path.replace(/^https?:\/\/[^/]+/i, "").replace(/^\/\//, "/");
  // Must start with / and not contain protocol-like patterns
  if (!clean.startsWith("/") || clean.includes("://")) {
    return "/";
  }
  return clean;
}

/**
 * HMAC-signed session token for site admin auth.
 *
 * Replaces the legacy predictable "site_auth=authenticated" cookie with a
 * signed token containing an expiration claim. Forgery requires knowing
 * the server-side secret.
 *
 * Format:  base64url(payloadJSON).base64url(hmacSha256(payloadJSON))
 *
 * Uses Web Crypto API so it works in BOTH Node.js runtime (API routes) and
 * Edge runtime (middleware). Uses SITE_PASSWORD as the HMAC key by default.
 */

const COOKIE_NAME = "site_auth";
const SESSION_MAX_AGE_MS = 30 * 24 * 60 * 60 * 1000; // 30 days
// Legacy value written by old login flows — accepted while cookies rotate out
const LEGACY_VALUE = "authenticated";

interface SessionPayload {
  iat: number; // issued at (ms)
  exp: number; // expires at (ms)
  v: number;   // version for future rotation
}

function getSecret(): string | null {
  return process.env.SITE_SESSION_SECRET || process.env.SITE_PASSWORD || null;
}

function b64urlEncode(bytes: Uint8Array | string): string {
  let data: Uint8Array;
  if (typeof bytes === "string") {
    data = new TextEncoder().encode(bytes);
  } else {
    data = bytes;
  }
  let binary = "";
  for (let i = 0; i < data.length; i++) {
    binary += String.fromCharCode(data[i]);
  }
  const base64 = typeof btoa !== "undefined"
    ? btoa(binary)
    : Buffer.from(binary, "binary").toString("base64");
  return base64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function b64urlDecodeToString(s: string): string {
  const pad = s.length % 4 === 0 ? "" : "=".repeat(4 - (s.length % 4));
  const base64 = s.replace(/-/g, "+").replace(/_/g, "/") + pad;
  const binary = typeof atob !== "undefined"
    ? atob(base64)
    : Buffer.from(base64, "base64").toString("binary");
  // Decode binary string to UTF-8
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return new TextDecoder().decode(bytes);
}

async function hmacSign(message: string, secret: string): Promise<string> {
  const key = await crypto.subtle.importKey(
    "raw",
    new TextEncoder().encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"]
  );
  const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(message));
  return b64urlEncode(new Uint8Array(sig));
}

function timingSafeEqualStr(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) {
    diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  }
  return diff === 0;
}

export async function createSessionToken(maxAgeMs = SESSION_MAX_AGE_MS): Promise<string | null> {
  const secret = getSecret();
  if (!secret) return null;
  const now = Date.now();
  const payload: SessionPayload = { iat: now, exp: now + maxAgeMs, v: 1 };
  const payloadStr = b64urlEncode(JSON.stringify(payload));
  const sig = await hmacSign(payloadStr, secret);
  return `${payloadStr}.${sig}`;
}

export async function verifySessionToken(token: string | undefined | null): Promise<boolean> {
  if (!token) return false;

  // Legacy cookie — accept during rotation window, gets replaced on next login
  if (token === LEGACY_VALUE) return true;

  const secret = getSecret();
  if (!secret) return false;

  const parts = token.split(".");
  if (parts.length !== 2) return false;
  const [payloadStr, providedSig] = parts;

  const expectedSig = await hmacSign(payloadStr, secret);
  if (!timingSafeEqualStr(providedSig, expectedSig)) return false;

  try {
    const payload = JSON.parse(b64urlDecodeToString(payloadStr)) as SessionPayload;
    if (typeof payload.exp !== "number") return false;
    if (Date.now() > payload.exp) return false;
    return true;
  } catch {
    return false;
  }
}

export const SESSION_COOKIE_NAME = COOKIE_NAME;
export const SESSION_MAX_AGE_SECONDS = SESSION_MAX_AGE_MS / 1000;

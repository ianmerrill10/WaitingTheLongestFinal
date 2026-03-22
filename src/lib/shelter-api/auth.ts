import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { createHash } from "crypto";
import type { ShelterApiKey, ApiKeyScope } from "@/types/shelter-crm";

const KEY_PREFIX = "wtl_sk_";

interface AuthResult {
  authenticated: boolean;
  key?: ShelterApiKey;
  shelter_id?: string;
  error?: string;
  status?: number;
}

/**
 * Extract and validate API key from Authorization header.
 * Returns shelter_id and key details on success.
 */
export async function authenticateApiKey(
  request: Request
): Promise<AuthResult> {
  const authHeader = request.headers.get("authorization");

  if (!authHeader) {
    return {
      authenticated: false,
      error: "Missing Authorization header. Use: Authorization: Bearer wtl_sk_...",
      status: 401,
    };
  }

  const parts = authHeader.split(" ");
  if (parts.length !== 2 || parts[0] !== "Bearer") {
    return {
      authenticated: false,
      error: "Invalid Authorization format. Use: Authorization: Bearer wtl_sk_...",
      status: 401,
    };
  }

  const rawKey = parts[1];
  if (!rawKey.startsWith(KEY_PREFIX)) {
    return {
      authenticated: false,
      error: "Invalid API key format",
      status: 401,
    };
  }

  const keyHash = createHash("sha256").update(rawKey).digest("hex");
  const supabase = createAdminClient();

  const { data, error } = await supabase
    .from("shelter_api_keys")
    .select("*")
    .eq("key_hash", keyHash)
    .eq("is_active", true)
    .single();

  if (error || !data) {
    return {
      authenticated: false,
      error: "Invalid or inactive API key",
      status: 401,
    };
  }

  const key = data as ShelterApiKey;

  // Check expiration
  if (key.expires_at && new Date(key.expires_at) < new Date()) {
    return {
      authenticated: false,
      error: "API key has expired",
      status: 401,
    };
  }

  // Check revocation
  if (key.revoked_at) {
    return {
      authenticated: false,
      error: "API key has been revoked",
      status: 401,
    };
  }

  // Update last_used_at and usage counters
  const ip = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() || "";
  await supabase
    .from("shelter_api_keys")
    .update({
      last_used_at: new Date().toISOString(),
      last_used_ip: ip,
      usage_count_today: key.usage_count_today + 1,
      usage_count_total: (key.usage_count_total || 0) + 1,
    })
    .eq("id", key.id);

  return {
    authenticated: true,
    key,
    shelter_id: key.shelter_id,
  };
}

/**
 * Check if key has a required scope.
 */
export function hasScope(key: ShelterApiKey, scope: string): boolean {
  return key.scopes?.includes(scope as ApiKeyScope) ?? false;
}

/**
 * Helper: return 401/403 error response.
 */
export function authError(result: AuthResult): NextResponse {
  return NextResponse.json(
    { error: result.error, code: "AUTH_ERROR" },
    { status: result.status || 401 }
  );
}

/**
 * Helper: return 403 for missing scope.
 */
export function scopeError(scope: string): NextResponse {
  return NextResponse.json(
    {
      error: `Insufficient permissions. Required scope: ${scope}`,
      code: "INSUFFICIENT_SCOPE",
    },
    { status: 403 }
  );
}

/**
 * Log an API request for audit trail.
 */
export async function logApiRequest(options: {
  shelter_id: string;
  api_key_id: string;
  method: string;
  path: string;
  query_params?: Record<string, unknown>;
  status_code: number;
  request_body_size?: number;
  response_body_size?: number;
  response_time_ms?: number;
  ip_address?: string;
  user_agent?: string;
  error_message?: string;
  rate_limited?: boolean;
}): Promise<void> {
  const supabase = createAdminClient();
  await supabase.from("api_request_logs").insert({
    shelter_id: options.shelter_id,
    api_key_id: options.api_key_id,
    method: options.method,
    path: options.path,
    query_params: options.query_params || null,
    status_code: options.status_code,
    request_body_size: options.request_body_size || null,
    response_body_size: options.response_body_size || null,
    response_time_ms: options.response_time_ms || null,
    ip_address: options.ip_address || null,
    user_agent: options.user_agent || null,
    error_message: options.error_message || null,
    rate_limited: options.rate_limited || false,
  });
}

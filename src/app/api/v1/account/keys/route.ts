import { NextResponse } from "next/server";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
  logApiRequest,
} from "@/lib/shelter-api/auth";
import {
  checkApiKeyRateLimit,
  rateLimitHeaders,
} from "@/lib/shelter-api/rate-limiter";
import { getKeysByShelter, generateApiKey } from "@/lib/shelter-crm/api-keys";
import type { ApiKeyScope } from "@/types/shelter-crm";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/v1/account/keys — List API keys (prefix only, never full key)
 */
export async function GET(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "read")) return scopeError("read");

  try {
    const keys = await getKeysByShelter(auth.shelter_id!);
    const safeKeys = keys.map((k) => ({
      id: k.id,
      key_prefix: k.key_prefix,
      label: k.label,
      environment: k.environment,
      scopes: k.scopes,
      is_active: k.is_active,
      last_used_at: k.last_used_at,
      usage_count_today: k.usage_count_today,
      usage_count_total: k.usage_count_total,
      expires_at: k.expires_at,
      created_at: k.created_at,
    }));

    await logApiRequest({
      shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
      method: "GET", path: "/api/v1/account/keys",
      status_code: 200, response_time_ms: Date.now() - startTime,
    });

    return NextResponse.json({ data: safeKeys }, { headers: rateLimitHeaders(rateLimit) });
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : "Failed to fetch keys" },
      { status: 500 }
    );
  }
}

/**
 * POST /api/v1/account/keys — Generate new API key (returns full key ONCE)
 */
export async function POST(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "write")) return scopeError("write");

  let body = {};
  try {
    body = await request.json();
  } catch {
    // Body is optional
  }

  const input = body as Record<string, unknown>;

  try {
    const result = await generateApiKey({
      shelter_id: auth.shelter_id!,
      label: (input.label as string) || "Default",
      environment: (input.environment as "production" | "staging" | "development") || "production",
      scopes: (input.scopes as ApiKeyScope[]) || ["read", "write"] as ApiKeyScope[],
      created_by: `api_key:${auth.key!.key_prefix}`,
    });

    await logApiRequest({
      shelter_id: auth.shelter_id!, api_key_id: auth.key!.id,
      method: "POST", path: "/api/v1/account/keys",
      status_code: 201, response_time_ms: Date.now() - startTime,
    });

    return NextResponse.json(
      {
        data: {
          id: result.key.id,
          key_prefix: result.key.key_prefix,
          label: result.key.label,
          environment: result.key.environment,
          scopes: result.key.scopes,
          created_at: result.key.created_at,
        },
        api_key: result.raw_key,
        message: "API key generated. Save this key now — it will never be shown again.",
      },
      { status: 201, headers: rateLimitHeaders(rateLimit) }
    );
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : "Failed to generate key" },
      { status: 500 }
    );
  }
}

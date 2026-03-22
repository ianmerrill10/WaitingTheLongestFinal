import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
} from "@/lib/shelter-api/auth";
import {
  checkApiKeyRateLimit,
  rateLimitHeaders,
} from "@/lib/shelter-api/rate-limiter";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/v1/account/usage — API usage statistics
 */
export async function GET(request: Request) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json({ error: "Rate limit exceeded" }, { status: 429, headers: rateLimitHeaders(rateLimit) });
  }

  if (!hasScope(auth.key!, "read")) return scopeError("read");

  const supabase = createAdminClient();
  const now = new Date();
  const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate()).toISOString();
  const weekAgo = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000).toISOString();
  const monthAgo = new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000).toISOString();

  const [todayResult, weekResult, monthResult, byMethodResult, errorResult] = await Promise.all([
    supabase.from("api_request_logs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).gte("created_at", todayStart),
    supabase.from("api_request_logs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).gte("created_at", weekAgo),
    supabase.from("api_request_logs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).gte("created_at", monthAgo),
    supabase.from("api_request_logs").select("method").eq("shelter_id", auth.shelter_id!).gte("created_at", monthAgo),
    supabase.from("api_request_logs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).gte("status_code", 400).gte("created_at", weekAgo),
  ]);

  // Count by method
  const methodCounts: Record<string, number> = {};
  for (const row of byMethodResult.data || []) {
    methodCounts[row.method] = (methodCounts[row.method] || 0) + 1;
  }

  return NextResponse.json({
    data: {
      today: todayResult.count || 0,
      this_week: weekResult.count || 0,
      this_month: monthResult.count || 0,
      errors_this_week: errorResult.count || 0,
      by_method: methodCounts,
      rate_limits: {
        per_minute: auth.key!.rate_limit_per_minute,
        per_hour: auth.key!.rate_limit_per_hour,
        per_day: auth.key!.rate_limit_per_day,
      },
    },
  }, { headers: rateLimitHeaders(rateLimit) });
}

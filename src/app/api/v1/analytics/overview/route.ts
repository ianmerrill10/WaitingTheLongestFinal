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
 * GET /api/v1/analytics/overview — Views, inquiries, adoptions summary
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

  const [
    totalDogsResult,
    availableDogsResult,
    adoptedDogsResult,
    totalViewsResult,
    urgentResult,
  ] = await Promise.all([
    supabase.from("dogs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!),
    supabase.from("dogs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).eq("is_available", true),
    supabase.from("dogs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).eq("is_available", false),
    supabase.from("dogs").select("view_count").eq("shelter_id", auth.shelter_id!).eq("is_available", true),
    supabase.from("dogs").select("*", { count: "exact", head: true }).eq("shelter_id", auth.shelter_id!).eq("is_available", true).in("urgency_level", ["critical", "high"]),
  ]);

  const totalViews = (totalViewsResult.data || []).reduce(
    (sum, d) => sum + (d.view_count || 0), 0
  );

  return NextResponse.json({
    data: {
      total_dogs: totalDogsResult.count || 0,
      available_dogs: availableDogsResult.count || 0,
      adopted_dogs: adoptedDogsResult.count || 0,
      urgent_dogs: urgentResult.count || 0,
      total_views: totalViews,
      avg_views_per_dog: availableDogsResult.count
        ? Math.round(totalViews / availableDogsResult.count)
        : 0,
    },
  }, { headers: rateLimitHeaders(rateLimit) });
}

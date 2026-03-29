import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  try {
    const supabase = createAdminClient();

    // Get most recent stats — use .limit(1) instead of .single() to avoid error when no rows
    const { data: latestRows } = await supabase
      .from("daily_stats")
      .select("*")
      .order("stat_date", { ascending: false })
      .limit(1);

    const latest = latestRows?.[0];
    if (!latest) {
      return NextResponse.json({ current: null, trends: null });
    }

    // Get stats from 7 and 30 days ago for trends
    const sevenDaysAgo = new Date(Date.now() - 7 * 86400000).toISOString().split("T")[0];
    const thirtyDaysAgo = new Date(Date.now() - 30 * 86400000).toISOString().split("T")[0];

    const [weekAgoRes, monthAgoRes] = await Promise.all([
      supabase.from("daily_stats").select("avg_wait_days_high_confidence, stat_date")
        .lte("stat_date", sevenDaysAgo)
        .order("stat_date", { ascending: false })
        .limit(1),
      supabase.from("daily_stats").select("avg_wait_days_high_confidence, stat_date")
        .lte("stat_date", thirtyDaysAgo)
        .order("stat_date", { ascending: false })
        .limit(1),
    ]);

    const currentAvg = latest.avg_wait_days_high_confidence;
    const weekAgoAvg = weekAgoRes.data?.[0]?.avg_wait_days_high_confidence ?? null;
    const monthAgoAvg = monthAgoRes.data?.[0]?.avg_wait_days_high_confidence ?? null;

    return NextResponse.json({
      current: {
        avg_wait_days: currentAvg,
        total_dogs: latest.total_available_dogs,
        high_confidence_dogs: latest.high_confidence_dogs,
        median_wait_days: latest.median_wait_days,
        max_wait_days: latest.max_wait_days,
        date_accuracy_pct: latest.date_accuracy_pct,
        verification_coverage_pct: latest.verification_coverage_pct,
        stat_date: latest.stat_date,
      },
      trends: {
        week_ago: weekAgoAvg,
        month_ago: monthAgoAvg,
        week_change: currentAvg != null && weekAgoAvg != null
          ? Math.round((currentAvg - weekAgoAvg) * 100) / 100 : null,
        month_change: currentAvg != null && monthAgoAvg != null
          ? Math.round((currentAvg - monthAgoAvg) * 100) / 100 : null,
      },
    });
  } catch (err) {
    console.error("[StatsAPI] Failed to fetch stats:", err);
    return NextResponse.json({ current: null, trends: null }, { status: 500 });
  }
}

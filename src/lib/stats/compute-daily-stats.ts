import { createAdminClient } from "@/lib/supabase/admin";

export interface DailyStatsResult {
  stat_date: string;
  total_available_dogs: number;
  high_confidence_dogs: number;
  avg_wait_days_all: number | null;
  avg_wait_days_high_confidence: number | null;
  median_wait_days: number | null;
  max_wait_days: number | null;
  dogs_with_birth_date: number;
  verification_coverage_pct: number;
  date_accuracy_pct: number;
  dogs_added_today: number;
  dogs_adopted_today: number;
}

export async function computeAndStoreDailyStats(): Promise<DailyStatsResult> {
  const supabase = createAdminClient();
  const today = new Date().toISOString().split("T")[0]; // YYYY-MM-DD
  const now = new Date();
  const todayStart = new Date(today + "T00:00:00Z");

  // Run all count queries in parallel
  const [
    totalRes,
    highConfRes,
    verifiedRes,
    birthDateRes,
    addedTodayRes,
    adoptedTodayRes,
  ] = await Promise.all([
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true)
      .in("date_confidence", ["verified", "high"]),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true)
      .not("last_verified_at", "is", null),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true)
      .not("birth_date", "is", null),
    supabase.from("dogs").select("id", { count: "exact", head: true })
      .gte("created_at", todayStart.toISOString()),
    supabase.from("dogs").select("id", { count: "exact", head: true })
      .eq("status", "adopted")
      .gte("updated_at", todayStart.toISOString()),
  ]);

  const totalDogs = totalRes.count ?? 0;
  const highConfDogs = highConfRes.count ?? 0;

  // Fetch intake dates for average computation
  // Use high-confidence dates for the primary average (more meaningful)
  const { data: highConfIntakes } = await supabase
    .from("dogs")
    .select("intake_date")
    .eq("is_available", true)
    .in("date_confidence", ["verified", "high"])
    .not("intake_date", "is", null);

  // Also compute all-dogs average
  const { data: allIntakes } = await supabase
    .from("dogs")
    .select("intake_date")
    .eq("is_available", true)
    .not("intake_date", "is", null);

  const computeStats = (intakes: { intake_date: string }[] | null): { avg: number | null; median: number | null; max: number | null } => {
    if (!intakes || intakes.length === 0) return { avg: null, median: null, max: null };
    const days = intakes.map(d =>
      Math.max(0, (now.getTime() - new Date(d.intake_date).getTime()) / 86400000)
    ).sort((a, b) => a - b);
    const avg = days.reduce((sum, d) => sum + d, 0) / days.length;
    const median = days[Math.floor(days.length / 2)];
    const max = days[days.length - 1];
    return {
      avg: Math.round(avg * 100) / 100,
      median: Math.round(median * 100) / 100,
      max: Math.round(max),
    };
  };

  const allStats = computeStats(allIntakes);
  const highConfStats = computeStats(highConfIntakes);

  const result: DailyStatsResult = {
    stat_date: today,
    total_available_dogs: totalDogs,
    high_confidence_dogs: highConfDogs,
    avg_wait_days_all: allStats.avg,
    avg_wait_days_high_confidence: highConfStats.avg,
    median_wait_days: highConfStats.median,
    max_wait_days: allStats.max,
    dogs_with_birth_date: birthDateRes.count ?? 0,
    verification_coverage_pct: totalDogs > 0
      ? Math.round(((verifiedRes.count ?? 0) / totalDogs) * 10000) / 100
      : 0,
    date_accuracy_pct: totalDogs > 0
      ? Math.round((highConfDogs / totalDogs) * 10000) / 100
      : 0,
    dogs_added_today: addedTodayRes.count ?? 0,
    dogs_adopted_today: adoptedTodayRes.count ?? 0,
  };

  // Upsert into daily_stats (in case cron runs twice on same day)
  await supabase.from("daily_stats").upsert(result, { onConflict: "stat_date" });

  return result;
}

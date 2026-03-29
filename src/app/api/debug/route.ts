/**
 * Debug Dashboard — Full site health and diagnostics
 *
 * Returns a comprehensive JSON dashboard with:
 * - Database connectivity + latency
 * - Dog counts (total, available, by urgency, by verification status)
 * - Date accuracy stats (confidence breakdown)
 * - Verification progress + coverage percentage
 * - Shelter stats
 * - RescueGroups API health
 * - Sync recency
 * - Environment variable status
 * - Overall health score (0-100)
 *
 * Optional params:
 *   ?fix=bad_dates     — repair intake dates >5 years old
 *   ?fix=stale_verify  — re-verify stale dogs (>30 days since verification)
 */
import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { RESCUEGROUPS_API_BASE } from "@/lib/constants";
import { getVerificationStats } from "@/lib/verification/engine";

export const runtime = "nodejs";
export const maxDuration = 60;

const ENV_VARS_TO_CHECK = [
  "NEXT_PUBLIC_SUPABASE_URL",
  "NEXT_PUBLIC_SUPABASE_ANON_KEY",
  "SUPABASE_SERVICE_ROLE_KEY",
  "RESCUEGROUPS_API_KEY",
  "CRON_SECRET",
  "THE_DOG_API_KEY",
  "SENDGRID_API_KEY",
];

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const fixParam = url.searchParams.get("fix");
  const startTime = Date.now();

  const dashboard: Record<string, unknown> = {
    timestamp: new Date().toISOString(),
    version: "2.0",
  };

  let supabase;
  try {
    supabase = createAdminClient();
    const dbStart = Date.now();
    const { error } = await supabase.from("states").select("code").limit(1);
    dashboard.database = {
      connected: !error,
      error: error ? error.message : null,
      latency_ms: Date.now() - dbStart,
    };
  } catch (err) {
    dashboard.database = { connected: false, error: String(err) };
    dashboard.environment = buildEnvCheck();
    dashboard.health_score = 0;
    dashboard.total_time_ms = Date.now() - startTime;
    return NextResponse.json(dashboard, { status: 500 });
  }

  // Run all queries in parallel for speed
  const [
    dogCounts,
    urgencyCounts,
    dateCounts,
    verificationStats,
    shelterCounts,
    lastSyncData,
    photoCount,
    auditRunData,
    fosterCount,
    rgApiHealth,
  ] = await Promise.all([
    getDogCounts(supabase),
    getUrgencyCounts(supabase),
    getDateConfidenceCounts(supabase),
    getVerificationStats().catch(() => null),
    getShelterCounts(supabase),
    getLastSync(supabase),
    getPhotoMissing(supabase),
    getLastAuditRun(supabase),
    getFosterApplicationCount(supabase),
    checkRescueGroupsApi(),
  ]);

  // ── Dog Overview ──
  dashboard.dogs = dogCounts;

  // ── Urgency Breakdown ──
  dashboard.urgency = urgencyCounts;

  // ── Date Accuracy ──
  dashboard.date_accuracy = {
    ...dateCounts,
    accuracy_score: dateCounts.total > 0
      ? Math.round(((dateCounts.verified + dateCounts.high) / dateCounts.total) * 100)
      : 0,
  };

  // ── Verification Status ──
  if (verificationStats) {
    const verificationPct = verificationStats.total > 0
      ? Math.round((verificationStats.verified / verificationStats.total) * 100)
      : 0;
    dashboard.verification = {
      ...verificationStats,
      coverage_pct: verificationPct,
      days_to_full_coverage: verificationStats.neverChecked > 0
        ? Math.ceil(verificationStats.neverChecked / 200) // ~200/day across both crons
        : 0,
    };
  }

  // ── Shelters ──
  dashboard.shelters = shelterCounts;

  // ── Photos ──
  dashboard.photos = {
    available_without_photo: photoCount,
    pct_with_photos: dogCounts.available > 0
      ? Math.round(((dogCounts.available - photoCount) / dogCounts.available) * 100)
      : 0,
  };

  // ── Sync Recency ──
  const syncAgeHours = lastSyncData.last_synced_at
    ? Math.round((Date.now() - new Date(lastSyncData.last_synced_at).getTime()) / (1000 * 60 * 60))
    : null;
  dashboard.sync = {
    last_synced_at: lastSyncData.last_synced_at,
    hours_ago: syncAgeHours,
    status: syncAgeHours === null ? "never" : syncAgeHours <= 26 ? "healthy" : syncAgeHours <= 50 ? "stale" : "critical",
  };

  // ── Last Audit Run ──
  dashboard.last_audit = auditRunData;

  // ── Foster Applications ──
  dashboard.foster_applications = { total: fosterCount };

  // ── RescueGroups API ──
  dashboard.rescuegroups_api = rgApiHealth;

  // ── Environment ──
  dashboard.environment = buildEnvCheck();

  // ── Overall Health Score (0-100) ──
  let healthScore = 100;
  // Database down = -50
  if (!(dashboard.database as { connected: boolean }).connected) healthScore -= 50;
  // RG API down = -20
  if (!rgApiHealth.connected) healthScore -= 20;
  // Sync stale >26h = -10, >50h = -20
  if (syncAgeHours && syncAgeHours > 50) healthScore -= 20;
  else if (syncAgeHours && syncAgeHours > 26) healthScore -= 10;
  // Low date accuracy (<50%) = -10
  const dateScore = (dashboard.date_accuracy as { accuracy_score: number }).accuracy_score;
  if (dateScore < 50) healthScore -= 10;
  // Low verification coverage (<10%) = -5
  if (verificationStats && verificationStats.total > 0) {
    const vPct = Math.round((verificationStats.verified / verificationStats.total) * 100);
    if (vPct < 10) healthScore -= 5;
  }
  // Many dogs without photos (>20%) = -5
  const photoPct = (dashboard.photos as { pct_with_photos: number }).pct_with_photos;
  if (photoPct < 80) healthScore -= 5;

  dashboard.health_score = Math.max(0, healthScore);
  dashboard.health_status = healthScore >= 90 ? "excellent" : healthScore >= 70 ? "good" : healthScore >= 50 ? "degraded" : "critical";

  // ── Fix tools ──
  if (fixParam === "bad_dates") {
    dashboard.fix_result = await fixBadDates(supabase);
  }

  if (fixParam === "seed_stats") {
    const { computeAndStoreDailyStats } = await import("@/lib/stats/compute-daily-stats");
    dashboard.fix_result = await computeAndStoreDailyStats();
  }

  if (fixParam === "run_migration") {
    dashboard.fix_result = await runMigration010(supabase);
  }

  dashboard.total_time_ms = Date.now() - startTime;

  // ── Automation Schedule ──
  dashboard.automation = {
    morning_agent: {
      schedule: "6:00 AM UTC daily",
      tasks: ["Sync 50 pages from RescueGroups", "Verify 200 dogs (140 new + 60 re-check)", "Quick audit (dates, staleness, statuses)", "Compute daily stats"],
    },
    evening_agent: {
      schedule: "6:00 PM UTC daily",
      tasks: ["Update urgency levels", "Verify 200 dogs (oldest)", "Parse description dates", "Age sanity check"],
    },
    daily_verification_rate: "~400 dogs/day (parallel batches of 5)",
    endpoints: {
      sync: "/api/cron/sync-dogs",
      urgency: "/api/cron/update-urgency",
      verify: "/api/verify-dogs?batch=N&priority=oldest|never_verified|stale",
      audit: "/api/cron/audit-dogs?mode=full|quick|dates_only|description_dates",
      debug: "/api/debug",
    },
  };

  return NextResponse.json(dashboard);
}

// ── Helper functions ──

async function getDogCounts(supabase: ReturnType<typeof createAdminClient>) {
  const [total, available, unavailable] = await Promise.all([
    supabase.from("dogs").select("id", { count: "exact", head: true }),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", false),
  ]);
  return {
    total: total.count ?? 0,
    available: available.count ?? 0,
    unavailable: unavailable.count ?? 0,
  };
}

async function getUrgencyCounts(supabase: ReturnType<typeof createAdminClient>) {
  const [critical, high, medium, normal] = await Promise.all([
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "critical"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "high"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "medium"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "normal"),
  ]);
  return {
    critical: critical.count ?? 0,
    high: high.count ?? 0,
    medium: medium.count ?? 0,
    normal: normal.count ?? 0,
  };
}

async function getDateConfidenceCounts(supabase: ReturnType<typeof createAdminClient>) {
  const [verified, high, medium, low, unknown] = await Promise.all([
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "verified"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "high"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "medium"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "low"),
    supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "unknown"),
  ]);
  const total = (verified.count ?? 0) + (high.count ?? 0) + (medium.count ?? 0) + (low.count ?? 0) + (unknown.count ?? 0);
  return {
    total,
    verified: verified.count ?? 0,
    high: high.count ?? 0,
    medium: medium.count ?? 0,
    low: low.count ?? 0,
    unknown: unknown.count ?? 0,
  };
}

async function getShelterCounts(supabase: ReturnType<typeof createAdminClient>) {
  const [total, active, withDogs] = await Promise.all([
    supabase.from("shelters").select("id", { count: "exact", head: true }),
    supabase.from("shelters").select("id", { count: "exact", head: true }).eq("is_active", true),
    supabase.from("shelters").select("id", { count: "exact", head: true }).gt("dog_count", 0),
  ]);
  return {
    total: total.count ?? 0,
    active: active.count ?? 0,
    with_dogs: withDogs.count ?? 0,
  };
}

async function getLastSync(supabase: ReturnType<typeof createAdminClient>) {
  const { data } = await supabase
    .from("dogs")
    .select("last_synced_at")
    .not("last_synced_at", "is", null)
    .order("last_synced_at", { ascending: false })
    .limit(1)
    .single();
  return { last_synced_at: data?.last_synced_at ?? null };
}

async function getPhotoMissing(supabase: ReturnType<typeof createAdminClient>) {
  const { count } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .is("primary_photo_url", null);
  return count ?? 0;
}

async function getLastAuditRun(supabase: ReturnType<typeof createAdminClient>) {
  try {
    const { data } = await supabase
      .from("audit_runs")
      .select("id, mode, trigger_source, started_at, completed_at, results")
      .order("started_at", { ascending: false })
      .limit(1)
      .single();

    if (!data) return { last_run: null };
    const hoursAgo = Math.round((Date.now() - new Date(data.started_at).getTime()) / (1000 * 60 * 60));
    return {
      last_run: data.started_at,
      hours_ago: hoursAgo,
      mode: data.mode,
      trigger: data.trigger_source,
      status: data.completed_at ? "completed" : "running",
    };
  } catch {
    return { last_run: null };
  }
}

async function getFosterApplicationCount(supabase: ReturnType<typeof createAdminClient>) {
  try {
    const { count } = await supabase
      .from("foster_applications")
      .select("id", { count: "exact", head: true });
    return count ?? 0;
  } catch {
    return 0;
  }
}

async function checkRescueGroupsApi() {
  const apiKey = process.env.RESCUEGROUPS_API_KEY;
  if (!apiKey) return { connected: false, status: "no_api_key" };

  try {
    const start = Date.now();
    const response = await fetch(
      `${RESCUEGROUPS_API_BASE}/public/animals/search/available/dogs/?limit=1`,
      {
        headers: {
          Authorization: apiKey,
          "Content-Type": "application/vnd.api+json",
        },
        signal: AbortSignal.timeout(10000),
      }
    );
    return {
      connected: response.ok,
      status_code: response.status,
      latency_ms: Date.now() - start,
    };
  } catch (err) {
    return { connected: false, error: String(err) };
  }
}

async function runMigration010(supabase: ReturnType<typeof createAdminClient>) {
  const results: string[] = [];
  try {
    // Test if columns already exist by querying them
    const { error: testErr } = await supabase
      .from("dogs")
      .select("birth_date")
      .limit(1);

    if (!testErr) {
      results.push("birth_date column already exists");
    } else {
      results.push("birth_date column needs to be added via SQL editor");
    }

    // Test if daily_stats table exists
    const { error: statsErr } = await supabase
      .from("daily_stats")
      .select("stat_date")
      .limit(1);

    if (!statsErr) {
      results.push("daily_stats table already exists");
    } else {
      results.push("daily_stats table needs to be created via SQL editor");
    }

    return {
      results,
      sql_to_run: `
-- Migration 010: Birth date, courtesy listing, and daily stats
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS birth_date TIMESTAMPTZ;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_birth_date_exact BOOLEAN DEFAULT NULL;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_courtesy_listing BOOLEAN DEFAULT FALSE;
CREATE INDEX IF NOT EXISTS idx_dogs_birth_date ON dogs(birth_date) WHERE birth_date IS NOT NULL;

CREATE TABLE IF NOT EXISTS daily_stats (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  stat_date DATE NOT NULL UNIQUE,
  total_available_dogs INTEGER NOT NULL DEFAULT 0,
  high_confidence_dogs INTEGER NOT NULL DEFAULT 0,
  avg_wait_days_all DECIMAL(10,2),
  avg_wait_days_high_confidence DECIMAL(10,2),
  median_wait_days DECIMAL(10,2),
  max_wait_days INTEGER,
  dogs_with_birth_date INTEGER DEFAULT 0,
  verification_coverage_pct DECIMAL(5,2),
  date_accuracy_pct DECIMAL(5,2),
  dogs_added_today INTEGER DEFAULT 0,
  dogs_adopted_today INTEGER DEFAULT 0,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_daily_stats_date ON daily_stats(stat_date DESC);
      `,
    };
  } catch (err) {
    return { results, error: String(err) };
  }
}

async function fixBadDates(supabase: ReturnType<typeof createAdminClient>) {
  const fiveYearsAgo = new Date();
  fiveYearsAgo.setFullYear(fiveYearsAgo.getFullYear() - 5);

  const { data: badDogs } = await supabase
    .from("dogs")
    .select("id, intake_date, original_intake_date, last_synced_at, created_at")
    .lt("intake_date", fiveYearsAgo.toISOString());

  if (!badDogs || badDogs.length === 0) {
    return { updated: 0, message: "No bad dates found" };
  }

  let updated = 0;
  for (const dog of badDogs) {
    const { error } = await supabase
      .from("dogs")
      .update({
        intake_date: dog.last_synced_at || dog.created_at,
        original_intake_date: dog.original_intake_date || dog.intake_date,
        date_confidence: "low",
        date_source: "debug_repair_bad_date",
        ranking_eligible: false,
        intake_date_observation_count: 1,
      })
      .eq("id", dog.id);
    if (!error) updated++;
  }
  return { found: badDogs.length, updated };
}

function buildEnvCheck(): Record<string, boolean> {
  const result: Record<string, boolean> = {};
  for (const key of ENV_VARS_TO_CHECK) {
    result[key] = !!process.env[key] && process.env[key]!.length > 0;
  }
  return result;
}

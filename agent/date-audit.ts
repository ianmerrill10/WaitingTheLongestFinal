/**
 * WaitingTheLongest — Date Health Audit
 *
 * Scans the dogs table for suspicious intake_date and euthanasia_date values.
 * Categorises every at-risk record so you know exactly what the new timer
 * display logic will encounter before it goes live.
 *
 * Run: npx tsx --tsconfig tsconfig.json agent/date-audit.ts
 * Add --json for machine-readable output.
 *
 * Exit codes:
 *   0 — all clear (zero at-risk records)
 *   1 — at-risk records found (report printed)
 *   2 — script error / DB unreachable
 */

import dotenv from "dotenv";
import path from "path";

dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const JSON_MODE = process.argv.includes("--json");

// ─── Thresholds ─────────────────────────────────────────────────────────────

const NOW = new Date();
const EARLIEST_PLAUSIBLE_INTAKE = new Date("2000-01-01T00:00:00Z"); // before modern shelter software
const LATEST_PLAUSIBLE_INTAKE   = new Date(NOW.getTime() + 24 * 60 * 60 * 1000); // tomorrow (grace for timezone skew)
const EARLIEST_PLAUSIBLE_EUTH   = new Date("2010-01-01T00:00:00Z");
const LATEST_PLAUSIBLE_EUTH     = new Date(NOW.getTime() + 365 * 24 * 60 * 60 * 1000); // > 1 year out = suspicious
const STALE_INTAKE_CUTOFF       = new Date("2015-01-01T00:00:00Z"); // ≥10 years ago — almost certainly a data error

// ─── Types ───────────────────────────────────────────────────────────────────

type Flag =
  | "intake_null"
  | "intake_unparseable"
  | "intake_before_2000"
  | "intake_in_future"
  | "intake_stale_10yr"
  | "intake_midnight_utc"          // bare date treated as UTC midnight — the TZ bug we just fixed
  | "euth_unparseable"
  | "euth_before_2010"
  | "euth_already_expired"         // past euthanasia date still on available dog
  | "euth_far_future"              // > 1 year out — likely bogus
  | "euth_midnight_utc"            // same bare-date TZ bug on euthanasia column
  | "wait_over_20yr"               // timer would show 20+ years — almost certainly wrong intake_date
  | "countdown_negative";          // countdown.is_expired but dog is still available

interface DogRow {
  id: string;
  name: string;
  intake_date: string | null;
  euthanasia_date: string | null;
  is_available: boolean;
  status: string;
  external_source: string | null;
  shelter_id: string;
}

interface FlaggedRecord {
  id: string;
  name: string;
  intake_date: string | null;
  euthanasia_date: string | null;
  status: string;
  external_source: string | null;
  flags: Flag[];
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

function isMidnightUTC(date: Date): boolean {
  return (
    date.getUTCHours() === 0 &&
    date.getUTCMinutes() === 0 &&
    date.getUTCSeconds() === 0 &&
    date.getUTCMilliseconds() === 0
  );
}

function log(msg: string) {
  if (!JSON_MODE) console.log(msg);
}

function analyseRow(row: DogRow): Flag[] {
  const flags: Flag[] = [];

  // ── intake_date ────────────────────────────────────────────────────────────
  if (!row.intake_date) {
    flags.push("intake_null");
  } else {
    const d = new Date(row.intake_date);
    if (isNaN(d.getTime())) {
      flags.push("intake_unparseable");
    } else {
      if (d < EARLIEST_PLAUSIBLE_INTAKE) flags.push("intake_before_2000");
      if (d > LATEST_PLAUSIBLE_INTAKE)  flags.push("intake_in_future");
      if (d < STALE_INTAKE_CUTOFF)      flags.push("intake_stale_10yr");
      if (isMidnightUTC(d))             flags.push("intake_midnight_utc");

      const ageMs = NOW.getTime() - d.getTime();
      const ageYears = ageMs / (365.25 * 24 * 60 * 60 * 1000);
      if (ageYears > 20) flags.push("wait_over_20yr");
    }
  }

  // ── euthanasia_date ────────────────────────────────────────────────────────
  if (row.euthanasia_date) {
    const e = new Date(row.euthanasia_date);
    if (isNaN(e.getTime())) {
      flags.push("euth_unparseable");
    } else {
      if (e < EARLIEST_PLAUSIBLE_EUTH) flags.push("euth_before_2010");
      if (e > LATEST_PLAUSIBLE_EUTH)  flags.push("euth_far_future");
      if (e < NOW && row.is_available) flags.push("euth_already_expired");
      if (e < NOW)                     flags.push("countdown_negative");
      if (isMidnightUTC(e))            flags.push("euth_midnight_utc");
    }
  }

  return flags;
}

// ─── Main ────────────────────────────────────────────────────────────────────

async function main() {
  const { createAdminClient } = await import("../src/lib/supabase/admin");
  const supabase = createAdminClient();

  log("\n=== WaitingTheLongest — Date Health Audit ===");
  log(`Run at: ${NOW.toISOString()}\n`);

  // Fetch all dogs in pages (avoid hitting PostgREST row limit)
  const PAGE = 1000;
  let offset = 0;
  const allDogs: DogRow[] = [];

  log("Fetching dogs from database...");
  while (true) {
    const { data, error } = await supabase
      .from("dogs")
      .select("id, name, intake_date, euthanasia_date, is_available, status, external_source, shelter_id")
      .range(offset, offset + PAGE - 1)
      .order("intake_date", { ascending: true });

    if (error) {
      console.error("DB error:", error.message);
      process.exit(2);
    }
    if (!data || data.length === 0) break;
    allDogs.push(...(data as DogRow[]));
    if (data.length < PAGE) break;
    offset += PAGE;
    process.stdout.write(`  ${allDogs.length} fetched...\r`);
  }

  log(`\nTotal dogs scanned: ${allDogs.length.toLocaleString()}`);

  // ── Analyse ────────────────────────────────────────────────────────────────
  const flagged: FlaggedRecord[] = [];
  const flagCounts: Record<Flag, number> = {} as Record<Flag, number>;

  for (const row of allDogs) {
    const flags = analyseRow(row);
    if (flags.length === 0) continue;

    flagged.push({
      id: row.id,
      name: row.name,
      intake_date: row.intake_date,
      euthanasia_date: row.euthanasia_date,
      status: row.status,
      external_source: row.external_source,
      flags,
    });

    for (const f of flags) {
      flagCounts[f] = (flagCounts[f] ?? 0) + 1;
    }
  }

  // ── Report ─────────────────────────────────────────────────────────────────
  const totalAtRisk = flagged.length;
  const pct = ((totalAtRisk / allDogs.length) * 100).toFixed(2);

  if (JSON_MODE) {
    console.log(JSON.stringify({ scanned: allDogs.length, at_risk: totalAtRisk, flag_counts: flagCounts, records: flagged }, null, 2));
    process.exit(totalAtRisk > 0 ? 1 : 0);
  }

  log("\n─── Summary ──────────────────────────────────────────────────────────────");
  log(`Total scanned  : ${allDogs.length.toLocaleString()}`);
  log(`At-risk records: ${totalAtRisk.toLocaleString()} (${pct}%)`);
  log(`Clean records  : ${(allDogs.length - totalAtRisk).toLocaleString()}\n`);

  if (totalAtRisk === 0) {
    log("✓ All clear — no suspicious dates found.");
    process.exit(0);
  }

  log("─── Flag breakdown ───────────────────────────────────────────────────────");
  const DESCRIPTIONS: Record<Flag, string> = {
    intake_null:          "intake_date is NULL (timer shows nothing)",
    intake_unparseable:   "intake_date can't be parsed as a date",
    intake_before_2000:   "intake_date before 2000-01-01 (before modern shelter software)",
    intake_in_future:     "intake_date more than 24h in the future",
    intake_stale_10yr:    "intake_date before 2015 — timer would show 10+ years",
    intake_midnight_utc:  "intake_date is UTC midnight (bare YYYY-MM-DD TZ bug)",
    euth_unparseable:     "euthanasia_date can't be parsed as a date",
    euth_before_2010:     "euthanasia_date before 2010 (implausible)",
    euth_already_expired: "euthanasia_date is in the past but dog is still available",
    euth_far_future:      "euthanasia_date more than 1 year out (suspicious)",
    euth_midnight_utc:    "euthanasia_date is UTC midnight (bare YYYY-MM-DD TZ bug)",
    wait_over_20yr:       "wait time would display as 20+ years",
    countdown_negative:   "countdown would be negative (euthanasia date already passed)",
  };

  // Sort by count descending
  const sorted = Object.entries(flagCounts).sort(([, a], [, b]) => b - a) as [Flag, number][];
  for (const [flag, count] of sorted) {
    const bar = "█".repeat(Math.min(40, Math.round((count / totalAtRisk) * 40)));
    log(`  ${count.toString().padStart(6)}  ${flag.padEnd(24)}  ${bar}`);
    log(`          ${DESCRIPTIONS[flag]}`);
  }

  // ── Detail: worst offenders (first 25) ────────────────────────────────────
  log("\n─── First 25 at-risk records ────────────────────────────────────────────");
  log("  (run with --json to get the full list)\n");
  const sample = flagged.slice(0, 25);
  for (const r of sample) {
    log(`  ID: ${r.id}`);
    log(`  Name: ${r.name}  |  Status: ${r.status}  |  Source: ${r.external_source ?? "manual"}`);
    log(`  intake_date    : ${r.intake_date ?? "NULL"}`);
    log(`  euthanasia_date: ${r.euthanasia_date ?? "—"}`);
    log(`  Flags: ${r.flags.join(", ")}`);
    log("");
  }

  // ── Source breakdown ───────────────────────────────────────────────────────
  log("─── At-risk records by source ────────────────────────────────────────────");
  const bySource: Record<string, number> = {};
  for (const r of flagged) {
    const src = r.external_source ?? "manual";
    bySource[src] = (bySource[src] ?? 0) + 1;
  }
  const srcSorted = Object.entries(bySource).sort(([, a], [, b]) => b - a);
  for (const [src, count] of srcSorted) {
    log(`  ${count.toString().padStart(6)}  ${src}`);
  }

  log("\n─── Recommended actions ──────────────────────────────────────────────────");
  if (flagCounts.intake_midnight_utc || flagCounts.euth_midnight_utc) {
    const n = (flagCounts.intake_midnight_utc ?? 0) + (flagCounts.euth_midnight_utc ?? 0);
    log(`  [TZ BUG]  ${n} records have midnight UTC timestamps.`);
    log(`            These were ingested before parseShelterDateToUTC() was added.`);
    log(`            Run the fix-wait-times API route or a backfill migration to correct.`);
  }
  if (flagCounts.euth_already_expired) {
    log(`  [URGENT]  ${flagCounts.euth_already_expired} available dogs have past euthanasia dates.`);
    log(`            Run: POST /api/admin/fix-wait-times to clean these up.`);
  }
  if (flagCounts.intake_null) {
    log(`  [DATA]    ${flagCounts.intake_null} dogs have NULL intake_date.`);
    log(`            Timer will not render for these dogs. Review source data.`);
  }
  if (flagCounts.intake_stale_10yr || flagCounts.wait_over_20yr) {
    const n = Math.max(flagCounts.intake_stale_10yr ?? 0, flagCounts.wait_over_20yr ?? 0);
    log(`  [DATA]    ${n} dogs have intake dates before 2015.`);
    log(`            Likely bad data from scraper — consider capping wait timer display at 10yr.`);
  }

  log("");
  process.exit(1);
}

main().catch((err) => {
  console.error("FATAL:", err instanceof Error ? err.message : String(err));
  process.exit(2);
});

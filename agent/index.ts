/**
 * WaitingTheLongest Local Agent v2
 *
 * Persistent Node.js process that manages dog data accuracy 24/7.
 * Features: real-time dog verification slideshow, error correction,
 * structured logging, SSE event streaming.
 *
 * Start: npx tsx --tsconfig tsconfig.json agent/index.ts
 * Dashboard: http://localhost:3847
 */

import dotenv from "dotenv";
import path from "path";
import http from "http";
import fs from "fs";
import { EventEmitter } from "events";

// Load .env.local before any @/ imports
dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

// Existing engine imports (resolved via tsconfig paths)
import { syncDogsFromRescueGroups, updateUrgencyLevels } from "../src/lib/rescuegroups/sync";
import { runVerification, getVerificationStats } from "../src/lib/verification/engine";
import type { DogVerificationEvent } from "../src/lib/verification/engine";
import { runAudit } from "../src/lib/audit/engine";
import { computeAndStoreDailyStats } from "../src/lib/stats/compute-daily-stats";
import { createAdminClient } from "../src/lib/supabase/admin";
import { runBackup, getBackupStatus } from "./backup";
import { runNycAccScrape } from "../src/lib/scrapers/nyc-acc";

// ─────────────────────────────────────────────
// SECTION 1: Types
// ─────────────────────────────────────────────

interface LogEntry {
  id: number;
  timestamp: string;
  level: "info" | "warn" | "error" | "success";
  category: string;
  message: string;
  details?: Record<string, unknown>;
}

// Real-time dog event sent to dashboard slideshow
interface DogEvent {
  type: "dog";
  dog: {
    id: string;
    name: string;
    breed: string;
    photo_url: string | null;
    shelter_name: string;
    shelter_city: string;
    shelter_state: string;
    intake_date: string;
    wait_days: number;
  };
  outcome: string;
  details: string;
}

// Error correction event
interface ErrorFixEvent {
  type: "error_fix";
  fixType: string;
  dogId: string;
  dogName: string;
  description: string;
  before: string;
  after: string;
}

interface TaskDef {
  name: string;
  description: string;
  fn: () => Promise<void>;
  intervalMs: number;
  lastRun: number | null;
  nextRun: number;
  isRunning: boolean;
  runCount: number;
  lastDuration: number | null;
  lastError: string | null;
  consecutiveErrors: number;
}

interface AgentMetrics {
  startedAt: string;
  uptimeMs: number;
  isPaused: boolean;
  currentTask: string | null;
  currentProgress: { current: number; total: number; label: string } | null;
  totalDogsProcessed: number;
  totalVerified: number;
  totalDeactivated: number;
  totalErrors: number;
  totalApiCalls: number;
  totalErrorsFixed: number;
  dogsPerHour: number;
  currentDog: DogEvent["dog"] | null;
  verifySpeed: number; // dogs per minute
  tasks: Array<{
    name: string;
    description: string;
    isRunning: boolean;
    runCount: number;
    lastRun: string | null;
    nextRun: string;
    lastDuration: number | null;
    lastError: string | null;
    consecutiveErrors: number;
  }>;
  db: {
    totalDogs: number;
    verifiedCount: number;
    verifiedPct: number;
    dateAccuracyPct: number;
    avgWaitDays: number;
    criticalDogs: number;
    highUrgencyDogs: number;
    neverChecked: number;
    daysToFullCoverage: number;
  } | null;
  recentErrors: Array<{ time: string; task: string; message: string }>;
  errorFixHistory: ErrorFixEvent[];
}

// ─────────────────────────────────────────────
// SECTION 2: EventBus (log + dog + error streaming)
// ─────────────────────────────────────────────

class EventBus extends EventEmitter {
  private logId = 0;
  private logs: LogEntry[] = [];
  private maxLogs = 2000;

  log(level: LogEntry["level"], category: string, message: string, details?: Record<string, unknown>) {
    const entry: LogEntry = {
      id: ++this.logId,
      timestamp: new Date().toISOString(),
      level,
      category,
      message,
      details,
    };
    this.logs.push(entry);
    if (this.logs.length > this.maxLogs) {
      this.logs = this.logs.slice(-this.maxLogs);
    }
    this.emit("log", entry);

    // Console output with color codes
    const colors: Record<string, string> = {
      info: "\x1b[37m",    // white
      warn: "\x1b[33m",    // yellow
      error: "\x1b[31m",   // red
      success: "\x1b[32m", // green
    };
    const reset = "\x1b[0m";
    const prefix = { info: "   ", warn: "⚠  ", error: "❌ ", success: "✅ " }[level];
    const time = new Date().toLocaleTimeString();
    console.log(`${colors[level]}${prefix}${time} [${category}] ${message}${reset}`);
  }

  // Emit a real-time dog event for the slideshow
  emitDog(event: DogEvent) {
    this.emit("dog", event);
  }

  // Emit an error correction event
  emitErrorFix(event: ErrorFixEvent) {
    this.emit("error_fix", event);
  }

  getRecent(count = 200): LogEntry[] {
    return this.logs.slice(-count);
  }
}

const bus = new EventBus();

// ─────────────────────────────────────────────
// SECTION 3: Shelter Name Cache
// ─────────────────────────────────────────────

const shelterCache: Map<string, { name: string; city: string; state: string }> = new Map();

async function getShelterInfo(shelterId: string | null): Promise<{ name: string; city: string; state: string }> {
  if (!shelterId) return { name: "Unknown Shelter", city: "", state: "" };

  const cached = shelterCache.get(shelterId);
  if (cached) return cached;

  try {
    const supabase = createAdminClient();
    const { data } = await supabase
      .from("shelters")
      .select("name, city, state_code")
      .eq("id", shelterId)
      .single();

    if (data) {
      const info = { name: data.name || "Unknown", city: data.city || "", state: data.state_code || "" };
      shelterCache.set(shelterId, info);
      return info;
    }
  } catch { /* cache miss is fine */ }

  return { name: "Unknown Shelter", city: "", state: "" };
}

// Pre-load shelter cache for faster lookups
async function warmShelterCache() {
  try {
    const supabase = createAdminClient();
    const { data } = await supabase
      .from("shelters")
      .select("id, name, city, state_code")
      .limit(5000);

    if (data) {
      for (const s of data) {
        shelterCache.set(s.id, { name: s.name || "Unknown", city: s.city || "", state: s.state_code || "" });
      }
      bus.log("info", "system", `Shelter cache warmed: ${data.length} shelters loaded`);
    }
  } catch (err) {
    bus.log("warn", "system", `Shelter cache warm failed: ${err}`);
  }
}

// ─────────────────────────────────────────────
// SECTION 4: Metrics & State
// ─────────────────────────────────────────────

const startedAt = new Date();
let totalDogsProcessed = 0;
let totalVerified = 0;
let totalDeactivated = 0;
let totalErrors = 0;
let totalApiCalls = 0;
let totalErrorsFixed = 0;
let isPaused = false;
let currentTask: string | null = null;
let currentProgress: AgentMetrics["currentProgress"] = null;
let dbStats: AgentMetrics["db"] = null;
let currentDog: DogEvent["dog"] | null = null;

// Track recent verification speed
const verifyTimestamps: number[] = [];
function recordVerify() {
  const now = Date.now();
  verifyTimestamps.push(now);
  // Keep only last 60 seconds of timestamps
  const cutoff = now - 60000;
  while (verifyTimestamps.length > 0 && verifyTimestamps[0] < cutoff) {
    verifyTimestamps.shift();
  }
}
function getVerifySpeed(): number {
  const now = Date.now();
  const cutoff = now - 60000;
  const recent = verifyTimestamps.filter(t => t >= cutoff);
  return recent.length; // dogs per minute
}

// Track recent errors for display
const recentErrors: Array<{ time: string; task: string; message: string }> = [];
function recordError(task: string, message: string) {
  recentErrors.push({ time: new Date().toISOString(), task, message: message.substring(0, 200) });
  if (recentErrors.length > 50) recentErrors.shift();
}

// Track error fixes
const errorFixHistory: ErrorFixEvent[] = [];
function recordErrorFix(event: ErrorFixEvent) {
  errorFixHistory.push(event);
  if (errorFixHistory.length > 100) errorFixHistory.shift();
  bus.emitErrorFix(event);
}

async function refreshDbStats() {
  try {
    const supabase = createAdminClient();
    const vStats = await getVerificationStats();

    const [verifiedConf, highConf, criticalRes, highRes] = await Promise.all([
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("date_confidence", "verified"),
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("date_confidence", "high"),
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("urgency_level", "critical"),
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("urgency_level", "high"),
    ]);

    const total = vStats.total;
    const verifiedConfCount = (verifiedConf.count ?? 0) + (highConf.count ?? 0);
    const dateAccuracyPct = total > 0 ? Math.round((verifiedConfCount / total) * 1000) / 10 : 0;
    const verifiedPct = total > 0 ? Math.round((vStats.verified / total) * 1000) / 10 : 0;

    const uptimeHours = (Date.now() - startedAt.getTime()) / (1000 * 60 * 60);
    const dogsPerDay = uptimeHours > 0.1 ? (totalVerified / uptimeHours) * 24 : 600;
    const remaining = vStats.neverChecked;
    const daysToFull = dogsPerDay > 0 ? Math.round((remaining / dogsPerDay) * 10) / 10 : 999;

    dbStats = {
      totalDogs: total,
      verifiedCount: vStats.verified,
      verifiedPct,
      dateAccuracyPct,
      avgWaitDays: 0,
      criticalDogs: criticalRes.count ?? 0,
      highUrgencyDogs: highRes.count ?? 0,
      neverChecked: vStats.neverChecked,
      daysToFullCoverage: daysToFull,
    };
  } catch (err) {
    bus.log("error", "system", `Failed to refresh DB stats: ${err}`);
  }
}

function getMetrics(): AgentMetrics {
  const now = Date.now();
  const uptimeMs = now - startedAt.getTime();
  const uptimeHours = uptimeMs / (1000 * 60 * 60);
  const dogsPerHour = uptimeHours > 0.01 ? Math.round(totalDogsProcessed / uptimeHours) : 0;

  return {
    startedAt: startedAt.toISOString(),
    uptimeMs,
    isPaused,
    currentTask,
    currentProgress,
    totalDogsProcessed,
    totalVerified,
    totalDeactivated,
    totalErrors,
    totalApiCalls,
    totalErrorsFixed,
    dogsPerHour,
    currentDog,
    verifySpeed: getVerifySpeed(),
    tasks: tasks.map(t => ({
      name: t.name,
      description: t.description,
      isRunning: t.isRunning,
      runCount: t.runCount,
      lastRun: t.lastRun ? new Date(t.lastRun).toISOString() : null,
      nextRun: new Date(t.nextRun).toISOString(),
      lastDuration: t.lastDuration,
      lastError: t.lastError,
      consecutiveErrors: t.consecutiveErrors,
    })),
    db: dbStats,
    recentErrors,
    errorFixHistory: errorFixHistory.slice(-20),
  };
}

// ─────────────────────────────────────────────
// SECTION 5: Task Operations
// ─────────────────────────────────────────────

let syncPage = 1;
const SYNC_PAGES = 10;
const MAX_RG_PAGES = 350;

async function taskSync() {
  bus.log("info", "sync", `Syncing pages ${syncPage}-${syncPage + SYNC_PAGES - 1} from RescueGroups...`);
  currentProgress = { current: 0, total: SYNC_PAGES, label: "Syncing from RescueGroups" };

  try {
    const result = await syncDogsFromRescueGroups(SYNC_PAGES, syncPage);
    totalDogsProcessed += result.totalFetched;
    totalApiCalls += SYNC_PAGES;
    totalErrors += result.errors;

    if (result.errors > 0) {
      bus.log("warn", "sync", `Sync had ${result.errors} errors during processing`);
      recordError("sync", `${result.errors} errors during sync pages ${syncPage}-${syncPage + SYNC_PAGES - 1}`);
    }

    bus.log("success", "sync",
      `Sync complete: +${result.inserted} new, ${result.updated} updated, ${result.sheltersCreated} shelters (${Math.round(result.duration / 1000)}s)`,
      result as unknown as Record<string, unknown>
    );

    syncPage += SYNC_PAGES;
    if (syncPage > MAX_RG_PAGES) {
      syncPage = 1;
      bus.log("info", "sync", "Wrapped around to page 1 — starting full cycle again");
    }
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("sync", msg);
    bus.log("error", "sync", `Sync failed: ${msg}`);

    // Error correction: if rate limited, back off
    if (msg.includes("429") || msg.toLowerCase().includes("rate limit")) {
      bus.log("warn", "sync", "Rate limit detected — delaying next sync by 10 minutes");
      const syncTask = tasks.find(t => t.name === "sync");
      if (syncTask) syncTask.nextRun = Date.now() + 10 * 60 * 1000;
    }
    throw err;
  } finally {
    currentProgress = null;
  }
}

async function taskVerify() {
  const TOTAL = 150;
  const MICRO = 15;
  let processed = 0;
  let verified = 0;
  let deactivated = 0;
  let errors = 0;
  let availDates = 0;
  let foundDates = 0;
  let killDates = 0;
  let consecutiveApiErrors = 0;

  bus.log("info", "verify", `Starting verification of ${TOTAL} dogs (micro-batches of ${MICRO})...`);

  for (let i = 0; i < TOTAL; i += MICRO) {
    const batchSize = Math.min(MICRO, TOTAL - i);
    const priority = i < 100 ? "never_verified" as const : "stale" as const;
    const batchNum = Math.floor(i / MICRO) + 1;
    const totalBatches = Math.ceil(TOTAL / MICRO);

    currentProgress = {
      current: processed,
      total: TOTAL,
      label: `Verifying dogs [batch ${batchNum}/${totalBatches}] (${verified} verified, ${deactivated} removed)`,
    };

    try {
      // Pass callback for real-time dog events
      const result = await runVerification(batchSize, priority, async (event: DogVerificationEvent) => {
        recordVerify();
        const shelter = await getShelterInfo(event.dog.shelter_id);
        const waitDays = Math.floor((Date.now() - new Date(event.dog.intake_date).getTime()) / (1000 * 60 * 60 * 24));

        const dogEvent: DogEvent = {
          type: "dog",
          dog: {
            id: event.dog.id,
            name: event.dog.name || "Unknown",
            breed: event.dog.breed_primary || "Mixed Breed",
            photo_url: event.dog.primary_photo_url,
            shelter_name: shelter.name,
            shelter_city: shelter.city,
            shelter_state: shelter.state,
            intake_date: event.dog.intake_date,
            wait_days: waitDays,
          },
          outcome: event.outcome,
          details: buildOutcomeDetails(event),
        };

        currentDog = dogEvent.dog;
        bus.emitDog(dogEvent);

        // Per-dog logging for significant events
        if (event.outcome === "deactivated") {
          bus.log("warn", "verify", `${event.dog.name} — DEACTIVATED (adopted/removed) from ${shelter.name}`);
        }
        if (event.capturedAvailableDate) {
          bus.log("success", "verify", `${event.dog.name} — captured availableDate (verified wait time)`);
        }
        if (event.capturedKillDate) {
          bus.log("error", "verify", `${event.dog.name} — KILL DATE FOUND — euthanasia scheduled`);
        }
        if (event.outcome === "error") {
          bus.log("error", "verify", `${event.dog.name} — verification error: ${event.error || "unknown"}`);
        }
      });

      processed += result.checked;
      verified += result.verified;
      deactivated += result.deactivated;
      errors += result.errors;
      availDates += result.availableDatesCaptured;
      foundDates += result.foundDatesCaptured;
      killDates += result.killDatesCaptured;
      totalApiCalls += result.checked;

      // Reset consecutive error counter on success
      if (result.errors === 0) consecutiveApiErrors = 0;
      else consecutiveApiErrors += result.errors;

      // Error correction: if too many consecutive errors, likely API issue
      if (consecutiveApiErrors >= 10) {
        bus.log("error", "verify", `${consecutiveApiErrors} consecutive API errors — stopping verification early, possible API outage`);
        recordError("verify", `Stopped early: ${consecutiveApiErrors} consecutive API errors`);
        break;
      }

      // Log batch summary
      if (result.deactivated > 0 || result.availableDatesCaptured > 0 || result.foundDatesCaptured > 0 || result.killDatesCaptured > 0) {
        bus.log("info", "verify", `Batch ${batchNum}/${totalBatches}: ${result.checked} checked, ${result.verified} OK, ${result.deactivated} removed, dates: ${result.availableDatesCaptured}A/${result.foundDatesCaptured}F/${result.killDatesCaptured}K`);
      }

    } catch (err) {
      errors++;
      const msg = err instanceof Error ? err.message : String(err);
      recordError("verify", `Batch ${batchNum} failed: ${msg}`);
      bus.log("error", "verify", `Batch ${batchNum} failed: ${msg}`);

      // Error correction: rate limit handling
      if (msg.includes("429") || msg.toLowerCase().includes("rate limit")) {
        bus.log("warn", "verify", "Rate limit hit — pausing verification for 5 minutes");
        await new Promise(r => setTimeout(r, 5 * 60 * 1000));
        consecutiveApiErrors = 0; // Reset after pause
      }
    }
  }

  totalDogsProcessed += processed;
  totalVerified += verified;
  totalDeactivated += deactivated;
  totalErrors += errors;
  currentDog = null;

  const speed = getVerifySpeed();
  bus.log("success", "verify",
    `Verification complete: ${processed} checked, ${verified} verified, ${deactivated} deactivated | ` +
    `Dates: ${availDates}avail/${foundDates}found/${killDates}kill | ${errors} errors | ${speed} dogs/min`
  );
  currentProgress = null;
}

function buildOutcomeDetails(event: DogVerificationEvent): string {
  const parts: string[] = [];
  if (event.outcome === "verified") parts.push("Still available");
  if (event.outcome === "deactivated") parts.push("Adopted or removed");
  if (event.outcome === "not_found") parts.push("Not found in RG but URL alive");
  if (event.outcome === "pending") parts.push("Status pending at shelter");
  if (event.outcome === "error") parts.push(`Error: ${event.error || "API failure"}`);
  if (event.capturedAvailableDate) parts.push("Got availableDate");
  if (event.capturedFoundDate) parts.push("Got foundDate");
  if (event.capturedKillDate) parts.push("KILL DATE CAPTURED");
  if (event.capturedBirthDate) parts.push("Got birthDate");
  if (event.externalUrlDead) parts.push("External URL dead");
  return parts.join(" | ");
}

async function taskErrorCorrection() {
  bus.log("info", "errors", "Running error detection & correction scan...");
  currentProgress = { current: 0, total: 7, label: "Scanning for data errors" };

  const supabase = createAdminClient();
  let totalFixed = 0;

  try {
    // ── Fix 1: intake_date in the future ──
    currentProgress = { current: 1, total: 7, label: "Checking future intake dates" };
    const now = new Date().toISOString();
    const { data: futureDates } = await supabase
      .from("dogs")
      .select("id, name, intake_date")
      .eq("is_available", true)
      .gt("intake_date", now)
      .limit(100);

    if (futureDates && futureDates.length > 0) {
      for (const dog of futureDates) {
        await supabase.from("dogs").update({
          intake_date: now,
          date_confidence: "low",
          date_source: "error_correction_future_date",
        }).eq("id", dog.id);

        recordErrorFix({
          type: "error_fix",
          fixType: "future_intake_date",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "Intake date was in the future",
          before: dog.intake_date,
          after: now,
        });
        totalFixed++;
      }
      bus.log("warn", "errors", `Fixed ${futureDates.length} dogs with future intake dates`);
    }

    // ── Fix 2: intake_date before birth_date ──
    currentProgress = { current: 2, total: 7, label: "Checking intake before birth" };
    const { data: birthIssues } = await supabase
      .from("dogs")
      .select("id, name, intake_date, birth_date")
      .eq("is_available", true)
      .not("birth_date", "is", null)
      .limit(500);

    if (birthIssues) {
      const badDogs = birthIssues.filter(d => d.birth_date && new Date(d.intake_date) < new Date(d.birth_date));
      for (const dog of badDogs) {
        await supabase.from("dogs").update({
          intake_date: dog.birth_date,
          date_confidence: "low",
          date_source: "error_correction_before_birth",
        }).eq("id", dog.id);

        recordErrorFix({
          type: "error_fix",
          fixType: "intake_before_birth",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "Intake date was before dog's birth date",
          before: dog.intake_date,
          after: dog.birth_date!,
        });
        totalFixed++;
      }
      if (badDogs.length > 0) {
        bus.log("warn", "errors", `Fixed ${badDogs.length} dogs with intake date before birth date`);
      }
    }

    // ── Fix 3: is_available=true but status='adopted' ──
    currentProgress = { current: 3, total: 7, label: "Checking status consistency" };
    const { data: statusMismatch, count: statusCount } = await supabase
      .from("dogs")
      .select("id, name", { count: "exact" })
      .eq("is_available", true)
      .eq("status", "adopted")
      .limit(200);

    if (statusMismatch && statusMismatch.length > 0) {
      const ids = statusMismatch.map(d => d.id);
      await supabase.from("dogs").update({ is_available: false }).in("id", ids);

      for (const dog of statusMismatch) {
        recordErrorFix({
          type: "error_fix",
          fixType: "status_mismatch",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "Marked available but status was 'adopted'",
          before: "is_available=true, status=adopted",
          after: "is_available=false, status=adopted",
        });
      }
      totalFixed += statusMismatch.length;
      bus.log("warn", "errors", `Fixed ${statusMismatch.length} dogs with status mismatch (available but adopted)`);
    }

    // ── Fix 4: Missing primary_photo_url but has photo_urls ──
    currentProgress = { current: 4, total: 7, label: "Checking missing primary photos" };
    const { data: missingPhotos } = await supabase
      .from("dogs")
      .select("id, name, photo_urls")
      .eq("is_available", true)
      .is("primary_photo_url", null)
      .limit(200);

    if (missingPhotos) {
      const fixable = missingPhotos.filter(d => d.photo_urls && Array.isArray(d.photo_urls) && d.photo_urls.length > 0);
      for (const dog of fixable) {
        await supabase.from("dogs").update({
          primary_photo_url: dog.photo_urls[0],
          thumbnail_url: dog.photo_urls[0],
        }).eq("id", dog.id);

        recordErrorFix({
          type: "error_fix",
          fixType: "missing_primary_photo",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "Had photos array but no primary_photo_url",
          before: "primary_photo_url=null",
          after: `primary_photo_url=${dog.photo_urls[0].substring(0, 50)}...`,
        });
        totalFixed++;
      }
      if (fixable.length > 0) {
        bus.log("success", "errors", `Fixed ${fixable.length} dogs with missing primary photo`);
      }
    }

    // ── Fix 5: Negative age_months ──
    currentProgress = { current: 5, total: 7, label: "Checking negative ages" };
    const { data: negAges } = await supabase
      .from("dogs")
      .select("id, name, age_months")
      .eq("is_available", true)
      .lt("age_months", 0)
      .limit(100);

    if (negAges && negAges.length > 0) {
      const ids = negAges.map(d => d.id);
      await supabase.from("dogs").update({ age_months: 0 }).in("id", ids);

      for (const dog of negAges) {
        recordErrorFix({
          type: "error_fix",
          fixType: "negative_age",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "age_months was negative",
          before: `age_months=${dog.age_months}`,
          after: "age_months=0",
        });
      }
      totalFixed += negAges.length;
      bus.log("warn", "errors", `Fixed ${negAges.length} dogs with negative age`);
    }

    // ── Fix 6: Past euthanasia_date still marked as on list ──
    currentProgress = { current: 6, total: 7, label: "Checking expired euthanasia dates" };
    const threeDaysAgo = new Date(Date.now() - 3 * 24 * 60 * 60 * 1000).toISOString();
    const { data: expiredEuth } = await supabase
      .from("dogs")
      .select("id, name, euthanasia_date")
      .eq("is_available", true)
      .eq("is_on_euthanasia_list", true)
      .lt("euthanasia_date", threeDaysAgo)
      .limit(100);

    if (expiredEuth && expiredEuth.length > 0) {
      const ids = expiredEuth.map(d => d.id);
      await supabase.from("dogs").update({
        is_on_euthanasia_list: false,
        euthanasia_date: null,
        urgency_level: "normal",
      }).in("id", ids);

      for (const dog of expiredEuth) {
        recordErrorFix({
          type: "error_fix",
          fixType: "expired_euthanasia",
          dogId: dog.id,
          dogName: dog.name || "Unknown",
          description: "Euthanasia date passed 3+ days ago but still on list",
          before: `euthanasia_date=${dog.euthanasia_date}`,
          after: "euthanasia cleared, urgency=normal",
        });
      }
      totalFixed += expiredEuth.length;
      bus.log("warn", "errors", `Fixed ${expiredEuth.length} dogs with expired euthanasia dates`);
    }

    // ── Fix 7: Duplicate external_ids ──
    currentProgress = { current: 7, total: 7, label: "Checking duplicate external IDs" };
    const { data: dupes } = await supabase.rpc("find_duplicate_external_ids").limit(50);
    // Note: This RPC may not exist. If not, skip silently.
    if (dupes && dupes.length > 0) {
      bus.log("warn", "errors", `Found ${dupes.length} duplicate external_id groups — manual review needed`);
    }

  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    // Don't fail the whole task for individual check failures
    if (!msg.includes("find_duplicate_external_ids")) {
      bus.log("error", "errors", `Error correction scan failed: ${msg}`);
      recordError("errors", msg);
    }
  }

  totalErrorsFixed += totalFixed;
  bus.log("success", "errors", `Error correction complete: ${totalFixed} issues fixed this run (${totalErrorsFixed} total)`);
  currentProgress = null;
}

async function taskAudit() {
  bus.log("info", "audit", "Running quick audit...");
  currentProgress = { current: 0, total: 1, label: "Running quick audit" };

  try {
    const result = await runAudit("quick", "local_agent");
    const repaired = result.logStats?.repairs ?? 0;
    const issues = Object.values(result.stats || {}).reduce(
      (sum: number, s: Record<string, unknown>) => sum + ((s as { checked?: number }).checked || 0), 0
    );

    bus.log("success", "audit", `Audit complete: ${issues} checked, ${repaired} repaired (${Math.round(result.duration / 1000)}s)`, {
      runId: result.runId,
      logStats: result.logStats,
    });
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("audit", msg);
    bus.log("error", "audit", `Audit failed: ${msg}`);
  } finally {
    currentProgress = null;
  }
}

async function taskFullAudit() {
  bus.log("info", "audit", "Running FULL audit (all 9 checks)...");
  currentProgress = { current: 0, total: 1, label: "Running full audit" };

  try {
    const result = await runAudit("full", "local_agent");
    const repaired = result.logStats?.repairs ?? 0;

    bus.log("success", "audit", `Full audit complete: ${repaired} repairs (${Math.round(result.duration / 1000)}s)`, {
      runId: result.runId,
      logStats: result.logStats,
    });
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("audit", msg);
    bus.log("error", "audit", `Full audit failed: ${msg}`);
  } finally {
    currentProgress = null;
  }
}

async function taskUrgency() {
  bus.log("info", "urgency", "Updating urgency levels...");
  currentProgress = { current: 0, total: 1, label: "Updating urgency levels" };

  try {
    const result = await updateUrgencyLevels();
    bus.log("success", "urgency", `Urgency update: ${result.updated} dogs updated, ${result.errors} errors`);
    if (result.errors > 0) {
      recordError("urgency", `${result.errors} errors during urgency update`);
    }
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("urgency", msg);
    bus.log("error", "urgency", `Urgency update failed: ${msg}`);
  } finally {
    currentProgress = null;
  }
}

async function taskStats() {
  bus.log("info", "stats", "Computing daily stats snapshot...");
  currentProgress = { current: 0, total: 1, label: "Computing daily stats" };

  try {
    const result = await computeAndStoreDailyStats();
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const r = result as any;
    bus.log("success", "stats", `Stats computed: ${r.total_available_dogs} dogs, verification ${r.verification_coverage_pct}%`);
    await refreshDbStats();
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("stats", msg);
    bus.log("error", "stats", `Stats computation failed: ${msg}`);
  } finally {
    currentProgress = null;
  }
}

async function taskBackup() {
  bus.log("info", "backup", "Starting scheduled backup...");
  currentProgress = { current: 0, total: 3, label: "Creating backup" };

  try {
    const result = await runBackup();
    if (result.success) {
      bus.log("success", "backup",
        `Backup complete: ${result.size} local${result.rcloneSync ? " + Google Drive synced" : ""} (${Math.round(result.duration / 1000)}s)`
      );
    } else {
      bus.log("error", "backup", `Backup failed: ${result.error}`);
      recordError("backup", result.error || "Unknown backup error");
    }
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    bus.log("error", "backup", `Backup failed: ${msg}`);
    recordError("backup", msg);
  } finally {
    currentProgress = null;
  }
}

async function taskNycAccScrape() {
  bus.log("info", "nyc-acc", "Scraping NYC ACC priority placement list...");
  currentProgress = { current: 0, total: 1, label: "Scraping NYC ACC" };

  try {
    const result = await runNycAccScrape({
      fetchDetails: true,
      maxDetailFetches: 10,
      delayMs: 3000,
    });

    if (result.totalDogs > 0) {
      bus.log("success", "nyc-acc",
        `NYC ACC: ${result.totalDogs} dogs found, ${result.inserted} new, ${result.updated} updated, ${result.detailsFetched} details fetched`
      );
    } else {
      bus.log("warn", "nyc-acc", "NYC ACC: no dogs found on priority placement page");
    }

    if (result.errors.length > 0) {
      for (const err of result.errors.slice(0, 3)) {
        recordError("nyc-acc", err);
      }
    }
  } catch (err) {
    totalErrors++;
    const msg = err instanceof Error ? err.message : String(err);
    recordError("nyc-acc", msg);
    bus.log("error", "nyc-acc", `NYC ACC scrape failed: ${msg}`);
  } finally {
    currentProgress = null;
  }
}

// ─────────────────────────────────────────────
// SECTION 6: Task Scheduler
// ─────────────────────────────────────────────

const MIN = 60 * 1000;
const HOUR = 60 * MIN;

const tasks: TaskDef[] = [
  { name: "verify", description: "Verify dog listings against RescueGroups API", fn: taskVerify, intervalMs: 30 * MIN, lastRun: null, nextRun: Date.now() + 5000, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "sync", description: "Sync new dogs from RescueGroups search API", fn: taskSync, intervalMs: 60 * MIN, lastRun: null, nextRun: Date.now() + 2 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "errorFix", description: "Detect and auto-correct data inconsistencies", fn: taskErrorCorrection, intervalMs: 3 * HOUR, lastRun: null, nextRun: Date.now() + 5 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "urgency", description: "Update euthanasia urgency countdowns", fn: taskUrgency, intervalMs: 2 * HOUR, lastRun: null, nextRun: Date.now() + 10 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "audit", description: "Quick data quality audit", fn: taskAudit, intervalMs: 4 * HOUR, lastRun: null, nextRun: Date.now() + 30 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "stats", description: "Compute daily stats snapshot", fn: taskStats, intervalMs: 6 * HOUR, lastRun: null, nextRun: Date.now() + 15 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "fullAudit", description: "Full 9-check comprehensive audit", fn: taskFullAudit, intervalMs: 24 * HOUR, lastRun: null, nextRun: Date.now() + 1 * HOUR, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "backup", description: "Zip backup + Google Drive sync", fn: taskBackup, intervalMs: 6 * HOUR, lastRun: null, nextRun: Date.now() + 20 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
  { name: "nycAcc", description: "Scrape NYC ACC priority placement at-risk dogs", fn: taskNycAccScrape, intervalMs: 6 * HOUR, lastRun: null, nextRun: Date.now() + 8 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null, consecutiveErrors: 0 },
];

let isRunningTask = false;

async function schedulerTick() {
  if (isPaused || isRunningTask) return;

  const now = Date.now();
  const overdue = tasks.filter(t => !t.isRunning && now >= t.nextRun);

  if (overdue.length === 0) return;

  // Run the most overdue task
  const task = overdue.sort((a, b) => a.nextRun - b.nextRun)[0];

  // Skip task if it has too many consecutive errors (circuit breaker)
  if (task.consecutiveErrors >= 5) {
    bus.log("error", "scheduler", `Task "${task.name}" disabled after ${task.consecutiveErrors} consecutive errors. Will retry in 30 minutes.`);
    task.nextRun = Date.now() + 30 * MIN;
    task.consecutiveErrors = 0; // Reset to allow retry
    return;
  }

  isRunningTask = true;
  task.isRunning = true;
  currentTask = task.name;

  const taskStart = Date.now();
  try {
    await task.fn();
    task.lastError = null;
    task.consecutiveErrors = 0;
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    task.lastError = msg;
    task.consecutiveErrors++;
    bus.log("error", "scheduler", `Task "${task.name}" failed (${task.consecutiveErrors} consecutive): ${msg}`);
  } finally {
    task.isRunning = false;
    task.lastRun = Date.now();
    task.lastDuration = Date.now() - taskStart;
    task.nextRun = Date.now() + task.intervalMs;
    task.runCount++;
    isRunningTask = false;
    currentTask = null;
    currentProgress = null;
    currentDog = null;
  }
}

// ─────────────────────────────────────────────
// SECTION 7: HTTP Server (SSE + API + Dashboard)
// ─────────────────────────────────────────────

const PORT = 3847;
const sseClients: http.ServerResponse[] = [];

// Forward log entries to SSE clients
bus.on("log", (entry: LogEntry) => {
  const data = `event: log\ndata: ${JSON.stringify(entry)}\n\n`;
  broadcastSSE(data);
});

// Forward dog events to SSE clients (for slideshow)
bus.on("dog", (event: DogEvent) => {
  const data = `event: dog\ndata: ${JSON.stringify(event)}\n\n`;
  broadcastSSE(data);
});

// Forward error fix events to SSE clients
bus.on("error_fix", (event: ErrorFixEvent) => {
  const data = `event: error_fix\ndata: ${JSON.stringify(event)}\n\n`;
  broadcastSSE(data);
});

function broadcastSSE(data: string) {
  for (let i = sseClients.length - 1; i >= 0; i--) {
    try {
      sseClients[i].write(data);
    } catch {
      sseClients.splice(i, 1);
    }
  }
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://localhost:${PORT}`);

  // CORS
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") {
    res.writeHead(200);
    res.end();
    return;
  }

  // ── Dashboard HTML ──
  if (url.pathname === "/" || url.pathname === "/dashboard") {
    try {
      const html = fs.readFileSync(path.join(__dirname, "dashboard.html"), "utf8");
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
      res.end(html);
    } catch {
      res.writeHead(500, { "Content-Type": "text/plain" });
      res.end("Dashboard HTML not found. Place dashboard.html in the agent/ directory.");
    }
    return;
  }

  // ── SSE Events ──
  if (url.pathname === "/events") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      "Connection": "keep-alive",
    });

    // Send recent logs as initial data
    for (const entry of bus.getRecent(100)) {
      res.write(`event: log\ndata: ${JSON.stringify(entry)}\n\n`);
    }

    sseClients.push(res);
    req.on("close", () => {
      const idx = sseClients.indexOf(res);
      if (idx >= 0) sseClients.splice(idx, 1);
    });
    return;
  }

  // ── Metrics API ──
  if (url.pathname === "/api/metrics") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(getMetrics()));
    return;
  }

  // ── Control: Pause ──
  if (url.pathname === "/api/pause" && req.method === "POST") {
    isPaused = true;
    bus.log("warn", "system", "Agent PAUSED by user");
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ paused: true }));
    return;
  }

  // ── Control: Resume ──
  if (url.pathname === "/api/resume" && req.method === "POST") {
    isPaused = false;
    bus.log("success", "system", "Agent RESUMED by user");
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ paused: false }));
    return;
  }

  // ── Control: Trigger Task ──
  if (url.pathname === "/api/trigger" && req.method === "POST") {
    let body = "";
    req.on("data", (chunk) => (body += chunk));
    req.on("end", async () => {
      try {
        const { task: taskName } = JSON.parse(body || "{}");
        const task = tasks.find(t => t.name === taskName);
        if (!task) {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: `Unknown task: ${taskName}` }));
          return;
        }
        if (isRunningTask) {
          res.writeHead(409, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "A task is already running" }));
          return;
        }

        task.nextRun = 0;
        bus.log("info", "system", `Task "${taskName}" triggered manually`);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ triggered: taskName }));
      } catch {
        res.writeHead(400, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid JSON body" }));
      }
    });
    return;
  }

  // ── Health Check ──
  if (url.pathname === "/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({
      status: "healthy",
      uptime: Date.now() - startedAt.getTime(),
      paused: isPaused,
      currentTask,
      errors: totalErrors,
    }));
    return;
  }

  // ── 404 ──
  res.writeHead(404, { "Content-Type": "text/plain" });
  res.end("Not found");
});

// ─────────────────────────────────────────────
// SECTION 8: Startup & Shutdown
// ─────────────────────────────────────────────

let isShuttingDown = false;

async function shutdown(signal: string) {
  if (isShuttingDown) return;
  isShuttingDown = true;
  isPaused = true;

  bus.log("warn", "system", `Shutting down (${signal})... waiting for current task to finish`);

  // Wait up to 30s for current task
  const deadline = Date.now() + 30000;
  while (isRunningTask && Date.now() < deadline) {
    await new Promise(r => setTimeout(r, 500));
  }

  bus.log("info", "system", "Agent stopped.");
  server.close();
  process.exit(0);
}

process.on("SIGINT", () => shutdown("SIGINT"));
process.on("SIGTERM", () => shutdown("SIGTERM"));

// Catch unhandled errors to prevent crashes
process.on("uncaughtException", (err) => {
  bus.log("error", "system", `UNCAUGHT EXCEPTION: ${err.message}`);
  recordError("system", `Uncaught exception: ${err.message}`);
  console.error("Uncaught exception:", err);
});

process.on("unhandledRejection", (reason) => {
  const msg = reason instanceof Error ? reason.message : String(reason);
  bus.log("error", "system", `UNHANDLED REJECTION: ${msg}`);
  recordError("system", `Unhandled rejection: ${msg}`);
  console.error("Unhandled rejection:", reason);
});

// Validate environment
function checkEnv() {
  const required = ["NEXT_PUBLIC_SUPABASE_URL", "SUPABASE_SERVICE_ROLE_KEY", "RESCUEGROUPS_API_KEY"];
  const missing = required.filter(k => !process.env[k]);
  if (missing.length > 0) {
    console.error(`\n\x1b[31m❌ Missing environment variables: ${missing.join(", ")}\x1b[0m`);
    console.error("Make sure .env.local exists in the project root.\n");
    process.exit(1);
  }
}

async function main() {
  checkEnv();

  console.log("\n" + "═".repeat(60));
  console.log("  WaitingTheLongest.com — Local Agent v2");
  console.log("  Real-time verification slideshow + error correction");
  console.log("  Dashboard: http://localhost:" + PORT);
  console.log("═".repeat(60) + "\n");

  // Warm shelter cache for fast dog event enrichment
  await warmShelterCache();

  server.listen(PORT, () => {
    bus.log("success", "system", `Agent v2 started. Dashboard at http://localhost:${PORT}`);
    bus.log("info", "system", `Tasks: ${tasks.map(t => `${t.name} (${t.description})`).join(", ")}`);
    bus.log("info", "system", "Features: real-time slideshow, error correction, circuit breaker, rate limit protection");
    bus.log("info", "system", "Press Ctrl+C to stop gracefully.");
  });

  // Initial DB stats
  await refreshDbStats();
  if (dbStats) {
    bus.log("info", "system", `Database: ${dbStats.totalDogs} dogs, ${dbStats.verifiedPct}% verified, ${dbStats.dateAccuracyPct}% date accuracy`);
    bus.log("info", "system", `Never checked: ${dbStats.neverChecked} dogs, est. ${dbStats.daysToFullCoverage} days to full coverage`);
    if (dbStats.criticalDogs > 0) {
      bus.log("error", "system", `⚡ ${dbStats.criticalDogs} CRITICAL dogs (euthanasia <24h)`);
    }
    if (dbStats.highUrgencyDogs > 0) {
      bus.log("warn", "system", `${dbStats.highUrgencyDogs} HIGH urgency dogs (euthanasia <72h)`);
    }
  }

  // Scheduler tick every 5 seconds
  setInterval(schedulerTick, 5000);

  // Refresh DB stats every 5 minutes
  setInterval(refreshDbStats, 5 * 60 * 1000);
}

main().catch(err => {
  console.error("\x1b[31mFatal agent error:\x1b[0m", err);
  process.exit(1);
});

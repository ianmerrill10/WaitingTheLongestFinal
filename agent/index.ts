/**
 * WaitingTheLongest Local Agent
 *
 * Persistent Node.js process that manages dog data accuracy 24/7.
 * Runs on your local Windows PC, connects to Supabase + RescueGroups.
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
import { runAudit } from "../src/lib/audit/engine";
import { computeAndStoreDailyStats } from "../src/lib/stats/compute-daily-stats";
import { createAdminClient } from "../src/lib/supabase/admin";

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

interface TaskDef {
  name: string;
  fn: () => Promise<void>;
  intervalMs: number;
  lastRun: number | null;
  nextRun: number;
  isRunning: boolean;
  runCount: number;
  lastDuration: number | null;
  lastError: string | null;
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
  dogsPerHour: number;
  tasks: Array<{
    name: string;
    isRunning: boolean;
    runCount: number;
    lastRun: string | null;
    nextRun: string;
    lastDuration: number | null;
    lastError: string | null;
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
}

// ─────────────────────────────────────────────
// SECTION 2: EventBus (log streaming)
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
    // Also print to console
    const prefix = { info: "   ", warn: "⚠  ", error: "❌ ", success: "✅ " }[level];
    console.log(`${prefix}[${category}] ${message}`);
  }

  getRecent(count = 200): LogEntry[] {
    return this.logs.slice(-count);
  }
}

const bus = new EventBus();

// ─────────────────────────────────────────────
// SECTION 3: Metrics
// ─────────────────────────────────────────────

const startedAt = new Date();
let totalDogsProcessed = 0;
let totalVerified = 0;
let totalDeactivated = 0;
let totalErrors = 0;
let totalApiCalls = 0;
let isPaused = false;
let currentTask: string | null = null;
let currentProgress: AgentMetrics["currentProgress"] = null;
let dbStats: AgentMetrics["db"] = null;

async function refreshDbStats() {
  try {
    const supabase = createAdminClient();
    const vStats = await getVerificationStats();

    // Get date accuracy
    const [verifiedConf, highConf] = await Promise.all([
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("date_confidence", "verified"),
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("date_confidence", "high"),
    ]);

    // Get urgency counts
    const [criticalRes, highRes] = await Promise.all([
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("urgency_level", "critical"),
      supabase.from("dogs").select("id", { count: "exact", head: true })
        .eq("is_available", true).eq("urgency_level", "high"),
    ]);

    const total = vStats.total;
    const verifiedConfCount = (verifiedConf.count ?? 0) + (highConf.count ?? 0);
    const dateAccuracyPct = total > 0 ? Math.round((verifiedConfCount / total) * 1000) / 10 : 0;
    const verifiedPct = total > 0 ? Math.round((vStats.verified / total) * 1000) / 10 : 0;

    // Estimate days to full coverage based on current rate
    const uptimeHours = (Date.now() - startedAt.getTime()) / (1000 * 60 * 60);
    const dogsPerDay = uptimeHours > 0.1 ? (totalVerified / uptimeHours) * 24 : 600;
    const remaining = vStats.neverChecked;
    const daysToFull = dogsPerDay > 0 ? Math.round((remaining / dogsPerDay) * 10) / 10 : 999;

    dbStats = {
      totalDogs: total,
      verifiedCount: vStats.verified,
      verifiedPct,
      dateAccuracyPct,
      avgWaitDays: 0, // computed below
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
    dogsPerHour,
    tasks: tasks.map(t => ({
      name: t.name,
      isRunning: t.isRunning,
      runCount: t.runCount,
      lastRun: t.lastRun ? new Date(t.lastRun).toISOString() : null,
      nextRun: new Date(t.nextRun).toISOString(),
      lastDuration: t.lastDuration,
      lastError: t.lastError,
    })),
    db: dbStats,
  };
}

// ─────────────────────────────────────────────
// SECTION 4: Task Operations
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

    bus.log("success", "sync", `Sync complete: +${result.inserted} new, ${result.updated} updated, ${result.sheltersCreated} shelters, ${result.errors} errors (${Math.round(result.duration / 1000)}s)`, result as unknown as Record<string, unknown>);

    syncPage += SYNC_PAGES;
    if (syncPage > MAX_RG_PAGES) {
      syncPage = 1;
      bus.log("info", "sync", "Wrapped around to page 1 — starting full cycle again");
    }
  } catch (err) {
    totalErrors++;
    bus.log("error", "sync", `Sync failed: ${err}`);
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

  bus.log("info", "verify", `Starting verification of ${TOTAL} dogs...`);

  for (let i = 0; i < TOTAL; i += MICRO) {
    const batchSize = Math.min(MICRO, TOTAL - i);
    const priority = i < 100 ? "never_verified" as const : "stale" as const;

    currentProgress = { current: processed, total: TOTAL, label: `Verifying dogs (${verified} confirmed, ${deactivated} removed)` };

    try {
      const result = await runVerification(batchSize, priority);
      processed += result.checked;
      verified += result.verified;
      deactivated += result.deactivated;
      errors += result.errors;
      availDates += result.availableDatesCaptured;
      foundDates += result.foundDatesCaptured;
      killDates += result.killDatesCaptured;
      totalApiCalls += result.checked;

      if (result.deactivated > 0) {
        bus.log("warn", "verify", `Batch ${Math.floor(i / MICRO) + 1}: ${result.deactivated} dogs deactivated (adopted/removed)`);
      }
      if (result.availableDatesCaptured > 0 || result.foundDatesCaptured > 0 || result.killDatesCaptured > 0) {
        bus.log("success", "verify", `Batch ${Math.floor(i / MICRO) + 1}: captured ${result.availableDatesCaptured} availableDates, ${result.foundDatesCaptured} foundDates, ${result.killDatesCaptured} killDates`);
      }
    } catch (err) {
      errors++;
      bus.log("error", "verify", `Batch ${Math.floor(i / MICRO) + 1} failed: ${err}`);
    }
  }

  totalDogsProcessed += processed;
  totalVerified += verified;
  totalDeactivated += deactivated;
  totalErrors += errors;

  bus.log("success", "verify", `Verification complete: ${processed} checked, ${verified} verified, ${deactivated} deactivated, ${availDates} availableDates, ${foundDates} foundDates, ${killDates} killDates, ${errors} errors`);
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
    bus.log("error", "audit", `Audit failed: ${err}`);
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
    bus.log("error", "audit", `Full audit failed: ${err}`);
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
  } catch (err) {
    totalErrors++;
    bus.log("error", "urgency", `Urgency update failed: ${err}`);
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
    bus.log("error", "stats", `Stats computation failed: ${err}`);
  } finally {
    currentProgress = null;
  }
}

// ─────────────────────────────────────────────
// SECTION 5: Task Scheduler
// ─────────────────────────────────────────────

const MIN = 60 * 1000;
const HOUR = 60 * MIN;

const tasks: TaskDef[] = [
  { name: "verify", fn: taskVerify, intervalMs: 30 * MIN, lastRun: null, nextRun: Date.now() + 5000, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
  { name: "sync", fn: taskSync, intervalMs: 60 * MIN, lastRun: null, nextRun: Date.now() + 2 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
  { name: "urgency", fn: taskUrgency, intervalMs: 2 * HOUR, lastRun: null, nextRun: Date.now() + 10 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
  { name: "audit", fn: taskAudit, intervalMs: 4 * HOUR, lastRun: null, nextRun: Date.now() + 30 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
  { name: "stats", fn: taskStats, intervalMs: 6 * HOUR, lastRun: null, nextRun: Date.now() + 15 * MIN, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
  { name: "fullAudit", fn: taskFullAudit, intervalMs: 24 * HOUR, lastRun: null, nextRun: Date.now() + 1 * HOUR, isRunning: false, runCount: 0, lastDuration: null, lastError: null },
];

let isRunningTask = false;

async function schedulerTick() {
  if (isPaused || isRunningTask) return;

  const now = Date.now();
  const overdue = tasks.filter(t => !t.isRunning && now >= t.nextRun);

  if (overdue.length === 0) return;

  // Run the most overdue task
  const task = overdue.sort((a, b) => a.nextRun - b.nextRun)[0];

  isRunningTask = true;
  task.isRunning = true;
  currentTask = task.name;

  const taskStart = Date.now();
  try {
    await task.fn();
    task.lastError = null;
  } catch (err) {
    task.lastError = String(err);
  } finally {
    task.isRunning = false;
    task.lastRun = Date.now();
    task.lastDuration = Date.now() - taskStart;
    task.nextRun = Date.now() + task.intervalMs;
    task.runCount++;
    isRunningTask = false;
    currentTask = null;
    currentProgress = null;
  }
}

// ─────────────────────────────────────────────
// SECTION 6: HTTP Server
// ─────────────────────────────────────────────

const PORT = 3847;
const sseClients: http.ServerResponse[] = [];

// Forward logs to SSE clients
bus.on("log", (entry: LogEntry) => {
  const data = `data: ${JSON.stringify(entry)}\n\n`;
  for (let i = sseClients.length - 1; i >= 0; i--) {
    try {
      sseClients[i].write(data);
    } catch {
      sseClients.splice(i, 1);
    }
  }
});

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

    // Send recent logs
    for (const entry of bus.getRecent(100)) {
      res.write(`data: ${JSON.stringify(entry)}\n\n`);
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

        // Run immediately
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

  // ── 404 ──
  res.writeHead(404, { "Content-Type": "text/plain" });
  res.end("Not found");
});

// ─────────────────────────────────────────────
// SECTION 7: Startup & Shutdown
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

// Validate environment
function checkEnv() {
  const required = ["NEXT_PUBLIC_SUPABASE_URL", "SUPABASE_SERVICE_ROLE_KEY", "RESCUEGROUPS_API_KEY"];
  const missing = required.filter(k => !process.env[k]);
  if (missing.length > 0) {
    console.error(`\n❌ Missing environment variables: ${missing.join(", ")}`);
    console.error("Make sure .env.local exists in the project root.\n");
    process.exit(1);
  }
}

async function main() {
  checkEnv();

  console.log("\n" + "=".repeat(60));
  console.log("  WaitingTheLongest.com — Local Agent");
  console.log("  Dashboard: http://localhost:" + PORT);
  console.log("=".repeat(60) + "\n");

  server.listen(PORT, () => {
    bus.log("success", "system", `Agent started. Dashboard at http://localhost:${PORT}`);
    bus.log("info", "system", `Tasks: ${tasks.map(t => t.name).join(", ")}`);
    bus.log("info", "system", "Press Ctrl+C to stop gracefully.");
  });

  // Initial DB stats
  await refreshDbStats();
  if (dbStats) {
    bus.log("info", "system", `Database: ${dbStats.totalDogs} dogs, ${dbStats.verifiedPct}% verified, ${dbStats.dateAccuracyPct}% date accuracy`);
    bus.log("info", "system", `Never checked: ${dbStats.neverChecked} dogs, est. ${dbStats.daysToFullCoverage} days to full coverage`);
  }

  // Scheduler tick every 5 seconds
  setInterval(schedulerTick, 5000);

  // Refresh DB stats every 5 minutes
  setInterval(refreshDbStats, 5 * 60 * 1000);
}

main().catch(err => {
  console.error("Fatal agent error:", err);
  process.exit(1);
});

/**
 * Freshness Agent — Ensures expired/stale dogs are properly handled.
 *
 * Port: 3850
 *
 * Core responsibilities:
 * 1. Verify dogs whose euthanasia_date has passed (0-48h window)
 *    - If still at shelter → reprieve (clear euth date, set urgency=medium)
 *    - If not at shelter → deactivate (status=outcome_unknown)
 * 2. Re-verify stale listings (no sync in 14+ days)
 * 3. Check recently deactivated dogs for false positives
 * 4. Track freshness metrics for the dashboard
 *
 * Usage: npx tsx --tsconfig tsconfig.json agent/freshness.ts
 */

import http from "http";
import path from "path";
import dotenv from "dotenv";
import { createClient } from "@supabase/supabase-js";

// Load .env.local before accessing env vars
dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const PORT = 3850;
const SUPABASE_URL = process.env.NEXT_PUBLIC_SUPABASE_URL!;
const SUPABASE_KEY = process.env.SUPABASE_SERVICE_ROLE_KEY!;

function createAdminClient() {
  return createClient(SUPABASE_URL, SUPABASE_KEY, {
    auth: { autoRefreshToken: false, persistSession: false },
  });
}

// ─── Types ───

interface FreshnessMetrics {
  startedAt: string;
  cycleCount: number;
  totalProcessed: number;
  reprieves: number;
  deactivated: number;
  staleVerified: number;
  staleDeactivated: number;
  errors: number;
  lastCycleAt: string | null;
  lastCycleDuration: number;
  currentTask: string;
  recentActions: ActionLog[];
  expiredQueue: number;
  staleQueue: number;
}

interface ActionLog {
  timestamp: string;
  dogName: string;
  dogId: string;
  action: "reprieve" | "deactivated" | "verified_fresh" | "stale_deactivated" | "error";
  detail: string;
}

// ─── State ───

const metrics: FreshnessMetrics = {
  startedAt: new Date().toISOString(),
  cycleCount: 0,
  totalProcessed: 0,
  reprieves: 0,
  deactivated: 0,
  staleVerified: 0,
  staleDeactivated: 0,
  errors: 0,
  lastCycleAt: null,
  lastCycleDuration: 0,
  currentTask: "starting",
  recentActions: [],
  expiredQueue: 0,
  staleQueue: 0,
};

const sseClients = new Set<http.ServerResponse>();

function addAction(action: ActionLog) {
  metrics.recentActions.unshift(action);
  if (metrics.recentActions.length > 100) metrics.recentActions.pop();
  broadcastSSE({ type: "action", data: action });
}

function broadcastSSE(data: unknown) {
  const msg = `data: ${JSON.stringify(data)}\n\n`;
  for (const client of Array.from(sseClients)) {
    try { client.write(msg); } catch { sseClients.delete(client); }
  }
}

// ─── Core: Process Expired Dogs ───

async function processExpiredDogs(): Promise<{ processed: number; reprieves: number; deactivated: number; errors: number }> {
  const supabase = createAdminClient();
  const now = new Date();
  const twoDaysAgo = new Date(now.getTime() - 48 * 60 * 60 * 1000);
  let processed = 0, reprieves = 0, deactivated = 0, errors = 0;

  // Find dogs with expired euthanasia dates, still available, within 48h window
  // (Dogs expired >48h are handled by the audit checkStatuses)
  const { data: expiredDogs } = await supabase
    .from("dogs")
    .select("id, name, external_id, external_source, external_url, shelter_id, euthanasia_date, breed_primary")
    .eq("is_available", true)
    .not("euthanasia_date", "is", null)
    .lt("euthanasia_date", now.toISOString())
    .gt("euthanasia_date", twoDaysAgo.toISOString())
    .order("euthanasia_date", { ascending: false }) // most recent first
    .limit(30);

  if (!expiredDogs || expiredDogs.length === 0) return { processed, reprieves, deactivated, errors };

  metrics.expiredQueue = expiredDogs.length;

  for (const dog of expiredDogs) {
    processed++;
    const hoursExpired = (now.getTime() - new Date(dog.euthanasia_date).getTime()) / (1000 * 60 * 60);

    try {
      let stillAvailable = false;

      // Strategy 1: RescueGroups API check
      if (dog.external_source === "rescuegroups" && dog.external_id) {
        stillAvailable = await checkRescueGroupsAvailability(dog.external_id);
      }

      // Strategy 2: External URL liveness check
      if (!stillAvailable && dog.external_url) {
        stillAvailable = await checkUrlAlive(dog.external_url);
      }

      // Strategy 3: For scraper-sourced dogs, check if shelter recently scraped
      if (!stillAvailable && dog.external_source?.startsWith("scraper_") && dog.shelter_id) {
        stillAvailable = await checkShelterRecentlySscraped(supabase, dog.shelter_id, dog.external_id);
      }

      if (stillAvailable) {
        // REPRIEVE! Dog is still at shelter — they got more time
        await supabase
          .from("dogs")
          .update({
            euthanasia_date: null,
            urgency_level: "medium",
            is_on_euthanasia_list: false,
            status: "available",
            verification_status: "verified",
            last_verified_at: now.toISOString(),
          })
          .eq("id", dog.id);

        reprieves++;
        addAction({
          timestamp: now.toISOString(),
          dogName: dog.name,
          dogId: dog.id,
          action: "reprieve",
          detail: `Reprieved! Still at shelter after ${hoursExpired.toFixed(1)}h past deadline`,
        });
      } else if (hoursExpired > 24) {
        // Expired >24h and not found — deactivate
        await supabase
          .from("dogs")
          .update({
            is_available: false,
            status: "outcome_unknown",
            urgency_level: "normal",
            last_verified_at: now.toISOString(),
          })
          .eq("id", dog.id);

        deactivated++;
        addAction({
          timestamp: now.toISOString(),
          dogName: dog.name,
          dogId: dog.id,
          action: "deactivated",
          detail: `Not found after ${hoursExpired.toFixed(1)}h past deadline — deactivated`,
        });
      }
      // If expired <24h and not found, wait longer before deactivating
    } catch (err) {
      errors++;
      addAction({
        timestamp: now.toISOString(),
        dogName: dog.name,
        dogId: dog.id,
        action: "error",
        detail: `Verification failed: ${err instanceof Error ? err.message : String(err)}`,
      });
    }
  }

  return { processed, reprieves, deactivated, errors };
}

// ─── Core: Process Stale Listings ───

async function processStaleDogs(): Promise<{ verified: number; deactivated: number; errors: number }> {
  const supabase = createAdminClient();
  const now = new Date();
  let verified = 0, deactivated = 0, errors = 0;

  // Find dogs not verified in 14+ days
  const fourteenDaysAgo = new Date(now.getTime() - 14 * 24 * 60 * 60 * 1000);
  const { data: staleDogs } = await supabase
    .from("dogs")
    .select("id, name, external_id, external_source, external_url, shelter_id, last_verified_at, last_synced_at")
    .eq("is_available", true)
    .or(`last_verified_at.lt.${fourteenDaysAgo.toISOString()},last_verified_at.is.null`)
    .or(`last_synced_at.lt.${fourteenDaysAgo.toISOString()},last_synced_at.is.null`)
    .order("last_verified_at", { ascending: true, nullsFirst: true })
    .limit(20);

  if (!staleDogs || staleDogs.length === 0) return { verified, deactivated, errors };

  metrics.staleQueue = staleDogs.length;

  for (const dog of staleDogs) {
    try {
      let stillAvailable = false;

      if (dog.external_source === "rescuegroups" && dog.external_id) {
        stillAvailable = await checkRescueGroupsAvailability(dog.external_id);
      } else if (dog.external_url) {
        stillAvailable = await checkUrlAlive(dog.external_url);
      }

      if (stillAvailable) {
        await supabase
          .from("dogs")
          .update({
            verification_status: "verified",
            last_verified_at: now.toISOString(),
          })
          .eq("id", dog.id);
        verified++;
      } else {
        // Not found after being stale for 14+ days — deactivate
        const lastSeen = dog.last_verified_at || dog.last_synced_at;
        const daysSinceLastSeen = lastSeen
          ? (now.getTime() - new Date(lastSeen).getTime()) / (1000 * 60 * 60 * 24)
          : 999;

        if (daysSinceLastSeen > 21) {
          // Stale >21 days and not found — definitely gone
          await supabase
            .from("dogs")
            .update({
              is_available: false,
              status: "adopted",
              verification_status: "not_found",
              last_verified_at: now.toISOString(),
            })
            .eq("id", dog.id);

          deactivated++;
          addAction({
            timestamp: now.toISOString(),
            dogName: dog.name,
            dogId: dog.id,
            action: "stale_deactivated",
            detail: `Stale ${Math.round(daysSinceLastSeen)} days, not found — deactivated`,
          });
        } else {
          // Mark as not found but keep available for a bit longer
          await supabase
            .from("dogs")
            .update({
              verification_status: "not_found",
              last_verified_at: now.toISOString(),
            })
            .eq("id", dog.id);

          addAction({
            timestamp: now.toISOString(),
            dogName: dog.name,
            dogId: dog.id,
            action: "verified_fresh",
            detail: `Stale ${Math.round(daysSinceLastSeen)} days, not found — flagged for follow-up`,
          });
        }
      }
    } catch {
      errors++;
    }
  }

  return { verified, deactivated, errors };
}

// ─── Core: Reactivation Check ───
// Check recently deactivated dogs for false positives (maybe we deactivated too aggressively)

async function checkRecentDeactivations(): Promise<{ reactivated: number }> {
  const supabase = createAdminClient();
  const now = new Date();
  const oneDayAgo = new Date(now.getTime() - 24 * 60 * 60 * 1000);
  let reactivated = 0;

  // Find dogs deactivated as "outcome_unknown" in the last 24h
  const { data: recentDeactivated } = await supabase
    .from("dogs")
    .select("id, name, external_id, external_source, external_url")
    .eq("is_available", false)
    .eq("status", "outcome_unknown")
    .gt("updated_at", oneDayAgo.toISOString())
    .limit(10);

  if (!recentDeactivated || recentDeactivated.length === 0) return { reactivated };

  for (const dog of recentDeactivated) {
    try {
      let stillAvailable = false;

      if (dog.external_source === "rescuegroups" && dog.external_id) {
        stillAvailable = await checkRescueGroupsAvailability(dog.external_id);
      } else if (dog.external_url) {
        stillAvailable = await checkUrlAlive(dog.external_url);
      }

      if (stillAvailable) {
        // False positive! Reactivate.
        await supabase
          .from("dogs")
          .update({
            is_available: true,
            status: "available",
            urgency_level: "normal",
            euthanasia_date: null,
            verification_status: "verified",
            last_verified_at: now.toISOString(),
          })
          .eq("id", dog.id);

        reactivated++;
        addAction({
          timestamp: now.toISOString(),
          dogName: dog.name,
          dogId: dog.id,
          action: "reprieve",
          detail: `Reactivated — false positive deactivation, dog still at shelter`,
        });
      }
    } catch {
      // Skip
    }
  }

  return { reactivated };
}

// ─── Helpers ───

async function checkRescueGroupsAvailability(externalId: string): Promise<boolean> {
  try {
    const apiKey = process.env.RESCUEGROUPS_API_KEY;
    if (!apiKey) return false;

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);

    const response = await fetch(`https://api.rescuegroups.org/v5/public/animals/${externalId}`, {
      headers: {
        Authorization: apiKey,
        "Content-Type": "application/vnd.api+json",
      },
      signal: controller.signal,
    });

    clearTimeout(timeout);

    if (response.status === 200) {
      const data = await response.json();
      const status = data?.data?.[0]?.attributes?.statuses?.name ||
                     data?.data?.attributes?.statuses?.name;
      return status === "Available" || status === "Available - Pending";
    }
    return false;
  } catch {
    return false;
  }
}

async function checkUrlAlive(url: string): Promise<boolean> {
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);

    const response = await fetch(url, {
      method: "HEAD",
      signal: controller.signal,
      redirect: "follow",
      headers: { "User-Agent": "WaitingTheLongest.com/1.0 (freshness-agent)" },
    });

    clearTimeout(timeout);
    return response.status >= 200 && response.status < 400;
  } catch {
    return false;
  }
}

async function checkShelterRecentlySscraped(
  supabase: ReturnType<typeof createAdminClient>,
  shelterId: string,
  externalId: string,
): Promise<boolean> {
  // Check if the dog was seen in a recent scrape (within last 24h)
  const oneDayAgo = new Date(Date.now() - 24 * 60 * 60 * 1000);
  const { data } = await supabase
    .from("dogs")
    .select("id")
    .eq("shelter_id", shelterId)
    .eq("external_id", externalId)
    .eq("is_available", true)
    .gt("last_synced_at", oneDayAgo.toISOString())
    .limit(1);

  return !!data && data.length > 0;
}

// ─── Main Cycle ───

async function runCycle() {
  const cycleStart = Date.now();
  metrics.cycleCount++;
  metrics.currentTask = "processing expired dogs";
  broadcastSSE({ type: "status", data: metrics.currentTask });

  try {
    // Phase 1: Process expired euthanasia dogs (most important)
    const expired = await processExpiredDogs();
    metrics.totalProcessed += expired.processed;
    metrics.reprieves += expired.reprieves;
    metrics.deactivated += expired.deactivated;
    metrics.errors += expired.errors;

    // Phase 2: Process stale listings
    metrics.currentTask = "checking stale listings";
    broadcastSSE({ type: "status", data: metrics.currentTask });

    const stale = await processStaleDogs();
    metrics.staleVerified += stale.verified;
    metrics.staleDeactivated += stale.deactivated;
    metrics.errors += stale.errors;

    // Phase 3: Check recent deactivations for false positives
    metrics.currentTask = "checking for false positives";
    broadcastSSE({ type: "status", data: metrics.currentTask });

    const recheck = await checkRecentDeactivations();
    metrics.reprieves += recheck.reactivated;

    // Update queue sizes
    const supabase = createAdminClient();
    const now = new Date();
    const twoDaysAgo = new Date(now.getTime() - 48 * 60 * 60 * 1000);

    const [expQ, staleQ] = await Promise.all([
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .not("euthanasia_date", "is", null)
        .lt("euthanasia_date", now.toISOString())
        .gt("euthanasia_date", twoDaysAgo.toISOString()),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .lt("last_verified_at", new Date(now.getTime() - 14 * 24 * 60 * 60 * 1000).toISOString()),
    ]);

    metrics.expiredQueue = expQ.count ?? 0;
    metrics.staleQueue = staleQ.count ?? 0;

  } catch (err) {
    metrics.errors++;
    console.error("[FRESHNESS] Cycle error:", err);
  }

  metrics.lastCycleAt = new Date().toISOString();
  metrics.lastCycleDuration = Date.now() - cycleStart;
  metrics.currentTask = "idle — next cycle in 10 min";

  broadcastSSE({ type: "metrics", data: metrics });
  console.log(
    `[FRESHNESS] Cycle #${metrics.cycleCount} complete: ` +
    `${metrics.totalProcessed} processed, ${metrics.reprieves} reprieves, ` +
    `${metrics.deactivated} deactivated (${metrics.lastCycleDuration}ms)`
  );
}

// ─── HTTP Dashboard ───

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", `http://localhost:${PORT}`);

  // CORS
  res.setHeader("Access-Control-Allow-Origin", "*");

  if (url.pathname === "/api/metrics") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(metrics));
    return;
  }

  if (url.pathname === "/api/events") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    });
    sseClients.add(res);
    res.write(`data: ${JSON.stringify({ type: "connected", data: metrics })}\n\n`);
    req.on("close", () => sseClients.delete(res));
    return;
  }

  if (url.pathname === "/api/trigger" && req.method === "POST") {
    runCycle().catch(console.error);
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: true, message: "Cycle triggered" }));
    return;
  }

  // Dashboard HTML
  res.writeHead(200, { "Content-Type": "text/html" });
  res.end(`<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><title>Freshness Agent</title>
<style>
  body { font-family: system-ui; background: #0f172a; color: #e2e8f0; margin: 0; padding: 20px; }
  h1 { color: #38bdf8; margin: 0 0 8px; }
  .subtitle { color: #94a3b8; margin-bottom: 24px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 24px; }
  .card { background: #1e293b; border-radius: 12px; padding: 16px; border: 1px solid #334155; }
  .card .label { color: #94a3b8; font-size: 12px; text-transform: uppercase; margin-bottom: 4px; }
  .card .value { font-size: 28px; font-weight: bold; font-variant-numeric: tabular-nums; }
  .green { color: #4ade80; } .red { color: #f87171; } .blue { color: #60a5fa; } .amber { color: #fbbf24; }
  .actions { background: #1e293b; border-radius: 12px; padding: 16px; border: 1px solid #334155; max-height: 400px; overflow-y: auto; }
  .action { padding: 8px 0; border-bottom: 1px solid #334155; font-size: 13px; display: flex; gap: 12px; }
  .action:last-child { border-bottom: none; }
  .action .time { color: #64748b; white-space: nowrap; }
  .action .badge { padding: 2px 8px; border-radius: 4px; font-size: 11px; font-weight: 600; }
  .badge-reprieve { background: #166534; color: #4ade80; }
  .badge-deactivated { background: #7f1d1d; color: #fca5a5; }
  .badge-error { background: #713f12; color: #fde047; }
  .badge-verified { background: #1e3a5f; color: #93c5fd; }
  .status { color: #94a3b8; font-size: 14px; margin-bottom: 16px; }
  button { background: #3b82f6; color: white; border: none; padding: 8px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; }
  button:hover { background: #2563eb; }
</style></head><body>
<h1>Freshness Agent</h1>
<p class="subtitle">Auto-verifies expired & stale dogs | Port ${PORT}</p>
<div class="status" id="status">Starting...</div>
<div class="grid">
  <div class="card"><div class="label">Cycles</div><div class="value blue" id="cycles">0</div></div>
  <div class="card"><div class="label">Processed</div><div class="value" id="processed">0</div></div>
  <div class="card"><div class="label">Reprieves</div><div class="value green" id="reprieves">0</div></div>
  <div class="card"><div class="label">Deactivated</div><div class="value red" id="deactivated">0</div></div>
  <div class="card"><div class="label">Stale Verified</div><div class="value blue" id="staleVerified">0</div></div>
  <div class="card"><div class="label">Expired Queue</div><div class="value amber" id="expiredQueue">0</div></div>
  <div class="card"><div class="label">Stale Queue</div><div class="value amber" id="staleQueue">0</div></div>
  <div class="card"><div class="label">Errors</div><div class="value red" id="errors">0</div></div>
</div>
<button onclick="fetch('/api/trigger',{method:'POST'})">Trigger Cycle Now</button>
<h2 style="margin-top:24px; color:#94a3b8; font-size:14px; text-transform:uppercase;">Recent Actions</h2>
<div class="actions" id="actions"><div style="color:#64748b;">Waiting for first cycle...</div></div>
<script>
const es = new EventSource('/api/events');
es.onmessage = (e) => {
  const {type, data} = JSON.parse(e.data);
  if (type === 'metrics' || type === 'connected') {
    document.getElementById('cycles').textContent = data.cycleCount;
    document.getElementById('processed').textContent = data.totalProcessed;
    document.getElementById('reprieves').textContent = data.reprieves;
    document.getElementById('deactivated').textContent = data.deactivated;
    document.getElementById('staleVerified').textContent = data.staleVerified;
    document.getElementById('expiredQueue').textContent = data.expiredQueue;
    document.getElementById('staleQueue').textContent = data.staleQueue;
    document.getElementById('errors').textContent = data.errors;
    document.getElementById('status').textContent = data.currentTask;
  }
  if (type === 'action') {
    const el = document.getElementById('actions');
    const badgeClass = {reprieve:'badge-reprieve', deactivated:'badge-deactivated', error:'badge-error', verified_fresh:'badge-verified', stale_deactivated:'badge-deactivated'}[data.action] || '';
    const html = '<div class="action"><span class="time">' + new Date(data.timestamp).toLocaleTimeString() + '</span><span class="badge ' + badgeClass + '">' + data.action.replace('_',' ') + '</span><span><b>' + data.dogName + '</b> — ' + data.detail + '</span></div>';
    if (el.querySelector('div[style]')) el.innerHTML = '';
    el.insertAdjacentHTML('afterbegin', html);
  }
};
</script></body></html>`);
});

// ─── Start ───

server.listen(PORT, () => {
  console.log(`[FRESHNESS] Agent running on http://localhost:${PORT}`);
  console.log("[FRESHNESS] Cycle interval: 10 minutes");

  // Run first cycle after 5 seconds
  setTimeout(() => runCycle().catch(console.error), 5000);

  // Then every 10 minutes
  setInterval(() => runCycle().catch(console.error), 10 * 60 * 1000);
});

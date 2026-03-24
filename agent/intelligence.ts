/**
 * Shelter Intelligence Agent — Analyzes patterns, scores risk, optimizes coverage.
 *
 * Port: 3851
 *
 * Core capabilities:
 * 1. Shelter Risk Scoring — rank shelters by euthanasia likelihood
 * 2. Pattern Detection — identify euthanasia timing patterns
 * 3. Coverage Gap Analysis — find under-served high-risk shelters
 * 4. Auto-Optimization — adjust scraper frequency for high-risk shelters
 * 5. Anomaly Detection — alert on mass euthanasia events
 * 6. Breed/State Risk Heatmaps — track which breeds and states are highest risk
 *
 * Usage: npx tsx --tsconfig tsconfig.json agent/intelligence.ts
 */

import http from "http";
import path from "path";
import dotenv from "dotenv";
import { createClient } from "@supabase/supabase-js";

// Load .env.local before accessing env vars
dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const PORT = 3851;
const SUPABASE_URL = process.env.NEXT_PUBLIC_SUPABASE_URL!;
const SUPABASE_KEY = process.env.SUPABASE_SERVICE_ROLE_KEY!;

function createAdminClient() {
  return createClient(SUPABASE_URL, SUPABASE_KEY, {
    auth: { autoRefreshToken: false, persistSession: false },
  });
}

// ─── Types ───

interface ShelterRisk {
  shelterId: string;
  shelterName: string;
  city: string;
  state: string;
  riskScore: number;       // 0-100
  totalDogs: number;
  urgentDogs: number;
  expiredDogs: number;     // dogs whose euth date passed
  avgDaysToEuth: number;   // average days from listing to euth date
  lastScrapedAt: string | null;
  scrapeFrequency: string; // current scraping interval
  recommendation: string;  // action recommendation
}

interface BreedRisk {
  breed: string;
  totalAvailable: number;
  totalUrgent: number;
  riskPct: number;
}

interface StateRisk {
  state: string;
  totalDogs: number;
  urgentDogs: number;
  shelterCount: number;
  riskPct: number;
}

interface Anomaly {
  timestamp: string;
  type: "mass_euthanasia" | "shelter_offline" | "surge" | "coverage_gap";
  severity: "critical" | "warning" | "info";
  shelterId?: string;
  shelterName?: string;
  message: string;
  data: Record<string, unknown>;
}

interface IntelMetrics {
  startedAt: string;
  cycleCount: number;
  lastCycleAt: string | null;
  lastCycleDuration: number;
  currentTask: string;
  shelterRiskScores: ShelterRisk[];
  topBreedRisks: BreedRisk[];
  stateRisks: StateRisk[];
  recentAnomalies: Anomaly[];
  coverageGaps: number;
  highRiskShelters: number;
  optimizationsApplied: number;
  totalSheltersAnalyzed: number;
}

// ─── State ───

const metrics: IntelMetrics = {
  startedAt: new Date().toISOString(),
  cycleCount: 0,
  lastCycleAt: null,
  lastCycleDuration: 0,
  currentTask: "starting",
  shelterRiskScores: [],
  topBreedRisks: [],
  stateRisks: [],
  recentAnomalies: [],
  coverageGaps: 0,
  highRiskShelters: 0,
  optimizationsApplied: 0,
  totalSheltersAnalyzed: 0,
};

const sseClients = new Set<http.ServerResponse>();

function broadcastSSE(data: unknown) {
  const msg = `data: ${JSON.stringify(data)}\n\n`;
  for (const client of Array.from(sseClients)) {
    try { client.write(msg); } catch { sseClients.delete(client); }
  }
}

function addAnomaly(anomaly: Anomaly) {
  metrics.recentAnomalies.unshift(anomaly);
  if (metrics.recentAnomalies.length > 50) metrics.recentAnomalies.pop();
  broadcastSSE({ type: "anomaly", data: anomaly });
}

// ─── Core: Shelter Risk Scoring ───

async function computeShelterRiskScores(): Promise<ShelterRisk[]> {
  const supabase = createAdminClient();
  const t0 = Date.now();

  // Get shelters with dogs
  console.log("[INTEL]   Fetching shelters...");
  const { data: shelters, error: shelterErr } = await supabase
    .from("shelters")
    .select("id, name, city, state_code, available_dog_count, urgent_dog_count, website_platform, last_scraped_at, next_scrape_at")
    .gt("available_dog_count", 0)
    .order("urgent_dog_count", { ascending: false })
    .limit(500);

  console.log(`[INTEL]   Shelters: ${shelters?.length ?? 0} fetched in ${Date.now() - t0}ms${shelterErr ? ` ERROR: ${shelterErr.message}` : ""}`);
  if (!shelters || shelters.length === 0) return [];

  const thirtyDaysAgo = new Date(Date.now() - 30 * 24 * 60 * 60 * 1000).toISOString();

  // Single query: Get ALL expired dogs from last 30 days, group client-side
  const expiredMap = new Map<string, number>();
  {
    const t1 = Date.now();
    console.log("[INTEL]   Fetching expired dogs...");
    const { data: expiredDogs, error: expErr } = await supabase
      .from("dogs")
      .select("shelter_id")
      .eq("is_available", false)
      .in("status", ["outcome_unknown", "euthanized", "deceased"])
      .gt("updated_at", thirtyDaysAgo)
      .limit(5000);

    console.log(`[INTEL]   Expired dogs: ${expiredDogs?.length ?? 0} fetched in ${Date.now() - t1}ms${expErr ? ` ERROR: ${expErr.message}` : ""}`);
    if (expiredDogs) {
      for (const dog of expiredDogs) {
        expiredMap.set(dog.shelter_id, (expiredMap.get(dog.shelter_id) || 0) + 1);
      }
    }
  }

  // Single query: Get ALL dogs with euthanasia dates, group client-side
  const euthMap = new Map<string, number>();
  {
    const t2 = Date.now();
    console.log("[INTEL]   Fetching euth date data...");
    const { data: euthDogs, error: euthErr } = await supabase
      .from("dogs")
      .select("shelter_id, intake_date, euthanasia_date")
      .eq("is_available", true)
      .not("euthanasia_date", "is", null)
      .not("intake_date", "is", null)
      .limit(5000);

    console.log(`[INTEL]   Euth dogs: ${euthDogs?.length ?? 0} fetched in ${Date.now() - t2}ms${euthErr ? ` ERROR: ${euthErr.message}` : ""}`);
    if (euthDogs) {
      const shelterDays = new Map<string, number[]>();
      for (const d of euthDogs) {
        const intake = new Date(d.intake_date);
        const euth = new Date(d.euthanasia_date);
        const days = (euth.getTime() - intake.getTime()) / (1000 * 60 * 60 * 24);
        if (days > 0 && days < 365) {
          const arr = shelterDays.get(d.shelter_id) || [];
          arr.push(days);
          shelterDays.set(d.shelter_id, arr);
        }
      }
      for (const [sid, days] of Array.from(shelterDays.entries())) {
        euthMap.set(sid, days.reduce((a: number, b: number) => a + b, 0) / days.length);
      }
    }
  }
  console.log(`[INTEL]   Total shelter risk computation: ${Date.now() - t0}ms`);

  // Now compute risk scores with pre-fetched data (no more individual queries)
  const risks: ShelterRisk[] = [];

  for (const shelter of shelters) {
    const expiredCount = expiredMap.get(shelter.id) || 0;
    const avgDaysToEuth = euthMap.get(shelter.id) || 0;

    const urgentPct = shelter.available_dog_count > 0
      ? (shelter.urgent_dog_count / shelter.available_dog_count) * 100
      : 0;
    const expiredFactor = Math.min(expiredCount * 5, 30);
    const urgentFactor = Math.min(urgentPct, 40);
    const windowFactor = avgDaysToEuth > 0 ? Math.max(0, 30 - avgDaysToEuth) : 0;

    const riskScore = Math.min(100, Math.round(urgentFactor + expiredFactor + windowFactor));

    let recommendation = "Monitor normally";
    let scrapeFrequency = "24h";
    if (riskScore >= 80) {
      recommendation = "CRITICAL — scrape every 6h, prioritize verification";
      scrapeFrequency = "6h";
    } else if (riskScore >= 60) {
      recommendation = "High risk — scrape every 12h";
      scrapeFrequency = "12h";
    } else if (riskScore >= 30) {
      recommendation = "Moderate risk — scrape every 24h";
      scrapeFrequency = "24h";
    } else {
      scrapeFrequency = "48h";
    }

    risks.push({
      shelterId: shelter.id,
      shelterName: shelter.name,
      city: shelter.city || "",
      state: shelter.state_code || "",
      riskScore,
      totalDogs: shelter.available_dog_count,
      urgentDogs: shelter.urgent_dog_count,
      expiredDogs: expiredCount,
      avgDaysToEuth: Math.round(avgDaysToEuth * 10) / 10,
      lastScrapedAt: shelter.last_scraped_at,
      scrapeFrequency,
      recommendation,
    });
  }

  risks.sort((a, b) => b.riskScore - a.riskScore);
  return risks;
}

// ─── Core: Breed Risk Analysis ───

async function computeBreedRisks(): Promise<BreedRisk[]> {
  const supabase = createAdminClient();

  // Use breed_counts view from shelters or aggregate from denormalized data
  // Fetch breed counts using paginated approach instead of pulling all 40K+ rows
  const breedCounts = new Map<string, { total: number; urgent: number }>();

  // Get total counts per breed (paginate to handle >1000 breeds)
  let offset = 0;
  while (true) {
    const { data: page } = await supabase
      .from("dogs")
      .select("breed_primary")
      .eq("is_available", true)
      .range(offset, offset + 4999);

    if (!page || page.length === 0) break;

    for (const dog of page) {
      const breed = dog.breed_primary || "Unknown";
      const entry = breedCounts.get(breed) || { total: 0, urgent: 0 };
      entry.total++;
      breedCounts.set(breed, entry);
    }

    if (page.length < 5000) break;
    offset += 5000;
  }

  // Get urgent counts (much smaller set, usually <1000)
  const { data: urgentBreeds } = await supabase
    .from("dogs")
    .select("breed_primary")
    .eq("is_available", true)
    .in("urgency_level", ["critical", "high"]);

  if (urgentBreeds) {
    for (const dog of urgentBreeds) {
      const breed = dog.breed_primary || "Unknown";
      const entry = breedCounts.get(breed);
      if (entry) entry.urgent++;
    }
  }

  const risks: BreedRisk[] = [];
  for (const [breed, counts] of Array.from(breedCounts.entries())) {
    if (counts.total < 5) continue;
    risks.push({
      breed,
      totalAvailable: counts.total,
      totalUrgent: counts.urgent,
      riskPct: counts.total > 0 ? Math.round((counts.urgent / counts.total) * 1000) / 10 : 0,
    });
  }

  risks.sort((a, b) => b.riskPct - a.riskPct);
  return risks.slice(0, 30);
}

// ─── Core: State Risk Analysis ───

async function computeStateRisks(): Promise<StateRisk[]> {
  const supabase = createAdminClient();

  const { data: shelters } = await supabase
    .from("shelters")
    .select("state_code, available_dog_count, urgent_dog_count")
    .gt("available_dog_count", 0);

  if (!shelters) return [];

  const stateMap = new Map<string, { total: number; urgent: number; shelters: number }>();
  for (const s of shelters) {
    const state = s.state_code || "Unknown";
    const entry = stateMap.get(state) || { total: 0, urgent: 0, shelters: 0 };
    entry.total += s.available_dog_count || 0;
    entry.urgent += s.urgent_dog_count || 0;
    entry.shelters++;
    stateMap.set(state, entry);
  }

  const risks: StateRisk[] = [];
  for (const [state, data] of Array.from(stateMap.entries())) {
    risks.push({
      state,
      totalDogs: data.total,
      urgentDogs: data.urgent,
      shelterCount: data.shelters,
      riskPct: data.total > 0 ? Math.round((data.urgent / data.total) * 1000) / 10 : 0,
    });
  }

  risks.sort((a, b) => b.urgentDogs - a.urgentDogs);
  return risks;
}

// ─── Core: Anomaly Detection ───

async function detectAnomalies(): Promise<void> {
  const supabase = createAdminClient();
  const now = new Date();
  const oneDayAgo = new Date(now.getTime() - 24 * 60 * 60 * 1000);

  // 1. Mass deactivation events (>10 dogs deactivated from one shelter in 24h)
  const { data: recentDeactivated } = await supabase
    .from("dogs")
    .select("shelter_id, shelters!inner(name, city, state_code)")
    .eq("is_available", false)
    .in("status", ["outcome_unknown", "adopted"])
    .gt("updated_at", oneDayAgo.toISOString())
    .limit(5000);

  if (recentDeactivated) {
    const shelterDeactivations = new Map<string, { count: number; name: string; state: string }>();
    for (const dog of recentDeactivated) {
      const s = dog.shelter_id;
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const shelter = dog.shelters as any;
      const entry = shelterDeactivations.get(s) || { count: 0, name: shelter?.name || "Unknown", state: shelter?.state_code || "" };
      entry.count++;
      shelterDeactivations.set(s, entry);
    }

    for (const [shelterId, data] of Array.from(shelterDeactivations.entries())) {
      if (data.count >= 10) {
        // Check if we already have this anomaly
        const exists = metrics.recentAnomalies.some(a =>
          a.type === "mass_euthanasia" && a.shelterId === shelterId &&
          new Date(a.timestamp).getTime() > oneDayAgo.getTime()
        );
        if (!exists) {
          addAnomaly({
            timestamp: now.toISOString(),
            type: "mass_euthanasia",
            severity: data.count >= 20 ? "critical" : "warning",
            shelterId,
            shelterName: data.name,
            message: `${data.count} dogs deactivated in 24h at ${data.name} (${data.state})`,
            data: { count: data.count },
          });
        }
      }
    }
  }

  // 2. Shelter offline detection (shelters with dogs but no scrape in 7+ days)
  const sevenDaysAgo = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
  const { data: offlineShelters } = await supabase
    .from("shelters")
    .select("id, name, state_code, available_dog_count, last_scraped_at")
    .gt("available_dog_count", 5)
    .lt("last_scraped_at", sevenDaysAgo.toISOString())
    .limit(20);

  if (offlineShelters) {
    for (const shelter of offlineShelters) {
      const exists = metrics.recentAnomalies.some(a =>
        a.type === "shelter_offline" && a.shelterId === shelter.id &&
        new Date(a.timestamp).getTime() > oneDayAgo.getTime()
      );
      if (!exists) {
        addAnomaly({
          timestamp: now.toISOString(),
          type: "shelter_offline",
          severity: "warning",
          shelterId: shelter.id,
          shelterName: shelter.name,
          message: `${shelter.name} (${shelter.state_code}) not scraped in 7+ days, has ${shelter.available_dog_count} dogs`,
          data: { dogCount: shelter.available_dog_count, lastScraped: shelter.last_scraped_at },
        });
      }
    }
  }

  // 3. Coverage gaps (high-risk shelters with no platform assigned)
  const { count: gapCount } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .gt("available_dog_count", 10)
    .is("website_platform", null);

  metrics.coverageGaps = gapCount ?? 0;
}

// ─── Core: Auto-Optimize Scraper Frequency ───

async function optimizeScraperFrequency(): Promise<number> {
  const supabase = createAdminClient();
  let optimized = 0;

  // Increase scrape frequency for high-risk shelters
  for (const risk of metrics.shelterRiskScores.slice(0, 50)) {
    if (risk.riskScore >= 80) {
      // Set to scrape every 6 hours
      const sixHoursFromNow = new Date(Date.now() + 6 * 60 * 60 * 1000);
      const { error } = await supabase
        .from("shelters")
        .update({ next_scrape_at: sixHoursFromNow.toISOString() })
        .eq("id", risk.shelterId)
        .or(`next_scrape_at.is.null,next_scrape_at.gt.${sixHoursFromNow.toISOString()}`);

      if (!error) optimized++;
    } else if (risk.riskScore >= 60) {
      // Set to scrape every 12 hours
      const twelveHoursFromNow = new Date(Date.now() + 12 * 60 * 60 * 1000);
      const { error } = await supabase
        .from("shelters")
        .update({ next_scrape_at: twelveHoursFromNow.toISOString() })
        .eq("id", risk.shelterId)
        .or(`next_scrape_at.is.null,next_scrape_at.gt.${twelveHoursFromNow.toISOString()}`);

      if (!error) optimized++;
    }
  }

  return optimized;
}

// ─── Timeout helper ───

function withTimeout<T>(promise: Promise<T>, ms: number, label: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`${label} timed out after ${ms}ms`)), ms);
    promise.then(
      (val) => { clearTimeout(timer); resolve(val); },
      (err) => { clearTimeout(timer); reject(err); }
    );
  });
}

// ─── Main Cycle ───

async function runCycle() {
  const cycleStart = Date.now();
  metrics.cycleCount++;
  const PHASE_TIMEOUT = 5 * 60 * 1000; // 5 min max per phase

  // Phase 1: Compute shelter risk scores
  try {
    metrics.currentTask = "computing shelter risk scores";
    broadcastSSE({ type: "status", data: metrics.currentTask });
    metrics.shelterRiskScores = await withTimeout(computeShelterRiskScores(), PHASE_TIMEOUT, "Shelter risk scores");
    metrics.totalSheltersAnalyzed = metrics.shelterRiskScores.length;
    metrics.highRiskShelters = metrics.shelterRiskScores.filter(s => s.riskScore >= 60).length;
  } catch (err) {
    console.error("[INTEL] Phase 1 (shelter risk) error:", err instanceof Error ? err.message : err);
  }

  // Phase 2: Breed risk analysis
  try {
    metrics.currentTask = "analyzing breed risks";
    broadcastSSE({ type: "status", data: metrics.currentTask });
    metrics.topBreedRisks = await withTimeout(computeBreedRisks(), PHASE_TIMEOUT, "Breed risks");
  } catch (err) {
    console.error("[INTEL] Phase 2 (breed risk) error:", err instanceof Error ? err.message : err);
  }

  // Phase 3: State risk analysis
  try {
    metrics.currentTask = "analyzing state risks";
    broadcastSSE({ type: "status", data: metrics.currentTask });
    metrics.stateRisks = await withTimeout(computeStateRisks(), PHASE_TIMEOUT, "State risks");
  } catch (err) {
    console.error("[INTEL] Phase 3 (state risk) error:", err instanceof Error ? err.message : err);
  }

  // Phase 4: Anomaly detection
  try {
    metrics.currentTask = "detecting anomalies";
    broadcastSSE({ type: "status", data: metrics.currentTask });
    await withTimeout(detectAnomalies(), PHASE_TIMEOUT, "Anomaly detection");
  } catch (err) {
    console.error("[INTEL] Phase 4 (anomaly) error:", err instanceof Error ? err.message : err);
  }

  // Phase 5: Auto-optimize scraper frequency
  try {
    metrics.currentTask = "optimizing scraper frequency";
    broadcastSSE({ type: "status", data: metrics.currentTask });
    const optimized = await withTimeout(optimizeScraperFrequency(), PHASE_TIMEOUT, "Scraper optimization");
    metrics.optimizationsApplied += optimized;
  } catch (err) {
    console.error("[INTEL] Phase 5 (optimization) error:", err instanceof Error ? err.message : err);
  }

  metrics.lastCycleAt = new Date().toISOString();
  metrics.lastCycleDuration = Date.now() - cycleStart;
  metrics.currentTask = "idle — next analysis in 30 min";

  broadcastSSE({ type: "metrics", data: metrics });
  console.log(
    `[INTEL] Cycle #${metrics.cycleCount}: ` +
    `${metrics.totalSheltersAnalyzed} shelters analyzed, ` +
    `${metrics.highRiskShelters} high-risk, ` +
    `${metrics.recentAnomalies.length} anomalies ` +
    `(${metrics.lastCycleDuration}ms)`
  );
}

// ─── HTTP Dashboard ───

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", `http://localhost:${PORT}`);
  res.setHeader("Access-Control-Allow-Origin", "*");

  if (url.pathname === "/api/metrics") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(metrics));
    return;
  }

  if (url.pathname === "/api/shelters") {
    // Return top risk shelters with optional state filter
    const state = url.searchParams.get("state");
    let results = metrics.shelterRiskScores;
    if (state) results = results.filter(s => s.state === state.toUpperCase());
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(results.slice(0, 100)));
    return;
  }

  if (url.pathname === "/api/breeds") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(metrics.topBreedRisks));
    return;
  }

  if (url.pathname === "/api/states") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(metrics.stateRisks));
    return;
  }

  if (url.pathname === "/api/anomalies") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(metrics.recentAnomalies));
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
    res.end(JSON.stringify({ ok: true }));
    return;
  }

  // Dashboard HTML
  res.writeHead(200, { "Content-Type": "text/html" });
  res.end(`<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><title>Shelter Intelligence</title>
<style>
  body { font-family: system-ui; background: #0f172a; color: #e2e8f0; margin: 0; padding: 20px; }
  h1 { color: #a78bfa; margin: 0 0 8px; }
  h2 { color: #94a3b8; font-size: 14px; text-transform: uppercase; margin: 24px 0 12px; }
  .subtitle { color: #94a3b8; margin-bottom: 24px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px; margin-bottom: 24px; }
  .card { background: #1e293b; border-radius: 12px; padding: 16px; border: 1px solid #334155; }
  .card .label { color: #94a3b8; font-size: 11px; text-transform: uppercase; margin-bottom: 4px; }
  .card .value { font-size: 24px; font-weight: bold; font-variant-numeric: tabular-nums; }
  .green { color: #4ade80; } .red { color: #f87171; } .blue { color: #60a5fa; }
  .amber { color: #fbbf24; } .purple { color: #a78bfa; }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th { text-align: left; color: #94a3b8; padding: 8px; border-bottom: 1px solid #334155; font-weight: 500; cursor: pointer; }
  th:hover { color: #e2e8f0; }
  td { padding: 8px; border-bottom: 1px solid #1e293b; }
  tr:hover { background: #1e293b; }
  .risk-bar { height: 6px; border-radius: 3px; background: #334155; overflow: hidden; }
  .risk-fill { height: 100%; border-radius: 3px; }
  .panel { background: #1e293b; border-radius: 12px; padding: 16px; border: 1px solid #334155; margin-bottom: 16px; overflow-x: auto; }
  .anomaly { padding: 10px 12px; border-radius: 8px; margin-bottom: 8px; font-size: 13px; }
  .anomaly-critical { background: #7f1d1d; border: 1px solid #991b1b; }
  .anomaly-warning { background: #713f12; border: 1px solid #854d0e; }
  .anomaly-info { background: #1e3a5f; border: 1px solid #1e40af; }
  .tabs { display: flex; gap: 4px; margin-bottom: 16px; }
  .tab { padding: 8px 16px; border-radius: 8px; cursor: pointer; font-size: 13px; font-weight: 600; background: #1e293b; color: #94a3b8; border: 1px solid #334155; }
  .tab.active { background: #3b82f6; color: white; border-color: #3b82f6; }
  button { background: #7c3aed; color: white; border: none; padding: 8px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; }
  button:hover { background: #6d28d9; }
  .status { color: #94a3b8; font-size: 14px; margin-bottom: 16px; }
  .search { padding: 8px 12px; background: #1e293b; border: 1px solid #334155; border-radius: 8px; color: #e2e8f0; font-size: 13px; width: 200px; }
</style></head><body>
<h1>Shelter Intelligence Agent</h1>
<p class="subtitle">Risk scoring, pattern detection, coverage optimization | Port ${PORT}</p>
<div class="status" id="status">Starting...</div>
<div class="grid">
  <div class="card"><div class="label">Shelters Analyzed</div><div class="value purple" id="sheltersAnalyzed">0</div></div>
  <div class="card"><div class="label">High Risk</div><div class="value red" id="highRisk">0</div></div>
  <div class="card"><div class="label">Coverage Gaps</div><div class="value amber" id="coverageGaps">0</div></div>
  <div class="card"><div class="label">Anomalies</div><div class="value red" id="anomalyCount">0</div></div>
  <div class="card"><div class="label">Optimizations</div><div class="value green" id="optimizations">0</div></div>
  <div class="card"><div class="label">Analysis Cycles</div><div class="value blue" id="cycles">0</div></div>
</div>
<button onclick="fetch('/api/trigger',{method:'POST'})">Run Analysis Now</button>

<div class="tabs" style="margin-top:24px">
  <div class="tab active" onclick="showTab('shelters',this)">Shelter Risk</div>
  <div class="tab" onclick="showTab('breeds',this)">Breed Risk</div>
  <div class="tab" onclick="showTab('states',this)">State Risk</div>
  <div class="tab" onclick="showTab('anomalies',this)">Anomalies</div>
</div>

<div id="tab-shelters" class="panel">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px">
    <span style="color:#94a3b8;font-size:12px" id="shelterCount">Loading...</span>
    <input class="search" placeholder="Search shelters..." oninput="filterShelters(this.value)">
  </div>
  <table id="shelterTable">
    <thead><tr>
      <th onclick="sortShelters('shelterName')">Shelter</th>
      <th onclick="sortShelters('state')">State</th>
      <th onclick="sortShelters('riskScore')" style="width:120px">Risk</th>
      <th onclick="sortShelters('totalDogs')" style="text-align:right">Dogs</th>
      <th onclick="sortShelters('urgentDogs')" style="text-align:right">Urgent</th>
      <th onclick="sortShelters('avgDaysToEuth')" style="text-align:right">Avg ETA</th>
      <th>Recommendation</th>
    </tr></thead>
    <tbody id="shelterBody"></tbody>
  </table>
</div>

<div id="tab-breeds" class="panel" style="display:none">
  <table>
    <thead><tr><th>Breed</th><th style="text-align:right">Available</th><th style="text-align:right">Urgent</th><th style="text-align:right">Risk %</th></tr></thead>
    <tbody id="breedBody"></tbody>
  </table>
</div>

<div id="tab-states" class="panel" style="display:none">
  <table>
    <thead><tr><th>State</th><th style="text-align:right">Dogs</th><th style="text-align:right">Urgent</th><th style="text-align:right">Shelters</th><th style="text-align:right">Risk %</th></tr></thead>
    <tbody id="stateBody"></tbody>
  </table>
</div>

<div id="tab-anomalies" class="panel" style="display:none">
  <div id="anomalyList"><div style="color:#64748b">Waiting for analysis...</div></div>
</div>

<script>
let currentData = null;
let shelterSort = { key: 'riskScore', dir: 'desc' };
let shelterFilter = '';

function showTab(tab, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  ['shelters','breeds','states','anomalies'].forEach(t => {
    document.getElementById('tab-'+t).style.display = t === tab ? 'block' : 'none';
  });
}

function sortShelters(key) {
  if (shelterSort.key === key) shelterSort.dir = shelterSort.dir === 'asc' ? 'desc' : 'asc';
  else { shelterSort.key = key; shelterSort.dir = 'desc'; }
  renderShelters();
}

function filterShelters(q) { shelterFilter = q.toLowerCase(); renderShelters(); }

function renderShelters() {
  if (!currentData) return;
  let shelters = [...currentData.shelterRiskScores];
  if (shelterFilter) shelters = shelters.filter(s =>
    s.shelterName.toLowerCase().includes(shelterFilter) ||
    s.state.toLowerCase().includes(shelterFilter) ||
    s.city.toLowerCase().includes(shelterFilter)
  );
  shelters.sort((a,b) => {
    const av = a[shelterSort.key], bv = b[shelterSort.key];
    if (typeof av === 'number') return shelterSort.dir === 'asc' ? av - bv : bv - av;
    return shelterSort.dir === 'asc' ? String(av).localeCompare(String(bv)) : String(bv).localeCompare(String(av));
  });
  document.getElementById('shelterCount').textContent = shelters.length + ' shelters';
  const body = document.getElementById('shelterBody');
  body.innerHTML = shelters.slice(0,100).map(s => {
    const color = s.riskScore >= 80 ? '#ef4444' : s.riskScore >= 60 ? '#f59e0b' : s.riskScore >= 30 ? '#3b82f6' : '#22c55e';
    return '<tr><td><b>' + s.shelterName + '</b><br><span style="color:#64748b;font-size:11px">' + s.city + '</span></td>'
      + '<td>' + s.state + '</td>'
      + '<td><div style="display:flex;align-items:center;gap:8px"><span style="color:' + color + ';font-weight:bold;width:24px">' + s.riskScore + '</span><div class="risk-bar" style="flex:1"><div class="risk-fill" style="width:' + s.riskScore + '%;background:' + color + '"></div></div></div></td>'
      + '<td style="text-align:right">' + s.totalDogs + '</td>'
      + '<td style="text-align:right;color:' + (s.urgentDogs > 0 ? '#ef4444' : '#64748b') + '">' + s.urgentDogs + '</td>'
      + '<td style="text-align:right">' + (s.avgDaysToEuth > 0 ? s.avgDaysToEuth + 'd' : '--') + '</td>'
      + '<td style="font-size:11px;color:#94a3b8">' + s.recommendation + '</td></tr>';
  }).join('');
}

function renderBreeds() {
  if (!currentData) return;
  document.getElementById('breedBody').innerHTML = currentData.topBreedRisks.map(b =>
    '<tr><td>' + b.breed + '</td><td style="text-align:right">' + b.totalAvailable + '</td><td style="text-align:right;color:#ef4444">' + b.totalUrgent + '</td><td style="text-align:right;color:' + (b.riskPct > 5 ? '#ef4444' : '#94a3b8') + '">' + b.riskPct + '%</td></tr>'
  ).join('');
}

function renderStates() {
  if (!currentData) return;
  document.getElementById('stateBody').innerHTML = currentData.stateRisks.map(s =>
    '<tr><td><b>' + s.state + '</b></td><td style="text-align:right">' + s.totalDogs.toLocaleString() + '</td><td style="text-align:right;color:' + (s.urgentDogs > 0 ? '#ef4444' : '#64748b') + '">' + s.urgentDogs + '</td><td style="text-align:right">' + s.shelterCount + '</td><td style="text-align:right">' + s.riskPct + '%</td></tr>'
  ).join('');
}

function renderAnomalies() {
  if (!currentData) return;
  const el = document.getElementById('anomalyList');
  if (currentData.recentAnomalies.length === 0) { el.innerHTML = '<div style="color:#64748b">No anomalies detected yet</div>'; return; }
  el.innerHTML = currentData.recentAnomalies.map(a =>
    '<div class="anomaly anomaly-' + a.severity + '"><div style="display:flex;justify-content:space-between"><b>' + a.message + '</b><span style="color:#64748b;font-size:11px">' + new Date(a.timestamp).toLocaleString() + '</span></div></div>'
  ).join('');
}

function updateAll(data) {
  currentData = data;
  document.getElementById('cycles').textContent = data.cycleCount;
  document.getElementById('sheltersAnalyzed').textContent = data.totalSheltersAnalyzed;
  document.getElementById('highRisk').textContent = data.highRiskShelters;
  document.getElementById('coverageGaps').textContent = data.coverageGaps;
  document.getElementById('anomalyCount').textContent = data.recentAnomalies.length;
  document.getElementById('optimizations').textContent = data.optimizationsApplied;
  document.getElementById('status').textContent = data.currentTask;
  renderShelters(); renderBreeds(); renderStates(); renderAnomalies();
}

const es = new EventSource('/api/events');
es.onmessage = (e) => {
  const {type, data} = JSON.parse(e.data);
  if (type === 'metrics' || type === 'connected') updateAll(data);
  if (type === 'anomaly') { if (currentData) { currentData.recentAnomalies.unshift(data); renderAnomalies(); } }
};
</script></body></html>`);
});

// ─── Start ───

server.listen(PORT, () => {
  console.log(`[INTEL] Shelter Intelligence Agent running on http://localhost:${PORT}`);
  console.log("[INTEL] Analysis interval: 30 minutes");

  // Run first analysis after 10 seconds
  setTimeout(() => runCycle().catch(console.error), 10000);

  // Then every 30 minutes
  setInterval(() => runCycle().catch(console.error), 30 * 60 * 1000);
});

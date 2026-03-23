/**
 * 50-State Universal Shelter Scraper Agent
 * Port 3849 — runs continuously, scraping every shelter website in America.
 *
 * Architecture:
 * - Single process with 50 internal state workers
 * - 5 states processed concurrently (configurable)
 * - 3 shelters per state scraped concurrently
 * - Round-robin through states with priority weighting
 * - Dashboard at http://localhost:3849
 *
 * Run with: npm run agent:scraper
 */
import * as http from "http";
import * as fs from "fs";
import * as path from "path";
import { createClient, SupabaseClient } from "@supabase/supabase-js";
import * as dotenv from "dotenv";
import { EventEmitter } from "events";

dotenv.config({ path: path.resolve(__dirname, "../.env.local") });

// Validate env
const SUPABASE_URL = process.env.NEXT_PUBLIC_SUPABASE_URL;
const SUPABASE_KEY = process.env.SUPABASE_SERVICE_ROLE_KEY;
if (!SUPABASE_URL || !SUPABASE_KEY) {
  console.error("Missing SUPABASE env vars");
  process.exit(1);
}

const supabase: SupabaseClient = createClient(SUPABASE_URL, SUPABASE_KEY, {
  auth: { autoRefreshToken: false, persistSession: false },
});

// ─── Dynamic imports (ESM modules from src/) ───
// We use inline require-style dynamic imports for the scraper modules
// since the agent runs with tsx which handles the path aliases.

const PORT = 3849;
const CONCURRENT_STATES = 5;
const CONCURRENT_SHELTERS_PER_STATE = 3;
const SCRAPE_DELAY_MS = 1000; // Delay between shelters within a state

const US_STATES = [
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
];

// ─── Event System ───
class ScraperEventBus extends EventEmitter {}
const events = new ScraperEventBus();
events.setMaxListeners(50);

// ─── Types ───
interface StateMetrics {
  stateCode: string;
  totalShelters: number;
  sheltersWithWebsite: number;
  sheltersScrapedThisRun: number;
  dogsFoundThisRun: number;
  errorsThisRun: number;
  lastScrapeAt: string | null;
  isRunning: boolean;
}

interface GlobalMetrics {
  startedAt: string;
  isPaused: boolean;
  currentStates: string[];
  totalSheltersScraped: number;
  totalDogsFound: number;
  totalDogsInserted: number;
  totalDogsUpdated: number;
  totalDogsDeactivated: number;
  totalErrors: number;
  cycleCount: number;
  states: Record<string, StateMetrics>;
  platformBreakdown: Record<string, number>;
}

// ─── Global State ───
let isPaused = false;
let isShuttingDown = false;
const startedAt = new Date().toISOString();

const metrics: GlobalMetrics = {
  startedAt,
  isPaused: false,
  currentStates: [],
  totalSheltersScraped: 0,
  totalDogsFound: 0,
  totalDogsInserted: 0,
  totalDogsUpdated: 0,
  totalDogsDeactivated: 0,
  totalErrors: 0,
  cycleCount: 0,
  states: {},
  platformBreakdown: {},
};

// Initialize state metrics
for (const state of US_STATES) {
  metrics.states[state] = {
    stateCode: state,
    totalShelters: 0,
    sheltersWithWebsite: 0,
    sheltersScrapedThisRun: 0,
    dogsFoundThisRun: 0,
    errorsThisRun: 0,
    lastScrapeAt: null,
    isRunning: false,
  };
}

// ─── Log Buffer ───
interface LogEntry {
  id: number;
  timestamp: string;
  level: "info" | "warn" | "error" | "success";
  category: string;
  message: string;
}

let logId = 0;
const logBuffer: LogEntry[] = [];
const MAX_LOG_BUFFER = 2000;

function log(level: LogEntry["level"], category: string, message: string) {
  const entry: LogEntry = {
    id: logId++,
    timestamp: new Date().toISOString(),
    level,
    category,
    message,
  };
  logBuffer.push(entry);
  if (logBuffer.length > MAX_LOG_BUFFER) logBuffer.shift();
  events.emit("log", entry);

  const colors: Record<string, string> = {
    info: "\x1b[37m",
    warn: "\x1b[33m",
    error: "\x1b[31m",
    success: "\x1b[32m",
  };
  const reset = "\x1b[0m";
  console.log(
    `${colors[level]}[${entry.timestamp.substring(11, 19)}] [${category}] ${message}${reset}`
  );
}

// ─── Core Scraping Logic ───

async function loadSheltersForState(
  stateCode: string
): Promise<
  Array<{
    id: string;
    name: string;
    website: string | null;
    website_platform: string;
    adoptable_page_url: string | null;
    platform_shelter_id: string | null;
    last_scraped_at: string | null;
    scrape_status: string;
    dogs_found_last_scrape: number;
    next_scrape_at: string | null;
  }>
> {
  const shelters: Array<{
    id: string;
    name: string;
    website: string | null;
    website_platform: string;
    adoptable_page_url: string | null;
    platform_shelter_id: string | null;
    last_scraped_at: string | null;
    scrape_status: string;
    dogs_found_last_scrape: number;
    next_scrape_at: string | null;
  }> = [];

  let offset = 0;
  const pageSize = 1000;

  while (true) {
    const { data } = await supabase
      .from("shelters")
      .select(
        "id, name, website, website_platform, adoptable_page_url, platform_shelter_id, last_scraped_at, scrape_status, dogs_found_last_scrape, next_scrape_at"
      )
      .eq("state_code", stateCode)
      .eq("is_active", true)
      .not("website", "is", null)
      .order("next_scrape_at", { ascending: true, nullsFirst: true })
      .range(offset, offset + pageSize - 1);

    if (!data || data.length === 0) break;
    shelters.push(...data);
    if (data.length < pageSize) break;
    offset += pageSize;
  }

  return shelters;
}

async function scrapeShelter(shelter: {
  id: string;
  name: string;
  website: string | null;
  website_platform: string;
  adoptable_page_url: string | null;
  platform_shelter_id: string | null;
  stateCode: string;
}): Promise<{ dogsFound: number; inserted: number; updated: number; deactivated: number; error?: string }> {
  if (!shelter.website) return { dogsFound: 0, inserted: 0, updated: 0, deactivated: 0 };

  const startTime = Date.now();

  try {
    // Dynamically import scraper modules (handles path aliases via tsx)
    const { detectPlatform } = await import("../src/lib/scrapers/platform-detector");
    const { getAdapter } = await import("../src/lib/scrapers/adapters/index");
    const { upsertScrapedDogs } = await import("../src/lib/scrapers/scraper-upsert");

    // Step 1: Detect platform if not yet classified
    let platform = shelter.website_platform;
    let platformId = shelter.platform_shelter_id;
    let adoptableUrl = shelter.adoptable_page_url || shelter.website;

    if (platform === "unknown" || !platform) {
      const detection = await detectPlatform(shelter.website);
      platform = detection.platform;
      platformId = detection.platformId;
      adoptableUrl = detection.adoptablePageUrl || shelter.website;

      // Save classification to DB
      await supabase
        .from("shelters")
        .update({
          website_platform: platform,
          platform_shelter_id: platformId,
          adoptable_page_url: adoptableUrl,
        })
        .eq("id", shelter.id);

      log("info", shelter.stateCode, `Classified ${shelter.name}: ${platform}`);

      // Update platform breakdown
      metrics.platformBreakdown[platform] =
        (metrics.platformBreakdown[platform] || 0) + 1;
    }

    // Step 2: Get the adapter
    const adapter = getAdapter(platform as import("../src/lib/scrapers/types").WebsitePlatform);
    if (!adapter) {
      await updateShelterScrapeStatus(shelter.id, "skipped", 0, `No adapter for ${platform}`);
      return { dogsFound: 0, inserted: 0, updated: 0, deactivated: 0 };
    }

    // Step 3: Scrape
    const dogs = await adapter.scrape(adoptableUrl!, platformId);
    const duration = Date.now() - startTime;

    // Step 4: Upsert to database
    let result = { inserted: 0, updated: 0, deactivated: 0, errors: 0 };
    if (dogs.length > 0) {
      result = await upsertScrapedDogs(shelter.id, dogs, platform);
    }

    // Step 5: Update shelter scrape status
    const nextScrapeHours = dogs.length > 50 ? 12 : dogs.length > 0 ? 24 : 72;
    const nextScrapeAt = new Date(
      Date.now() + nextScrapeHours * 60 * 60 * 1000
    ).toISOString();

    await updateShelterScrapeStatus(
      shelter.id,
      dogs.length > 0 ? "success" : "no_dogs_page",
      dogs.length,
      null,
      nextScrapeAt
    );

    log(
      dogs.length > 0 ? "success" : "info",
      shelter.stateCode,
      `${shelter.name}: ${dogs.length} dogs (${result.inserted} new, ${result.updated} updated, ${result.deactivated} adopted) [${duration}ms]`
    );

    events.emit("shelter_scraped", {
      shelterName: shelter.name,
      stateCode: shelter.stateCode,
      platform,
      dogsFound: dogs.length,
      inserted: result.inserted,
      updated: result.updated,
      deactivated: result.deactivated,
      duration,
    });

    return {
      dogsFound: dogs.length,
      inserted: result.inserted,
      updated: result.updated,
      deactivated: result.deactivated,
    };
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    log("error", shelter.stateCode, `${shelter.name}: ${message}`);

    // Exponential backoff for errored shelters
    const nextScrapeAt = new Date(
      Date.now() + 48 * 60 * 60 * 1000
    ).toISOString();
    await updateShelterScrapeStatus(
      shelter.id,
      "error",
      0,
      message.substring(0, 500),
      nextScrapeAt
    );

    return { dogsFound: 0, inserted: 0, updated: 0, deactivated: 0, error: message };
  }
}

async function updateShelterScrapeStatus(
  shelterId: string,
  status: string,
  dogsFound: number,
  error: string | null,
  nextScrapeAt?: string
) {
  const update: Record<string, unknown> = {
    scrape_status: status,
    last_scraped_at: new Date().toISOString(),
    dogs_found_last_scrape: dogsFound,
    scrape_error: error,
  };
  if (nextScrapeAt) update.next_scrape_at = nextScrapeAt;

  // Fetch current scrape_count, then increment
  const { data: current } = await supabase
    .from("shelters")
    .select("scrape_count")
    .eq("id", shelterId)
    .single();
  update.scrape_count = ((current?.scrape_count as number) || 0) + 1;

  await supabase.from("shelters").update(update).eq("id", shelterId);
}

// ─── State Worker ───

async function processState(stateCode: string): Promise<void> {
  if (isPaused || isShuttingDown) return;

  const stateMetrics = metrics.states[stateCode];
  stateMetrics.isRunning = true;
  stateMetrics.sheltersScrapedThisRun = 0;
  stateMetrics.dogsFoundThisRun = 0;
  stateMetrics.errorsThisRun = 0;

  log("info", stateCode, `Starting state scrape...`);

  const shelters = await loadSheltersForState(stateCode);
  stateMetrics.totalShelters = shelters.length;
  stateMetrics.sheltersWithWebsite = shelters.filter((s) => s.website).length;

  // Filter to shelters due for scraping
  const now = new Date();
  const dueForScrape = shelters.filter((s) => {
    if (!s.next_scrape_at) return true; // Never scraped
    return new Date(s.next_scrape_at) <= now;
  });

  log(
    "info",
    stateCode,
    `${dueForScrape.length}/${shelters.length} shelters due for scraping`
  );

  // Process shelters in batches of CONCURRENT_SHELTERS_PER_STATE
  for (let i = 0; i < dueForScrape.length; i += CONCURRENT_SHELTERS_PER_STATE) {
    if (isPaused || isShuttingDown) break;

    const batch = dueForScrape
      .slice(i, i + CONCURRENT_SHELTERS_PER_STATE)
      .map((s) => ({ ...s, stateCode }));

    const results = await Promise.allSettled(
      batch.map((shelter) => scrapeShelter(shelter))
    );

    for (const result of results) {
      if (result.status === "fulfilled") {
        stateMetrics.sheltersScrapedThisRun++;
        stateMetrics.dogsFoundThisRun += result.value.dogsFound;
        metrics.totalSheltersScraped++;
        metrics.totalDogsFound += result.value.dogsFound;
        metrics.totalDogsInserted += result.value.inserted;
        metrics.totalDogsUpdated += result.value.updated;
        metrics.totalDogsDeactivated += result.value.deactivated;
        if (result.value.error) {
          stateMetrics.errorsThisRun++;
          metrics.totalErrors++;
        }
      } else {
        stateMetrics.errorsThisRun++;
        metrics.totalErrors++;
      }
    }

    // Delay between batches
    if (i + CONCURRENT_SHELTERS_PER_STATE < dueForScrape.length) {
      await new Promise((r) => setTimeout(r, SCRAPE_DELAY_MS));
    }
  }

  stateMetrics.lastScrapeAt = new Date().toISOString();
  stateMetrics.isRunning = false;

  log(
    "success",
    stateCode,
    `Complete: ${stateMetrics.sheltersScrapedThisRun} shelters, ${stateMetrics.dogsFoundThisRun} dogs, ${stateMetrics.errorsThisRun} errors`
  );
}

// ─── Main Loop ───

async function mainLoop(): Promise<void> {
  log("info", "SYSTEM", "Starting 50-state scraper cycle...");
  metrics.cycleCount++;

  // Process states in batches of CONCURRENT_STATES
  const stateQueue = [...US_STATES];

  // Sort by priority: states with more shelters first
  stateQueue.sort((a, b) => {
    const aMetrics = metrics.states[a];
    const bMetrics = metrics.states[b];
    // Prioritize states not recently scraped
    if (!aMetrics.lastScrapeAt && bMetrics.lastScrapeAt) return -1;
    if (aMetrics.lastScrapeAt && !bMetrics.lastScrapeAt) return 1;
    return 0;
  });

  for (let i = 0; i < stateQueue.length; i += CONCURRENT_STATES) {
    if (isPaused || isShuttingDown) break;

    const batch = stateQueue.slice(i, i + CONCURRENT_STATES);
    metrics.currentStates = batch;

    log(
      "info",
      "SYSTEM",
      `Processing states: ${batch.join(", ")}`
    );

    await Promise.allSettled(batch.map((state) => processState(state)));

    metrics.currentStates = [];
  }

  log(
    "success",
    "SYSTEM",
    `Cycle ${metrics.cycleCount} complete. ${metrics.totalSheltersScraped} shelters scraped, ${metrics.totalDogsFound} dogs found total.`
  );
}

// ─── Scheduler ───

async function startScheduler(): Promise<void> {
  log("info", "SYSTEM", "Scraper agent starting...");

  // Initial load: count shelters per state
  for (const state of US_STATES) {
    const { count } = await supabase
      .from("shelters")
      .select("id", { count: "exact", head: true })
      .eq("state_code", state)
      .eq("is_active", true);
    metrics.states[state].totalShelters = count || 0;
  }

  log("info", "SYSTEM", "Shelter counts loaded. Starting main loop...");

  // Run continuously
  while (!isShuttingDown) {
    if (!isPaused) {
      try {
        await mainLoop();
      } catch (err) {
        log(
          "error",
          "SYSTEM",
          `Main loop error: ${err instanceof Error ? err.message : String(err)}`
        );
      }
    }

    // Between cycles: discover websites for IRS-only shelters (low priority)
    if (!isShuttingDown && !isPaused) {
      try {
        const { discoverWebsiteBatch } = await import("../src/lib/scrapers/website-discovery");
        const discovered = await discoverWebsiteBatch(25);
        if (discovered > 0) {
          log("success", "DISCOVERY", `Found ${discovered} new shelter websites`);
        }
      } catch {
        // Website discovery is best-effort
      }
    }

    // Wait 5 minutes between cycles
    if (!isShuttingDown) {
      log("info", "SYSTEM", "Waiting 5 minutes before next cycle...");
      await new Promise((r) => setTimeout(r, 5 * 60 * 1000));
    }
  }
}

// ─── HTTP Server ───

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://localhost:${PORT}`);

  // CORS
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") {
    res.writeHead(204);
    res.end();
    return;
  }

  // Dashboard
  if (url.pathname === "/" || url.pathname === "/dashboard") {
    const dashboardPath = path.join(__dirname, "scraper-dashboard.html");
    if (fs.existsSync(dashboardPath)) {
      res.writeHead(200, { "Content-Type": "text/html" });
      res.end(fs.readFileSync(dashboardPath, "utf-8"));
    } else {
      res.writeHead(200, { "Content-Type": "text/html" });
      res.end("<html><body><h1>Scraper Agent Running</h1><p>Dashboard file not found.</p></body></html>");
    }
    return;
  }

  // SSE Events
  if (url.pathname === "/events") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    });

    // Send recent logs
    for (const entry of logBuffer.slice(-50)) {
      res.write(`data: ${JSON.stringify(entry)}\n\n`);
    }

    const onLog = (entry: LogEntry) => {
      res.write(`data: ${JSON.stringify(entry)}\n\n`);
    };
    const onShelter = (data: unknown) => {
      res.write(`event: shelter\ndata: ${JSON.stringify(data)}\n\n`);
    };

    events.on("log", onLog);
    events.on("shelter_scraped", onShelter);

    req.on("close", () => {
      events.off("log", onLog);
      events.off("shelter_scraped", onShelter);
    });
    return;
  }

  // Metrics API
  if (url.pathname === "/api/metrics") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(
      JSON.stringify({
        ...metrics,
        uptimeMs: Date.now() - new Date(startedAt).getTime(),
      })
    );
    return;
  }

  // State detail API
  if (url.pathname.startsWith("/api/state/")) {
    const stateCode = url.pathname.split("/").pop()?.toUpperCase();
    if (stateCode && metrics.states[stateCode]) {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify(metrics.states[stateCode]));
    } else {
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "State not found" }));
    }
    return;
  }

  // Pause
  if (url.pathname === "/api/pause" && req.method === "POST") {
    isPaused = true;
    metrics.isPaused = true;
    log("warn", "SYSTEM", "Agent PAUSED by user");
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ paused: true }));
    return;
  }

  // Resume
  if (url.pathname === "/api/resume" && req.method === "POST") {
    isPaused = false;
    metrics.isPaused = false;
    log("info", "SYSTEM", "Agent RESUMED by user");
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ paused: false }));
    return;
  }

  // Trigger specific state
  if (url.pathname === "/api/trigger" && req.method === "POST") {
    let body = "";
    req.on("data", (c) => (body += c));
    req.on("end", async () => {
      try {
        const { state } = JSON.parse(body);
        if (state && US_STATES.includes(state.toUpperCase())) {
          const stateCode = state.toUpperCase();
          log("info", "SYSTEM", `Manual trigger: ${stateCode}`);
          processState(stateCode).catch(() => {});
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ triggered: stateCode }));
        } else {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "Invalid state code" }));
        }
      } catch {
        res.writeHead(400, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid JSON" }));
      }
    });
    return;
  }

  // Health
  if (url.pathname === "/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(
      JSON.stringify({
        status: "ok",
        uptime: Date.now() - new Date(startedAt).getTime(),
        isPaused,
        sheltersScraped: metrics.totalSheltersScraped,
        dogsFound: metrics.totalDogsFound,
      })
    );
    return;
  }

  res.writeHead(404);
  res.end("Not Found");
});

// ─── Startup ───

server.listen(PORT, () => {
  console.log(`\n${"═".repeat(60)}`);
  console.log(`  50-STATE SHELTER SCRAPER AGENT`);
  console.log(`  Dashboard: http://localhost:${PORT}`);
  console.log(`  States: ${US_STATES.length} | Concurrent: ${CONCURRENT_STATES}`);
  console.log(`${"═".repeat(60)}\n`);

  startScheduler().catch((err) => {
    log("error", "SYSTEM", `Fatal: ${err.message}`);
    process.exit(1);
  });
});

// ─── Graceful Shutdown ───

function shutdown() {
  if (isShuttingDown) return;
  isShuttingDown = true;
  log("warn", "SYSTEM", "Shutting down...");

  setTimeout(() => {
    server.close();
    process.exit(0);
  }, 5000);
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);

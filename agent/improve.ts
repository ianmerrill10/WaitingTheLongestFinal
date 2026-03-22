/**
 * WaitingTheLongest — Perpetual Improvement Agent
 *
 * Scans the entire project and suggests improvements, finds bugs,
 * runs automated audits on a schedule. Has its own GUI dashboard.
 *
 * Start: npx tsx --tsconfig tsconfig.json agent/improve.ts
 * Dashboard: http://localhost:3848
 *
 * Runs independently from the main data agent (port 3847).
 * Can run in perpetuity even after development is "complete".
 */

import dotenv from "dotenv";
import path from "path";
import http from "http";
import fs from "fs";
import { execSync } from "child_process";

dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const PROJECT_ROOT = path.join(__dirname, "..");
const PORT = 3848;
const STATE_FILE = path.join(__dirname, "improve-state.json");

// ─────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────

type FindingCategory = "bug" | "improvement" | "performance" | "security" | "style" | "docs" | "debt";
type FindingSeverity = "critical" | "high" | "medium" | "low" | "info";
type FindingStatus = "open" | "fixed" | "dismissed" | "in_progress";

interface Finding {
  id: string;
  category: FindingCategory;
  severity: FindingSeverity;
  status: FindingStatus;
  title: string;
  description: string;
  file: string;
  line?: number;
  suggestion?: string;
  checker: string;
  firstSeen: string;
  lastSeen: string;
  fixedAt?: string;
}

interface ScanResult {
  checker: string;
  duration: number;
  findingsCount: number;
  newFindings: number;
  error?: string;
}

interface AgentState {
  findings: Finding[];
  lastFullScan: string | null;
  scanHistory: Array<{ date: string; results: ScanResult[]; totalFindings: number; newFindings: number }>;
  totalScans: number;
  stats: {
    totalFound: number;
    totalFixed: number;
    totalDismissed: number;
    totalOpen: number;
  };
}

// ─────────────────────────────────────────────
// State Management
// ─────────────────────────────────────────────

let state: AgentState = {
  findings: [],
  lastFullScan: null,
  scanHistory: [],
  totalScans: 0,
  stats: { totalFound: 0, totalFixed: 0, totalDismissed: 0, totalOpen: 0 },
};

function loadState() {
  try {
    if (fs.existsSync(STATE_FILE)) {
      state = JSON.parse(fs.readFileSync(STATE_FILE, "utf8"));
      log(`Loaded state: ${state.findings.length} findings, ${state.totalScans} scans`);
    }
  } catch (err) {
    log(`Could not load state: ${err}`);
  }
}

function saveState() {
  try {
    // Recompute stats
    state.stats.totalOpen = state.findings.filter(f => f.status === "open").length;
    state.stats.totalFixed = state.findings.filter(f => f.status === "fixed").length;
    state.stats.totalDismissed = state.findings.filter(f => f.status === "dismissed").length;
    state.stats.totalFound = state.findings.length;

    fs.writeFileSync(STATE_FILE, JSON.stringify(state, null, 2));
  } catch (err) {
    logError(`Could not save state: ${err}`);
  }
}

function addFinding(finding: Omit<Finding, "id" | "firstSeen" | "lastSeen" | "status">): boolean {
  // Check for existing duplicate
  const existing = state.findings.find(f =>
    f.file === finding.file &&
    f.title === finding.title &&
    f.checker === finding.checker &&
    f.line === finding.line
  );

  if (existing) {
    existing.lastSeen = new Date().toISOString();
    // If it was fixed/dismissed but reappeared, reopen it
    if (existing.status === "fixed") {
      existing.status = "open";
      return true; // counts as new
    }
    return false; // already known
  }

  const id = `${finding.checker}-${Date.now()}-${Math.random().toString(36).substring(2, 8)}`;
  state.findings.push({
    ...finding,
    id,
    status: "open",
    firstSeen: new Date().toISOString(),
    lastSeen: new Date().toISOString(),
  });
  return true;
}

// ─────────────────────────────────────────────
// Logging
// ─────────────────────────────────────────────

const logs: Array<{ time: string; level: string; message: string }> = [];

function log(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.log(`   ${time} [improve] ${msg}`);
  logs.push({ time: new Date().toISOString(), level: "info", message: msg });
  if (logs.length > 500) logs.shift();
}

function logError(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.error(`\x1b[31m   ${time} [improve] ${msg}\x1b[0m`);
  logs.push({ time: new Date().toISOString(), level: "error", message: msg });
}

function logSuccess(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.log(`\x1b[32m   ${time} [improve] ${msg}\x1b[0m`);
  logs.push({ time: new Date().toISOString(), level: "success", message: msg });
}

// ─────────────────────────────────────────────
// Checkers
// ─────────────────────────────────────────────

function readFilesSafe(dir: string, extensions: string[], exclude: string[] = []): Array<{ path: string; content: string; lines: string[] }> {
  const results: Array<{ path: string; content: string; lines: string[] }> = [];

  function walk(d: string) {
    try {
      const entries = fs.readdirSync(d, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = path.join(d, entry.name);
        const rel = path.relative(PROJECT_ROOT, fullPath);

        // Skip excluded dirs
        if (exclude.some(ex => rel.startsWith(ex) || entry.name === ex)) continue;

        if (entry.isDirectory()) {
          walk(fullPath);
        } else if (entry.isFile() && extensions.some(ext => entry.name.endsWith(ext))) {
          try {
            const content = fs.readFileSync(fullPath, "utf8");
            results.push({ path: rel.replace(/\\/g, "/"), content, lines: content.split("\n") });
          } catch { /* skip unreadable files */ }
        }
      }
    } catch { /* skip unreadable dirs */ }
  }

  walk(dir);
  return results;
}

/** Check 1: TypeScript compiler errors */
async function checkTypeScript(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  try {
    execSync("npx tsc --noEmit 2>&1", { cwd: PROJECT_ROOT, stdio: "pipe", encoding: "utf8", timeout: 120000 });
    // No errors
  } catch (err: unknown) {
    const output = (err as { stdout?: string }).stdout || "";
    const lines = output.split("\n").filter(l => l.includes("error TS"));

    for (const line of lines.slice(0, 50)) {
      const match = line.match(/^(.+?)\((\d+),\d+\):\s*error\s+(TS\d+):\s*(.+)/);
      if (match) {
        const isNew = addFinding({
          category: "bug",
          severity: "high",
          title: `TypeScript error ${match[3]}`,
          description: match[4],
          file: match[1].replace(/\\/g, "/"),
          line: parseInt(match[2]),
          checker: "typescript",
        });
        if (isNew) newFindings++;
      }
    }
  }

  return { checker: "typescript", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "typescript" && f.status === "open").length, newFindings };
}

/** Check 2: ESLint issues */
async function checkESLint(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  try {
    const output = execSync("npx eslint src/ --format json --max-warnings 999 2>/dev/null", {
      cwd: PROJECT_ROOT, stdio: "pipe", encoding: "utf8", timeout: 120000,
    });

    const results = JSON.parse(output);
    for (const file of results) {
      if (!file.messages || file.messages.length === 0) continue;
      const rel = path.relative(PROJECT_ROOT, file.filePath).replace(/\\/g, "/");

      for (const msg of file.messages.slice(0, 20)) {
        const severity: FindingSeverity = msg.severity === 2 ? "medium" : "low";
        const isNew = addFinding({
          category: "style",
          severity,
          title: `ESLint: ${msg.ruleId || "unknown"}`,
          description: msg.message,
          file: rel,
          line: msg.line,
          suggestion: msg.fix ? "Auto-fixable with --fix" : undefined,
          checker: "eslint",
        });
        if (isNew) newFindings++;
      }
    }
  } catch { /* eslint might not be configured or has errors */ }

  return { checker: "eslint", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "eslint" && f.status === "open").length, newFindings };
}

/** Check 3: any usage in TypeScript */
async function checkAnyUsage(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src"), [".ts", ".tsx"], ["node_modules", ".next"]);

  for (const file of files) {
    for (let i = 0; i < file.lines.length; i++) {
      const line = file.lines[i];
      // Skip eslint-disable comments and type assertion comments
      if (line.includes("eslint-disable") || line.includes("@ts-")) continue;

      // Check for explicit `any` type usage (not in comments or strings)
      const trimmed = line.trim();
      if (trimmed.startsWith("//") || trimmed.startsWith("*")) continue;

      // Match patterns like `: any`, `as any`, `<any>` but not `company`, `many`, etc.
      if (/[:\s<(,]any[>\s;,)\]|&]/.test(line) || line.endsWith(": any") || /:\s*any\s*[;=]/.test(line)) {
        const isNew = addFinding({
          category: "debt",
          severity: "low",
          title: "Explicit `any` type usage",
          description: `Line uses explicit \`any\` type which bypasses TypeScript checks`,
          file: file.path,
          line: i + 1,
          suggestion: "Replace with a specific type or use `unknown`",
          checker: "any-usage",
        });
        if (isNew) newFindings++;
      }
    }
  }

  return { checker: "any-usage", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "any-usage" && f.status === "open").length, newFindings };
}

/** Check 4: TODO/FIXME/HACK comments */
async function checkTodoComments(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src"), [".ts", ".tsx", ".js", ".jsx"], ["node_modules", ".next"]);

  for (const file of files) {
    for (let i = 0; i < file.lines.length; i++) {
      const line = file.lines[i];
      const match = line.match(/\/\/\s*(TODO|FIXME|HACK|XXX|BUG|OPTIMIZE)[\s:]+(.+)/i);
      if (match) {
        const tag = match[1].toUpperCase();
        const severity: FindingSeverity = tag === "BUG" || tag === "FIXME" ? "medium" : "info";
        const category: FindingCategory = tag === "BUG" || tag === "FIXME" ? "bug" : tag === "OPTIMIZE" ? "performance" : "debt";

        const isNew = addFinding({
          category,
          severity,
          title: `${tag}: ${match[2].trim().substring(0, 80)}`,
          description: match[2].trim(),
          file: file.path,
          line: i + 1,
          checker: "todo-comments",
        });
        if (isNew) newFindings++;
      }
    }
  }

  return { checker: "todo-comments", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "todo-comments" && f.status === "open").length, newFindings };
}

/** Check 5: Console.log in production code */
async function checkConsoleLogs(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src"), [".ts", ".tsx"], ["node_modules", ".next"]);

  for (const file of files) {
    // Skip test files and scripts
    if (file.path.includes("test") || file.path.includes("seed") || file.path.includes("script")) continue;

    for (let i = 0; i < file.lines.length; i++) {
      const line = file.lines[i].trim();
      if (line.startsWith("//")) continue;

      if (/console\.(log|debug|info)\(/.test(line)) {
        const isNew = addFinding({
          category: "style",
          severity: "low",
          title: "console.log in production code",
          description: `Remove console output before production deployment`,
          file: file.path,
          line: i + 1,
          suggestion: "Remove or replace with proper logging",
          checker: "console-log",
        });
        if (isNew) newFindings++;
      }
    }
  }

  return { checker: "console-log", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "console-log" && f.status === "open").length, newFindings };
}

/** Check 6: Large files (>400 lines) */
async function checkLargeFiles(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src"), [".ts", ".tsx"], ["node_modules", ".next"]);

  for (const file of files) {
    if (file.lines.length > 400) {
      const isNew = addFinding({
        category: "debt",
        severity: "info",
        title: `Large file: ${file.lines.length} lines`,
        description: `File has ${file.lines.length} lines. Consider splitting into smaller modules.`,
        file: file.path,
        suggestion: "Split into smaller, focused modules",
        checker: "large-files",
      });
      if (isNew) newFindings++;
    }
  }

  return { checker: "large-files", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "large-files" && f.status === "open").length, newFindings };
}

/** Check 7: API routes missing error handling */
async function checkApiErrorHandling(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src", "app", "api"), [".ts"], ["node_modules"]);

  for (const file of files) {
    if (!file.path.includes("route.ts")) continue;

    const hasTryCatch = file.content.includes("try {") || file.content.includes("try{");
    const hasExport = file.content.includes("export async function") || file.content.includes("export function");

    if (hasExport && !hasTryCatch) {
      const isNew = addFinding({
        category: "bug",
        severity: "medium",
        title: "API route missing try/catch",
        description: "API route handler has no error handling. Unhandled errors will return 500 with no useful message.",
        file: file.path,
        suggestion: "Wrap handler body in try/catch, return proper error responses",
        checker: "api-error-handling",
      });
      if (isNew) newFindings++;
    }
  }

  return { checker: "api-error-handling", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "api-error-handling" && f.status === "open").length, newFindings };
}

/** Check 8: Security issues (hardcoded secrets pattern) */
async function checkSecurityPatterns(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  const files = readFilesSafe(path.join(PROJECT_ROOT, "src"), [".ts", ".tsx"], ["node_modules", ".next"]);

  const patterns = [
    { regex: /password\s*[:=]\s*["'][^"']+["']/i, title: "Hardcoded password" },
    { regex: /api[_-]?key\s*[:=]\s*["'][a-zA-Z0-9]{16,}["']/i, title: "Hardcoded API key" },
    { regex: /secret\s*[:=]\s*["'][^"']+["']/i, title: "Hardcoded secret" },
    { regex: /Bearer\s+[a-zA-Z0-9\-._~+/]+=*/i, title: "Hardcoded bearer token" },
  ];

  for (const file of files) {
    // Skip type definitions and test files
    if (file.path.includes(".d.ts") || file.path.includes("test")) continue;

    for (let i = 0; i < file.lines.length; i++) {
      const line = file.lines[i];
      if (line.trim().startsWith("//")) continue;

      for (const pattern of patterns) {
        if (pattern.regex.test(line)) {
          // Skip env var access patterns
          if (line.includes("process.env") || line.includes("env.")) continue;

          const isNew = addFinding({
            category: "security",
            severity: "critical",
            title: pattern.title,
            description: `Potential hardcoded credential detected. Use environment variables instead.`,
            file: file.path,
            line: i + 1,
            suggestion: "Move to environment variable (.env.local)",
            checker: "security-patterns",
          });
          if (isNew) newFindings++;
        }
      }
    }
  }

  return { checker: "security-patterns", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "security-patterns" && f.status === "open").length, newFindings };
}

/** Check 9: Missing loading states in page components */
async function checkLoadingStates(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  // Check for page.tsx files without corresponding loading.tsx
  const srcApp = path.join(PROJECT_ROOT, "src", "app");
  function walkDirs(dir: string) {
    try {
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      const hasPage = entries.some(e => e.name === "page.tsx");
      const hasLoading = entries.some(e => e.name === "loading.tsx");

      if (hasPage && !hasLoading) {
        const rel = path.relative(PROJECT_ROOT, dir).replace(/\\/g, "/");
        // Skip API routes and simple pages
        if (!rel.includes("/api/")) {
          const isNew = addFinding({
            category: "improvement",
            severity: "low",
            title: `Missing loading.tsx`,
            description: `Route ${rel} has no loading.tsx — users see no feedback while the page loads`,
            file: rel + "/page.tsx",
            suggestion: "Add loading.tsx with a skeleton/spinner component",
            checker: "loading-states",
          });
          if (isNew) newFindings++;
        }
      }

      for (const entry of entries) {
        if (entry.isDirectory() && !entry.name.startsWith(".") && entry.name !== "node_modules") {
          walkDirs(path.join(dir, entry.name));
        }
      }
    } catch { /* skip */ }
  }

  walkDirs(srcApp);

  return { checker: "loading-states", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "loading-states" && f.status === "open").length, newFindings };
}

/** Check 10: npm audit */
async function checkNpmAudit(): Promise<ScanResult> {
  const start = Date.now();
  let newFindings = 0;

  try {
    const output = execSync("npm audit --json 2>/dev/null", {
      cwd: PROJECT_ROOT, stdio: "pipe", encoding: "utf8", timeout: 60000,
    });

    const audit = JSON.parse(output);
    if (audit.vulnerabilities) {
      for (const [pkg, vuln] of Object.entries(audit.vulnerabilities)) {
        const v = vuln as { severity: string; via: unknown[]; fixAvailable: boolean };
        const severity: FindingSeverity =
          v.severity === "critical" ? "critical" :
          v.severity === "high" ? "high" :
          v.severity === "moderate" ? "medium" : "low";

        const isNew = addFinding({
          category: "security",
          severity,
          title: `npm vulnerability: ${pkg}`,
          description: `${v.severity} severity vulnerability in ${pkg}`,
          file: "package.json",
          suggestion: v.fixAvailable ? "Run npm audit fix" : "Check for alternative package",
          checker: "npm-audit",
        });
        if (isNew) newFindings++;
      }
    }
  } catch { /* npm audit might fail */ }

  return { checker: "npm-audit", duration: Date.now() - start, findingsCount: state.findings.filter(f => f.checker === "npm-audit" && f.status === "open").length, newFindings };
}

// ─────────────────────────────────────────────
// Full Scan
// ─────────────────────────────────────────────

const ALL_CHECKERS = [
  { name: "TypeScript Errors", fn: checkTypeScript },
  { name: "ESLint Issues", fn: checkESLint },
  { name: "any Usage", fn: checkAnyUsage },
  { name: "TODO/FIXME", fn: checkTodoComments },
  { name: "Console Logs", fn: checkConsoleLogs },
  { name: "Large Files", fn: checkLargeFiles },
  { name: "API Error Handling", fn: checkApiErrorHandling },
  { name: "Security Patterns", fn: checkSecurityPatterns },
  { name: "Loading States", fn: checkLoadingStates },
  { name: "npm Audit", fn: checkNpmAudit },
];

let isScanning = false;
let scanProgress = { current: 0, total: ALL_CHECKERS.length, checker: "" };

async function runFullScan(): Promise<void> {
  if (isScanning) {
    log("Scan already in progress, skipping...");
    return;
  }

  isScanning = true;
  const start = Date.now();
  const results: ScanResult[] = [];
  let totalNew = 0;

  log(`Starting full scan (${ALL_CHECKERS.length} checkers)...`);

  for (let i = 0; i < ALL_CHECKERS.length; i++) {
    const checker = ALL_CHECKERS[i];
    scanProgress = { current: i + 1, total: ALL_CHECKERS.length, checker: checker.name };

    log(`Running: ${checker.name}...`);

    try {
      const result = await checker.fn();
      results.push(result);
      totalNew += result.newFindings;

      if (result.newFindings > 0) {
        logSuccess(`${checker.name}: ${result.newFindings} new findings (${result.findingsCount} total open)`);
      } else {
        log(`${checker.name}: ${result.findingsCount} open findings, ${result.newFindings} new`);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      logError(`${checker.name} failed: ${msg}`);
      results.push({ checker: checker.name, duration: 0, findingsCount: 0, newFindings: 0, error: msg });
    }
  }

  // Mark findings that weren't seen this scan as potentially fixed
  const now = new Date().toISOString();
  const fiveMinAgo = new Date(Date.now() - 5 * 60 * 1000).toISOString();
  for (const f of state.findings) {
    if (f.status === "open" && f.lastSeen < fiveMinAgo) {
      // If not seen in this scan, might be fixed
      // Don't auto-mark as fixed yet — just flag it
    }
  }

  state.lastFullScan = now;
  state.totalScans++;
  state.scanHistory.push({
    date: now,
    results,
    totalFindings: state.findings.filter(f => f.status === "open").length,
    newFindings: totalNew,
  });

  // Keep last 50 scan histories
  if (state.scanHistory.length > 50) {
    state.scanHistory = state.scanHistory.slice(-50);
  }

  saveState();
  isScanning = false;
  scanProgress = { current: 0, total: ALL_CHECKERS.length, checker: "" };

  const duration = Math.round((Date.now() - start) / 1000);
  const openCount = state.findings.filter(f => f.status === "open").length;
  logSuccess(`Full scan complete in ${duration}s: ${openCount} open findings, ${totalNew} new this scan`);
}

// ─────────────────────────────────────────────
// HTTP Server + Dashboard
// ─────────────────────────────────────────────

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://localhost:${PORT}`);

  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") {
    res.writeHead(200);
    res.end();
    return;
  }

  // Dashboard
  if (url.pathname === "/" || url.pathname === "/dashboard") {
    try {
      const html = fs.readFileSync(path.join(__dirname, "improve-dashboard.html"), "utf8");
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
      res.end(html);
    } catch {
      res.writeHead(500, { "Content-Type": "text/plain" });
      res.end("Dashboard HTML not found.");
    }
    return;
  }

  // API: Get all findings
  if (url.pathname === "/api/findings") {
    const category = url.searchParams.get("category");
    const severity = url.searchParams.get("severity");
    const status = url.searchParams.get("status") || "open";

    let findings = state.findings;
    if (status !== "all") findings = findings.filter(f => f.status === status);
    if (category) findings = findings.filter(f => f.category === category);
    if (severity) findings = findings.filter(f => f.severity === severity);

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ findings, total: findings.length }));
    return;
  }

  // API: Get stats
  if (url.pathname === "/api/stats") {
    const byCategory: Record<string, number> = {};
    const bySeverity: Record<string, number> = {};
    const byChecker: Record<string, number> = {};

    for (const f of state.findings.filter(f => f.status === "open")) {
      byCategory[f.category] = (byCategory[f.category] || 0) + 1;
      bySeverity[f.severity] = (bySeverity[f.severity] || 0) + 1;
      byChecker[f.checker] = (byChecker[f.checker] || 0) + 1;
    }

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({
      stats: state.stats,
      byCategory,
      bySeverity,
      byChecker,
      lastFullScan: state.lastFullScan,
      totalScans: state.totalScans,
      isScanning,
      scanProgress,
      recentScans: state.scanHistory.slice(-10),
      logs: logs.slice(-100),
    }));
    return;
  }

  // API: Update finding status
  if (url.pathname === "/api/findings/update" && req.method === "POST") {
    let body = "";
    req.on("data", chunk => body += chunk);
    req.on("end", () => {
      try {
        const { id, status: newStatus } = JSON.parse(body);
        const finding = state.findings.find(f => f.id === id);
        if (!finding) {
          res.writeHead(404, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "Finding not found" }));
          return;
        }

        finding.status = newStatus;
        if (newStatus === "fixed") finding.fixedAt = new Date().toISOString();
        saveState();

        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, finding }));
      } catch {
        res.writeHead(400, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: "Invalid request" }));
      }
    });
    return;
  }

  // API: Trigger scan
  if (url.pathname === "/api/scan" && req.method === "POST") {
    if (isScanning) {
      res.writeHead(409, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "Scan already in progress" }));
      return;
    }

    // Run scan async
    runFullScan().catch(err => logError(`Scan failed: ${err}`));

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ started: true }));
    return;
  }

  // API: Health
  if (url.pathname === "/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ status: "healthy", findings: state.stats.totalOpen, scans: state.totalScans }));
    return;
  }

  res.writeHead(404, { "Content-Type": "text/plain" });
  res.end("Not found");
});

// ─────────────────────────────────────────────
// Scheduler
// ─────────────────────────────────────────────

const SCAN_INTERVAL_MS = 4 * 60 * 60 * 1000; // Every 4 hours

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

async function main() {
  console.log("\n" + "═".repeat(55));
  console.log("  WaitingTheLongest — Perpetual Improvement Agent");
  console.log("  Dashboard: http://localhost:" + PORT);
  console.log("═".repeat(55) + "\n");

  loadState();

  server.listen(PORT, () => {
    logSuccess(`Improvement agent started. Dashboard at http://localhost:${PORT}`);
    log(`${state.findings.filter(f => f.status === "open").length} open findings from previous sessions`);
    log(`Scans run every ${SCAN_INTERVAL_MS / (60 * 60 * 1000)} hours`);
  });

  // Run initial scan after 10 seconds
  setTimeout(() => {
    log("Running initial scan...");
    runFullScan().catch(err => logError(`Initial scan failed: ${err}`));
  }, 10000);

  // Schedule periodic scans
  setInterval(() => {
    log("Scheduled scan starting...");
    runFullScan().catch(err => logError(`Scheduled scan failed: ${err}`));
  }, SCAN_INTERVAL_MS);
}

main().catch(err => {
  console.error("Fatal error:", err);
  process.exit(1);
});

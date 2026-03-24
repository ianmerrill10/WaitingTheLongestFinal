/**
 * Master Agent Launcher — starts ALL agents in parallel.
 *
 * Agents:
 *   - Data Agent (port 3847): RG sync, verification, audit, stats
 *   - Scraper Agent (port 3849): 50-state shelter website scraping
 *   - Improvement Agent (port 3848): code quality scanning
 *   - Backup System: automated zip + cloud backup
 *
 * Usage: npx tsx --tsconfig tsconfig.json agent/start-all.ts
 *   or:  npm run agent:all
 */

import { spawn, ChildProcess } from "child_process";
import path from "path";

const ROOT = path.join(__dirname, "..");

interface AgentConfig {
  name: string;
  script: string;
  port: number;
  color: string;
  restartOnCrash: boolean;
  restartDelayMs: number;
}

const AGENTS: AgentConfig[] = [
  {
    name: "DATA",
    script: "agent/index.ts",
    port: 3847,
    color: "\x1b[36m",    // cyan
    restartOnCrash: true,
    restartDelayMs: 5000,
  },
  {
    name: "SCRAPER",
    script: "agent/scraper.ts",
    port: 3849,
    color: "\x1b[32m",    // green
    restartOnCrash: true,
    restartDelayMs: 10000,
  },
  {
    name: "FRESHNESS",
    script: "agent/freshness.ts",
    port: 3850,
    color: "\x1b[35m",    // magenta
    restartOnCrash: true,
    restartDelayMs: 10000,
  },
  {
    name: "INTEL",
    script: "agent/intelligence.ts",
    port: 3851,
    color: "\x1b[34m",    // blue
    restartOnCrash: true,
    restartDelayMs: 15000,
  },
  {
    name: "IMPROVE",
    script: "agent/improve.ts",
    port: 3848,
    color: "\x1b[33m",    // yellow
    restartOnCrash: true,
    restartDelayMs: 30000,
  },
];

const RESET = "\x1b[0m";

function log(agent: string, color: string, msg: string) {
  const time = new Date().toLocaleTimeString();
  console.log(`${color}[${time}] [${agent}]${RESET} ${msg}`);
}

function startAgent(config: AgentConfig): ChildProcess {
  log(config.name, config.color, `Starting on port ${config.port}...`);

  const child = spawn(
    "npx",
    ["tsx", "--tsconfig", "tsconfig.json", config.script],
    {
      cwd: ROOT,
      stdio: ["ignore", "pipe", "pipe"],
      shell: true,
      env: { ...process.env, FORCE_COLOR: "1" },
    }
  );

  child.stdout?.on("data", (data: Buffer) => {
    const lines = data.toString().split("\n").filter(Boolean);
    for (const line of lines) {
      console.log(`${config.color}[${config.name}]${RESET} ${line}`);
    }
  });

  child.stderr?.on("data", (data: Buffer) => {
    const lines = data.toString().split("\n").filter(Boolean);
    for (const line of lines) {
      console.error(`${config.color}[${config.name}]${RESET} \x1b[31m${line}${RESET}`);
    }
  });

  child.on("exit", (code, signal) => {
    log(config.name, config.color, `Exited (code=${code}, signal=${signal})`);

    if (config.restartOnCrash && !isShuttingDown) {
      log(config.name, config.color, `Restarting in ${config.restartDelayMs / 1000}s...`);
      setTimeout(() => {
        if (!isShuttingDown) {
          const newChild = startAgent(config);
          children.set(config.name, newChild);
        }
      }, config.restartDelayMs);
    }
  });

  return child;
}

// Track all children for graceful shutdown
const children = new Map<string, ChildProcess>();
let isShuttingDown = false;

function shutdown() {
  if (isShuttingDown) return;
  isShuttingDown = true;
  console.log("\n\x1b[31m[MASTER] Shutting down all agents...\x1b[0m");

  for (const [name, child] of Array.from(children.entries())) {
    log(name, "\x1b[31m", "Sending SIGTERM...");
    child.kill("SIGTERM");
  }

  // Force kill after 10 seconds
  setTimeout(() => {
    for (const [name, child] of Array.from(children.entries())) {
      if (!child.killed) {
        log(name, "\x1b[31m", "Force killing...");
        child.kill("SIGKILL");
      }
    }
    process.exit(0);
  }, 10000);
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);

// Banner
console.log(`
\x1b[32m════════════════════════════════════════════════════════════
  WAITINGTHELONGEST.COM — MASTER AGENT LAUNCHER

  Data Agent:      http://localhost:3847
  Improvement:     http://localhost:3848
  Scraper (50-st): http://localhost:3849
  Freshness:       http://localhost:3850
  Intelligence:    http://localhost:3851

  All agents auto-restart on crash.
  Press Ctrl+C to stop all.
════════════════════════════════════════════════════════════\x1b[0m
`);

// Start all agents
for (const config of AGENTS) {
  const child = startAgent(config);
  children.set(config.name, child);
}

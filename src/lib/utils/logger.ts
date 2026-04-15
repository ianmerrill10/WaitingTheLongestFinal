/**
 * Structured logger for production.
 *
 * Emits JSON log lines with a stable schema so logs can be:
 *   - grepped/filtered by level, module, event
 *   - correlated by request_id across a request's lifecycle
 *   - ingested by log aggregators (Vercel, Datadog, etc.) without parsing
 *
 * Usage:
 *   import { logger } from "@/lib/utils/logger";
 *
 *   logger.info("dog_sync_start", { source: "rescuegroups" });
 *   logger.error("dog_sync_failed", { source: "rescuegroups", error: err });
 *
 *   const log = logger.child({ request_id: "abc123", module: "webhook" });
 *   log.warn("rate_limit_hit", { ip: "1.2.3.4" });
 */

export type LogLevel = "debug" | "info" | "warn" | "error";

const LEVEL_PRIORITY: Record<LogLevel, number> = {
  debug: 0,
  info: 1,
  warn: 2,
  error: 3,
};

const envLevel = (process.env.LOG_LEVEL || "info").toLowerCase() as LogLevel;
const MIN_LEVEL = LEVEL_PRIORITY[envLevel] ?? LEVEL_PRIORITY.info;

// Keys that should never be logged (credentials, PII)
const REDACT_KEYS = new Set([
  "password",
  "token",
  "secret",
  "api_key",
  "apikey",
  "authorization",
  "cookie",
  "set-cookie",
  "credit_card",
  "ssn",
  "service_role_key",
  "cron_secret",
  "webhook_secret",
]);

function redact(obj: unknown, depth = 0): unknown {
  if (depth > 6) return "[MAX_DEPTH]";
  if (obj === null || obj === undefined) return obj;
  if (typeof obj !== "object") return obj;
  if (obj instanceof Error) {
    return {
      name: obj.name,
      message: obj.message,
      stack: obj.stack?.split("\n").slice(0, 5).join("\n"),
    };
  }
  if (Array.isArray(obj)) {
    return obj.slice(0, 100).map((v) => redact(v, depth + 1));
  }
  const out: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(obj as Record<string, unknown>)) {
    if (REDACT_KEYS.has(key.toLowerCase())) {
      out[key] = "[REDACTED]";
    } else {
      out[key] = redact(value, depth + 1);
    }
  }
  return out;
}

interface LogContext {
  [key: string]: unknown;
}

function emit(level: LogLevel, event: string, context: LogContext) {
  if (LEVEL_PRIORITY[level] < MIN_LEVEL) return;

  const record = {
    ts: new Date().toISOString(),
    level,
    event,
    ...(redact(context) as LogContext),
  };

  const line = JSON.stringify(record);
  if (level === "error") {
    console.error(line);
  } else if (level === "warn") {
    console.warn(line);
  } else {
    console.log(line);
  }
}

export interface Logger {
  debug(event: string, context?: LogContext): void;
  info(event: string, context?: LogContext): void;
  warn(event: string, context?: LogContext): void;
  error(event: string, context?: LogContext): void;
  child(context: LogContext): Logger;
}

function createLogger(baseContext: LogContext = {}): Logger {
  return {
    debug: (event, ctx = {}) => emit("debug", event, { ...baseContext, ...ctx }),
    info: (event, ctx = {}) => emit("info", event, { ...baseContext, ...ctx }),
    warn: (event, ctx = {}) => emit("warn", event, { ...baseContext, ...ctx }),
    error: (event, ctx = {}) => emit("error", event, { ...baseContext, ...ctx }),
    child: (ctx) => createLogger({ ...baseContext, ...ctx }),
  };
}

export const logger = createLogger();

/**
 * Generate a short correlation ID for request tracing.
 * Not crypto-secure — don't use for auth.
 */
export function correlationId(): string {
  return Math.random().toString(36).slice(2, 10) + Date.now().toString(36).slice(-4);
}

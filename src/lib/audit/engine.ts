// Audit Engine - orchestrates all audit checks and repairs
// Runs as a Vercel cron job every 4 hours or on-demand via API

import { AuditLogger } from "./logger";
import {
  checkDates,
  checkDescriptionDates,
  checkDescriptionUrgency,
  checkAgeSanity,
  checkStaleness,
  checkDuplicates,
  checkStatuses,
  checkPhotos,
  checkShelters,
  checkDataQuality,
} from "./checks";

export type AuditMode = "full" | "quick" | "dates_only" | "stale_only" | "repair_only" | "description_dates" | "age_sanity" | "urgency_parse";

export interface AuditResult {
  runId: string;
  mode: AuditMode;
  duration: number;
  stats: {
    dates?: { checked: number; flagged: number; repaired: number };
    staleness?: { checked: number; stale: number; deactivated: number };
    duplicates?: { checked: number; duplicates: number; removed: number };
    statuses?: { checked: number; issues: number; repaired: number };
    photos?: { checked: number; missing: number };
    shelters?: { checked: number; issues: number };
    data_quality?: { checked: number; issues: number; repaired: number };
    description_dates?: { checked: number; updated: number };
    urgency_parse?: { checked: number; flagged: number; datesSet: number };
    age_sanity?: { checked: number; fixed: number };
  };
  logStats: {
    info: number;
    warning: number;
    error: number;
    critical: number;
    repairs: number;
  };
  logsWritten: number;
}

export async function runAudit(
  mode: AuditMode = "full",
  triggerSource = "cron"
): Promise<AuditResult> {
  const startTime = Date.now();

  // Start a run record
  const runId = await AuditLogger.startRun(mode, triggerSource);
  const logger = new AuditLogger(runId);
  const stats: AuditResult["stats"] = {};

  try {
    logger.log({
      audit_type: "system",
      entity_type: "dog",
      severity: "info",
      message: `Audit started: mode=${mode}, trigger=${triggerSource}`,
    });

    // Run checks based on mode
    if (mode === "full" || mode === "quick" || mode === "dates_only" || mode === "repair_only") {
      stats.dates = await checkDates(logger);
      await logger.flush();
    }

    // Description date parsing — extract real dates from "Posted 2/18/18" etc.
    if (mode === "full" || mode === "dates_only" || mode === "repair_only" || mode === "description_dates") {
      stats.description_dates = await checkDescriptionDates(logger);
      await logger.flush();
    }

    // Description urgency parsing — extract euthanasia signals from descriptions
    if (mode === "full" || mode === "repair_only" || mode === "urgency_parse") {
      stats.urgency_parse = await checkDescriptionUrgency(logger);
      await logger.flush();
    }

    // Age sanity check — wait time cannot exceed dog's age
    if (mode === "full" || mode === "dates_only" || mode === "repair_only" || mode === "age_sanity") {
      stats.age_sanity = await checkAgeSanity(logger);
      await logger.flush();
    }

    if (mode === "full" || mode === "quick" || mode === "stale_only") {
      stats.staleness = await checkStaleness(logger);
      await logger.flush();
    }

    if (mode === "full" || mode === "repair_only") {
      stats.duplicates = await checkDuplicates(logger);
      await logger.flush();
    }

    if (mode === "full" || mode === "quick") {
      stats.statuses = await checkStatuses(logger);
      stats.photos = await checkPhotos(logger);
      await logger.flush();
    }

    if (mode === "full") {
      stats.shelters = await checkShelters(logger);
      stats.data_quality = await checkDataQuality(logger);
      await logger.flush();
    }

    // Calculate totals
    const totalRepairs =
      (stats.dates?.repaired ?? 0) +
      (stats.description_dates?.updated ?? 0) +
      (stats.urgency_parse?.datesSet ?? 0) +
      (stats.age_sanity?.fixed ?? 0) +
      (stats.duplicates?.removed ?? 0) +
      (stats.statuses?.repaired ?? 0) +
      (stats.data_quality?.repaired ?? 0) +
      (stats.staleness?.deactivated ?? 0);

    const totalIssues =
      (stats.dates?.flagged ?? 0) +
      (stats.staleness?.stale ?? 0) +
      (stats.duplicates?.duplicates ?? 0) +
      (stats.statuses?.issues ?? 0) +
      (stats.photos?.missing ?? 0) +
      (stats.shelters?.issues ?? 0) +
      (stats.data_quality?.issues ?? 0) +
      (stats.urgency_parse?.flagged ?? 0) +
      (stats.age_sanity?.fixed ?? 0);

    logger.log({
      audit_type: "system",
      entity_type: "dog",
      severity: "info",
      message: `Audit completed: ${totalIssues} issues found, ${totalRepairs} auto-repaired`,
      details: { duration_ms: Date.now() - startTime, stats },
    });

    // Final flush
    const logsWritten = await logger.flush();
    const logStats = logger.getStats();

    // Complete the run
    await AuditLogger.completeRun(runId, {
      ...stats,
      totalIssues,
      totalRepairs,
      duration_ms: Date.now() - startTime,
      logStats,
    });

    return {
      runId,
      mode,
      duration: Date.now() - startTime,
      stats,
      logStats,
      logsWritten,
    };
  } catch (err) {
    logger.log({
      audit_type: "system",
      entity_type: "dog",
      severity: "critical",
      message: `Audit failed: ${err instanceof Error ? err.message : String(err)}`,
    });
    await logger.flush();
    await AuditLogger.completeRun(
      runId,
      { error: String(err), duration_ms: Date.now() - startTime },
      true
    );

    return {
      runId,
      mode,
      duration: Date.now() - startTime,
      stats,
      logStats: logger.getStats(),
      logsWritten: 0,
    };
  }
}

/**
 * Unified Daily Agent — Sync + Verify + Audit
 *
 * Runs at 6am daily via Vercel cron. Does everything in one pass:
 * 1. Sync new dogs from RescueGroups (50 pages = ~5K dogs)
 * 2. Verify oldest unverified dogs against RG API (batch of 100)
 * 3. Run quick audit (date checks, staleness, statuses)
 * 4. Parse description dates for accuracy
 *
 * All automated. No manual intervention needed.
 */
import { NextResponse } from "next/server";
import { syncDogsFromRescueGroups } from "@/lib/rescuegroups/sync";
import { runAudit } from "@/lib/audit/engine";
import { runVerification, getVerificationStats } from "@/lib/verification/engine";
import { computeAndStoreDailyStats } from "@/lib/stats/compute-daily-stats";

export const runtime = "nodejs";
export const maxDuration = 300; // 5 minutes

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const maxPages = Math.min(parseInt(url.searchParams.get("pages") || "10", 10), 50);
  const startPage = Math.max(parseInt(url.searchParams.get("start") || "1", 10), 1);
  const skipSync = url.searchParams.get("skip_sync") === "true";
  const skipVerify = url.searchParams.get("skip_verify") === "true";
  const skipAudit = url.searchParams.get("skip_audit") === "true";
  const verifyBatch = Math.min(parseInt(url.searchParams.get("verify_batch") || "300", 10), 500);

  const startTime = Date.now();
  const pipeline: Record<string, unknown> = {
    started_at: new Date().toISOString(),
    steps: [],
  };

  try {
    // ── Step 1: Sync dogs from RescueGroups ──
    if (!skipSync) {
      const syncStart = Date.now();
      try {
        const syncResult = await syncDogsFromRescueGroups(maxPages, startPage);
        const step = {
          step: "sync",
          status: "success",
          duration_ms: Date.now() - syncStart,
          ...syncResult,
        };
        (pipeline.steps as unknown[]).push(step);
        pipeline.sync = step;
      } catch (err) {
        (pipeline.steps as unknown[]).push({
          step: "sync",
          status: "error",
          error: err instanceof Error ? err.message : String(err),
          duration_ms: Date.now() - syncStart,
        });
      }
    }

    // ── Step 2: Verify dogs against RG API ──
    // Check oldest unverified dogs first, then stale verifications
    if (!skipVerify) {
      const verifyStart = Date.now();
      try {
        // Split batch: 70% oldest never-verified, 30% stale re-verification
        const neverBatch = Math.floor(verifyBatch * 0.7);
        const staleBatch = verifyBatch - neverBatch;

        const neverResult = await runVerification(neverBatch, "never_verified");
        const staleResult = await runVerification(staleBatch, "stale");

        const combined = {
          checked: neverResult.checked + staleResult.checked,
          verified: neverResult.verified + staleResult.verified,
          notFound: neverResult.notFound + staleResult.notFound,
          deactivated: neverResult.deactivated + staleResult.deactivated,
          pending: neverResult.pending + staleResult.pending,
          errors: neverResult.errors + staleResult.errors,
          availableDatesCaptured: neverResult.availableDatesCaptured + staleResult.availableDatesCaptured,
          foundDatesCaptured: neverResult.foundDatesCaptured + staleResult.foundDatesCaptured,
          killDatesCaptured: neverResult.killDatesCaptured + staleResult.killDatesCaptured,
        };

        const stats = await getVerificationStats();

        const step = {
          step: "verify",
          status: "success",
          duration_ms: Date.now() - verifyStart,
          ...combined,
          overall_stats: stats,
        };
        (pipeline.steps as unknown[]).push(step);
        pipeline.verification = step;
      } catch (err) {
        (pipeline.steps as unknown[]).push({
          step: "verify",
          status: "error",
          error: err instanceof Error ? err.message : String(err),
          duration_ms: Date.now() - verifyStart,
        });
      }
    }

    // ── Step 3: Quick audit ──
    if (!skipAudit) {
      const auditStart = Date.now();
      try {
        const auditResult = await runAudit("quick", "daily_agent");
        const step = {
          step: "audit",
          status: "success",
          duration_ms: Date.now() - auditStart,
          runId: auditResult.runId,
          stats: auditResult.stats,
          logStats: auditResult.logStats,
        };
        (pipeline.steps as unknown[]).push(step);
        pipeline.audit = step;
      } catch (err) {
        (pipeline.steps as unknown[]).push({
          step: "audit",
          status: "error",
          error: err instanceof Error ? err.message : String(err),
          duration_ms: Date.now() - auditStart,
        });
      }
    }

    // ── Step 4: Compute daily stats snapshot ──
    const statsStart = Date.now();
    try {
      const statsResult = await computeAndStoreDailyStats();
      const step = {
        step: "daily_stats",
        status: "success",
        duration_ms: Date.now() - statsStart,
        ...statsResult,
      };
      (pipeline.steps as unknown[]).push(step);
      pipeline.daily_stats = step;
    } catch (err) {
      (pipeline.steps as unknown[]).push({
        step: "daily_stats",
        status: "error",
        error: err instanceof Error ? err.message : String(err),
        duration_ms: Date.now() - statsStart,
      });
    }

    pipeline.completed_at = new Date().toISOString();
    pipeline.total_duration_ms = Date.now() - startTime;

    // Build summary message
    const steps = pipeline.steps as { step: string; status: string }[];
    const successSteps = steps.filter(s => s.status === "success").map(s => s.step);
    const failedSteps = steps.filter(s => s.status === "error").map(s => s.step);
    pipeline.message = `Daily agent complete: ${successSteps.join(", ")} succeeded${failedSteps.length ? `, ${failedSteps.join(", ")} failed` : ""}`;

    return NextResponse.json({ success: true, ...pipeline });
  } catch (error) {
    console.error("Daily agent error:", error);
    return NextResponse.json(
      { success: false, error: String(error), pipeline },
      { status: 500 }
    );
  }
}

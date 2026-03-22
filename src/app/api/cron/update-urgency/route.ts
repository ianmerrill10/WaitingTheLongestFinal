/**
 * Evening Agent — Urgency Update + Verification + Date Accuracy
 *
 * Runs at 6pm daily via Vercel cron:
 * 1. Recalculate urgency levels for all dogs with euthanasia dates
 * 2. Verify another batch of dogs against RG API (oldest first)
 * 3. Run description date parsing for any missed dogs
 * 4. Age sanity check — cap intake_date when wait time > dog's age
 *
 * Combined with the 6am sync agent, this means verification runs
 * twice daily — ~200 dogs/day verified = all 33K dogs in ~165 days.
 */
import { NextResponse } from "next/server";
import { updateUrgencyLevels } from "@/lib/rescuegroups/sync";
import { runVerification, getVerificationStats } from "@/lib/verification/engine";
import { runAudit } from "@/lib/audit/engine";

export const runtime = "nodejs";
export const maxDuration = 300;

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const startTime = Date.now();
  const pipeline: Record<string, unknown> = {
    started_at: new Date().toISOString(),
    steps: [],
  };

  try {
    // ── Step 1: Update urgency levels ──
    const urgencyStart = Date.now();
    try {
      const result = await updateUrgencyLevels();
      const step = {
        step: "urgency",
        status: "success",
        duration_ms: Date.now() - urgencyStart,
        ...result,
      };
      (pipeline.steps as unknown[]).push(step);
      pipeline.urgency = step;
    } catch (err) {
      (pipeline.steps as unknown[]).push({
        step: "urgency",
        status: "error",
        error: err instanceof Error ? err.message : String(err),
        duration_ms: Date.now() - urgencyStart,
      });
    }

    // ── Step 2: Verify batch of dogs ──
    const verifyStart = Date.now();
    try {
      const verifyResult = await runVerification(300, "oldest");
      const stats = await getVerificationStats();
      const step = {
        step: "verify",
        status: "success",
        duration_ms: Date.now() - verifyStart,
        ...verifyResult,
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

    // ── Step 3: Description date parsing (catch any missed) ──
    const auditStart = Date.now();
    try {
      const auditResult = await runAudit("description_dates", "evening_agent");
      const step = {
        step: "description_dates",
        status: "success",
        duration_ms: Date.now() - auditStart,
        stats: auditResult.stats.description_dates,
      };
      (pipeline.steps as unknown[]).push(step);
      pipeline.description_dates = step;
    } catch (err) {
      (pipeline.steps as unknown[]).push({
        step: "description_dates",
        status: "error",
        error: err instanceof Error ? err.message : String(err),
        duration_ms: Date.now() - auditStart,
      });
    }

    // ── Step 4: Age sanity check — wait time cannot exceed dog's age ──
    const ageStart = Date.now();
    try {
      const ageResult = await runAudit("age_sanity", "evening_agent");
      const step = {
        step: "age_sanity",
        status: "success",
        duration_ms: Date.now() - ageStart,
        stats: ageResult.stats.age_sanity,
      };
      (pipeline.steps as unknown[]).push(step);
      pipeline.age_sanity = step;
    } catch (err) {
      (pipeline.steps as unknown[]).push({
        step: "age_sanity",
        status: "error",
        error: err instanceof Error ? err.message : String(err),
        duration_ms: Date.now() - ageStart,
      });
    }

    pipeline.completed_at = new Date().toISOString();
    pipeline.total_duration_ms = Date.now() - startTime;

    const steps = pipeline.steps as { step: string; status: string }[];
    const successSteps = steps.filter(s => s.status === "success").map(s => s.step);
    const failedSteps = steps.filter(s => s.status === "error").map(s => s.step);
    pipeline.message = `Evening agent complete: ${successSteps.join(", ")} succeeded${failedSteps.length ? `, ${failedSteps.join(", ")} failed` : ""}`;

    return NextResponse.json({ success: true, ...pipeline });
  } catch (error) {
    console.error("Evening agent error:", error);
    return NextResponse.json(
      { success: false, error: String(error), pipeline },
      { status: 500 }
    );
  }
}

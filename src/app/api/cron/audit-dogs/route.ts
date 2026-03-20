// Cron: Dog Data Audit Agent
// Runs every 4 hours to audit all dog data for accuracy
// Schedule: 0 */4 * * * (vercel.json)

import { NextResponse } from "next/server";
import { runAudit } from "@/lib/audit/engine";
import type { AuditMode } from "@/lib/audit/engine";

export const runtime = "nodejs";
export const maxDuration = 300; // 5 min max

export async function GET(request: Request) {
  // Verify authorization
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const mode = (url.searchParams.get("mode") || "quick") as AuditMode;
  const validModes: AuditMode[] = ["full", "quick", "dates_only", "stale_only", "repair_only", "description_dates", "age_sanity"];

  if (!validModes.includes(mode)) {
    return NextResponse.json(
      { error: `Invalid mode. Use: ${validModes.join(", ")}` },
      { status: 400 }
    );
  }

  try {
    const result = await runAudit(mode, "cron");

    return NextResponse.json({
      success: true,
      runId: result.runId,
      mode: result.mode,
      duration_ms: result.duration,
      stats: result.stats,
      logStats: result.logStats,
      logsWritten: result.logsWritten,
      message: `Audit complete: ${result.logStats.warning} warnings, ${result.logStats.error} errors, ${result.logStats.critical} critical, ${result.logStats.repairs} auto-repairs`,
    });
  } catch (err) {
    return NextResponse.json(
      {
        success: false,
        error: err instanceof Error ? err.message : String(err),
      },
      { status: 500 }
    );
  }
}

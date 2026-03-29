// Audit Dashboard API — view logs, runs, trigger manual audits
// GET: view recent runs, logs, summary
// POST: trigger a manual audit

import { NextResponse } from "next/server";
import { AuditLogger } from "@/lib/audit/logger";
import { runAudit } from "@/lib/audit/engine";
import type { AuditMode } from "@/lib/audit/engine";

export const runtime = "nodejs";
export const maxDuration = 300;

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const action = url.searchParams.get("action") || "summary";

  try {
    switch (action) {
      case "summary": {
        const hours = parseInt(url.searchParams.get("hours") || "24");
        const [summary, recentRuns] = await Promise.all([
          AuditLogger.getSummary(hours),
          AuditLogger.getRecentRuns(10),
        ]);
        return NextResponse.json({
          summary,
          recent_runs: recentRuns.map((r) => ({
            id: r.id,
            type: r.run_type,
            status: r.status,
            started_at: r.started_at,
            completed_at: r.completed_at,
            trigger: r.trigger_source,
            stats: r.stats,
          })),
        });
      }

      case "runs": {
        const limit = parseInt(url.searchParams.get("limit") || "20");
        const runs = await AuditLogger.getRecentRuns(limit);
        return NextResponse.json({ runs });
      }

      case "logs": {
        const runId = url.searchParams.get("run_id");
        if (!runId) {
          return NextResponse.json(
            { error: "run_id required" },
            { status: 400 }
          );
        }
        const severity = url.searchParams.get("severity") as
          | "info"
          | "warning"
          | "error"
          | "critical"
          | null;
        const limit = parseInt(url.searchParams.get("limit") || "200");
        const logs = await AuditLogger.getRunLogs(
          runId,
          severity || undefined,
          limit
        );
        return NextResponse.json({ logs, count: logs.length });
      }

      default:
        return NextResponse.json(
          {
            error: "Unknown action",
            actions: ["summary", "runs", "logs"],
          },
          { status: 400 }
        );
    }
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : String(err) },
      { status: 500 }
    );
  }
}

export async function POST(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await request.json().catch(() => ({}));
  const mode = (body.mode || "quick") as AuditMode;

  try {
    const result = await runAudit(mode, "manual");
    return NextResponse.json({
      success: true,
      ...result,
    });
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : String(err) },
      { status: 500 }
    );
  }
}

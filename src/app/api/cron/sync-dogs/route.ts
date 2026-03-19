import { NextResponse } from "next/server";
import { syncDogsFromRescueGroups } from "@/lib/rescuegroups/sync";
import { runAudit } from "@/lib/audit/engine";

export const runtime = "nodejs";
export const maxDuration = 300; // 5 minutes

export async function GET(request: Request) {
  // Verify cron secret for Vercel Cron
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    // Allow ?pages=N&start=N to control sync size and offset
    const url = new URL(request.url);
    const maxPages = Math.min(
      parseInt(url.searchParams.get("pages") || "10", 10),
      50
    );
    const startPage = Math.max(
      parseInt(url.searchParams.get("start") || "1", 10),
      1
    );
    const shouldAudit = url.searchParams.get("audit") === "true";

    const result = await syncDogsFromRescueGroups(maxPages, startPage);

    // Run audit after sync if requested
    let auditResult = null;
    if (shouldAudit) {
      try {
        auditResult = await runAudit("quick", "post_sync");
      } catch (auditErr) {
        console.error("Post-sync audit error:", auditErr);
        auditResult = { error: String(auditErr) };
      }
    }

    return NextResponse.json({
      success: true,
      ...result,
      audit: auditResult,
      message: `Synced ${result.totalFetched} dogs: ${result.inserted} new, ${result.updated} updated, ${result.sheltersCreated} shelters created, ${result.errors} errors${auditResult ? " + audit completed" : ""}`,
    });
  } catch (error) {
    console.error("Sync error:", error);
    return NextResponse.json(
      { success: false, error: String(error) },
      { status: 500 }
    );
  }
}

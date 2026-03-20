/**
 * Dog Verification API
 *
 * Verifies that dogs on our site still exist in RescueGroups.
 * Can be triggered manually or by cron.
 *
 * Query params:
 *   batch=N      — number of dogs to verify (default 50, max 200)
 *   priority=X   — 'oldest' | 'never_verified' | 'stale' (default 'oldest')
 *   stats=true   — return verification stats only, no verification run
 */
import { NextResponse } from "next/server";
import { runVerification, getVerificationStats } from "@/lib/verification/engine";

export const runtime = "nodejs";
export const maxDuration = 300; // 5 min max

export async function GET(request: Request) {
  // Auth required
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const statsOnly = url.searchParams.get("stats") === "true";

  if (statsOnly) {
    const stats = await getVerificationStats();
    return NextResponse.json({ success: true, stats });
  }

  const batch = Math.min(parseInt(url.searchParams.get("batch") || "50"), 200);
  const priority = (url.searchParams.get("priority") || "oldest") as "oldest" | "never_verified" | "stale";

  if (!["oldest", "never_verified", "stale"].includes(priority)) {
    return NextResponse.json(
      { error: "Invalid priority. Use: oldest, never_verified, stale" },
      { status: 400 }
    );
  }

  try {
    const result = await runVerification(batch, priority);

    return NextResponse.json({
      success: true,
      ...result,
      message: `Verified ${result.checked} dogs: ${result.verified} confirmed, ${result.notFound} not found, ${result.deactivated} deactivated, ${result.pending} adoption pending, ${result.errors} API errors`,
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

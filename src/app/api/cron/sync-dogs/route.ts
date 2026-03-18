import { NextResponse } from "next/server";
import { syncDogsFromRescueGroups } from "@/lib/rescuegroups/sync";

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

    const result = await syncDogsFromRescueGroups(maxPages, startPage);

    return NextResponse.json({
      success: true,
      ...result,
      message: `Synced ${result.totalFetched} dogs: ${result.inserted} new, ${result.updated} updated, ${result.sheltersCreated} shelters created, ${result.errors} errors`,
    });
  } catch (error) {
    console.error("Sync error:", error);
    return NextResponse.json(
      { success: false, error: String(error) },
      { status: 500 }
    );
  }
}

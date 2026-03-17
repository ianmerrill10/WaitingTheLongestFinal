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
    const result = await syncDogsFromRescueGroups(50);

    return NextResponse.json({
      success: true,
      ...result,
      message: `Synced ${result.totalFetched} dogs: ${result.inserted} new, ${result.updated} updated, ${result.errors} errors`,
    });
  } catch (error) {
    console.error("Sync error:", error);
    return NextResponse.json(
      { success: false, error: String(error) },
      { status: 500 }
    );
  }
}

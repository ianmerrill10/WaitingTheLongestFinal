import { NextResponse } from "next/server";
import { syncGovOpenData } from "@/lib/connectors/gov-data-sync";

export const dynamic = "force-dynamic";
export const maxDuration = 300; // 5 minutes

/**
 * Sync dogs from government open data portals (Socrata).
 * These provide gold-standard intake dates from official records.
 *
 * GET /api/cron/sync-gov-data?state=TX
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const state = searchParams.get("state") || undefined;

    const results = await syncGovOpenData(state);

    const totalFetched = results.reduce((s, r) => s + r.fetched, 0);
    const totalUpgrades = results.reduce((s, r) => s + r.dateUpgrades, 0);
    const totalInserts = results.reduce((s, r) => s + r.newInserts, 0);
    const totalErrors = results.reduce((s, r) => s + r.errors, 0);

    return NextResponse.json({
      success: true,
      summary: {
        sources: results.length,
        totalFetched,
        totalUpgrades,
        totalInserts,
        totalErrors,
      },
      details: results,
    });
  } catch (err) {
    console.error("[sync-gov-data] Error:", err);
    return NextResponse.json(
      { success: false, error: String(err) },
      { status: 500 }
    );
  }
}

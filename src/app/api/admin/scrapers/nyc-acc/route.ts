/**
 * Admin API: NYC ACC Scraper
 *
 * POST /api/admin/scrapers/nyc-acc — trigger a scrape of the NYC ACC priority placement page
 * GET  /api/admin/scrapers/nyc-acc — get status/info about NYC ACC data
 */

import { NextResponse } from "next/server";
import { runNycAccScrape } from "@/lib/scrapers/nyc-acc";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const maxDuration = 120;

export async function POST() {
  try {
    const result = await runNycAccScrape({
      fetchDetails: true,
      maxDetailFetches: 20,
      delayMs: 2000,
    });

    return NextResponse.json({
      success: true,
      ...result,
    });
  } catch (error) {
    console.error("NYC ACC scraper error:", error);
    return NextResponse.json(
      { success: false, error: String(error) },
      { status: 500 }
    );
  }
}

export async function GET() {
  const supabase = createAdminClient();

  const { count: totalNycAcc } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("external_source", "nyc_acc")
    .eq("is_available", true);

  const { count: onEuthList } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("external_source", "nyc_acc")
    .eq("is_on_euthanasia_list", true)
    .eq("is_available", true);

  const { data: recentDogs } = await supabase
    .from("dogs")
    .select("name, external_id, urgency_level, intake_date, tags")
    .eq("external_source", "nyc_acc")
    .eq("is_available", true)
    .order("updated_at", { ascending: false })
    .limit(10);

  return NextResponse.json({
    source: "nyc_acc",
    totalDogs: totalNycAcc ?? 0,
    onEuthanasiaList: onEuthList ?? 0,
    recentDogs: recentDogs ?? [],
  });
}

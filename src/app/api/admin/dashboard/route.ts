import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { getVerificationStats } from "@/lib/verification/engine";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function GET() {
  const supabase = createAdminClient();

  try {
    const [
      vStats,
      totalRes,
      verifiedConfRes,
      highConfRes,
      medConfRes,
      lowConfRes,
      criticalRes,
      highUrgRes,
      medUrgRes,
      euthanasiaRes,
      dailyStatsRes,
      auditRunsRes,
      availDateRes,
      foundDateRes,
      returnedRes,
      createdDateRes,
      createdActiveRes,
      updatedDateRes,
      descParsedRes,
      ageCappedRes,
    ] = await Promise.all([
      getVerificationStats(),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "verified"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "high"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "medium"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "low"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "critical"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "high"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "medium"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).not("euthanasia_date", "is", null),
      supabase.from("daily_stats").select("*").order("stat_date", { ascending: false }).limit(30),
      supabase.from("audit_runs").select("*").order("started_at", { ascending: false }).limit(10),
      // Date source counts
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_available_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_found_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_returned_after_adoption"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_created_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_created_date_active"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_updated_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).like("date_source", "description_parsed%"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).like("date_source", "%age_capped%"),
    ]);

    const total = totalRes.count ?? 0;

    return NextResponse.json({
      dogs: {
        total,
        critical: criticalRes.count ?? 0,
        highUrgency: highUrgRes.count ?? 0,
        mediumUrgency: medUrgRes.count ?? 0,
        withEuthanasiaDate: euthanasiaRes.count ?? 0,
      },
      verification: vStats,
      dateConfidence: {
        verified: verifiedConfRes.count ?? 0,
        high: highConfRes.count ?? 0,
        medium: medConfRes.count ?? 0,
        low: lowConfRes.count ?? 0,
        accuracyPct: total > 0 ? Math.round(((verifiedConfRes.count ?? 0) + (highConfRes.count ?? 0)) / total * 1000) / 10 : 0,
      },
      dateSources: {
        rescuegroups_available_date: availDateRes.count ?? 0,
        rescuegroups_found_date: foundDateRes.count ?? 0,
        rescuegroups_returned_after_adoption: returnedRes.count ?? 0,
        rescuegroups_created_date: createdDateRes.count ?? 0,
        rescuegroups_created_date_active: createdActiveRes.count ?? 0,
        rescuegroups_updated_date: updatedDateRes.count ?? 0,
        description_parsed: descParsedRes.count ?? 0,
        age_capped: ageCappedRes.count ?? 0,
      },
      dailyTrends: dailyStatsRes.data ?? [],
      recentAudits: auditRunsRes.data ?? [],
    });
  } catch (err) {
    return NextResponse.json({ error: String(err) }, { status: 500 });
  }
}

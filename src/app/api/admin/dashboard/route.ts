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
      // New: state, shelter, and source data
      shelterDataRes,
      topSheltersRes,
      rgSourceRes,
      scraperSourceRes,
      expiredRes,
      outcomeUnknownRes,
    ] = await Promise.all([
      getVerificationStats(),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "verified"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "high"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "medium"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_confidence", "low"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "critical").gt("euthanasia_date", new Date().toISOString()),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "high").gt("euthanasia_date", new Date().toISOString()),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("urgency_level", "medium"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).not("euthanasia_date", "is", null),
      supabase.from("daily_stats").select("*").order("stat_date", { ascending: false }).limit(30),
      supabase.from("audit_runs").select("*").order("started_at", { ascending: false }).limit(20),
      // Date sources
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_available_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_found_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_returned_after_adoption"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_created_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_created_date_active"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("date_source", "rescuegroups_updated_date"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).like("date_source", "description_parsed%"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).like("date_source", "%age_capped%"),
      // State breakdown from shelters
      supabase.from("shelters").select("id, name, city, state, available_dog_count, urgent_dog_count, platform, website, last_scraped_at").gt("available_dog_count", 0),
      // Top shelters
      supabase.from("shelters").select("id, name, city, state, available_dog_count, urgent_dog_count, platform, website, last_scraped_at, org_type").gt("available_dog_count", 0).order("available_dog_count", { ascending: false }).limit(500),
      // Source breakdown
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).eq("external_source", "rescuegroups"),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).like("external_source", "scraper_%"),
      // Expired dogs (euth date passed, still available)
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).not("euthanasia_date", "is", null).lt("euthanasia_date", new Date().toISOString()),
      // Outcome unknown (deactivated by freshness/audit)
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("status", "outcome_unknown"),
    ]);

    const total = totalRes.count ?? 0;

    // Compute state breakdown from shelter data
    const stateMap = new Map<string, { total: number; urgent: number; shelters: number }>();
    for (const s of (shelterDataRes.data || [])) {
      const st = s.state || "Unknown";
      const entry = stateMap.get(st) || { total: 0, urgent: 0, shelters: 0 };
      entry.total += s.available_dog_count || 0;
      entry.urgent += s.urgent_dog_count || 0;
      entry.shelters++;
      stateMap.set(st, entry);
    }
    const stateBreakdown = Array.from(stateMap.entries())
      .map(([state, data]) => ({
        state,
        totalDogs: data.total,
        urgentDogs: data.urgent,
        shelterCount: data.shelters,
        urgentPct: data.total > 0 ? Math.round((data.urgent / data.total) * 1000) / 10 : 0,
      }))
      .sort((a, b) => b.totalDogs - a.totalDogs);

    return NextResponse.json({
      generatedAt: new Date().toISOString(),
      dogs: {
        total,
        critical: criticalRes.count ?? 0,
        highUrgency: highUrgRes.count ?? 0,
        mediumUrgency: medUrgRes.count ?? 0,
        withEuthanasiaDate: euthanasiaRes.count ?? 0,
        expiredStillAvailable: expiredRes.count ?? 0,
        outcomeUnknown: outcomeUnknownRes.count ?? 0,
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
      sourceBreakdown: {
        rescuegroups: rgSourceRes.count ?? 0,
        scraper: scraperSourceRes.count ?? 0,
        other: Math.max(0, total - (rgSourceRes.count ?? 0) - (scraperSourceRes.count ?? 0)),
      },
      stateBreakdown,
      topShelters: (topSheltersRes.data || []).map((s) => ({
        id: s.id,
        name: s.name,
        city: s.city,
        state: s.state,
        dogCount: s.available_dog_count,
        urgentCount: s.urgent_dog_count,
        platform: s.platform,
        website: s.website,
        lastScraped: s.last_scraped_at,
        orgType: s.org_type,
      })),
      dailyTrends: dailyStatsRes.data ?? [],
      recentAudits: auditRunsRes.data ?? [],
    });
  } catch (err) {
    return NextResponse.json({ error: String(err) }, { status: 500 });
  }
}

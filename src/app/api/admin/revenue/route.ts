import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

// Auth enforced by middleware — /api/admin/* requires site_auth cookie

export async function GET() {
  try {
    const supabase = createAdminClient();

    const [partnersResult, clicksResult] = await Promise.all([
      supabase
        .from("affiliate_partners")
        .select(
          "id, name, total_clicks, total_conversions, commission_rate, is_active"
        )
        .eq("is_active", true)
        .order("total_clicks", { ascending: false }),
      supabase
        .from("affiliate_clicks")
        .select("*", { count: "exact", head: true }),
    ]);

    const byPartner = (partnersResult.data || []).map((p) => ({
      name: p.name,
      clicks: p.total_clicks || 0,
      conversions: p.total_conversions || 0,
      revenue: (p.total_conversions || 0) * (p.commission_rate || 0),
    }));

    const totalConversions = byPartner.reduce((s, p) => s + p.conversions, 0);
    const totalRevenue = byPartner.reduce((s, p) => s + p.revenue, 0);

    return NextResponse.json({
      total_revenue: totalRevenue,
      total_clicks: clicksResult.count || 0,
      total_conversions: totalConversions,
      by_partner: byPartner,
    });
  } catch (err) {
    console.error("Admin revenue API error:", err);
    return NextResponse.json(
      { error: "Failed to load revenue data" },
      { status: 500 }
    );
  }
}

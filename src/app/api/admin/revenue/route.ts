import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export async function GET() {
  const supabase = createAdminClient();

  // Get affiliate partners with click/conversion stats
  const { data: partners } = await supabase
    .from("affiliate_partners")
    .select("id, name, total_clicks, total_conversions, commission_rate, is_active")
    .eq("is_active", true)
    .order("total_clicks", { ascending: false });

  // Get total clicks from affiliate_clicks table
  const { count: totalClicks } = await supabase
    .from("affiliate_clicks")
    .select("*", { count: "exact", head: true });

  // Compute summary
  const byPartner = (partners || []).map((p) => ({
    name: p.name,
    clicks: p.total_clicks || 0,
    conversions: p.total_conversions || 0,
    revenue: (p.total_conversions || 0) * (p.commission_rate || 0),
  }));

  const totalConversions = byPartner.reduce((s, p) => s + p.conversions, 0);
  const totalRevenue = byPartner.reduce((s, p) => s + p.revenue, 0);

  return NextResponse.json({
    total_revenue: totalRevenue,
    total_clicks: totalClicks || 0,
    total_conversions: totalConversions,
    by_partner: byPartner,
  });
}

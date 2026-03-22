import { createAdminClient } from "@/lib/supabase/admin";
import { createHash } from "crypto";

interface ClickEvent {
  product_id: string;
  partner_id: string;
  dog_id?: string;
  page_url: string;
  ip: string;
  user_agent: string;
  referrer?: string;
  session_id?: string;
}

/**
 * Track an affiliate link click.
 */
export async function trackClick(event: ClickEvent): Promise<string | null> {
  const supabase = createAdminClient();

  // Hash the IP for privacy
  const ipHash = createHash("sha256")
    .update(event.ip + "wtl-salt-2026")
    .digest("hex");

  const { data, error } = await supabase
    .from("affiliate_clicks")
    .insert({
      product_id: event.product_id,
      partner_id: event.partner_id,
      dog_id: event.dog_id || null,
      page_url: event.page_url,
      ip_hash: ipHash,
      user_agent: event.user_agent.slice(0, 500),
      referrer: event.referrer?.slice(0, 500) || null,
      session_id: event.session_id || null,
    })
    .select("id")
    .single();

  if (error) {
    console.error("Click tracking error:", error);
    return null;
  }

  // Increment product click count
  await supabase.rpc("increment_field", {
    table_name: "affiliate_products",
    field_name: "click_count",
    row_id: event.product_id,
  });

  // Increment partner click count
  await supabase.rpc("increment_field", {
    table_name: "affiliate_partners",
    field_name: "total_clicks",
    row_id: event.partner_id,
  });

  return data?.id || null;
}

/**
 * Get click stats for a time period.
 */
export async function getClickStats(days: number = 30): Promise<{
  total_clicks: number;
  by_partner: Record<string, number>;
  by_category: Record<string, number>;
  top_products: Array<{ name: string; clicks: number }>;
  daily_clicks: Array<{ date: string; clicks: number }>;
}> {
  const supabase = createAdminClient();
  const since = new Date(Date.now() - days * 24 * 3600 * 1000).toISOString();

  // Total clicks
  const { count: totalClicks } = await supabase
    .from("affiliate_clicks")
    .select("id", { count: "exact", head: true })
    .gte("clicked_at", since);

  // Clicks by partner
  const { data: clicksByPartner } = await supabase
    .from("affiliate_clicks")
    .select("partner_id, affiliate_partners!inner(name)")
    .gte("clicked_at", since);

  const byPartner: Record<string, number> = {};
  for (const click of clicksByPartner || []) {
    const partner = click.affiliate_partners as unknown as { name: string };
    const name = partner?.name || "Unknown";
    byPartner[name] = (byPartner[name] || 0) + 1;
  }

  // Top products
  const { data: topProducts } = await supabase
    .from("affiliate_products")
    .select("name, click_count")
    .gt("click_count", 0)
    .order("click_count", { ascending: false })
    .limit(10);

  return {
    total_clicks: totalClicks || 0,
    by_partner: byPartner,
    by_category: {},
    top_products: (topProducts || []).map((p) => ({
      name: p.name,
      clicks: p.click_count,
    })),
    daily_clicks: [],
  };
}

/**
 * Get revenue summary.
 */
export async function getRevenueSummary(): Promise<{
  total_revenue: number;
  total_clicks: number;
  total_conversions: number;
  by_partner: Array<{
    name: string;
    clicks: number;
    conversions: number;
    revenue: number;
  }>;
}> {
  const supabase = createAdminClient();

  const { data: partners } = await supabase
    .from("affiliate_partners")
    .select("name, total_clicks, total_conversions, total_revenue")
    .eq("is_active", true)
    .order("total_revenue", { ascending: false });

  let totalRevenue = 0;
  let totalClicks = 0;
  let totalConversions = 0;

  const byPartner = (partners || []).map((p) => {
    totalRevenue += Number(p.total_revenue) || 0;
    totalClicks += Number(p.total_clicks) || 0;
    totalConversions += Number(p.total_conversions) || 0;
    return {
      name: p.name,
      clicks: Number(p.total_clicks) || 0,
      conversions: Number(p.total_conversions) || 0,
      revenue: Number(p.total_revenue) || 0,
    };
  });

  return { total_revenue: totalRevenue, total_clicks: totalClicks, total_conversions: totalConversions, by_partner: byPartner };
}

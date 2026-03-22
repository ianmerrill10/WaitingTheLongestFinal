import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export async function GET() {
  const supabase = createAdminClient();

  // Fetch campaigns
  const { data: campaigns } = await supabase
    .from("outreach_campaigns")
    .select("*")
    .order("created_at", { ascending: false })
    .limit(50);

  // Compute stats
  const { count: totalShelters } = await supabase
    .from("shelters")
    .select("*", { count: "exact", head: true });

  const { count: withEmail } = await supabase
    .from("shelters")
    .select("*", { count: "exact", head: true })
    .not("email", "is", null)
    .neq("email", "");

  const activeCampaigns = (campaigns || []).filter(
    (c) => c.status === "active"
  ).length;

  const totalSentAlltime = (campaigns || []).reduce(
    (sum, c) => sum + (c.total_sent || 0),
    0
  );

  return NextResponse.json({
    campaigns: campaigns || [],
    stats: {
      total_shelters: totalShelters || 0,
      with_email: withEmail || 0,
      campaigns_active: activeCampaigns,
      total_sent_alltime: totalSentAlltime,
    },
  });
}

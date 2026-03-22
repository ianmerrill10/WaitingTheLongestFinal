import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

// Auth enforced by middleware — /api/admin/* requires site_auth cookie

export async function GET() {
  try {
    const supabase = createAdminClient();

    const [campaignsResult, shelterCountResult, emailCountResult] =
      await Promise.all([
        supabase
          .from("outreach_campaigns")
          .select("*")
          .order("created_at", { ascending: false })
          .limit(50),
        supabase
          .from("shelters")
          .select("*", { count: "exact", head: true }),
        supabase
          .from("shelters")
          .select("*", { count: "exact", head: true })
          .not("email", "is", null)
          .neq("email", ""),
      ]);

    const campaigns = campaignsResult.data || [];
    const activeCampaigns = campaigns.filter(
      (c) => c.status === "active"
    ).length;
    const totalSentAlltime = campaigns.reduce(
      (sum, c) => sum + (c.total_sent || 0),
      0
    );

    return NextResponse.json({
      campaigns,
      stats: {
        total_shelters: shelterCountResult.count || 0,
        with_email: emailCountResult.count || 0,
        campaigns_active: activeCampaigns,
        total_sent_alltime: totalSentAlltime,
      },
    });
  } catch (err) {
    console.error("Admin campaigns API error:", err);
    return NextResponse.json(
      { error: "Failed to load campaigns" },
      { status: 500 }
    );
  }
}

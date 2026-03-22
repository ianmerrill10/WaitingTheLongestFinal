import { NextRequest, NextResponse } from "next/server";
import { processOutreachQueue, createCampaign } from "@/lib/outreach/agent";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/outreach/process — Process outreach queue for a campaign
 */
export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { action } = body;

    if (action === "create_campaign") {
      const result = await createCampaign({
        name: body.name || `Campaign ${new Date().toLocaleDateString()}`,
        campaign_type: body.campaign_type || "initial_outreach",
        state_filter: body.state,
        min_dogs: body.min_dogs,
        max_targets: body.max_targets || 500,
      });

      if (!result) {
        return NextResponse.json(
          { error: "Failed to create campaign" },
          { status: 500 }
        );
      }

      return NextResponse.json({
        success: true,
        campaign_id: result.campaign_id,
        targets_added: result.targets_added,
      });
    }

    if (action === "process") {
      const campaignId = body.campaign_id;
      if (!campaignId) {
        return NextResponse.json(
          { error: "Missing campaign_id" },
          { status: 400 }
        );
      }

      const result = await processOutreachQueue(
        campaignId,
        body.batch_size || 20
      );

      return NextResponse.json({
        success: true,
        ...result,
      });
    }

    return NextResponse.json(
      { error: "Invalid action. Use 'create_campaign' or 'process'" },
      { status: 400 }
    );
  } catch (err) {
    console.error("Outreach process error:", err);
    return NextResponse.json(
      { error: "Failed to process outreach" },
      { status: 500 }
    );
  }
}

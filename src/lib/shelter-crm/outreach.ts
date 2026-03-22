import { createAdminClient } from "@/lib/supabase/admin";
import type {
  OutreachCampaign,
  OutreachTarget,
  CampaignStats,
  CampaignStatus,
} from "@/types/shelter-crm";
import { logCommunication } from "./communications";

const supabase = createAdminClient();

/**
 * Create a new outreach campaign.
 */
export async function createCampaign(input: {
  name: string;
  description?: string;
  target_criteria: OutreachCampaign["target_criteria"];
  email_template_id?: string;
  send_rate_per_hour?: number;
  scheduled_start?: string;
}): Promise<OutreachCampaign> {
  const { data, error } = await supabase
    .from("outreach_campaigns")
    .insert({
      name: input.name,
      description: input.description || null,
      target_criteria: input.target_criteria,
      email_template_id: input.email_template_id || null,
      send_rate_per_hour: input.send_rate_per_hour || 50,
      scheduled_start: input.scheduled_start || null,
      status: input.scheduled_start ? "scheduled" : "draft",
    })
    .select()
    .single();

  if (error)
    throw new Error(`Failed to create campaign: ${error.message}`);
  return data as OutreachCampaign;
}

/**
 * Add targets to a campaign based on criteria.
 * Queries shelters matching criteria and creates outreach_target records.
 */
export async function addTargets(
  campaignId: string,
  criteria: OutreachCampaign["target_criteria"]
): Promise<number> {
  // Build shelter query from criteria
  let query = supabase
    .from("shelters")
    .select("id, email, name, state_code")
    .eq("is_active", true)
    .not("email", "is", null);

  if (criteria.state_codes?.length) {
    query = query.in("state_code", criteria.state_codes);
  }
  if (criteria.is_kill_shelter !== undefined) {
    query = query.eq("is_kill_shelter", criteria.is_kill_shelter);
  }
  if (criteria.shelter_types?.length) {
    query = query.in("shelter_type", criteria.shelter_types);
  }

  const { data: shelters, error } = await query;
  if (error)
    throw new Error(`Failed to query shelters: ${error.message}`);

  if (!shelters?.length) return 0;

  // Filter out opted-out contacts if requested
  let shelterIds = shelters.map((s) => s.id);

  if (criteria.exclude_opted_out) {
    const { data: optedOut } = await supabase
      .from("shelter_contacts")
      .select("shelter_id")
      .in("shelter_id", shelterIds)
      .eq("is_opted_out", true);

    const optedOutIds = new Set(
      (optedOut || []).map((c) => c.shelter_id)
    );
    shelterIds = shelterIds.filter((id) => !optedOutIds.has(id));
  }

  // Check for existing targets in this campaign to avoid duplicates
  const { data: existingTargets } = await supabase
    .from("outreach_targets")
    .select("shelter_id")
    .eq("campaign_id", campaignId);

  const existingIds = new Set(
    (existingTargets || []).map((t) => t.shelter_id)
  );

  // Create target records
  const newTargets = shelters
    .filter(
      (s) => shelterIds.includes(s.id) && !existingIds.has(s.id) && s.email
    )
    .map((s) => ({
      campaign_id: campaignId,
      shelter_id: s.id,
      email: s.email!,
      status: "pending" as const,
    }));

  if (newTargets.length === 0) return 0;

  // Insert in batches of 500
  let inserted = 0;
  for (let i = 0; i < newTargets.length; i += 500) {
    const batch = newTargets.slice(i, i + 500);
    const { error: insertError } = await supabase
      .from("outreach_targets")
      .insert(batch);

    if (!insertError) inserted += batch.length;
  }

  // Update campaign total_targets
  await supabase
    .from("outreach_campaigns")
    .update({ total_targets: inserted })
    .eq("id", campaignId);

  return inserted;
}

/**
 * Process the outreach queue: send emails to pending targets.
 */
export async function processQueue(
  campaignId: string,
  batchSize = 50
): Promise<{ processed: number; failed: number }> {
  // Get campaign
  const { data: campaign } = await supabase
    .from("outreach_campaigns")
    .select("*")
    .eq("id", campaignId)
    .single();

  if (!campaign || campaign.status !== "running") {
    return { processed: 0, failed: 0 };
  }

  // Get pending targets
  const { data: targets } = await supabase
    .from("outreach_targets")
    .select("*")
    .eq("campaign_id", campaignId)
    .eq("status", "pending")
    .order("created_at", { ascending: true })
    .limit(batchSize);

  if (!targets?.length) {
    // No more targets — mark campaign as completed
    await supabase
      .from("outreach_campaigns")
      .update({
        status: "completed",
        completed_at: new Date().toISOString(),
      })
      .eq("id", campaignId);

    return { processed: 0, failed: 0 };
  }

  let processed = 0;
  let failed = 0;

  for (const target of targets) {
    try {
      // Queue the email (actual sending handled by email-sender)
      await supabase
        .from("outreach_targets")
        .update({
          status: "queued",
          sent_at: new Date().toISOString(),
        })
        .eq("id", target.id);

      // Log communication
      if (target.shelter_id) {
        await logCommunication({
          shelter_id: target.shelter_id,
          comm_type: "ai_outreach",
          direction: "outbound",
          subject: `Outreach: ${campaign.name}`,
          body: `Automated outreach email sent to ${target.email}`,
          status: "queued",
          sent_by: "ai_agent",
          channel: "sendgrid",
          related_entity_type: "outreach_campaign",
          related_entity_id: campaignId,
        });
      }

      processed++;
    } catch {
      failed++;
      await supabase
        .from("outreach_targets")
        .update({ status: "skipped", notes: "Failed to queue" })
        .eq("id", target.id);
    }
  }

  // Update campaign counters
  await supabase
    .from("outreach_campaigns")
    .update({
      sent_count: (campaign.sent_count || 0) + processed,
    })
    .eq("id", campaignId);

  return { processed, failed };
}

/**
 * Track a response to an outreach email.
 */
export async function trackResponse(
  targetId: string,
  responseType: "opened" | "clicked" | "replied" | "bounced" | "opted_out"
): Promise<void> {
  const now = new Date().toISOString();
  const update: Record<string, unknown> = {};

  switch (responseType) {
    case "opened":
      update.status = "opened";
      update.opened_at = now;
      break;
    case "clicked":
      update.status = "clicked";
      update.clicked_at = now;
      break;
    case "replied":
      update.status = "replied";
      update.replied_at = now;
      break;
    case "bounced":
      update.status = "bounced";
      break;
    case "opted_out":
      update.status = "opted_out";
      break;
  }

  await supabase
    .from("outreach_targets")
    .update(update)
    .eq("id", targetId);

  // Update campaign counters
  const { data: target } = await supabase
    .from("outreach_targets")
    .select("campaign_id")
    .eq("id", targetId)
    .single();

  if (target) {
    const counterField = `${responseType}_count`;
    const { data: campaign } = await supabase
      .from("outreach_campaigns")
      .select("*")
      .eq("id", target.campaign_id)
      .single();

    if (campaign) {
      const current = (campaign as unknown as Record<string, number>)[counterField] || 0;
      await supabase
        .from("outreach_campaigns")
        .update({
          [counterField]: current + 1,
        })
        .eq("id", target.campaign_id);
    }
  }
}

/**
 * Update campaign status.
 */
export async function updateCampaignStatus(
  campaignId: string,
  status: CampaignStatus
): Promise<void> {
  const update: Record<string, unknown> = { status };

  if (status === "running") update.started_at = new Date().toISOString();
  if (status === "completed") update.completed_at = new Date().toISOString();

  await supabase
    .from("outreach_campaigns")
    .update(update)
    .eq("id", campaignId);
}

/**
 * Get campaign statistics.
 */
export async function getCampaignStats(
  campaignId: string
): Promise<CampaignStats> {
  const { data } = await supabase
    .from("outreach_campaigns")
    .select(
      "total_targets, sent_count, opened_count, replied_count, converted_count, bounced_count"
    )
    .eq("id", campaignId)
    .single();

  if (!data) throw new Error("Campaign not found");

  const sent = data.sent_count || 1; // Avoid division by zero
  return {
    total_targets: data.total_targets,
    sent_count: data.sent_count,
    opened_count: data.opened_count,
    replied_count: data.replied_count,
    converted_count: data.converted_count,
    bounced_count: data.bounced_count,
    open_rate: (data.opened_count / sent) * 100,
    reply_rate: (data.replied_count / sent) * 100,
    conversion_rate: (data.converted_count / sent) * 100,
    bounce_rate: (data.bounced_count / sent) * 100,
  };
}

/**
 * Get all campaigns with optional status filter.
 */
export async function getCampaigns(
  status?: CampaignStatus
): Promise<OutreachCampaign[]> {
  let query = supabase
    .from("outreach_campaigns")
    .select("*")
    .order("created_at", { ascending: false });

  if (status) query = query.eq("status", status);

  const { data, error } = await query;
  if (error) throw new Error(`Failed to get campaigns: ${error.message}`);
  return (data || []) as OutreachCampaign[];
}

/**
 * Get targets for a campaign with optional status filter.
 */
export async function getCampaignTargets(
  campaignId: string,
  options: { status?: string; limit?: number; offset?: number } = {}
): Promise<{ targets: OutreachTarget[]; total: number }> {
  const { status, limit = 50, offset = 0 } = options;

  let query = supabase
    .from("outreach_targets")
    .select("*", { count: "exact" })
    .eq("campaign_id", campaignId)
    .order("created_at", { ascending: true })
    .range(offset, offset + limit - 1);

  if (status) query = query.eq("status", status);

  const { data, error, count } = await query;
  if (error) throw new Error(`Failed to get targets: ${error.message}`);

  return {
    targets: (data || []) as OutreachTarget[],
    total: count || 0,
  };
}

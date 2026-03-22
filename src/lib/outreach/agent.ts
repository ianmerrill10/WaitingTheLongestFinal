import { createAdminClient } from "@/lib/supabase/admin";
import { sendEmail, canSendEmail, recordEmailSent, getUnsubscribeUrl, isValidOutreachEmail } from "./email-sender";
import { getTemplate, renderTemplate } from "./email-templates";
import { computePriorityScore, scoreToPriority } from "./scorer";

interface ProcessResult {
  processed: number;
  sent: number;
  skipped: number;
  errors: number;
  details: string[];
}

/**
 * Process outreach campaign queue.
 * Sends pending emails, respects rate limits.
 */
export async function processOutreachQueue(
  campaignId: string,
  batchSize: number = 20
): Promise<ProcessResult> {
  const supabase = createAdminClient();
  const result: ProcessResult = {
    processed: 0,
    sent: 0,
    skipped: 0,
    errors: 0,
    details: [],
  };

  // Get campaign info
  const { data: campaign } = await supabase
    .from("outreach_campaigns")
    .select("*")
    .eq("id", campaignId)
    .single();

  if (!campaign) {
    result.details.push("Campaign not found");
    return result;
  }

  if (campaign.status !== "active") {
    result.details.push(`Campaign status is ${campaign.status}, not active`);
    return result;
  }

  // Get pending targets
  const { data: targets } = await supabase
    .from("outreach_targets")
    .select("*, shelters!inner(name, email, city, state, total_dogs, shelter_type)")
    .eq("campaign_id", campaignId)
    .eq("status", "pending")
    .order("priority_score", { ascending: false })
    .limit(batchSize);

  if (!targets || targets.length === 0) {
    result.details.push("No pending targets in queue");
    return result;
  }

  for (const target of targets) {
    result.processed++;

    // Check rate limit
    if (!canSendEmail()) {
      result.details.push("Rate limit reached (50/hour). Stopping.");
      break;
    }

    const shelter = target.shelters as unknown as {
      name: string;
      email: string;
      city: string;
      state: string;
      total_dogs: number;
      shelter_type: string;
    };

    const email = target.contact_email || shelter.email;
    if (!email || !isValidOutreachEmail(email)) {
      result.skipped++;
      await supabase
        .from("outreach_targets")
        .update({ status: "skipped", notes: "Invalid/missing email" })
        .eq("id", target.id);
      continue;
    }

    // Determine which template to use based on send count
    const sendCount = target.send_count || 0;
    const templateMap: Record<number, string> = {
      0: "initial_outreach",
      1: "follow_up_1",
      2: "follow_up_2",
      3: "follow_up_3",
    };

    const templateId = templateMap[sendCount];
    if (!templateId) {
      result.skipped++;
      await supabase
        .from("outreach_targets")
        .update({ status: "completed", notes: "All follow-ups sent" })
        .eq("id", target.id);
      continue;
    }

    const template = getTemplate(templateId);
    if (!template) {
      result.errors++;
      result.details.push(`Template ${templateId} not found`);
      continue;
    }

    // Render template with shelter data
    const contactName = target.contact_name || "Shelter Team";
    const firstName = contactName.split(" ")[0] || "there";

    // Pull actual view counts for this shelter's dogs
    const { data: viewData } = await supabase
      .from("dogs")
      .select("view_count")
      .eq("shelter_id", target.shelter_id);
    const totalViews = (viewData || []).reduce((s, d) => s + (d.view_count || 0), 0);

    const rendered = renderTemplate(template, {
      shelter_name: shelter.name,
      contact_name: contactName,
      contact_first_name: firstName,
      city: shelter.city || "",
      state: shelter.state || "",
      dog_count: String(shelter.total_dogs || 0),
      shelter_url: `https://waitingthelongest.com/shelters/${target.shelter_id}`,
      register_url: "https://waitingthelongest.com/partners/register",
      unsubscribe_url: getUnsubscribeUrl(target.id),
      view_count: String(totalViews),
    });

    // Send the email
    const sendResult = await sendEmail({
      to: email,
      ...rendered,
      categories: ["outreach", campaign.campaign_type || "general", templateId],
      customArgs: {
        campaign_id: campaignId,
        target_id: target.id,
        shelter_id: target.shelter_id,
      },
    });

    if (sendResult.success) {
      result.sent++;
      recordEmailSent();

      // Update target
      await supabase
        .from("outreach_targets")
        .update({
          status: sendCount === 0 ? "contacted" : "follow_up_sent",
          send_count: sendCount + 1,
          last_sent_at: new Date().toISOString(),
          last_sent_template: templateId,
        })
        .eq("id", target.id);

      // Log communication
      await supabase.from("shelter_communications").insert({
        shelter_id: target.shelter_id,
        comm_type: "email",
        direction: "outbound",
        subject: rendered.subject,
        to_address: email,
        sent_by: "ai_outreach_agent",
        status: "sent",
        metadata: {
          campaign_id: campaignId,
          template_id: templateId,
          message_id: sendResult.messageId,
        },
      });
    } else {
      result.errors++;
      result.details.push(
        `Failed to send to ${shelter.name}: ${sendResult.error}`
      );

      await supabase
        .from("outreach_targets")
        .update({ status: "error", notes: sendResult.error })
        .eq("id", target.id);
    }
  }

  // Update campaign stats
  await supabase
    .from("outreach_campaigns")
    .update({
      total_sent: campaign.total_sent + result.sent,
      updated_at: new Date().toISOString(),
    })
    .eq("id", campaignId);

  return result;
}

/**
 * Create a new outreach campaign with auto-populated targets.
 */
export async function createCampaign(params: {
  name: string;
  campaign_type: string;
  state_filter?: string;
  min_dogs?: number;
  max_targets?: number;
}): Promise<{ campaign_id: string; targets_added: number } | null> {
  const supabase = createAdminClient();

  // Create campaign
  const { data: campaign, error: campError } = await supabase
    .from("outreach_campaigns")
    .insert({
      name: params.name,
      campaign_type: params.campaign_type,
      status: "draft",
    })
    .select("id")
    .single();

  if (campError || !campaign) {
    console.error("Failed to create campaign:", campError);
    return null;
  }

  // Find target shelters
  let shelterQuery = supabase
    .from("shelters")
    .select("id, name, email, city, state, total_dogs, shelter_type")
    .not("email", "is", null)
    .or("partner_status.is.null,partner_status.eq.none");

  if (params.state_filter) {
    shelterQuery = shelterQuery.eq("state", params.state_filter);
  }
  if (params.min_dogs) {
    shelterQuery = shelterQuery.gte("total_dogs", params.min_dogs);
  }

  shelterQuery = shelterQuery
    .order("total_dogs", { ascending: false })
    .limit(params.max_targets || 500);

  const { data: shelters } = await shelterQuery;
  if (!shelters || shelters.length === 0) {
    return { campaign_id: campaign.id, targets_added: 0 };
  }

  // Exclude already-targeted shelters
  const shelterIds = shelters.map((s) => s.id);
  const { data: existing } = await supabase
    .from("outreach_targets")
    .select("shelter_id")
    .in("shelter_id", shelterIds)
    .not("status", "in", "(bounced,error)");

  const existingSet = new Set((existing || []).map((e) => e.shelter_id));

  // Build target inserts
  const targets = shelters
    .filter((s) => !existingSet.has(s.id) && s.email)
    .map((s) => {
      const scoring = computePriorityScore({
        shelter_type: s.shelter_type,
        total_dogs: s.total_dogs,
        email: s.email,
        state: s.state,
      });

      return {
        campaign_id: campaign.id,
        shelter_id: s.id,
        contact_email: s.email,
        contact_name: null,
        status: "pending",
        priority_score: scoring.score,
        priority: scoreToPriority(scoring.score),
      };
    });

  // Batch insert
  if (targets.length > 0) {
    const batchSize = 500;
    for (let i = 0; i < targets.length; i += batchSize) {
      await supabase
        .from("outreach_targets")
        .insert(targets.slice(i, i + batchSize));
    }
  }

  // Update campaign with target count
  await supabase
    .from("outreach_campaigns")
    .update({ total_targets: targets.length })
    .eq("id", campaign.id);

  return { campaign_id: campaign.id, targets_added: targets.length };
}

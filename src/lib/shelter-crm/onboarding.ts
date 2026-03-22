import { createAdminClient } from "@/lib/supabase/admin";
import type {
  ShelterOnboarding,
  SubmitOnboardingInput,
  OnboardingPipelineStats,
} from "@/types/shelter-crm";
import { logCommunication } from "./communications";
import { createContact } from "./contacts";

const supabase = createAdminClient();

/**
 * Submit a new shelter registration application.
 * Creates onboarding record, contact record, and logs the submission.
 */
export async function submitApplication(
  input: SubmitOnboardingInput
): Promise<ShelterOnboarding> {
  // Check for duplicate by email
  const { data: existing } = await supabase
    .from("shelter_onboarding")
    .select("id, status")
    .eq("contact_email", input.contact_email)
    .not("status", "in", "('rejected','withdrawn')")
    .limit(1)
    .single();

  if (existing) {
    throw new Error(
      "An application with this email already exists. Please check your email for next steps."
    );
  }

  // Try to match against existing shelter by name + state
  const { data: matchedShelter } = await supabase
    .from("shelters")
    .select("id")
    .ilike("name", input.org_name)
    .eq("state_code", input.state_code)
    .limit(1)
    .single();

  // Compute priority score
  let score = 0;
  if (input.estimated_dog_count && input.estimated_dog_count > 50) score += 20;
  if (input.website) score += 10;
  if (input.data_feed_url) score += 15;
  // Kill shelter detection would be +50, but we can't know from form input alone

  const { data, error } = await supabase
    .from("shelter_onboarding")
    .insert({
      shelter_id: matchedShelter?.id || null,
      org_name: input.org_name,
      org_type: input.org_type,
      ein: input.ein || null,
      website: input.website || null,
      contact_first_name: input.contact_first_name,
      contact_last_name: input.contact_last_name,
      contact_email: input.contact_email,
      contact_phone: input.contact_phone || null,
      contact_title: input.contact_title || null,
      address: input.address || null,
      city: input.city,
      state_code: input.state_code,
      zip_code: input.zip_code,
      estimated_dog_count: input.estimated_dog_count || null,
      current_software: input.current_software || null,
      data_feed_url: input.data_feed_url || null,
      social_facebook: input.social_facebook || null,
      social_instagram: input.social_instagram || null,
      social_tiktok: input.social_tiktok || null,
      integration_preference: input.integration_preference || "api",
      status: "submitted",
      stage: "review",
      priority: score >= 30 ? "high" : "normal",
      score,
      source: "self_registration",
      referral_source: input.referral_source || null,
    })
    .select()
    .single();

  if (error)
    throw new Error(`Failed to submit application: ${error.message}`);

  const onboarding = data as ShelterOnboarding;

  // Create contact record if we matched a shelter
  if (matchedShelter?.id) {
    try {
      await createContact({
        shelter_id: matchedShelter.id,
        first_name: input.contact_first_name,
        last_name: input.contact_last_name,
        email: input.contact_email,
        phone: input.contact_phone,
        title: input.contact_title,
        is_primary: true,
      });
    } catch {
      // Contact may already exist — not fatal
    }

    // Log the submission as a communication
    await logCommunication({
      shelter_id: matchedShelter.id,
      comm_type: "form_submission",
      direction: "inbound",
      subject: "Partner Registration Application",
      body: `${input.contact_first_name} ${input.contact_last_name} submitted a partner registration for ${input.org_name}.`,
      status: "received",
      sent_by: input.contact_email,
      related_entity_type: "onboarding",
      related_entity_id: onboarding.id,
    });

    // Update shelter partner status
    await supabase
      .from("shelters")
      .update({ partner_status: "applied" })
      .eq("id", matchedShelter.id);
  }

  return onboarding;
}

export async function getOnboardingById(
  id: string
): Promise<ShelterOnboarding | null> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .select("*")
    .eq("id", id)
    .single();

  if (error) return null;
  return data as ShelterOnboarding;
}

export async function reviewApplication(
  id: string,
  assignedTo: string
): Promise<ShelterOnboarding> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .update({
      status: "under_review",
      assigned_to: assignedTo,
      reviewed_at: new Date().toISOString(),
    })
    .eq("id", id)
    .select()
    .single();

  if (error)
    throw new Error(`Failed to review application: ${error.message}`);
  return data as ShelterOnboarding;
}

export async function approveApplication(
  id: string
): Promise<ShelterOnboarding> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .update({
      status: "pending_integration",
      stage: "integration",
      approved_at: new Date().toISOString(),
    })
    .eq("id", id)
    .select()
    .single();

  if (error)
    throw new Error(`Failed to approve application: ${error.message}`);

  const onboarding = data as ShelterOnboarding;

  // Update shelter partner status if linked
  if (onboarding.shelter_id) {
    await supabase
      .from("shelters")
      .update({
        partner_status: "onboarding",
        partner_since: new Date().toISOString(),
      })
      .eq("id", onboarding.shelter_id);
  }

  return onboarding;
}

export async function rejectApplication(
  id: string,
  reason: string
): Promise<ShelterOnboarding> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .update({
      status: "rejected",
      stage: "closed",
      rejection_reason: reason,
      rejected_at: new Date().toISOString(),
    })
    .eq("id", id)
    .select()
    .single();

  if (error)
    throw new Error(`Failed to reject application: ${error.message}`);
  return data as ShelterOnboarding;
}

export async function activatePartner(id: string): Promise<ShelterOnboarding> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .update({
      status: "active",
      stage: "active",
      activated_at: new Date().toISOString(),
    })
    .eq("id", id)
    .select()
    .single();

  if (error)
    throw new Error(`Failed to activate partner: ${error.message}`);

  const onboarding = data as ShelterOnboarding;

  if (onboarding.shelter_id) {
    await supabase
      .from("shelters")
      .update({ partner_status: "active" })
      .eq("id", onboarding.shelter_id);
  }

  return onboarding;
}

export async function getOnboardingPipeline(): Promise<OnboardingPipelineStats> {
  const { data, error } = await supabase
    .from("shelter_onboarding")
    .select("status");

  if (error)
    throw new Error(`Failed to get pipeline: ${error.message}`);

  const stats: OnboardingPipelineStats = {
    submitted: 0,
    under_review: 0,
    outreach_sent: 0,
    outreach_responded: 0,
    pending_agreement: 0,
    pending_integration: 0,
    testing: 0,
    active: 0,
    rejected: 0,
    total: 0,
  };

  for (const row of data || []) {
    stats.total++;
    const status = row.status as keyof OnboardingPipelineStats;
    if (typeof stats[status] === "number") {
      (stats[status] as number)++;
    }
  }

  return stats;
}

export async function getOnboardingList(options: {
  status?: string;
  stage?: string;
  state_code?: string;
  limit?: number;
  offset?: number;
} = {}): Promise<{ applications: ShelterOnboarding[]; total: number }> {
  const { status, stage, state_code, limit = 50, offset = 0 } = options;

  let query = supabase
    .from("shelter_onboarding")
    .select("*", { count: "exact" })
    .order("score", { ascending: false })
    .order("created_at", { ascending: false })
    .range(offset, offset + limit - 1);

  if (status) query = query.eq("status", status);
  if (stage) query = query.eq("stage", stage);
  if (state_code) query = query.eq("state_code", state_code);

  const { data, error, count } = await query;
  if (error) throw new Error(`Failed to get applications: ${error.message}`);

  return {
    applications: (data || []) as ShelterOnboarding[],
    total: count || 0,
  };
}

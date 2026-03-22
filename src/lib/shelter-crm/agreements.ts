import { createAdminClient } from "@/lib/supabase/admin";
import { createHash } from "crypto";
import type {
  ShelterAgreement,
  CreateAgreementInput,
  AgreementDocumentType,
} from "@/types/shelter-crm";

const supabase = createAdminClient();

/** Current versions of each legal document */
export const CURRENT_DOCUMENT_VERSIONS: Record<AgreementDocumentType, string> = {
  terms_of_service: "1.0",
  data_sharing_agreement: "1.0",
  privacy_policy: "1.0",
  api_usage_agreement: "1.0",
  shelter_listing_agreement: "1.0",
};

/** URLs for each legal document */
export const DOCUMENT_URLS: Record<AgreementDocumentType, string> = {
  terms_of_service: "/legal/shelter-terms",
  data_sharing_agreement: "/legal/data-sharing",
  privacy_policy: "/legal/privacy-policy",
  api_usage_agreement: "/legal/api-terms",
  shelter_listing_agreement: "/legal/shelter-terms",
};

/**
 * Record acceptance of a legal agreement.
 */
export async function createAgreement(
  input: CreateAgreementInput
): Promise<ShelterAgreement> {
  // Supersede any existing current agreement of same type
  const { data: existing } = await supabase
    .from("shelter_agreements")
    .select("id")
    .eq("shelter_id", input.shelter_id)
    .eq("document_type", input.document_type)
    .eq("is_current", true)
    .single();

  // Create signature hash from acceptance payload
  const signaturePayload = JSON.stringify({
    shelter_id: input.shelter_id,
    document_type: input.document_type,
    document_version: input.document_version,
    accepted_by_name: input.accepted_by_name,
    accepted_by_email: input.accepted_by_email,
    accepted_at: new Date().toISOString(),
  });
  const signatureHash = createHash("sha256")
    .update(signaturePayload)
    .digest("hex");

  const { data, error } = await supabase
    .from("shelter_agreements")
    .insert({
      shelter_id: input.shelter_id,
      contact_id: input.contact_id || null,
      document_type: input.document_type,
      document_version: input.document_version,
      document_url: input.document_url || DOCUMENT_URLS[input.document_type],
      document_hash: input.document_hash || null,
      accepted_by_name: input.accepted_by_name,
      accepted_by_email: input.accepted_by_email,
      accepted_by_title: input.accepted_by_title || null,
      accepted_by_ip: input.accepted_by_ip || null,
      accepted_by_user_agent: input.accepted_by_user_agent || null,
      is_current: true,
      signature_hash: signatureHash,
    })
    .select()
    .single();

  if (error)
    throw new Error(`Failed to create agreement: ${error.message}`);

  const agreement = data as ShelterAgreement;

  // Mark old agreement as superseded
  if (existing) {
    await supabase
      .from("shelter_agreements")
      .update({
        is_current: false,
        superseded_by: agreement.id,
      })
      .eq("id", existing.id);
  }

  return agreement;
}

/**
 * Check if a shelter has accepted all required agreements at current versions.
 */
export async function checkAgreementStatus(
  shelterId: string
): Promise<{
  complete: boolean;
  missing: AgreementDocumentType[];
  outdated: AgreementDocumentType[];
  agreements: ShelterAgreement[];
}> {
  const { data, error } = await supabase
    .from("shelter_agreements")
    .select("*")
    .eq("shelter_id", shelterId)
    .eq("is_current", true);

  if (error)
    throw new Error(`Failed to check agreements: ${error.message}`);

  const agreements = (data || []) as ShelterAgreement[];
  const requiredTypes: AgreementDocumentType[] = [
    "terms_of_service",
    "data_sharing_agreement",
    "privacy_policy",
  ];

  const missing: AgreementDocumentType[] = [];
  const outdated: AgreementDocumentType[] = [];

  for (const docType of requiredTypes) {
    const agreement = agreements.find((a) => a.document_type === docType);
    if (!agreement) {
      missing.push(docType);
    } else if (
      agreement.document_version !== CURRENT_DOCUMENT_VERSIONS[docType]
    ) {
      outdated.push(docType);
    }
  }

  return {
    complete: missing.length === 0 && outdated.length === 0,
    missing,
    outdated,
    agreements,
  };
}

/**
 * Get all required agreements and their current acceptance status.
 */
export async function getRequiredAgreements(
  shelterId: string
): Promise<
  Array<{
    document_type: AgreementDocumentType;
    required_version: string;
    url: string;
    accepted: boolean;
    accepted_version: string | null;
    accepted_at: string | null;
  }>
> {
  const { agreements } = await checkAgreementStatus(shelterId);
  const requiredTypes: AgreementDocumentType[] = [
    "terms_of_service",
    "data_sharing_agreement",
    "privacy_policy",
    "api_usage_agreement",
  ];

  return requiredTypes.map((docType) => {
    const agreement = agreements.find((a) => a.document_type === docType);
    return {
      document_type: docType,
      required_version: CURRENT_DOCUMENT_VERSIONS[docType],
      url: DOCUMENT_URLS[docType],
      accepted: !!agreement &&
        agreement.document_version === CURRENT_DOCUMENT_VERSIONS[docType],
      accepted_version: agreement?.document_version || null,
      accepted_at: agreement?.accepted_at || null,
    };
  });
}

/**
 * Get agreements by shelter.
 */
export async function getAgreementsByShelter(
  shelterId: string,
  currentOnly = true
): Promise<ShelterAgreement[]> {
  let query = supabase
    .from("shelter_agreements")
    .select("*")
    .eq("shelter_id", shelterId)
    .order("accepted_at", { ascending: false });

  if (currentOnly) {
    query = query.eq("is_current", true);
  }

  const { data, error } = await query;
  if (error)
    throw new Error(`Failed to get agreements: ${error.message}`);
  return (data || []) as ShelterAgreement[];
}

/**
 * Revoke an agreement (e.g., partner requests termination).
 */
export async function revokeAgreement(
  agreementId: string,
  reason: string
): Promise<void> {
  await supabase
    .from("shelter_agreements")
    .update({
      is_current: false,
      revoked_at: new Date().toISOString(),
      revoke_reason: reason,
    })
    .eq("id", agreementId);
}

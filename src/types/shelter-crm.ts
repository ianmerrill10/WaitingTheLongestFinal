// TypeScript interfaces for the Shelter CRM & Enterprise Platform
// Maps to migration 011: 10 new tables + extended shelters columns

// =====================================================
// SHELTER EXTENSION (added to existing Shelter interface)
// =====================================================

export type PartnerStatus = 'unregistered' | 'applied' | 'onboarding' | 'active' | 'suspended' | 'churned';
export type PartnerTier = 'free' | 'basic' | 'premium';
export type IntegrationType = 'none' | 'rescuegroups' | 'api_direct' | 'webhook' | 'feed_url' | 'csv_upload' | 'manual';

export interface ShelterPartnerFields {
  partner_status: PartnerStatus;
  partner_tier: PartnerTier;
  partner_since: string | null;
  primary_contact_id: string | null;
  integration_type: IntegrationType;
  feed_url: string | null;
  last_feed_sync_at: string | null;
  social_facebook: string | null;
  social_instagram: string | null;
  social_tiktok: string | null;
  social_youtube: string | null;
  response_time_hours: number | null;
  adoption_success_rate: number | null;
  total_communications: number;
}

// =====================================================
// TABLE 1: shelter_contacts
// =====================================================

export type ContactRole = 'director' | 'manager' | 'intake' | 'medical' | 'volunteer_coord' | 'it_admin' | 'foster_coord' | 'general' | 'board_member' | 'social_media';
export type PreferredContact = 'email' | 'phone' | 'text';

export interface ShelterContact {
  id: string;
  shelter_id: string;
  first_name: string;
  last_name: string;
  title: string | null;
  role: ContactRole;
  email: string;
  phone: string | null;
  phone_ext: string | null;
  preferred_contact: PreferredContact;
  contact_hours: string | null;
  timezone: string | null;
  notes: string | null;
  is_primary: boolean;
  is_active: boolean;
  is_opted_out: boolean;
  opted_out_at: string | null;
  last_contacted_at: string | null;
  last_responded_at: string | null;
  total_communications: number;
  created_at: string;
  updated_at: string;
}

export interface CreateShelterContactInput {
  shelter_id: string;
  first_name: string;
  last_name: string;
  title?: string;
  role?: ContactRole;
  email: string;
  phone?: string;
  phone_ext?: string;
  preferred_contact?: PreferredContact;
  contact_hours?: string;
  timezone?: string;
  notes?: string;
  is_primary?: boolean;
}

export interface UpdateShelterContactInput {
  first_name?: string;
  last_name?: string;
  title?: string;
  role?: ContactRole;
  email?: string;
  phone?: string;
  phone_ext?: string;
  preferred_contact?: PreferredContact;
  contact_hours?: string;
  timezone?: string;
  notes?: string;
  is_primary?: boolean;
  is_active?: boolean;
  is_opted_out?: boolean;
}

// =====================================================
// TABLE 2: shelter_communications
// =====================================================

export type CommType = 'email' | 'phone_call' | 'text_message' | 'in_app_message' | 'form_submission' | 'webhook_notification' | 'status_change' | 'note' | 'ai_outreach' | 'social_media_dm';
export type CommDirection = 'inbound' | 'outbound' | 'internal';
export type CommStatus = 'draft' | 'queued' | 'sent' | 'delivered' | 'opened' | 'clicked' | 'replied' | 'bounced' | 'failed' | 'received' | 'read' | 'archived';
export type CommPriority = 'low' | 'normal' | 'high' | 'urgent';

export interface ShelterCommunication {
  id: string;
  shelter_id: string;
  contact_id: string | null;
  comm_type: CommType;
  direction: CommDirection;
  subject: string | null;
  body: string | null;
  body_html: string | null;
  status: CommStatus;
  priority: CommPriority;
  sent_by: string | null;
  channel: string | null;
  template_id: string | null;
  external_message_id: string | null;
  related_entity_type: string | null;
  related_entity_id: string | null;
  metadata: Record<string, unknown>;
  error_message: string | null;
  scheduled_for: string | null;
  opened_at: string | null;
  clicked_at: string | null;
  replied_at: string | null;
  created_at: string;
  updated_at: string;
}

export interface LogCommunicationInput {
  shelter_id: string;
  contact_id?: string;
  comm_type: CommType;
  direction?: CommDirection;
  subject?: string;
  body?: string;
  body_html?: string;
  status?: CommStatus;
  priority?: CommPriority;
  sent_by?: string;
  channel?: string;
  template_id?: string;
  external_message_id?: string;
  related_entity_type?: string;
  related_entity_id?: string;
  metadata?: Record<string, unknown>;
  scheduled_for?: string;
}

// =====================================================
// TABLE 3: shelter_onboarding
// =====================================================

export type OnboardingStatus = 'submitted' | 'under_review' | 'outreach_sent' | 'outreach_responded' | 'pending_agreement' | 'pending_integration' | 'testing' | 'active' | 'rejected' | 'withdrawn' | 'duplicate';
export type OnboardingStage = 'review' | 'outreach' | 'agreement' | 'integration' | 'testing' | 'active' | 'closed';
export type OnboardingSource = 'self_registration' | 'ai_outreach' | 'referral' | 'import' | 'manual';
export type OnboardingPriority = 'low' | 'normal' | 'high' | 'urgent';
export type IntegrationPreference = 'api' | 'webhook' | 'feed' | 'csv_upload' | 'manual';

export interface ShelterOnboarding {
  id: string;
  shelter_id: string | null;
  org_name: string;
  org_type: string;
  ein: string | null;
  website: string | null;
  contact_first_name: string;
  contact_last_name: string;
  contact_email: string;
  contact_phone: string | null;
  contact_title: string | null;
  address: string | null;
  city: string;
  state_code: string;
  zip_code: string;
  estimated_dog_count: number | null;
  current_software: string | null;
  data_feed_url: string | null;
  social_facebook: string | null;
  social_instagram: string | null;
  social_tiktok: string | null;
  integration_preference: IntegrationPreference;
  status: OnboardingStatus;
  stage: OnboardingStage;
  assigned_to: string | null;
  priority: OnboardingPriority;
  score: number;
  source: OnboardingSource;
  referral_source: string | null;
  internal_notes: string | null;
  rejection_reason: string | null;
  submitted_at: string;
  first_contact_at: string | null;
  last_contact_at: string | null;
  reviewed_at: string | null;
  approved_at: string | null;
  activated_at: string | null;
  rejected_at: string | null;
  created_at: string;
  updated_at: string;
}

export interface SubmitOnboardingInput {
  org_name: string;
  org_type: string;
  ein?: string;
  website?: string;
  contact_first_name: string;
  contact_last_name: string;
  contact_email: string;
  contact_phone?: string;
  contact_title?: string;
  address?: string;
  city: string;
  state_code: string;
  zip_code: string;
  estimated_dog_count?: number;
  current_software?: string;
  data_feed_url?: string;
  social_facebook?: string;
  social_instagram?: string;
  social_tiktok?: string;
  integration_preference?: IntegrationPreference;
  referral_source?: string;
}

// =====================================================
// TABLE 4: shelter_api_keys
// =====================================================

export type ApiKeyEnvironment = 'production' | 'staging' | 'development';
export type ApiKeyScope = 'read' | 'write' | 'delete' | 'webhook_manage' | 'bulk_import';

export interface ShelterApiKey {
  id: string;
  shelter_id: string;
  key_prefix: string;
  key_hash: string;
  label: string;
  environment: ApiKeyEnvironment;
  scopes: ApiKeyScope[];
  rate_limit_per_minute: number;
  rate_limit_per_hour: number;
  rate_limit_per_day: number;
  is_active: boolean;
  last_used_at: string | null;
  last_used_ip: string | null;
  usage_count_today: number;
  usage_count_total: number;
  expires_at: string | null;
  rotated_from_key_id: string | null;
  created_by: string | null;
  revoked_at: string | null;
  revoked_by: string | null;
  revoke_reason: string | null;
  created_at: string;
  updated_at: string;
}

export interface GenerateApiKeyInput {
  shelter_id: string;
  label?: string;
  environment?: ApiKeyEnvironment;
  scopes?: ApiKeyScope[];
  rate_limit_per_minute?: number;
  rate_limit_per_hour?: number;
  rate_limit_per_day?: number;
  expires_at?: string;
  created_by?: string;
}

/** Returned only once at key creation — includes the raw key */
export interface GeneratedApiKey {
  key: ShelterApiKey;
  raw_key: string;
}

// =====================================================
// TABLE 5: shelter_agreements
// =====================================================

export type AgreementDocumentType = 'terms_of_service' | 'data_sharing_agreement' | 'privacy_policy' | 'api_usage_agreement' | 'shelter_listing_agreement';

export interface ShelterAgreement {
  id: string;
  shelter_id: string;
  contact_id: string | null;
  document_type: AgreementDocumentType;
  document_version: string;
  document_url: string | null;
  document_hash: string | null;
  accepted_at: string;
  accepted_by_name: string;
  accepted_by_email: string;
  accepted_by_title: string | null;
  accepted_by_ip: string | null;
  accepted_by_user_agent: string | null;
  is_current: boolean;
  superseded_by: string | null;
  revoked_at: string | null;
  revoke_reason: string | null;
  signature_hash: string | null;
  created_at: string;
}

export interface CreateAgreementInput {
  shelter_id: string;
  contact_id?: string;
  document_type: AgreementDocumentType;
  document_version: string;
  document_url?: string;
  document_hash?: string;
  accepted_by_name: string;
  accepted_by_email: string;
  accepted_by_title?: string;
  accepted_by_ip?: string;
  accepted_by_user_agent?: string;
}

// =====================================================
// TABLE 6: webhook_endpoints
// =====================================================

export type WebhookEvent = 'dog.created' | 'dog.updated' | 'dog.adopted' | 'dog.urgent' | 'dog.viewed' | 'dog.inquiry_received' | 'dog.shared' | 'account.updated';

export interface WebhookEndpoint {
  id: string;
  shelter_id: string;
  url: string;
  description: string | null;
  events: WebhookEvent[];
  signing_secret: string;
  headers: Record<string, string>;
  is_active: boolean;
  is_verified: boolean;
  verified_at: string | null;
  last_delivery_at: string | null;
  last_success_at: string | null;
  last_failure_at: string | null;
  consecutive_failures: number;
  total_deliveries: number;
  total_successes: number;
  total_failures: number;
  disabled_at: string | null;
  disable_reason: string | null;
  created_at: string;
  updated_at: string;
}

// =====================================================
// TABLE 7: webhook_deliveries
// =====================================================

export type DeliveryStatus = 'pending' | 'success' | 'failed' | 'retrying';

export interface WebhookDelivery {
  id: string;
  webhook_endpoint_id: string;
  shelter_id: string;
  event_type: string;
  event_id: string | null;
  payload: Record<string, unknown>;
  response_status: number | null;
  response_body: string | null;
  response_time_ms: number | null;
  attempt_number: number;
  max_attempts: number;
  status: DeliveryStatus;
  next_retry_at: string | null;
  delivered_at: string | null;
  created_at: string;
}

// =====================================================
// TABLE 8: api_request_logs
// =====================================================

export interface ApiRequestLog {
  id: string;
  shelter_id: string | null;
  api_key_id: string | null;
  method: string;
  path: string;
  query_params: Record<string, unknown> | null;
  status_code: number;
  request_body_size: number | null;
  response_body_size: number | null;
  response_time_ms: number | null;
  ip_address: string | null;
  user_agent: string | null;
  error_message: string | null;
  rate_limited: boolean;
  created_at: string;
}

// =====================================================
// TABLE 9: outreach_campaigns
// =====================================================

export type CampaignStatus = 'draft' | 'scheduled' | 'running' | 'paused' | 'completed' | 'cancelled';

export interface OutreachCampaign {
  id: string;
  name: string;
  description: string | null;
  target_criteria: {
    state_codes?: string[];
    is_kill_shelter?: boolean;
    min_dogs?: number;
    max_dogs?: number;
    shelter_types?: string[];
    has_email?: boolean;
    has_website?: boolean;
    exclude_opted_out?: boolean;
  };
  email_template_id: string | null;
  status: CampaignStatus;
  total_targets: number;
  sent_count: number;
  opened_count: number;
  replied_count: number;
  converted_count: number;
  bounced_count: number;
  opted_out_count: number;
  scheduled_start: string | null;
  started_at: string | null;
  completed_at: string | null;
  send_rate_per_hour: number;
  created_at: string;
  updated_at: string;
}

// =====================================================
// TABLE 10: outreach_targets
// =====================================================

export type OutreachTargetStatus = 'pending' | 'queued' | 'sent' | 'delivered' | 'opened' | 'clicked' | 'replied' | 'bounced' | 'opted_out' | 'converted' | 'skipped';

export interface OutreachTarget {
  id: string;
  campaign_id: string;
  shelter_id: string | null;
  contact_id: string | null;
  email: string;
  status: OutreachTargetStatus;
  sent_at: string | null;
  opened_at: string | null;
  clicked_at: string | null;
  replied_at: string | null;
  follow_up_count: number;
  last_follow_up_at: string | null;
  next_follow_up_at: string | null;
  communication_id: string | null;
  notes: string | null;
  created_at: string;
  updated_at: string;
}

// =====================================================
// PIPELINE & STATISTICS TYPES
// =====================================================

export interface OnboardingPipelineStats {
  submitted: number;
  under_review: number;
  outreach_sent: number;
  outreach_responded: number;
  pending_agreement: number;
  pending_integration: number;
  testing: number;
  active: number;
  rejected: number;
  total: number;
}

export interface CampaignStats {
  total_targets: number;
  sent_count: number;
  opened_count: number;
  replied_count: number;
  converted_count: number;
  bounced_count: number;
  open_rate: number;
  reply_rate: number;
  conversion_rate: number;
  bounce_rate: number;
}

export interface ApiKeyUsageStats {
  key_id: string;
  key_prefix: string;
  label: string;
  today: number;
  this_hour: number;
  this_minute: number;
  total: number;
  last_used_at: string | null;
}

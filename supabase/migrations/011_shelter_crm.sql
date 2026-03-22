-- Migration 011: Shelter CRM & Enterprise Platform Tables
-- Adds 10 new tables for shelter partner management, API access, webhooks, outreach, and monetization
-- Plus extends existing shelters table with partner fields

-- =====================================================
-- EXTEND EXISTING SHELTERS TABLE
-- =====================================================

ALTER TABLE shelters
  ADD COLUMN IF NOT EXISTS partner_status VARCHAR(30) DEFAULT 'unregistered',
  ADD COLUMN IF NOT EXISTS partner_tier VARCHAR(20) DEFAULT 'free',
  ADD COLUMN IF NOT EXISTS partner_since TIMESTAMPTZ,
  ADD COLUMN IF NOT EXISTS primary_contact_id UUID,
  ADD COLUMN IF NOT EXISTS integration_type VARCHAR(30) DEFAULT 'none',
  ADD COLUMN IF NOT EXISTS feed_url VARCHAR(500),
  ADD COLUMN IF NOT EXISTS last_feed_sync_at TIMESTAMPTZ,
  ADD COLUMN IF NOT EXISTS social_facebook VARCHAR(500),
  ADD COLUMN IF NOT EXISTS social_instagram VARCHAR(300),
  ADD COLUMN IF NOT EXISTS social_tiktok VARCHAR(300),
  ADD COLUMN IF NOT EXISTS social_youtube VARCHAR(300),
  ADD COLUMN IF NOT EXISTS response_time_hours INTEGER,
  ADD COLUMN IF NOT EXISTS adoption_success_rate DECIMAL(5,2),
  ADD COLUMN IF NOT EXISTS total_communications INTEGER DEFAULT 0;

ALTER TABLE shelters
  ADD CONSTRAINT chk_shelters_partner_status
    CHECK (partner_status IN ('unregistered', 'applied', 'onboarding', 'active', 'suspended', 'churned')),
  ADD CONSTRAINT chk_shelters_partner_tier
    CHECK (partner_tier IN ('free', 'basic', 'premium')),
  ADD CONSTRAINT chk_shelters_integration_type
    CHECK (integration_type IN ('none', 'rescuegroups', 'api_direct', 'webhook', 'feed_url', 'csv_upload', 'manual'));

CREATE INDEX IF NOT EXISTS idx_shelters_partner_status ON shelters(partner_status);
CREATE INDEX IF NOT EXISTS idx_shelters_partner_tier ON shelters(partner_tier);
CREATE INDEX IF NOT EXISTS idx_shelters_integration_type ON shelters(integration_type);

-- =====================================================
-- TABLE 1: shelter_contacts
-- Every person at every shelter
-- =====================================================

CREATE TABLE shelter_contacts (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  first_name VARCHAR(100) NOT NULL,
  last_name VARCHAR(100) NOT NULL,
  title VARCHAR(100),
  role VARCHAR(50) NOT NULL DEFAULT 'general',
  email VARCHAR(200) NOT NULL,
  phone VARCHAR(30),
  phone_ext VARCHAR(10),
  preferred_contact VARCHAR(20) DEFAULT 'email',
  contact_hours TEXT,
  timezone VARCHAR(50),
  notes TEXT,
  is_primary BOOLEAN DEFAULT FALSE,
  is_active BOOLEAN DEFAULT TRUE,
  is_opted_out BOOLEAN DEFAULT FALSE,
  opted_out_at TIMESTAMPTZ,
  last_contacted_at TIMESTAMPTZ,
  last_responded_at TIMESTAMPTZ,
  total_communications INTEGER DEFAULT 0,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_contact_role CHECK (role IN ('director', 'manager', 'intake', 'medical', 'volunteer_coord', 'it_admin', 'foster_coord', 'general', 'board_member', 'social_media')),
  CONSTRAINT chk_contact_preferred CHECK (preferred_contact IN ('email', 'phone', 'text')),
  CONSTRAINT uq_shelter_contact_email UNIQUE (shelter_id, email)
);

CREATE INDEX idx_shelter_contacts_shelter ON shelter_contacts(shelter_id);
CREATE INDEX idx_shelter_contacts_email ON shelter_contacts(email);
CREATE INDEX idx_shelter_contacts_role ON shelter_contacts(shelter_id, role);
CREATE INDEX idx_shelter_contacts_primary ON shelter_contacts(shelter_id, is_primary) WHERE is_primary = TRUE;

-- Add FK from shelters.primary_contact_id to shelter_contacts
ALTER TABLE shelters
  ADD CONSTRAINT fk_shelters_primary_contact
    FOREIGN KEY (primary_contact_id) REFERENCES shelter_contacts(id) ON DELETE SET NULL;

-- =====================================================
-- TABLE 2: shelter_communications
-- Every interaction logged forever
-- =====================================================

CREATE TABLE shelter_communications (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  contact_id UUID REFERENCES shelter_contacts(id) ON DELETE SET NULL,
  comm_type VARCHAR(30) NOT NULL,
  direction VARCHAR(10) NOT NULL DEFAULT 'outbound',
  subject VARCHAR(500),
  body TEXT,
  body_html TEXT,
  to_address VARCHAR(200),
  status VARCHAR(30) NOT NULL DEFAULT 'sent',
  priority VARCHAR(20) DEFAULT 'normal',
  sent_by VARCHAR(200),
  channel VARCHAR(50),
  template_id VARCHAR(100),
  external_message_id VARCHAR(200),
  related_entity_type VARCHAR(50),
  related_entity_id UUID,
  metadata JSONB DEFAULT '{}',
  error_message TEXT,
  scheduled_for TIMESTAMPTZ,
  opened_at TIMESTAMPTZ,
  clicked_at TIMESTAMPTZ,
  replied_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_comm_type CHECK (comm_type IN ('email', 'phone_call', 'text_message', 'in_app_message', 'form_submission', 'webhook_notification', 'status_change', 'note', 'ai_outreach', 'social_media_dm')),
  CONSTRAINT chk_comm_direction CHECK (direction IN ('inbound', 'outbound', 'internal')),
  CONSTRAINT chk_comm_status CHECK (status IN ('draft', 'queued', 'sent', 'delivered', 'opened', 'clicked', 'replied', 'bounced', 'failed', 'received', 'read', 'archived')),
  CONSTRAINT chk_comm_priority CHECK (priority IN ('low', 'normal', 'high', 'urgent'))
);

CREATE INDEX idx_shelter_comms_shelter ON shelter_communications(shelter_id);
CREATE INDEX idx_shelter_comms_contact ON shelter_communications(contact_id);
CREATE INDEX idx_shelter_comms_type ON shelter_communications(comm_type);
CREATE INDEX idx_shelter_comms_status ON shelter_communications(status);
CREATE INDEX idx_shelter_comms_shelter_date ON shelter_communications(shelter_id, created_at DESC);
CREATE INDEX idx_shelter_comms_related ON shelter_communications(related_entity_type, related_entity_id);
CREATE INDEX idx_shelter_comms_queue ON shelter_communications(status, scheduled_for) WHERE status IN ('queued', 'draft');

-- =====================================================
-- TABLE 3: shelter_onboarding
-- Registration pipeline from application to activation
-- =====================================================

CREATE TABLE shelter_onboarding (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
  org_name VARCHAR(200) NOT NULL,
  org_type VARCHAR(50) NOT NULL,
  ein VARCHAR(20),
  website VARCHAR(500),
  contact_first_name VARCHAR(100) NOT NULL,
  contact_last_name VARCHAR(100) NOT NULL,
  contact_email VARCHAR(200) NOT NULL,
  contact_phone VARCHAR(30),
  contact_title VARCHAR(100),
  address VARCHAR(300),
  city VARCHAR(100) NOT NULL,
  state_code VARCHAR(2) NOT NULL,
  zip_code VARCHAR(10) NOT NULL,
  estimated_dog_count INTEGER,
  current_software VARCHAR(200),
  data_feed_url VARCHAR(500),
  social_facebook VARCHAR(500),
  social_instagram VARCHAR(300),
  social_tiktok VARCHAR(300),
  integration_preference VARCHAR(50) DEFAULT 'api',
  status VARCHAR(30) NOT NULL DEFAULT 'submitted',
  stage VARCHAR(50) NOT NULL DEFAULT 'review',
  assigned_to VARCHAR(200),
  priority VARCHAR(20) DEFAULT 'normal',
  score INTEGER DEFAULT 0,
  source VARCHAR(50) DEFAULT 'self_registration',
  referral_source VARCHAR(200),
  internal_notes TEXT,
  rejection_reason TEXT,
  submitted_at TIMESTAMPTZ DEFAULT NOW(),
  first_contact_at TIMESTAMPTZ,
  last_contact_at TIMESTAMPTZ,
  reviewed_at TIMESTAMPTZ,
  approved_at TIMESTAMPTZ,
  activated_at TIMESTAMPTZ,
  rejected_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_onboarding_integration CHECK (integration_preference IN ('api', 'webhook', 'feed', 'csv_upload', 'manual')),
  CONSTRAINT chk_onboarding_status CHECK (status IN ('submitted', 'under_review', 'outreach_sent', 'outreach_responded', 'pending_agreement', 'pending_integration', 'testing', 'active', 'rejected', 'withdrawn', 'duplicate')),
  CONSTRAINT chk_onboarding_stage CHECK (stage IN ('review', 'outreach', 'agreement', 'integration', 'testing', 'active', 'closed')),
  CONSTRAINT chk_onboarding_priority CHECK (priority IN ('low', 'normal', 'high', 'urgent')),
  CONSTRAINT chk_onboarding_source CHECK (source IN ('self_registration', 'ai_outreach', 'referral', 'import', 'manual'))
);

CREATE INDEX idx_onboarding_status ON shelter_onboarding(status);
CREATE INDEX idx_onboarding_stage ON shelter_onboarding(stage);
CREATE INDEX idx_onboarding_email ON shelter_onboarding(contact_email);
CREATE INDEX idx_onboarding_shelter ON shelter_onboarding(shelter_id);
CREATE INDEX idx_onboarding_state ON shelter_onboarding(state_code);
CREATE INDEX idx_onboarding_priority_score ON shelter_onboarding(priority, score DESC);
CREATE INDEX idx_onboarding_created ON shelter_onboarding(created_at DESC);

-- =====================================================
-- TABLE 4: shelter_api_keys
-- API key management with rotation support
-- =====================================================

CREATE TABLE shelter_api_keys (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  key_prefix VARCHAR(12) NOT NULL,
  key_hash VARCHAR(128) NOT NULL,
  label VARCHAR(100) NOT NULL DEFAULT 'Default',
  environment VARCHAR(20) DEFAULT 'production',
  scopes TEXT[] DEFAULT '{read,write}',
  rate_limit_per_minute INTEGER DEFAULT 60,
  rate_limit_per_hour INTEGER DEFAULT 1000,
  rate_limit_per_day INTEGER DEFAULT 10000,
  is_active BOOLEAN DEFAULT TRUE,
  last_used_at TIMESTAMPTZ,
  last_used_ip VARCHAR(45),
  usage_count_today INTEGER DEFAULT 0,
  usage_count_total BIGINT DEFAULT 0,
  expires_at TIMESTAMPTZ,
  rotated_from_key_id UUID REFERENCES shelter_api_keys(id) ON DELETE SET NULL,
  created_by VARCHAR(200),
  revoked_at TIMESTAMPTZ,
  revoked_by VARCHAR(200),
  revoke_reason TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_apikey_environment CHECK (environment IN ('production', 'staging', 'development'))
);

CREATE INDEX idx_apikeys_shelter ON shelter_api_keys(shelter_id);
CREATE INDEX idx_apikeys_prefix ON shelter_api_keys(key_prefix);
CREATE INDEX idx_apikeys_hash_active ON shelter_api_keys(key_hash) WHERE is_active = TRUE;
CREATE INDEX idx_apikeys_shelter_active ON shelter_api_keys(shelter_id, is_active);
CREATE INDEX idx_apikeys_expires ON shelter_api_keys(expires_at) WHERE expires_at IS NOT NULL;

-- =====================================================
-- TABLE 5: shelter_agreements
-- Legal document acceptance with versioning
-- =====================================================

CREATE TABLE shelter_agreements (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  contact_id UUID REFERENCES shelter_contacts(id) ON DELETE SET NULL,
  document_type VARCHAR(50) NOT NULL,
  document_version VARCHAR(20) NOT NULL,
  document_url VARCHAR(500),
  document_hash VARCHAR(128),
  accepted_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  accepted_by_name VARCHAR(200) NOT NULL,
  accepted_by_email VARCHAR(200) NOT NULL,
  accepted_by_title VARCHAR(100),
  accepted_by_ip VARCHAR(45),
  accepted_by_user_agent TEXT,
  is_current BOOLEAN DEFAULT TRUE,
  superseded_by UUID REFERENCES shelter_agreements(id) ON DELETE SET NULL,
  revoked_at TIMESTAMPTZ,
  revoke_reason TEXT,
  signature_hash VARCHAR(128),
  created_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_agreement_type CHECK (document_type IN ('terms_of_service', 'data_sharing_agreement', 'privacy_policy', 'api_usage_agreement', 'shelter_listing_agreement'))
);

CREATE INDEX idx_agreements_shelter ON shelter_agreements(shelter_id);
CREATE INDEX idx_agreements_shelter_type ON shelter_agreements(shelter_id, document_type);
CREATE INDEX idx_agreements_current ON shelter_agreements(shelter_id, document_type, is_current) WHERE is_current = TRUE;

-- =====================================================
-- TABLE 6: webhook_endpoints
-- Shelter-registered webhook receivers
-- =====================================================

CREATE TABLE webhook_endpoints (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  url VARCHAR(500) NOT NULL,
  description VARCHAR(200),
  events TEXT[] NOT NULL DEFAULT '{dog.created,dog.updated,dog.adopted,dog.urgent}',
  signing_secret VARCHAR(128) NOT NULL,
  headers JSONB DEFAULT '{}',
  is_active BOOLEAN DEFAULT TRUE,
  is_verified BOOLEAN DEFAULT FALSE,
  verified_at TIMESTAMPTZ,
  last_delivery_at TIMESTAMPTZ,
  last_success_at TIMESTAMPTZ,
  last_failure_at TIMESTAMPTZ,
  consecutive_failures INTEGER DEFAULT 0,
  total_deliveries BIGINT DEFAULT 0,
  total_successes BIGINT DEFAULT 0,
  total_failures BIGINT DEFAULT 0,
  disabled_at TIMESTAMPTZ,
  disable_reason TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_webhooks_shelter ON webhook_endpoints(shelter_id);
CREATE INDEX idx_webhooks_active ON webhook_endpoints(is_active) WHERE is_active = TRUE;

-- =====================================================
-- TABLE 7: webhook_deliveries
-- Every delivery attempt logged
-- =====================================================

CREATE TABLE webhook_deliveries (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  webhook_endpoint_id UUID NOT NULL REFERENCES webhook_endpoints(id) ON DELETE CASCADE,
  shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
  event_type VARCHAR(50) NOT NULL,
  event_id VARCHAR(100),
  payload JSONB NOT NULL,
  response_status INTEGER,
  response_body VARCHAR(2000),
  response_time_ms INTEGER,
  attempt_number INTEGER DEFAULT 1,
  max_attempts INTEGER DEFAULT 5,
  status VARCHAR(20) DEFAULT 'pending',
  next_retry_at TIMESTAMPTZ,
  delivered_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_delivery_status CHECK (status IN ('pending', 'success', 'failed', 'retrying'))
);

CREATE INDEX idx_deliveries_endpoint ON webhook_deliveries(webhook_endpoint_id);
CREATE INDEX idx_deliveries_shelter ON webhook_deliveries(shelter_id);
CREATE INDEX idx_deliveries_event ON webhook_deliveries(event_type);
CREATE INDEX idx_deliveries_pending ON webhook_deliveries(status) WHERE status IN ('pending', 'retrying');
CREATE INDEX idx_deliveries_created ON webhook_deliveries(created_at DESC);

-- =====================================================
-- TABLE 8: api_request_logs
-- Full API usage audit trail
-- =====================================================

CREATE TABLE api_request_logs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
  api_key_id UUID REFERENCES shelter_api_keys(id) ON DELETE SET NULL,
  method VARCHAR(10) NOT NULL,
  path VARCHAR(500) NOT NULL,
  query_params JSONB,
  status_code INTEGER NOT NULL,
  request_body_size INTEGER,
  response_body_size INTEGER,
  response_time_ms INTEGER,
  ip_address VARCHAR(45),
  user_agent VARCHAR(500),
  error_message TEXT,
  rate_limited BOOLEAN DEFAULT FALSE,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_api_logs_shelter ON api_request_logs(shelter_id);
CREATE INDEX idx_api_logs_key ON api_request_logs(api_key_id);
CREATE INDEX idx_api_logs_shelter_date ON api_request_logs(shelter_id, created_at DESC);
CREATE INDEX idx_api_logs_created ON api_request_logs(created_at);

-- =====================================================
-- TABLE 9: outreach_campaigns
-- AI agent outreach tracking
-- =====================================================

CREATE TABLE outreach_campaigns (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  name VARCHAR(200) NOT NULL,
  description TEXT,
  campaign_type VARCHAR(50) DEFAULT 'initial_outreach',
  target_criteria JSONB DEFAULT '{}',
  email_template_id VARCHAR(100),
  status VARCHAR(30) DEFAULT 'draft',
  total_targets INTEGER DEFAULT 0,
  total_sent INTEGER DEFAULT 0,
  total_responded INTEGER DEFAULT 0,
  total_converted INTEGER DEFAULT 0,
  total_bounced INTEGER DEFAULT 0,
  total_opted_out INTEGER DEFAULT 0,
  scheduled_start TIMESTAMPTZ,
  started_at TIMESTAMPTZ,
  completed_at TIMESTAMPTZ,
  send_rate_per_hour INTEGER DEFAULT 50,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_campaign_status CHECK (status IN ('draft', 'active', 'scheduled', 'running', 'paused', 'completed', 'cancelled'))
);

-- =====================================================
-- TABLE 10: outreach_targets
-- Individual targets within a campaign
-- =====================================================

CREATE TABLE outreach_targets (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  campaign_id UUID NOT NULL REFERENCES outreach_campaigns(id) ON DELETE CASCADE,
  shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
  contact_id UUID REFERENCES shelter_contacts(id) ON DELETE SET NULL,
  contact_email VARCHAR(200) NOT NULL,
  contact_name VARCHAR(200),
  status VARCHAR(30) DEFAULT 'pending',
  priority_score INTEGER DEFAULT 0,
  priority VARCHAR(20) DEFAULT 'normal',
  send_count INTEGER DEFAULT 0,
  last_sent_at TIMESTAMPTZ,
  last_sent_template VARCHAR(100),
  opened_at TIMESTAMPTZ,
  clicked_at TIMESTAMPTZ,
  replied_at TIMESTAMPTZ,
  next_follow_up_at TIMESTAMPTZ,
  communication_id UUID REFERENCES shelter_communications(id) ON DELETE SET NULL,
  notes TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_target_status CHECK (status IN ('pending', 'contacted', 'follow_up_sent', 'completed', 'error', 'skipped', 'bounced', 'opted_out', 'converted', 'replied'))
);

CREATE INDEX idx_targets_campaign ON outreach_targets(campaign_id);
CREATE INDEX idx_targets_shelter ON outreach_targets(shelter_id);
CREATE INDEX idx_targets_status ON outreach_targets(status);
CREATE INDEX idx_targets_follow_up ON outreach_targets(next_follow_up_at) WHERE next_follow_up_at IS NOT NULL;

-- =====================================================
-- MONETIZATION TABLES
-- =====================================================

-- TABLE 11: affiliate_partners
CREATE TABLE affiliate_partners (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  name VARCHAR(100) NOT NULL,
  slug VARCHAR(50) NOT NULL UNIQUE,
  program_type VARCHAR(30),
  affiliate_id VARCHAR(200),
  commission_rate DECIMAL(5,2),
  cookie_duration_days INTEGER,
  base_url VARCHAR(500),
  tracking_param VARCHAR(50),
  is_active BOOLEAN DEFAULT TRUE,
  total_clicks BIGINT DEFAULT 0,
  total_conversions BIGINT DEFAULT 0,
  total_revenue DECIMAL(12,2) DEFAULT 0,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- TABLE 12: affiliate_products
CREATE TABLE affiliate_products (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  partner_id UUID NOT NULL REFERENCES affiliate_partners(id) ON DELETE CASCADE,
  name VARCHAR(200) NOT NULL,
  category VARCHAR(50) NOT NULL,
  affiliate_url VARCHAR(1000) NOT NULL,
  image_url VARCHAR(500),
  price_min DECIMAL(8,2),
  price_max DECIMAL(8,2),
  description TEXT,
  target_sizes TEXT[],
  target_ages TEXT[],
  target_breeds TEXT[],
  target_conditions TEXT[],
  target_seasons TEXT[],
  priority INTEGER DEFAULT 50,
  is_active BOOLEAN DEFAULT TRUE,
  click_count BIGINT DEFAULT 0,
  conversion_count BIGINT DEFAULT 0,
  revenue_total DECIMAL(10,2) DEFAULT 0,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_product_category CHECK (category IN ('food', 'treats', 'toys', 'beds', 'crates', 'leashes', 'grooming', 'health', 'training', 'clothing', 'accessories', 'insurance'))
);

CREATE INDEX idx_products_partner ON affiliate_products(partner_id);
CREATE INDEX idx_products_category ON affiliate_products(category);
CREATE INDEX idx_products_active ON affiliate_products(is_active, priority DESC) WHERE is_active = TRUE;

-- TABLE 13: affiliate_clicks
CREATE TABLE affiliate_clicks (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  product_id UUID NOT NULL REFERENCES affiliate_products(id) ON DELETE CASCADE,
  partner_id UUID NOT NULL REFERENCES affiliate_partners(id) ON DELETE CASCADE,
  dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
  page_url VARCHAR(500),
  ip_hash VARCHAR(64),
  user_agent VARCHAR(500),
  referrer VARCHAR(500),
  session_id VARCHAR(100),
  clicked_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_clicks_product ON affiliate_clicks(product_id);
CREATE INDEX idx_clicks_partner ON affiliate_clicks(partner_id);
CREATE INDEX idx_clicks_dog ON affiliate_clicks(dog_id);
CREATE INDEX idx_clicks_date ON affiliate_clicks(clicked_at);

-- =====================================================
-- SOCIAL MEDIA TABLES
-- =====================================================

-- TABLE 14: social_accounts
CREATE TABLE social_accounts (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  platform VARCHAR(30) NOT NULL,
  handle VARCHAR(200) NOT NULL,
  display_name VARCHAR(200),
  profile_url VARCHAR(500),
  access_token TEXT,
  refresh_token TEXT,
  token_expires_at TIMESTAMPTZ,
  is_active BOOLEAN DEFAULT TRUE,
  follower_count INTEGER DEFAULT 0,
  following_count INTEGER DEFAULT 0,
  post_count INTEGER DEFAULT 0,
  last_posted_at TIMESTAMPTZ,
  last_synced_at TIMESTAMPTZ,
  metadata JSONB DEFAULT '{}',
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_social_platform CHECK (platform IN ('tiktok', 'facebook', 'instagram', 'youtube', 'reddit', 'snapchat', 'twitter', 'threads', 'bluesky'))
);

-- TABLE 15: social_posts
CREATE TABLE social_posts (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  account_id UUID NOT NULL REFERENCES social_accounts(id) ON DELETE CASCADE,
  platform VARCHAR(30) NOT NULL,
  content_type VARCHAR(50) NOT NULL,
  dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
  shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
  caption TEXT NOT NULL,
  hashtags TEXT[],
  media_urls TEXT[],
  media_type VARCHAR(20),
  video_script TEXT,
  status VARCHAR(30) DEFAULT 'draft',
  scheduled_at TIMESTAMPTZ,
  posted_at TIMESTAMPTZ,
  external_post_id VARCHAR(200),
  external_post_url VARCHAR(500),
  engagement JSONB DEFAULT '{}',
  last_engagement_sync TIMESTAMPTZ,
  performance_score DECIMAL(5,2),
  generated_by VARCHAR(50),
  reviewed_by VARCHAR(200),
  reviewed_at TIMESTAMPTZ,
  notes TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  CONSTRAINT chk_post_content_type CHECK (content_type IN ('dog_spotlight', 'urgent_alert', 'happy_tails', 'did_you_know', 'breed_facts', 'weekly_roundup', 'longest_waiting', 'new_arrivals', 'foster_appeal', 'shelter_spotlight')),
  CONSTRAINT chk_post_status CHECK (status IN ('draft', 'generating', 'review', 'scheduled', 'posting', 'posted', 'failed', 'archived'))
);

CREATE INDEX idx_posts_account ON social_posts(account_id);
CREATE INDEX idx_posts_dog ON social_posts(dog_id);
CREATE INDEX idx_posts_status ON social_posts(status);
CREATE INDEX idx_posts_scheduled ON social_posts(scheduled_at) WHERE status = 'scheduled';

-- TABLE 16: social_templates
CREATE TABLE social_templates (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  name VARCHAR(200) NOT NULL,
  platform VARCHAR(30),
  content_type VARCHAR(50) NOT NULL,
  caption_template TEXT NOT NULL,
  hashtag_sets TEXT[],
  media_requirements JSONB,
  is_active BOOLEAN DEFAULT TRUE,
  usage_count INTEGER DEFAULT 0,
  avg_engagement DECIMAL(5,2),
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- =====================================================
-- AUTO-UPDATE TRIGGERS for updated_at
-- =====================================================

-- Reuse existing update_updated_at_column() function if it exists
DO $$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_proc WHERE proname = 'update_updated_at_column') THEN
    CREATE FUNCTION update_updated_at_column()
    RETURNS TRIGGER AS $func$
    BEGIN
      NEW.updated_at = NOW();
      RETURN NEW;
    END;
    $func$ LANGUAGE plpgsql;
  END IF;
END
$$;

CREATE TRIGGER trg_shelter_contacts_updated BEFORE UPDATE ON shelter_contacts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_shelter_communications_updated BEFORE UPDATE ON shelter_communications FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_shelter_onboarding_updated BEFORE UPDATE ON shelter_onboarding FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_shelter_api_keys_updated BEFORE UPDATE ON shelter_api_keys FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_webhook_endpoints_updated BEFORE UPDATE ON webhook_endpoints FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_outreach_campaigns_updated BEFORE UPDATE ON outreach_campaigns FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_outreach_targets_updated BEFORE UPDATE ON outreach_targets FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_affiliate_products_updated BEFORE UPDATE ON affiliate_products FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_social_accounts_updated BEFORE UPDATE ON social_accounts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER trg_social_posts_updated BEFORE UPDATE ON social_posts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- =====================================================
-- ROW LEVEL SECURITY
-- =====================================================

ALTER TABLE shelter_contacts ENABLE ROW LEVEL SECURITY;
ALTER TABLE shelter_communications ENABLE ROW LEVEL SECURITY;
ALTER TABLE shelter_onboarding ENABLE ROW LEVEL SECURITY;
ALTER TABLE shelter_api_keys ENABLE ROW LEVEL SECURITY;
ALTER TABLE shelter_agreements ENABLE ROW LEVEL SECURITY;
ALTER TABLE webhook_endpoints ENABLE ROW LEVEL SECURITY;
ALTER TABLE webhook_deliveries ENABLE ROW LEVEL SECURITY;
ALTER TABLE api_request_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE outreach_campaigns ENABLE ROW LEVEL SECURITY;
ALTER TABLE outreach_targets ENABLE ROW LEVEL SECURITY;
ALTER TABLE affiliate_partners ENABLE ROW LEVEL SECURITY;
ALTER TABLE affiliate_products ENABLE ROW LEVEL SECURITY;
ALTER TABLE affiliate_clicks ENABLE ROW LEVEL SECURITY;
ALTER TABLE social_accounts ENABLE ROW LEVEL SECURITY;
ALTER TABLE social_posts ENABLE ROW LEVEL SECURITY;
ALTER TABLE social_templates ENABLE ROW LEVEL SECURITY;

-- Allow service role full access (API routes use admin client)
CREATE POLICY "Service role full access" ON shelter_contacts FOR ALL USING (true);
CREATE POLICY "Service role full access" ON shelter_communications FOR ALL USING (true);
CREATE POLICY "Service role full access" ON shelter_onboarding FOR ALL USING (true);
CREATE POLICY "Service role full access" ON shelter_api_keys FOR ALL USING (true);
CREATE POLICY "Service role full access" ON shelter_agreements FOR ALL USING (true);
CREATE POLICY "Service role full access" ON webhook_endpoints FOR ALL USING (true);
CREATE POLICY "Service role full access" ON webhook_deliveries FOR ALL USING (true);
CREATE POLICY "Service role full access" ON api_request_logs FOR ALL USING (true);
CREATE POLICY "Service role full access" ON outreach_campaigns FOR ALL USING (true);
CREATE POLICY "Service role full access" ON outreach_targets FOR ALL USING (true);
CREATE POLICY "Service role full access" ON affiliate_partners FOR ALL USING (true);
CREATE POLICY "Service role full access" ON affiliate_products FOR ALL USING (true);
CREATE POLICY "Service role full access" ON affiliate_clicks FOR ALL USING (true);
CREATE POLICY "Service role full access" ON social_accounts FOR ALL USING (true);
CREATE POLICY "Service role full access" ON social_posts FOR ALL USING (true);
CREATE POLICY "Service role full access" ON social_templates FOR ALL USING (true);

-- =====================================================
-- TABLE COMMENTS
-- =====================================================

COMMENT ON TABLE shelter_contacts IS 'Every contact person at every shelter in the system';
COMMENT ON TABLE shelter_communications IS 'Complete log of every interaction with every shelter';
COMMENT ON TABLE shelter_onboarding IS 'Registration pipeline from application to activation';
COMMENT ON TABLE shelter_api_keys IS 'API key management with rotation and scope support';
COMMENT ON TABLE shelter_agreements IS 'Legal document acceptance tracking with versioning';
COMMENT ON TABLE webhook_endpoints IS 'Shelter-registered webhook receiver endpoints';
COMMENT ON TABLE webhook_deliveries IS 'Every webhook delivery attempt with response tracking';
COMMENT ON TABLE api_request_logs IS 'Full API usage audit trail for billing and debugging';
COMMENT ON TABLE outreach_campaigns IS 'AI-powered outreach campaign management';
COMMENT ON TABLE outreach_targets IS 'Individual shelter targets within outreach campaigns';
COMMENT ON TABLE affiliate_partners IS 'Affiliate program partner tracking (Chewy, Amazon, etc.)';
COMMENT ON TABLE affiliate_products IS 'Product catalog with breed-specific targeting';
COMMENT ON TABLE affiliate_clicks IS 'Click tracking for affiliate attribution';
COMMENT ON TABLE social_accounts IS 'Connected social media platform accounts';
COMMENT ON TABLE social_posts IS 'Generated and published social media content';
COMMENT ON TABLE social_templates IS 'Reusable content templates for social media posts';

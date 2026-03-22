import { NextResponse } from "next/server";
import { Client } from "pg";

export const maxDuration = 30;

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const results: string[] = [];

  try {
    // Try multiple connection methods
    const connectionConfigs = [
      {
        name: "session_pooler",
        host: "aws-0-us-east-1.pooler.supabase.com",
        port: 5432,
        user: "postgres.hpssqzqwtsczsxvdfktt",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
      {
        name: "transaction_pooler",
        host: "aws-0-us-east-1.pooler.supabase.com",
        port: 6543,
        user: "postgres.hpssqzqwtsczsxvdfktt",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
      {
        name: "direct",
        host: "db.hpssqzqwtsczsxvdfktt.supabase.co",
        port: 5432,
        user: "postgres",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
    ];

    let client: Client | null = null;
    for (const config of connectionConfigs) {
      try {
        const c = new Client({
          host: config.host,
          port: config.port,
          user: config.user,
          password: config.password,
          database: config.database,
          ssl: config.ssl,
          connectionTimeoutMillis: 10000,
        });
        await c.connect();
        results.push(`Connected via ${config.name}`);
        client = c;
        break;
      } catch (err) {
        results.push(`${config.name} failed: ${err instanceof Error ? err.message : String(err)}`);
      }
    }

    if (!client) {
      return NextResponse.json({
        success: false,
        results,
        error: "Could not connect to database with any method",
        sql_to_run_manually: "See /api/debug?fix=run_migration for the SQL to paste into Supabase SQL Editor",
      });
    }

    // Migration 008: foster_applications
    await client.query(`
      CREATE TABLE IF NOT EXISTS foster_applications (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        first_name TEXT NOT NULL,
        last_name TEXT NOT NULL,
        email TEXT NOT NULL,
        zip_code TEXT NOT NULL,
        housing_type TEXT NOT NULL,
        dog_experience TEXT NOT NULL,
        preferred_size TEXT DEFAULT 'any',
        notes TEXT,
        status TEXT NOT NULL DEFAULT 'pending',
        created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
        updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
      );
    `);
    results.push("foster_applications table: OK");

    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_status ON foster_applications(status);"
    );
    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_created ON foster_applications(created_at DESC);"
    );
    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_zip ON foster_applications(zip_code);"
    );
    results.push("foster_applications indexes: OK");

    // Migration 010: birth_date columns + daily_stats table
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS birth_date TIMESTAMPTZ;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_birth_date_exact BOOLEAN DEFAULT NULL;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_courtesy_listing BOOLEAN DEFAULT FALSE;");
    await client.query("CREATE INDEX IF NOT EXISTS idx_dogs_birth_date ON dogs(birth_date) WHERE birth_date IS NOT NULL;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS external_url_alive BOOLEAN DEFAULT NULL;");
    results.push("dogs birth_date + external_url_alive columns: OK");

    await client.query(`
      CREATE TABLE IF NOT EXISTS daily_stats (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        stat_date DATE NOT NULL UNIQUE,
        total_available_dogs INTEGER NOT NULL DEFAULT 0,
        high_confidence_dogs INTEGER NOT NULL DEFAULT 0,
        avg_wait_days_all DECIMAL(10,2),
        avg_wait_days_high_confidence DECIMAL(10,2),
        median_wait_days DECIMAL(10,2),
        max_wait_days INTEGER,
        dogs_with_birth_date INTEGER DEFAULT 0,
        verification_coverage_pct DECIMAL(5,2),
        date_accuracy_pct DECIMAL(5,2),
        dogs_added_today INTEGER DEFAULT 0,
        dogs_adopted_today INTEGER DEFAULT 0,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_daily_stats_date ON daily_stats(stat_date DESC);");
    results.push("daily_stats table: OK");

    // Enable RLS on daily_stats
    await client.query("ALTER TABLE daily_stats ENABLE ROW LEVEL SECURITY;");
    await client.query("CREATE POLICY IF NOT EXISTS daily_stats_read ON daily_stats FOR SELECT USING (true);");
    await client.query("CREATE POLICY IF NOT EXISTS daily_stats_write ON daily_stats FOR ALL USING (true) WITH CHECK (true);");
    results.push("daily_stats RLS: OK");

    // ============================================================
    // Migration 011: Shelter CRM & Enterprise Platform Tables
    // ============================================================

    // Extend shelters table with partner fields
    await client.query(`
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
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelters_partner_status ON shelters(partner_status);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelters_partner_tier ON shelters(partner_tier);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelters_integration_type ON shelters(integration_type);");
    results.push("shelters partner columns: OK");

    // shelter_contacts
    await client.query(`
      CREATE TABLE IF NOT EXISTS shelter_contacts (
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelter_contacts_shelter ON shelter_contacts(shelter_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelter_contacts_email ON shelter_contacts(email);");
    results.push("shelter_contacts: OK");

    // shelter_communications
    await client.query(`
      CREATE TABLE IF NOT EXISTS shelter_communications (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
        contact_id UUID,
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelter_comms_shelter ON shelter_communications(shelter_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelter_comms_type ON shelter_communications(comm_type);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_shelter_comms_status ON shelter_communications(status);");
    results.push("shelter_communications: OK");

    // shelter_onboarding
    await client.query(`
      CREATE TABLE IF NOT EXISTS shelter_onboarding (
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_onboarding_status ON shelter_onboarding(status);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_onboarding_stage ON shelter_onboarding(stage);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_onboarding_email ON shelter_onboarding(contact_email);");
    results.push("shelter_onboarding: OK");

    // shelter_api_keys
    await client.query(`
      CREATE TABLE IF NOT EXISTS shelter_api_keys (
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
        rotated_from_key_id UUID,
        created_by VARCHAR(200),
        revoked_at TIMESTAMPTZ,
        revoked_by VARCHAR(200),
        revoke_reason TEXT,
        created_at TIMESTAMPTZ DEFAULT NOW(),
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_apikeys_shelter ON shelter_api_keys(shelter_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_apikeys_prefix ON shelter_api_keys(key_prefix);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_apikeys_hash_active ON shelter_api_keys(key_hash) WHERE is_active = TRUE;");
    results.push("shelter_api_keys: OK");

    // shelter_agreements
    await client.query(`
      CREATE TABLE IF NOT EXISTS shelter_agreements (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
        contact_id UUID,
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
        superseded_by UUID,
        revoked_at TIMESTAMPTZ,
        revoke_reason TEXT,
        signature_hash VARCHAR(128),
        created_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_agreements_shelter ON shelter_agreements(shelter_id);");
    results.push("shelter_agreements: OK");

    // webhook_endpoints
    await client.query(`
      CREATE TABLE IF NOT EXISTS webhook_endpoints (
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
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_webhooks_shelter ON webhook_endpoints(shelter_id);");
    results.push("webhook_endpoints: OK");

    // webhook_deliveries
    await client.query(`
      CREATE TABLE IF NOT EXISTS webhook_deliveries (
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
        created_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_deliveries_endpoint ON webhook_deliveries(webhook_endpoint_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_deliveries_pending ON webhook_deliveries(status) WHERE status IN ('pending', 'retrying');");
    results.push("webhook_deliveries: OK");

    // api_request_logs
    await client.query(`
      CREATE TABLE IF NOT EXISTS api_request_logs (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
        api_key_id UUID,
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
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_api_logs_shelter ON api_request_logs(shelter_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_api_logs_created ON api_request_logs(created_at);");
    results.push("api_request_logs: OK");

    // outreach_campaigns
    await client.query(`
      CREATE TABLE IF NOT EXISTS outreach_campaigns (
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    results.push("outreach_campaigns: OK");

    // outreach_targets
    await client.query(`
      CREATE TABLE IF NOT EXISTS outreach_targets (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        campaign_id UUID NOT NULL REFERENCES outreach_campaigns(id) ON DELETE CASCADE,
        shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
        contact_id UUID,
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
        communication_id UUID,
        notes TEXT,
        created_at TIMESTAMPTZ DEFAULT NOW(),
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_targets_campaign ON outreach_targets(campaign_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_targets_shelter ON outreach_targets(shelter_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_targets_status ON outreach_targets(status);");
    results.push("outreach_targets: OK");

    // affiliate_partners
    await client.query(`
      CREATE TABLE IF NOT EXISTS affiliate_partners (
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
    `);
    results.push("affiliate_partners: OK");

    // affiliate_products
    await client.query(`
      CREATE TABLE IF NOT EXISTS affiliate_products (
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_products_partner ON affiliate_products(partner_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_products_category ON affiliate_products(category);");
    results.push("affiliate_products: OK");

    // affiliate_clicks
    await client.query(`
      CREATE TABLE IF NOT EXISTS affiliate_clicks (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        product_id UUID NOT NULL REFERENCES affiliate_products(id) ON DELETE CASCADE,
        partner_id UUID NOT NULL REFERENCES affiliate_partners(id) ON DELETE CASCADE,
        dog_id UUID,
        page_url VARCHAR(500),
        ip_hash VARCHAR(64),
        user_agent VARCHAR(500),
        referrer VARCHAR(500),
        session_id VARCHAR(100),
        clicked_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_clicks_product ON affiliate_clicks(product_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_clicks_date ON affiliate_clicks(clicked_at);");
    results.push("affiliate_clicks: OK");

    // social_accounts
    await client.query(`
      CREATE TABLE IF NOT EXISTS social_accounts (
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    results.push("social_accounts: OK");

    // social_posts
    await client.query(`
      CREATE TABLE IF NOT EXISTS social_posts (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        account_id UUID NOT NULL REFERENCES social_accounts(id) ON DELETE CASCADE,
        platform VARCHAR(30) NOT NULL,
        content_type VARCHAR(50) NOT NULL,
        dog_id UUID,
        shelter_id UUID,
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
        updated_at TIMESTAMPTZ DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_posts_account ON social_posts(account_id);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_posts_status ON social_posts(status);");
    await client.query("CREATE INDEX IF NOT EXISTS idx_posts_scheduled ON social_posts(scheduled_at) WHERE status = 'scheduled';");
    results.push("social_posts: OK");

    // social_templates
    await client.query(`
      CREATE TABLE IF NOT EXISTS social_templates (
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
    `);
    results.push("social_templates: OK");

    // Enable RLS on all new tables
    const rlsTables = [
      "shelter_contacts", "shelter_communications", "shelter_onboarding",
      "shelter_api_keys", "shelter_agreements", "webhook_endpoints",
      "webhook_deliveries", "api_request_logs", "outreach_campaigns",
      "outreach_targets", "affiliate_partners", "affiliate_products",
      "affiliate_clicks", "social_accounts", "social_posts", "social_templates"
    ];
    for (const table of rlsTables) {
      await client.query(`ALTER TABLE ${table} ENABLE ROW LEVEL SECURITY;`);
      await client.query(`
        DO $$ BEGIN
          IF NOT EXISTS (SELECT 1 FROM pg_policies WHERE tablename = '${table}' AND policyname = 'service_role_full_access') THEN
            CREATE POLICY service_role_full_access ON ${table} FOR ALL USING (true);
          END IF;
        END $$;
      `);
    }
    results.push("RLS enabled on all 16 tables: OK");

    // Create updated_at trigger function if not exists
    await client.query(`
      CREATE OR REPLACE FUNCTION update_updated_at_column()
      RETURNS TRIGGER AS $$
      BEGIN
        NEW.updated_at = NOW();
        RETURN NEW;
      END;
      $$ LANGUAGE plpgsql;
    `);

    // Add update triggers
    const triggerTables = [
      "shelter_contacts", "shelter_communications", "shelter_onboarding",
      "shelter_api_keys", "webhook_endpoints", "outreach_campaigns",
      "outreach_targets", "affiliate_products", "social_accounts", "social_posts"
    ];
    for (const table of triggerTables) {
      await client.query(`
        DO $$ BEGIN
          IF NOT EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_${table}_updated') THEN
            CREATE TRIGGER trg_${table}_updated BEFORE UPDATE ON ${table} FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
          END IF;
        END $$;
      `);
    }
    results.push("Update triggers: OK");
    results.push("Migration 011 complete: 16 tables created");

    await client.end();
    results.push("Done");

    return NextResponse.json({ success: true, results });
  } catch (error) {
    const message =
      error instanceof Error ? error.message : "Unknown error";
    return NextResponse.json(
      { success: false, results, error: message },
      { status: 500 }
    );
  }
}

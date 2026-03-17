-- ============================================================================
-- Migration: admin_schema.sql
-- Purpose: Create admin dashboard tables (system_config, admin_audit_log, feature_flags)
-- Author: Agent 20 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (user foreign key)
--
-- ROLLBACK: See admin_schema_rollback.sql
-- ============================================================================

-- ============================================================================
-- SYSTEM_CONFIG TABLE
-- Purpose: Store runtime-configurable system settings
-- ============================================================================
CREATE TABLE IF NOT EXISTS system_config (
    key VARCHAR(255) PRIMARY KEY,           -- Configuration key (e.g., "urgency.threshold.critical")
    value TEXT NOT NULL,                    -- Configuration value
    category VARCHAR(100),                  -- Category for grouping (urgency, auth, site, etc.)
    description TEXT,                       -- Human-readable description
    value_type VARCHAR(50) DEFAULT 'string', -- Data type: string, int, bool, json
    is_sensitive BOOLEAN DEFAULT FALSE,     -- Should value be masked in logs/exports
    requires_restart BOOLEAN DEFAULT FALSE, -- Does changing this require app restart
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    updated_by UUID REFERENCES users(id)    -- Who last updated this setting
);

-- Add comments
COMMENT ON TABLE system_config IS 'Runtime system configuration stored in database for admin management';
COMMENT ON COLUMN system_config.key IS 'Configuration key using dot notation (e.g., urgency.threshold.critical)';
COMMENT ON COLUMN system_config.value IS 'String value - parsed based on value_type';
COMMENT ON COLUMN system_config.category IS 'Category for grouping related settings';
COMMENT ON COLUMN system_config.is_sensitive IS 'If true, value should be masked in exports/logs';
COMMENT ON COLUMN system_config.requires_restart IS 'If true, changes require application restart';

-- Index for category lookups
CREATE INDEX IF NOT EXISTS idx_system_config_category ON system_config(category);

-- ============================================================================
-- ADMIN_AUDIT_LOG TABLE
-- Purpose: Log all administrative actions for compliance and debugging
-- ============================================================================
CREATE TABLE IF NOT EXISTS admin_audit_log (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,                  -- Admin user who performed action
    action VARCHAR(100) NOT NULL,           -- Action type (UPDATE_DOG, DELETE_USER, etc.)
    entity_type VARCHAR(50) NOT NULL,       -- Entity type (dog, shelter, user, config)
    entity_id VARCHAR(255) NOT NULL,        -- ID of entity being acted upon
    details JSONB,                          -- Additional details about the action
    ip_address INET,                        -- IP address of requester
    user_agent TEXT,                        -- Browser/client user agent
    created_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE admin_audit_log IS 'Audit trail of all administrative actions for compliance';
COMMENT ON COLUMN admin_audit_log.action IS 'Action performed (UPDATE_DOG, DELETE_USER, CHANGE_ROLE, etc.)';
COMMENT ON COLUMN admin_audit_log.entity_type IS 'Type of entity: dog, shelter, user, config, etc.';
COMMENT ON COLUMN admin_audit_log.entity_id IS 'ID of the affected entity';
COMMENT ON COLUMN admin_audit_log.details IS 'JSON object with additional context (old values, new values, etc.)';

-- Indexes for common query patterns
CREATE INDEX IF NOT EXISTS idx_audit_log_user_id ON admin_audit_log(user_id);
CREATE INDEX IF NOT EXISTS idx_audit_log_action ON admin_audit_log(action);
CREATE INDEX IF NOT EXISTS idx_audit_log_entity ON admin_audit_log(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_audit_log_created_at ON admin_audit_log(created_at DESC);

-- Composite index for filtering queries
CREATE INDEX IF NOT EXISTS idx_audit_log_filter ON admin_audit_log(user_id, action, entity_type, created_at DESC);

-- ============================================================================
-- FEATURE_FLAGS TABLE
-- Purpose: Feature flags for gradual rollouts and A/B testing
-- ============================================================================
CREATE TABLE IF NOT EXISTS feature_flags (
    key VARCHAR(100) PRIMARY KEY,           -- Feature flag key
    name VARCHAR(255),                      -- Human-readable name
    description TEXT,                       -- Description of what the flag controls
    enabled BOOLEAN DEFAULT FALSE,          -- Is the feature enabled
    rollout_percentage INTEGER DEFAULT 100  -- Percentage of users (0-100)
        CHECK (rollout_percentage >= 0 AND rollout_percentage <= 100),
    conditions JSONB,                       -- Additional conditions (roles, user IDs, etc.)
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE feature_flags IS 'Feature flags for gradual feature rollouts and A/B testing';
COMMENT ON COLUMN feature_flags.key IS 'Unique key for the feature flag';
COMMENT ON COLUMN feature_flags.enabled IS 'Master switch - if false, feature is completely off';
COMMENT ON COLUMN feature_flags.rollout_percentage IS 'Percentage of users to enable for (0-100)';
COMMENT ON COLUMN feature_flags.conditions IS 'JSON conditions like {"roles": ["admin"], "user_ids": [...]}';

-- ============================================================================
-- CONTENT_MODERATION TABLE
-- Purpose: Queue for content requiring admin approval
-- ============================================================================
CREATE TABLE IF NOT EXISTS content_moderation (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    content_type VARCHAR(50) NOT NULL,      -- Type: tiktok_post, blog_post, user_content
    content_id VARCHAR(255),                -- Reference to the actual content
    content_preview TEXT,                   -- Preview/summary of the content
    status VARCHAR(20) DEFAULT 'pending'    -- pending, approved, rejected
        CHECK (status IN ('pending', 'approved', 'rejected')),
    submitted_by UUID REFERENCES users(id), -- User who submitted the content
    submitted_at TIMESTAMP DEFAULT NOW(),
    reviewed_by UUID REFERENCES users(id),  -- Admin who reviewed
    reviewed_at TIMESTAMP,
    rejection_reason TEXT,                  -- Reason for rejection if applicable
    metadata JSONB                          -- Additional content-specific data
);

-- Add comments
COMMENT ON TABLE content_moderation IS 'Queue for content items requiring admin moderation';
COMMENT ON COLUMN content_moderation.content_type IS 'Type of content being moderated';
COMMENT ON COLUMN content_moderation.content_preview IS 'Text preview of the content for quick review';
COMMENT ON COLUMN content_moderation.status IS 'Current status: pending, approved, rejected';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_content_mod_status ON content_moderation(status);
CREATE INDEX IF NOT EXISTS idx_content_mod_submitted_at ON content_moderation(submitted_at DESC);
CREATE INDEX IF NOT EXISTS idx_content_mod_pending ON content_moderation(status, submitted_at DESC)
    WHERE status = 'pending';

-- ============================================================================
-- ADMIN_SESSIONS TABLE
-- Purpose: Track active admin sessions for security monitoring
-- ============================================================================
CREATE TABLE IF NOT EXISTS admin_sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id),
    session_token_hash VARCHAR(64) NOT NULL,  -- SHA256 hash of session token
    ip_address INET,
    user_agent TEXT,
    last_activity TIMESTAMP DEFAULT NOW(),
    created_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP NOT NULL,
    is_active BOOLEAN DEFAULT TRUE
);

-- Add comments
COMMENT ON TABLE admin_sessions IS 'Active admin sessions for security monitoring';

-- Index for session lookup
CREATE INDEX IF NOT EXISTS idx_admin_sessions_token ON admin_sessions(session_token_hash);
CREATE INDEX IF NOT EXISTS idx_admin_sessions_user ON admin_sessions(user_id, is_active);

-- ============================================================================
-- SCHEDULED_TASKS TABLE
-- Purpose: Track scheduled background tasks
-- ============================================================================
CREATE TABLE IF NOT EXISTS scheduled_tasks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(100) UNIQUE NOT NULL,      -- Task name
    description TEXT,                       -- What the task does
    schedule VARCHAR(100),                  -- Cron expression or interval
    last_run TIMESTAMP,
    next_run TIMESTAMP,
    status VARCHAR(20) DEFAULT 'active'     -- active, paused, disabled
        CHECK (status IN ('active', 'paused', 'disabled')),
    last_result VARCHAR(20),                -- success, failure, running
    last_error TEXT,                        -- Error message if failed
    run_count INTEGER DEFAULT 0,
    failure_count INTEGER DEFAULT 0,
    metadata JSONB,                         -- Task-specific configuration
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE scheduled_tasks IS 'Background scheduled tasks and their status';

-- Index for active tasks
CREATE INDEX IF NOT EXISTS idx_scheduled_tasks_status ON scheduled_tasks(status);
CREATE INDEX IF NOT EXISTS idx_scheduled_tasks_next_run ON scheduled_tasks(next_run)
    WHERE status = 'active';

-- ============================================================================
-- INSERT DEFAULT CONFIGURATION VALUES
-- ============================================================================
INSERT INTO system_config (key, value, category, description, value_type, is_sensitive, requires_restart) VALUES
    -- Urgency thresholds
    ('urgency.threshold.critical_hours', '24', 'urgency', 'Hours before euthanasia to mark as CRITICAL', 'int', false, false),
    ('urgency.threshold.high_hours', '72', 'urgency', 'Hours before euthanasia to mark as HIGH', 'int', false, false),
    ('urgency.threshold.medium_hours', '168', 'urgency', 'Hours before euthanasia to mark as MEDIUM', 'int', false, false),

    -- Worker configuration
    ('worker.urgency.interval_minutes', '15', 'workers', 'Minutes between urgency recalculations', 'int', false, true),
    ('worker.wait_time.interval_seconds', '1', 'workers', 'Seconds between wait time updates', 'int', false, true),
    ('worker.cleanup.interval_hours', '24', 'workers', 'Hours between cleanup runs', 'int', false, true),

    -- Pagination
    ('pagination.default_page_size', '20', 'pagination', 'Default items per page', 'int', false, false),
    ('pagination.max_page_size', '100', 'pagination', 'Maximum items per page', 'int', false, false),

    -- Authentication
    ('auth.access_token_ttl_hours', '1', 'auth', 'Access token time-to-live in hours', 'int', false, false),
    ('auth.refresh_token_ttl_days', '7', 'auth', 'Refresh token time-to-live in days', 'int', false, false),
    ('auth.max_sessions_per_user', '5', 'auth', 'Maximum concurrent sessions per user', 'int', false, false),

    -- Site settings
    ('site.name', 'WaitingTheLongest', 'site', 'Site display name', 'string', false, false),
    ('site.tagline', 'Saving Dogs One Day at a Time', 'site', 'Site tagline', 'string', false, false),
    ('site.contact_email', '', 'site', 'Contact email address', 'string', false, false),
    ('site.maintenance_mode', 'false', 'site', 'Enable maintenance mode', 'bool', false, false),

    -- Notifications
    ('notifications.email_enabled', 'true', 'notifications', 'Enable email notifications', 'bool', false, false),
    ('notifications.critical_alert_email', '', 'notifications', 'Email for critical dog alerts', 'string', false, false),
    ('notifications.daily_digest_enabled', 'true', 'notifications', 'Enable daily digest emails', 'bool', false, false),

    -- Data retention
    ('retention.audit_log_days', '90', 'retention', 'Days to keep audit log entries', 'int', false, false),
    ('retention.error_log_days', '30', 'retention', 'Days to keep error log entries', 'int', false, false),
    ('retention.session_cleanup_hours', '24', 'retention', 'Hours after which expired sessions are deleted', 'int', false, false)
ON CONFLICT (key) DO NOTHING;

-- ============================================================================
-- INSERT DEFAULT FEATURE FLAGS
-- ============================================================================
INSERT INTO feature_flags (key, name, description, enabled, rollout_percentage) VALUES
    ('feature.foster_matching', 'Foster Matching Algorithm', 'Enable intelligent foster-dog matching', true, 100),
    ('feature.urgency_alerts', 'Urgency Alerts', 'Enable urgency alert notifications', true, 100),
    ('feature.social_sharing', 'Social Sharing', 'Enable social media sharing features', true, 100),
    ('feature.advanced_search', 'Advanced Search', 'Enable advanced search filters', true, 100),
    ('feature.ai_descriptions', 'AI Dog Descriptions', 'Use AI to generate dog descriptions', false, 0),
    ('feature.video_profiles', 'Video Profiles', 'Enable video uploads for dog profiles', false, 50),
    ('feature.donation_integration', 'Donation Integration', 'Enable donation processing', false, 0),
    ('feature.mobile_app_api', 'Mobile App API', 'Enable mobile app specific endpoints', true, 100)
ON CONFLICT (key) DO NOTHING;

-- ============================================================================
-- INSERT DEFAULT SCHEDULED TASKS
-- ============================================================================
INSERT INTO scheduled_tasks (name, description, schedule, status, metadata) VALUES
    ('urgency_recalculation', 'Recalculate urgency levels for all dogs', '*/15 * * * *', 'active', '{"interval_minutes": 15}'),
    ('wait_time_update', 'Update wait time displays', '* * * * *', 'active', '{"interval_seconds": 1}'),
    ('session_cleanup', 'Clean up expired sessions', '0 * * * *', 'active', '{"keep_hours": 24}'),
    ('error_log_cleanup', 'Clean up old error logs', '0 0 * * *', 'active', '{"keep_days": 30}'),
    ('audit_log_cleanup', 'Clean up old audit logs', '0 0 * * *', 'active', '{"keep_days": 90}'),
    ('shelter_data_sync', 'Sync data from external shelter APIs', '0 */6 * * *', 'active', '{"interval_hours": 6}'),
    ('daily_digest', 'Send daily digest emails', '0 8 * * *', 'active', '{"send_hour": 8}'),
    ('statistics_update', 'Update cached statistics', '*/30 * * * *', 'active', '{"interval_minutes": 30}')
ON CONFLICT (name) DO NOTHING;

-- ============================================================================
-- FUNCTIONS
-- ============================================================================

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Trigger for system_config
DROP TRIGGER IF EXISTS update_system_config_updated_at ON system_config;
CREATE TRIGGER update_system_config_updated_at
    BEFORE UPDATE ON system_config
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for feature_flags
DROP TRIGGER IF EXISTS update_feature_flags_updated_at ON feature_flags;
CREATE TRIGGER update_feature_flags_updated_at
    BEFORE UPDATE ON feature_flags
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for scheduled_tasks
DROP TRIGGER IF EXISTS update_scheduled_tasks_updated_at ON scheduled_tasks;
CREATE TRIGGER update_scheduled_tasks_updated_at
    BEFORE UPDATE ON scheduled_tasks
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- CLEANUP FUNCTIONS
-- ============================================================================

-- Function to clean up old audit logs
CREATE OR REPLACE FUNCTION cleanup_old_audit_logs(days_to_keep INTEGER DEFAULT 90)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM admin_audit_log
    WHERE created_at < NOW() - (days_to_keep || ' days')::INTERVAL;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up expired admin sessions
CREATE OR REPLACE FUNCTION cleanup_expired_admin_sessions()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM admin_sessions
    WHERE expires_at < NOW() OR is_active = FALSE;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- GRANTS (uncomment and modify for your user)
-- ============================================================================
-- GRANT ALL ON system_config TO your_app_user;
-- GRANT ALL ON admin_audit_log TO your_app_user;
-- GRANT ALL ON feature_flags TO your_app_user;
-- GRANT ALL ON content_moderation TO your_app_user;
-- GRANT ALL ON admin_sessions TO your_app_user;
-- GRANT ALL ON scheduled_tasks TO your_app_user;

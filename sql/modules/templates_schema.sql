-- ============================================================================
-- Migration: templates_schema.sql
-- Purpose: Database schema for content templates system
-- Author: Agent 16 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DESCRIPTION:
-- Creates tables for storing and versioning content templates used for
-- social media posts, emails, SMS, and other content generation.
--
-- TABLES:
-- - content_templates: Main template storage
-- - template_versions: Version history for templates
--
-- DEPENDENCIES:
-- - uuid-ossp extension (for UUID generation)
-- - 001_extensions.sql must be run first
--
-- ROLLBACK: DROP TABLE template_versions; DROP TABLE content_templates;
-- ============================================================================

-- ============================================================================
-- CONTENT TEMPLATES TABLE
-- Purpose: Store content templates for social media, email, SMS, etc.
-- ============================================================================

CREATE TABLE IF NOT EXISTS content_templates (
    -- Primary key
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Template identification
    name VARCHAR(100) NOT NULL,                      -- Template name (e.g., "urgent_countdown")
    category VARCHAR(50) NOT NULL,                   -- Category (tiktok, instagram, facebook, twitter, email, sms)
    type VARCHAR(100),                               -- Content type (URGENT_COUNTDOWN, LONGEST_WAITING, etc.)

    -- Template content
    content TEXT NOT NULL,                           -- The actual template content with {{variables}}
    description TEXT,                                -- Human-readable description of template purpose

    -- Configuration as JSONB
    variables JSONB DEFAULT '{}',                    -- Required variables with descriptions/defaults
    config JSONB DEFAULT '{}',                       -- Additional configuration (music, overlay, etc.)

    -- Metadata
    version INTEGER DEFAULT 1,                       -- Template version number
    is_active BOOLEAN DEFAULT TRUE,                  -- Whether template is active
    is_default BOOLEAN DEFAULT FALSE,                -- Whether this is the default for its type

    -- Usage constraints
    use_for_urgency TEXT[] DEFAULT '{}',            -- Urgency levels: {critical, high, medium, normal}
    use_for_platforms TEXT[] DEFAULT '{}',          -- Platforms: {tiktok, instagram, facebook, etc.}
    hashtags TEXT[] DEFAULT '{}',                    -- Default hashtags for this template
    character_limit INTEGER DEFAULT 0,               -- Max characters (0 = no limit)
    overlay_style VARCHAR(50),                       -- Video overlay style for TikTok

    -- Audit fields
    created_by UUID,                                 -- User who created the template
    updated_by UUID,                                 -- User who last updated
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),

    -- Constraints
    CONSTRAINT content_templates_unique_name UNIQUE (category, name),
    CONSTRAINT content_templates_valid_category CHECK (
        category IN ('tiktok', 'instagram', 'facebook', 'twitter', 'email', 'sms', 'push', 'social', 'custom')
    )
);

-- Comments for documentation
COMMENT ON TABLE content_templates IS 'Content templates for social media posts, emails, SMS, and other content generation';
COMMENT ON COLUMN content_templates.name IS 'Template name - unique within category';
COMMENT ON COLUMN content_templates.category IS 'Template category (tiktok, email, sms, etc.)';
COMMENT ON COLUMN content_templates.type IS 'Content type like URGENT_COUNTDOWN, LONGEST_WAITING';
COMMENT ON COLUMN content_templates.content IS 'Template content with Mustache/Handlebars syntax {{variable}}';
COMMENT ON COLUMN content_templates.variables IS 'JSON object describing required variables';
COMMENT ON COLUMN content_templates.config IS 'Additional configuration (music, overlays, etc.)';
COMMENT ON COLUMN content_templates.use_for_urgency IS 'Array of urgency levels this template applies to';
COMMENT ON COLUMN content_templates.use_for_platforms IS 'Array of platforms this template can be used on';
COMMENT ON COLUMN content_templates.hashtags IS 'Default hashtags to include with this template';
COMMENT ON COLUMN content_templates.character_limit IS 'Maximum character limit (0 = unlimited)';
COMMENT ON COLUMN content_templates.overlay_style IS 'Video overlay style for TikTok/Reels';

-- ============================================================================
-- TEMPLATE VERSIONS TABLE
-- Purpose: Track version history of templates for rollback and auditing
-- ============================================================================

CREATE TABLE IF NOT EXISTS template_versions (
    -- Primary key
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Foreign key to main template
    template_id UUID NOT NULL REFERENCES content_templates(id) ON DELETE CASCADE,

    -- Version information
    version INTEGER NOT NULL,                        -- Version number
    content TEXT NOT NULL,                           -- Template content at this version
    config JSONB DEFAULT '{}',                       -- Config at this version

    -- Change tracking
    change_description TEXT,                         -- Description of changes
    changed_by UUID,                                 -- User who made the change

    -- Timestamp
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),

    -- Constraints
    CONSTRAINT template_versions_unique_version UNIQUE (template_id, version)
);

-- Comments for documentation
COMMENT ON TABLE template_versions IS 'Version history for content templates';
COMMENT ON COLUMN template_versions.template_id IS 'Reference to the main template';
COMMENT ON COLUMN template_versions.version IS 'Version number at time of change';
COMMENT ON COLUMN template_versions.content IS 'Template content at this version';
COMMENT ON COLUMN template_versions.change_description IS 'Description of what changed';
COMMENT ON COLUMN template_versions.changed_by IS 'User who made this change';

-- ============================================================================
-- INDEXES
-- ============================================================================

-- Index for looking up templates by category
CREATE INDEX IF NOT EXISTS idx_templates_category
    ON content_templates(category);

-- Index for looking up templates by type
CREATE INDEX IF NOT EXISTS idx_templates_type
    ON content_templates(type);

-- Index for active templates
CREATE INDEX IF NOT EXISTS idx_templates_active
    ON content_templates(is_active) WHERE is_active = TRUE;

-- Index for default templates
CREATE INDEX IF NOT EXISTS idx_templates_default
    ON content_templates(is_default) WHERE is_default = TRUE;

-- Index for urgency-based lookups using GIN
CREATE INDEX IF NOT EXISTS idx_templates_urgency
    ON content_templates USING GIN(use_for_urgency);

-- Index for platform-based lookups using GIN
CREATE INDEX IF NOT EXISTS idx_templates_platforms
    ON content_templates USING GIN(use_for_platforms);

-- Index for version history lookups
CREATE INDEX IF NOT EXISTS idx_template_versions_template_id
    ON template_versions(template_id);

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Update timestamp trigger function
CREATE OR REPLACE FUNCTION update_template_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply trigger to content_templates
DROP TRIGGER IF EXISTS trigger_update_template_timestamp ON content_templates;
CREATE TRIGGER trigger_update_template_timestamp
    BEFORE UPDATE ON content_templates
    FOR EACH ROW
    EXECUTE FUNCTION update_template_timestamp();

-- Version tracking trigger - creates version entry on update
CREATE OR REPLACE FUNCTION track_template_version()
RETURNS TRIGGER AS $$
BEGIN
    -- Only create version if content changed
    IF OLD.content IS DISTINCT FROM NEW.content THEN
        INSERT INTO template_versions (
            template_id,
            version,
            content,
            config,
            change_description,
            changed_by
        ) VALUES (
            OLD.id,
            OLD.version,
            OLD.content,
            OLD.config,
            'Content updated',
            NEW.updated_by
        );

        -- Increment version number
        NEW.version = OLD.version + 1;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply version tracking trigger
DROP TRIGGER IF EXISTS trigger_track_template_version ON content_templates;
CREATE TRIGGER trigger_track_template_version
    BEFORE UPDATE ON content_templates
    FOR EACH ROW
    EXECUTE FUNCTION track_template_version();

-- ============================================================================
-- VIEWS
-- ============================================================================

-- View of active templates with computed fields
CREATE OR REPLACE VIEW active_templates AS
SELECT
    ct.id,
    ct.name,
    ct.category,
    ct.type,
    ct.content,
    ct.description,
    ct.version,
    ct.is_default,
    ct.use_for_urgency,
    ct.use_for_platforms,
    ct.hashtags,
    ct.character_limit,
    ct.overlay_style,
    ct.created_at,
    ct.updated_at,
    LENGTH(ct.content) as content_length,
    array_length(ct.hashtags, 1) as hashtag_count,
    (SELECT COUNT(*) FROM template_versions tv WHERE tv.template_id = ct.id) as version_count
FROM content_templates ct
WHERE ct.is_active = TRUE;

-- Comment on view
COMMENT ON VIEW active_templates IS 'View of active templates with computed statistics';

-- ============================================================================
-- HELPER FUNCTIONS
-- ============================================================================

-- Function to get template by category and name
CREATE OR REPLACE FUNCTION get_template(p_category VARCHAR, p_name VARCHAR)
RETURNS TABLE (
    id UUID,
    name VARCHAR,
    category VARCHAR,
    type VARCHAR,
    content TEXT,
    description TEXT,
    variables JSONB,
    config JSONB,
    version INTEGER,
    hashtags TEXT[],
    character_limit INTEGER
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        ct.id,
        ct.name,
        ct.category,
        ct.type,
        ct.content,
        ct.description,
        ct.variables,
        ct.config,
        ct.version,
        ct.hashtags,
        ct.character_limit
    FROM content_templates ct
    WHERE ct.category = p_category
      AND ct.name = p_name
      AND ct.is_active = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Function to get templates for urgency level
CREATE OR REPLACE FUNCTION get_templates_for_urgency(p_urgency VARCHAR)
RETURNS SETOF content_templates AS $$
BEGIN
    RETURN QUERY
    SELECT *
    FROM content_templates ct
    WHERE p_urgency = ANY(ct.use_for_urgency)
      AND ct.is_active = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Function to get default template for type
CREATE OR REPLACE FUNCTION get_default_template(p_type VARCHAR)
RETURNS SETOF content_templates AS $$
BEGIN
    RETURN QUERY
    SELECT *
    FROM content_templates ct
    WHERE ct.type = p_type
      AND ct.is_active = TRUE
      AND ct.is_default = TRUE
    LIMIT 1;
END;
$$ LANGUAGE plpgsql;

-- Function to rollback to previous version
CREATE OR REPLACE FUNCTION rollback_template_version(p_template_id UUID, p_target_version INTEGER)
RETURNS content_templates AS $$
DECLARE
    v_old_content TEXT;
    v_old_config JSONB;
    v_result content_templates;
BEGIN
    -- Get the version to rollback to
    SELECT content, config INTO v_old_content, v_old_config
    FROM template_versions
    WHERE template_id = p_template_id AND version = p_target_version;

    IF v_old_content IS NULL THEN
        RAISE EXCEPTION 'Version % not found for template %', p_target_version, p_template_id;
    END IF;

    -- Update the template
    UPDATE content_templates
    SET content = v_old_content,
        config = v_old_config
    WHERE id = p_template_id
    RETURNING * INTO v_result;

    RETURN v_result;
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- SEED DATA - Default Templates
-- ============================================================================

-- Insert default TikTok urgent countdown template
INSERT INTO content_templates (name, category, type, description, content, hashtags, use_for_urgency, use_for_platforms, overlay_style, is_default)
VALUES (
    'urgent_countdown',
    'tiktok',
    'URGENT_COUNTDOWN',
    'TikTok template for dogs with urgent countdown timers',
    '{{urgencyEmoji urgency_level}} {{dog_name}} has only {{countdown_human}} left.

{{dog_name}} has waited {{wait_time_human}} for a home. Now time is running out.

{{shelter_name}}, {{shelter_location}}

#SaveThisLife #AdoptDontShop #UrgentDogs #ShelterDogs',
    ARRAY['SaveThisLife', 'AdoptDontShop', 'UrgentDogs', 'ShelterDogs'],
    ARRAY['critical', 'high'],
    ARRAY['tiktok', 'instagram_reels'],
    'countdown_red',
    TRUE
) ON CONFLICT (category, name) DO NOTHING;

-- Insert default email urgent alert template indicator
INSERT INTO content_templates (name, category, type, description, content, use_for_urgency, use_for_platforms, is_default)
VALUES (
    'urgent_alert',
    'email',
    'URGENT_ALERT',
    'Email template for urgent dog alerts',
    '-- See content_templates/email/urgent_alert.html for full template --',
    ARRAY['critical', 'high'],
    ARRAY['email'],
    TRUE
) ON CONFLICT (category, name) DO NOTHING;

-- Insert default SMS urgent alert template
INSERT INTO content_templates (name, category, type, description, content, use_for_urgency, use_for_platforms, character_limit, is_default)
VALUES (
    'urgent_alert',
    'sms',
    'URGENT_ALERT',
    'SMS template for urgent dog alerts',
    'URGENT: {{dog_name}} needs help NOW! {{#if countdown_is_critical}}Only {{countdown_human}} left.{{/if}} {{dog_breed}}, {{dog_age}}, {{shelter_location}}. Reply STOP to unsubscribe',
    ARRAY['critical', 'high'],
    ARRAY['sms'],
    160,
    TRUE
) ON CONFLICT (category, name) DO NOTHING;

-- ============================================================================
-- PERMISSIONS
-- ============================================================================

-- Grant select on templates to public (for API access)
-- GRANT SELECT ON content_templates TO api_user;
-- GRANT SELECT ON template_versions TO api_user;
-- GRANT SELECT ON active_templates TO api_user;

-- Grant full access to admin
-- GRANT ALL ON content_templates TO admin_user;
-- GRANT ALL ON template_versions TO admin_user;

-- ============================================================================
-- END OF MIGRATION
-- ============================================================================

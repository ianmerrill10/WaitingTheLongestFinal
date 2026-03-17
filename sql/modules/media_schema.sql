-- ============================================================================
-- Media System Database Schema
-- ============================================================================
-- FILE: media_schema.sql
-- PURPOSE: Database schema for media processing system including jobs queue,
--          media storage records, and related tables.
-- AUTHOR: Agent 15 - WaitingTheLongest Build System
-- DATE: 2024-01-28
-- ============================================================================

-- ============================================================================
-- MEDIA JOBS TABLE
-- ============================================================================
-- Stores all media processing jobs for async queue

CREATE TABLE IF NOT EXISTS media_jobs (
    id VARCHAR(64) PRIMARY KEY,
    job_type VARCHAR(32) NOT NULL DEFAULT 'video_generation',
    status VARCHAR(16) NOT NULL DEFAULT 'pending',
    priority VARCHAR(16) NOT NULL DEFAULT 'normal',
    dog_id VARCHAR(64) NOT NULL,

    -- JSON data fields
    input_data JSONB DEFAULT '{}',
    result_data JSONB DEFAULT '{}',

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,

    -- Progress tracking
    progress DECIMAL(5,2) DEFAULT 0.0,
    current_stage VARCHAR(128) DEFAULT '',
    error_message TEXT DEFAULT '',

    -- Retry management
    retry_count INTEGER DEFAULT 0,
    max_retries INTEGER DEFAULT 3,

    -- Webhook callback
    callback_url VARCHAR(512) DEFAULT '',

    -- Constraints
    CONSTRAINT media_jobs_status_check
        CHECK (status IN ('pending', 'processing', 'completed', 'failed', 'cancelled')),
    CONSTRAINT media_jobs_priority_check
        CHECK (priority IN ('low', 'normal', 'high', 'urgent')),
    CONSTRAINT media_jobs_type_check
        CHECK (job_type IN ('video_generation', 'image_processing', 'share_card',
                           'thumbnail', 'platform_transcode', 'batch_process', 'cleanup')),
    CONSTRAINT media_jobs_progress_check
        CHECK (progress >= 0 AND progress <= 100)
);

-- Indexes for job queries
CREATE INDEX IF NOT EXISTS idx_media_jobs_status ON media_jobs(status);
CREATE INDEX IF NOT EXISTS idx_media_jobs_dog_id ON media_jobs(dog_id);
CREATE INDEX IF NOT EXISTS idx_media_jobs_created_at ON media_jobs(created_at);
CREATE INDEX IF NOT EXISTS idx_media_jobs_priority_status
    ON media_jobs(priority, status, created_at);
CREATE INDEX IF NOT EXISTS idx_media_jobs_pending
    ON media_jobs(status, priority, created_at) WHERE status = 'pending';

-- ============================================================================
-- MEDIA RECORDS TABLE
-- ============================================================================
-- Stores metadata for all stored media files

CREATE TABLE IF NOT EXISTS media_records (
    id VARCHAR(64) PRIMARY KEY,
    dog_id VARCHAR(64) NOT NULL,
    media_type VARCHAR(32) NOT NULL DEFAULT 'image',
    filename VARCHAR(256) NOT NULL,
    storage_path VARCHAR(512) NOT NULL,
    content_type VARCHAR(128) DEFAULT 'application/octet-stream',

    -- File information
    file_size BIGINT DEFAULT 0,
    width INTEGER DEFAULT 0,
    height INTEGER DEFAULT 0,
    duration DECIMAL(10,3) DEFAULT 0.0,

    -- Storage info
    backend VARCHAR(16) DEFAULT 'local',
    url VARCHAR(1024) DEFAULT '',

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE,

    -- Additional metadata
    metadata JSONB DEFAULT '{}',

    -- Constraints
    CONSTRAINT media_records_type_check
        CHECK (media_type IN ('image', 'video', 'thumbnail', 'share_card', 'overlay', 'audio')),
    CONSTRAINT media_records_backend_check
        CHECK (backend IN ('local', 's3', 'auto'))
);

-- Indexes for media queries
CREATE INDEX IF NOT EXISTS idx_media_records_dog_id ON media_records(dog_id);
CREATE INDEX IF NOT EXISTS idx_media_records_type ON media_records(media_type);
CREATE INDEX IF NOT EXISTS idx_media_records_dog_type ON media_records(dog_id, media_type);
CREATE INDEX IF NOT EXISTS idx_media_records_created_at ON media_records(created_at);
CREATE INDEX IF NOT EXISTS idx_media_records_expires_at ON media_records(expires_at)
    WHERE expires_at IS NOT NULL;

-- ============================================================================
-- VIDEO TEMPLATES TABLE
-- ============================================================================
-- Stores custom video templates

CREATE TABLE IF NOT EXISTS video_templates (
    id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    description TEXT DEFAULT '',
    template_type VARCHAR(32) NOT NULL DEFAULT 'slideshow',

    -- Configuration (JSON)
    config JSONB NOT NULL DEFAULT '{}',

    -- Output settings
    output_width INTEGER DEFAULT 1920,
    output_height INTEGER DEFAULT 1080,
    output_fps INTEGER DEFAULT 30,

    -- Status
    is_active BOOLEAN DEFAULT true,
    is_default BOOLEAN DEFAULT false,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    CONSTRAINT video_templates_type_check
        CHECK (template_type IN ('slideshow', 'ken_burns', 'countdown', 'story',
                                 'promotional', 'quick_share', 'adoption_appeal'))
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_video_templates_type ON video_templates(template_type);
CREATE INDEX IF NOT EXISTS idx_video_templates_active ON video_templates(is_active);

-- ============================================================================
-- OVERLAY CONFIGS TABLE
-- ============================================================================
-- Stores overlay configurations

CREATE TABLE IF NOT EXISTS overlay_configs (
    id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    description TEXT DEFAULT '',

    -- Configuration (JSON)
    led_counter_config JSONB DEFAULT '{}',
    urgency_badge_config JSONB DEFAULT '{}',
    text_overlays JSONB DEFAULT '[]',
    logo_config JSONB DEFAULT '{}',

    -- Status
    is_active BOOLEAN DEFAULT true,
    is_default BOOLEAN DEFAULT false,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_overlay_configs_active ON overlay_configs(is_active);

-- ============================================================================
-- MEDIA STATS TABLE
-- ============================================================================
-- Stores media generation statistics

CREATE TABLE IF NOT EXISTS media_stats (
    id SERIAL PRIMARY KEY,
    stat_date DATE NOT NULL DEFAULT CURRENT_DATE,
    stat_type VARCHAR(32) NOT NULL,

    -- Counts
    total_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    failure_count INTEGER DEFAULT 0,

    -- Performance
    avg_processing_time_ms DECIMAL(10,2) DEFAULT 0.0,
    total_processing_time_ms BIGINT DEFAULT 0,

    -- Storage
    total_bytes_processed BIGINT DEFAULT 0,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,

    -- Unique constraint for daily stats
    UNIQUE(stat_date, stat_type)
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_media_stats_date ON media_stats(stat_date);
CREATE INDEX IF NOT EXISTS idx_media_stats_type ON media_stats(stat_type);

-- ============================================================================
-- PLATFORM VERSIONS TABLE
-- ============================================================================
-- Stores platform-specific video versions

CREATE TABLE IF NOT EXISTS platform_versions (
    id VARCHAR(64) PRIMARY KEY,
    source_media_id VARCHAR(64) NOT NULL,
    platform VARCHAR(32) NOT NULL,

    -- File info
    storage_path VARCHAR(512) NOT NULL,
    url VARCHAR(1024) DEFAULT '',
    file_size BIGINT DEFAULT 0,
    width INTEGER DEFAULT 0,
    height INTEGER DEFAULT 0,
    duration DECIMAL(10,3) DEFAULT 0.0,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,

    -- Foreign key
    CONSTRAINT fk_platform_versions_source
        FOREIGN KEY (source_media_id) REFERENCES media_records(id) ON DELETE CASCADE,

    -- Constraints
    CONSTRAINT platform_versions_platform_check
        CHECK (platform IN ('youtube', 'instagram', 'tiktok', 'facebook', 'twitter', 'original'))
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_platform_versions_source ON platform_versions(source_media_id);
CREATE INDEX IF NOT EXISTS idx_platform_versions_platform ON platform_versions(platform);

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Update timestamp trigger function
CREATE OR REPLACE FUNCTION update_media_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply triggers
DROP TRIGGER IF EXISTS trigger_video_templates_updated ON video_templates;
CREATE TRIGGER trigger_video_templates_updated
    BEFORE UPDATE ON video_templates
    FOR EACH ROW
    EXECUTE FUNCTION update_media_updated_at();

DROP TRIGGER IF EXISTS trigger_overlay_configs_updated ON overlay_configs;
CREATE TRIGGER trigger_overlay_configs_updated
    BEFORE UPDATE ON overlay_configs
    FOR EACH ROW
    EXECUTE FUNCTION update_media_updated_at();

DROP TRIGGER IF EXISTS trigger_media_stats_updated ON media_stats;
CREATE TRIGGER trigger_media_stats_updated
    BEFORE UPDATE ON media_stats
    FOR EACH ROW
    EXECUTE FUNCTION update_media_updated_at();

-- ============================================================================
-- DEFAULT DATA
-- ============================================================================

-- Default video template
INSERT INTO video_templates (id, name, description, template_type, is_default, config)
VALUES (
    'default_slideshow',
    'Default Slideshow',
    'Standard slideshow template with fade transitions',
    'slideshow',
    true,
    '{
        "duration_per_photo": 3.0,
        "transition_duration": 0.5,
        "transition_style": "fade",
        "motion_type": "none",
        "music_style": "uplifting",
        "music_volume": 0.3
    }'::jsonb
) ON CONFLICT (id) DO NOTHING;

INSERT INTO video_templates (id, name, description, template_type, is_default, config)
VALUES (
    'ken_burns_template',
    'Ken Burns Effect',
    'Cinematic zoom and pan effects',
    'ken_burns',
    false,
    '{
        "duration_per_photo": 4.0,
        "transition_duration": 0.8,
        "transition_style": "crossfade",
        "motion_type": "ken_burns",
        "motion_intensity": 0.3,
        "music_style": "emotional",
        "music_volume": 0.25
    }'::jsonb
) ON CONFLICT (id) DO NOTHING;

INSERT INTO video_templates (id, name, description, template_type, is_default, config)
VALUES (
    'countdown_template',
    'Countdown Urgency',
    'Template with countdown timer for urgent adoptions',
    'countdown',
    false,
    '{
        "duration_per_photo": 2.5,
        "transition_duration": 0.3,
        "transition_style": "fade",
        "motion_type": "slight_zoom",
        "music_style": "urgent",
        "music_volume": 0.35,
        "show_countdown": true
    }'::jsonb
) ON CONFLICT (id) DO NOTHING;

-- Default overlay config
INSERT INTO overlay_configs (id, name, description, is_default, led_counter_config, urgency_badge_config)
VALUES (
    'default_overlay',
    'Default Overlay',
    'Standard overlay with wait time counter',
    true,
    '{
        "enabled": true,
        "position": "top_right",
        "digit_width": 40,
        "digit_height": 60,
        "digit_spacing": 4,
        "background_color": {"r": 0, "g": 0, "b": 0, "a": 180},
        "on_color": {"r": 255, "g": 50, "b": 50, "a": 255},
        "off_color": {"r": 30, "g": 30, "b": 30, "a": 255},
        "label": "WAITING",
        "show_labels": true
    }'::jsonb,
    '{
        "enabled": true,
        "position": "top_left",
        "width": 120,
        "height": 40,
        "show_on_urgency": ["warning", "critical"],
        "critical_color": {"r": 220, "g": 53, "b": 69, "a": 255},
        "warning_color": {"r": 255, "g": 193, "b": 7, "a": 255}
    }'::jsonb
) ON CONFLICT (id) DO NOTHING;

-- ============================================================================
-- COMMENTS
-- ============================================================================

COMMENT ON TABLE media_jobs IS 'Queue for async media processing jobs';
COMMENT ON TABLE media_records IS 'Metadata for stored media files';
COMMENT ON TABLE video_templates IS 'Custom video generation templates';
COMMENT ON TABLE overlay_configs IS 'Overlay configuration presets';
COMMENT ON TABLE media_stats IS 'Daily media processing statistics';
COMMENT ON TABLE platform_versions IS 'Platform-specific video versions';

COMMENT ON COLUMN media_jobs.input_data IS 'JSON input data for job processing';
COMMENT ON COLUMN media_jobs.result_data IS 'JSON result data after processing';
COMMENT ON COLUMN media_jobs.progress IS 'Processing progress 0-100';
COMMENT ON COLUMN media_records.duration IS 'Duration in seconds for video/audio';
COMMENT ON COLUMN media_records.metadata IS 'Additional metadata as JSON';

-- ============================================================================
-- GRANTS (adjust role names as needed)
-- ============================================================================

-- GRANT SELECT, INSERT, UPDATE, DELETE ON media_jobs TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON media_records TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON video_templates TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON overlay_configs TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON media_stats TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON platform_versions TO wtl_app;
-- GRANT USAGE, SELECT ON SEQUENCE media_stats_id_seq TO wtl_app;

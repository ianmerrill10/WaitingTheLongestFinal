-- ============================================================================
-- Migration: aggregator_schema.sql
-- Purpose: Create tables for external API aggregator system
-- Author: Agent 19 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (user foreign key for audit)
-- - 006_dogs.sql (dog foreign key)
-- - 007_shelters.sql (shelter foreign key)
--
-- ROLLBACK: See aggregator_schema_rollback.sql
-- ============================================================================

-- ============================================================================
-- AGGREGATOR_SOURCES TABLE
-- Purpose: Store configuration for each external data source
-- ============================================================================
CREATE TABLE IF NOT EXISTS aggregator_sources (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(100) UNIQUE NOT NULL,         -- Unique source name (rescuegroups, shelter_direct, etc.) NOTE: NO Petfinder API exists
    display_name VARCHAR(255),                 -- Human-readable name
    source_type VARCHAR(50) NOT NULL           -- Type: rescuegroups, shelter_direct, custom (NO Petfinder API)
        CHECK (source_type IN ('rescuegroups', 'shelter_direct', 'custom')),
    api_base_url VARCHAR(500),                 -- Base URL for API
    enabled BOOLEAN DEFAULT TRUE,              -- Whether source is active
    priority INTEGER DEFAULT 0,                -- Sync priority (higher = first)
    rate_limit_requests INTEGER DEFAULT 100,   -- Requests per period
    rate_limit_period_seconds INTEGER DEFAULT 60, -- Rate limit period
    auth_type VARCHAR(50) DEFAULT 'none'       -- none, api_key, oauth2, basic
        CHECK (auth_type IN ('none', 'api_key', 'oauth2', 'basic')),
    auth_config JSONB,                         -- Auth configuration (encrypted)
    sync_config JSONB,                         -- Source-specific sync configuration
    field_mappings JSONB,                      -- Field mapping overrides
    filters JSONB,                             -- Default filters for this source
    last_sync_at TIMESTAMP,
    last_sync_status VARCHAR(20)               -- success, failure, partial
        CHECK (last_sync_status IN ('success', 'failure', 'partial', 'never')),
    last_sync_error TEXT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE aggregator_sources IS 'Configuration for external data source aggregators';
COMMENT ON COLUMN aggregator_sources.name IS 'Unique identifier for the source (lowercase, no spaces)';
COMMENT ON COLUMN aggregator_sources.source_type IS 'Type of aggregator implementation to use';
COMMENT ON COLUMN aggregator_sources.auth_config IS 'Encrypted authentication configuration';
COMMENT ON COLUMN aggregator_sources.sync_config IS 'Source-specific sync settings (intervals, pagination, etc.)';
COMMENT ON COLUMN aggregator_sources.field_mappings IS 'Custom field mappings to override defaults';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_aggregator_sources_enabled ON aggregator_sources(enabled);
CREATE INDEX IF NOT EXISTS idx_aggregator_sources_type ON aggregator_sources(source_type);

-- ============================================================================
-- AGGREGATOR_SYNC_HISTORY TABLE
-- Purpose: Track sync operations and their results
-- ============================================================================
CREATE TABLE IF NOT EXISTS aggregator_sync_history (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID NOT NULL REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    source_name VARCHAR(100) NOT NULL,         -- Denormalized for quick queries
    sync_type VARCHAR(20) NOT NULL             -- full, incremental, manual
        CHECK (sync_type IN ('full', 'incremental', 'manual', 'scheduled')),
    status VARCHAR(20) NOT NULL                -- running, completed, failed, cancelled
        CHECK (status IN ('running', 'completed', 'failed', 'cancelled')),
    started_at TIMESTAMP NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP,
    duration_ms BIGINT,                        -- Duration in milliseconds

    -- Statistics
    dogs_fetched INTEGER DEFAULT 0,
    dogs_added INTEGER DEFAULT 0,
    dogs_updated INTEGER DEFAULT 0,
    dogs_unchanged INTEGER DEFAULT 0,
    dogs_removed INTEGER DEFAULT 0,
    dogs_failed INTEGER DEFAULT 0,
    shelters_added INTEGER DEFAULT 0,
    shelters_updated INTEGER DEFAULT 0,

    -- Counts
    total_requests INTEGER DEFAULT 0,
    successful_requests INTEGER DEFAULT 0,
    failed_requests INTEGER DEFAULT 0,
    rate_limited_requests INTEGER DEFAULT 0,

    -- Errors
    error_count INTEGER DEFAULT 0,
    warning_count INTEGER DEFAULT 0,
    last_error TEXT,
    errors JSONB,                              -- Array of error details

    -- Metadata
    triggered_by UUID REFERENCES users(id),    -- User who triggered (if manual)
    metadata JSONB                             -- Additional sync metadata
);

-- Add comments
COMMENT ON TABLE aggregator_sync_history IS 'History of sync operations for each aggregator source';
COMMENT ON COLUMN aggregator_sync_history.sync_type IS 'Type of sync: full resync, incremental update, or manual trigger';
COMMENT ON COLUMN aggregator_sync_history.dogs_fetched IS 'Total dogs returned from API';
COMMENT ON COLUMN aggregator_sync_history.dogs_added IS 'New dogs added to database';
COMMENT ON COLUMN aggregator_sync_history.dogs_updated IS 'Existing dogs updated';
COMMENT ON COLUMN aggregator_sync_history.dogs_unchanged IS 'Dogs that had no changes';

-- Indexes for common queries
CREATE INDEX IF NOT EXISTS idx_sync_history_source ON aggregator_sync_history(source_id);
CREATE INDEX IF NOT EXISTS idx_sync_history_source_name ON aggregator_sync_history(source_name);
CREATE INDEX IF NOT EXISTS idx_sync_history_started ON aggregator_sync_history(started_at DESC);
CREATE INDEX IF NOT EXISTS idx_sync_history_status ON aggregator_sync_history(status);
CREATE INDEX IF NOT EXISTS idx_sync_history_source_started ON aggregator_sync_history(source_id, started_at DESC);

-- ============================================================================
-- AGGREGATOR_ERRORS TABLE
-- Purpose: Detailed error tracking for sync operations
-- ============================================================================
CREATE TABLE IF NOT EXISTS aggregator_errors (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID NOT NULL REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    sync_id UUID REFERENCES aggregator_sync_history(id) ON DELETE CASCADE,
    error_type VARCHAR(50) NOT NULL,           -- auth, network, rate_limit, parse, validation, unknown
    error_code VARCHAR(50),                    -- API error code if applicable
    error_message TEXT NOT NULL,
    request_url TEXT,                          -- URL that caused error
    request_method VARCHAR(10),                -- HTTP method
    response_status INTEGER,                   -- HTTP status code
    response_body TEXT,                        -- Response body (truncated)
    retry_count INTEGER DEFAULT 0,
    resolved BOOLEAN DEFAULT FALSE,
    resolved_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE aggregator_errors IS 'Detailed error logs for aggregator sync operations';
COMMENT ON COLUMN aggregator_errors.error_type IS 'Category of error: auth, network, rate_limit, parse, validation';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_aggregator_errors_source ON aggregator_errors(source_id);
CREATE INDEX IF NOT EXISTS idx_aggregator_errors_sync ON aggregator_errors(sync_id);
CREATE INDEX IF NOT EXISTS idx_aggregator_errors_created ON aggregator_errors(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_aggregator_errors_unresolved ON aggregator_errors(source_id, resolved)
    WHERE resolved = FALSE;

-- ============================================================================
-- EXTERNAL_DOG_MAPPINGS TABLE
-- Purpose: Map external IDs to internal dog IDs for deduplication
-- ============================================================================
CREATE TABLE IF NOT EXISTS external_dog_mappings (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    internal_dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    source_id UUID NOT NULL REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    source_name VARCHAR(100) NOT NULL,         -- Denormalized for quick lookup
    external_id VARCHAR(255) NOT NULL,         -- ID from external source
    external_url TEXT,                         -- URL on external site
    external_data JSONB,                       -- Raw data from source (for comparison)
    data_hash VARCHAR(64),                     -- SHA256 hash for change detection
    first_seen_at TIMESTAMP DEFAULT NOW(),
    last_seen_at TIMESTAMP DEFAULT NOW(),
    last_synced_at TIMESTAMP DEFAULT NOW(),
    sync_count INTEGER DEFAULT 1,
    is_primary BOOLEAN DEFAULT FALSE,          -- Is this the primary source for this dog
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique constraint: one mapping per dog per source
    UNIQUE(source_id, external_id)
);

-- Add comments
COMMENT ON TABLE external_dog_mappings IS 'Maps external dog IDs to internal IDs for deduplication';
COMMENT ON COLUMN external_dog_mappings.external_id IS 'ID of the dog in the external system';
COMMENT ON COLUMN external_dog_mappings.data_hash IS 'Hash of external data for change detection';
COMMENT ON COLUMN external_dog_mappings.is_primary IS 'If true, this source is the primary data source for this dog';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_dog_mappings_internal ON external_dog_mappings(internal_dog_id);
CREATE INDEX IF NOT EXISTS idx_dog_mappings_external ON external_dog_mappings(source_id, external_id);
CREATE INDEX IF NOT EXISTS idx_dog_mappings_source_name ON external_dog_mappings(source_name, external_id);
CREATE INDEX IF NOT EXISTS idx_dog_mappings_last_seen ON external_dog_mappings(last_seen_at);

-- ============================================================================
-- EXTERNAL_SHELTER_MAPPINGS TABLE
-- Purpose: Map external shelter IDs to internal shelter IDs
-- ============================================================================
CREATE TABLE IF NOT EXISTS external_shelter_mappings (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    internal_shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
    source_id UUID NOT NULL REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    source_name VARCHAR(100) NOT NULL,
    external_id VARCHAR(255) NOT NULL,
    external_url TEXT,
    external_data JSONB,
    data_hash VARCHAR(64),
    first_seen_at TIMESTAMP DEFAULT NOW(),
    last_seen_at TIMESTAMP DEFAULT NOW(),
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    UNIQUE(source_id, external_id)
);

-- Add comments
COMMENT ON TABLE external_shelter_mappings IS 'Maps external shelter IDs to internal IDs';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_shelter_mappings_internal ON external_shelter_mappings(internal_shelter_id);
CREATE INDEX IF NOT EXISTS idx_shelter_mappings_external ON external_shelter_mappings(source_id, external_id);

-- ============================================================================
-- DIRECT_FEEDS TABLE
-- Purpose: Configuration for direct shelter feeds
-- ============================================================================
CREATE TABLE IF NOT EXISTS direct_feeds (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID NOT NULL REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
    feed_name VARCHAR(255) NOT NULL,
    feed_url VARCHAR(500) NOT NULL,
    feed_format VARCHAR(20) DEFAULT 'json'     -- json, xml, csv
        CHECK (feed_format IN ('json', 'xml', 'csv')),
    enabled BOOLEAN DEFAULT TRUE,
    auth_type VARCHAR(20) DEFAULT 'none'
        CHECK (auth_type IN ('none', 'basic', 'bearer', 'api_key')),
    auth_value TEXT,                           -- Encrypted auth value
    animals_path VARCHAR(255) DEFAULT 'animals', -- JSON path to animals array
    field_mappings JSONB,                      -- Custom field mappings
    last_fetched_at TIMESTAMP,
    last_status VARCHAR(20),
    last_error TEXT,
    fetch_count INTEGER DEFAULT 0,
    error_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE direct_feeds IS 'Configuration for direct shelter data feeds';
COMMENT ON COLUMN direct_feeds.feed_url IS 'URL of the feed endpoint';
COMMENT ON COLUMN direct_feeds.animals_path IS 'JSON path to the animals array in the response';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_direct_feeds_source ON direct_feeds(source_id);
CREATE INDEX IF NOT EXISTS idx_direct_feeds_shelter ON direct_feeds(shelter_id);
CREATE INDEX IF NOT EXISTS idx_direct_feeds_enabled ON direct_feeds(enabled);

-- ============================================================================
-- DUPLICATE_CANDIDATES TABLE
-- Purpose: Track potential duplicate dogs for manual review
-- ============================================================================
CREATE TABLE IF NOT EXISTS duplicate_candidates (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id_1 UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    dog_id_2 UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    match_score DECIMAL(5,4) NOT NULL,         -- Similarity score (0.0000-1.0000)
    match_type VARCHAR(50) NOT NULL,           -- external_id, name_shelter, name_breed_age, image
    match_details JSONB,                       -- Details about the match
    status VARCHAR(20) DEFAULT 'pending'       -- pending, confirmed, rejected, auto_merged
        CHECK (status IN ('pending', 'confirmed', 'rejected', 'auto_merged')),
    reviewed_by UUID REFERENCES users(id),
    reviewed_at TIMESTAMP,
    resolution_notes TEXT,
    created_at TIMESTAMP DEFAULT NOW(),

    -- Ensure ordered pair to avoid duplicates
    CHECK (dog_id_1 < dog_id_2),
    UNIQUE(dog_id_1, dog_id_2)
);

-- Add comments
COMMENT ON TABLE duplicate_candidates IS 'Potential duplicate dogs identified by the duplicate detector';
COMMENT ON COLUMN duplicate_candidates.match_score IS 'Similarity score from 0 (no match) to 1 (perfect match)';
COMMENT ON COLUMN duplicate_candidates.match_type IS 'How the duplicate was detected';

-- Indexes
CREATE INDEX IF NOT EXISTS idx_duplicates_dog1 ON duplicate_candidates(dog_id_1);
CREATE INDEX IF NOT EXISTS idx_duplicates_dog2 ON duplicate_candidates(dog_id_2);
CREATE INDEX IF NOT EXISTS idx_duplicates_pending ON duplicate_candidates(status)
    WHERE status = 'pending';
CREATE INDEX IF NOT EXISTS idx_duplicates_score ON duplicate_candidates(match_score DESC);

-- ============================================================================
-- AGGREGATOR_RATE_LIMITS TABLE
-- Purpose: Track rate limit state for each source
-- ============================================================================
CREATE TABLE IF NOT EXISTS aggregator_rate_limits (
    source_id UUID PRIMARY KEY REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    tokens_remaining DECIMAL(10,2) NOT NULL DEFAULT 100,
    max_tokens DECIMAL(10,2) NOT NULL DEFAULT 100,
    refill_rate DECIMAL(10,4) NOT NULL DEFAULT 1.6667, -- Tokens per second
    last_refill_at TIMESTAMP DEFAULT NOW(),
    reset_at TIMESTAMP,                        -- When rate limit resets (from API headers)
    retry_after_seconds INTEGER,               -- Retry-After header value
    is_rate_limited BOOLEAN DEFAULT FALSE,
    rate_limited_at TIMESTAMP,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE aggregator_rate_limits IS 'Token bucket state for rate limiting each source';
COMMENT ON COLUMN aggregator_rate_limits.tokens_remaining IS 'Current available tokens';
COMMENT ON COLUMN aggregator_rate_limits.refill_rate IS 'Tokens added per second';

-- ============================================================================
-- AGGREGATOR_METRICS TABLE
-- Purpose: Store aggregated metrics for dashboards
-- ============================================================================
CREATE TABLE IF NOT EXISTS aggregator_metrics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID REFERENCES aggregator_sources(id) ON DELETE CASCADE,
    metric_date DATE NOT NULL,

    -- Sync metrics
    total_syncs INTEGER DEFAULT 0,
    successful_syncs INTEGER DEFAULT 0,
    failed_syncs INTEGER DEFAULT 0,
    total_sync_duration_ms BIGINT DEFAULT 0,
    avg_sync_duration_ms INTEGER,

    -- Dog metrics
    dogs_fetched INTEGER DEFAULT 0,
    dogs_added INTEGER DEFAULT 0,
    dogs_updated INTEGER DEFAULT 0,
    dogs_removed INTEGER DEFAULT 0,

    -- Request metrics
    total_requests INTEGER DEFAULT 0,
    failed_requests INTEGER DEFAULT 0,
    rate_limited_count INTEGER DEFAULT 0,

    -- Error metrics
    error_count INTEGER DEFAULT 0,
    auth_errors INTEGER DEFAULT 0,
    network_errors INTEGER DEFAULT 0,
    parse_errors INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    UNIQUE(source_id, metric_date)
);

-- Add comments
COMMENT ON TABLE aggregator_metrics IS 'Daily aggregated metrics for each source';

-- Index for time-series queries
CREATE INDEX IF NOT EXISTS idx_aggregator_metrics_date ON aggregator_metrics(metric_date DESC);
CREATE INDEX IF NOT EXISTS idx_aggregator_metrics_source_date ON aggregator_metrics(source_id, metric_date DESC);

-- ============================================================================
-- INSERT DEFAULT SOURCES
-- ============================================================================
-- DEPRECATED: No Petfinder API exists. The petfinder seed row has been removed.
INSERT INTO aggregator_sources (name, display_name, source_type, api_base_url, auth_type, sync_config) VALUES
    ('rescuegroups', 'RescueGroups.org', 'rescuegroups', 'https://api.rescuegroups.org/v5', 'api_key',
     '{"sync_interval_hours": 6, "page_size": 250, "max_pages": 50}'),
    ('shelter_direct', 'Direct Shelter Feeds', 'shelter_direct', NULL, 'none',
     '{"sync_interval_hours": 4}')
ON CONFLICT (name) DO NOTHING;

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Trigger for aggregator_sources updated_at
DROP TRIGGER IF EXISTS update_aggregator_sources_updated_at ON aggregator_sources;
CREATE TRIGGER update_aggregator_sources_updated_at
    BEFORE UPDATE ON aggregator_sources
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for external_dog_mappings updated_at
DROP TRIGGER IF EXISTS update_dog_mappings_updated_at ON external_dog_mappings;
CREATE TRIGGER update_dog_mappings_updated_at
    BEFORE UPDATE ON external_dog_mappings
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for external_shelter_mappings updated_at
DROP TRIGGER IF EXISTS update_shelter_mappings_updated_at ON external_shelter_mappings;
CREATE TRIGGER update_shelter_mappings_updated_at
    BEFORE UPDATE ON external_shelter_mappings
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for direct_feeds updated_at
DROP TRIGGER IF EXISTS update_direct_feeds_updated_at ON direct_feeds;
CREATE TRIGGER update_direct_feeds_updated_at
    BEFORE UPDATE ON direct_feeds
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Trigger for aggregator_metrics updated_at
DROP TRIGGER IF EXISTS update_aggregator_metrics_updated_at ON aggregator_metrics;
CREATE TRIGGER update_aggregator_metrics_updated_at
    BEFORE UPDATE ON aggregator_metrics
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- FUNCTIONS
-- ============================================================================

-- Function to update sync history on completion
CREATE OR REPLACE FUNCTION complete_sync_history(
    p_sync_id UUID,
    p_status VARCHAR(20),
    p_dogs_added INTEGER,
    p_dogs_updated INTEGER,
    p_dogs_unchanged INTEGER,
    p_error_count INTEGER
) RETURNS VOID AS $$
BEGIN
    UPDATE aggregator_sync_history
    SET
        status = p_status,
        completed_at = NOW(),
        duration_ms = EXTRACT(EPOCH FROM (NOW() - started_at)) * 1000,
        dogs_added = p_dogs_added,
        dogs_updated = p_dogs_updated,
        dogs_unchanged = p_dogs_unchanged,
        error_count = p_error_count
    WHERE id = p_sync_id;
END;
$$ LANGUAGE plpgsql;

-- Function to get sync stats for a source
CREATE OR REPLACE FUNCTION get_source_sync_stats(p_source_name VARCHAR)
RETURNS TABLE (
    total_syncs BIGINT,
    successful_syncs BIGINT,
    failed_syncs BIGINT,
    total_dogs_added BIGINT,
    total_dogs_updated BIGINT,
    avg_duration_ms NUMERIC,
    last_sync_at TIMESTAMP,
    last_sync_status VARCHAR
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        COUNT(*)::BIGINT as total_syncs,
        COUNT(*) FILTER (WHERE ash.status = 'completed')::BIGINT as successful_syncs,
        COUNT(*) FILTER (WHERE ash.status = 'failed')::BIGINT as failed_syncs,
        COALESCE(SUM(ash.dogs_added), 0)::BIGINT as total_dogs_added,
        COALESCE(SUM(ash.dogs_updated), 0)::BIGINT as total_dogs_updated,
        AVG(ash.duration_ms) as avg_duration_ms,
        MAX(ash.started_at) as last_sync_at,
        (SELECT sh.status FROM aggregator_sync_history sh
         WHERE sh.source_name = p_source_name
         ORDER BY sh.started_at DESC LIMIT 1) as last_sync_status
    FROM aggregator_sync_history ash
    WHERE ash.source_name = p_source_name;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up old sync history
CREATE OR REPLACE FUNCTION cleanup_old_sync_history(days_to_keep INTEGER DEFAULT 30)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM aggregator_sync_history
    WHERE started_at < NOW() - (days_to_keep || ' days')::INTERVAL
    AND status != 'running';

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up old aggregator errors
CREATE OR REPLACE FUNCTION cleanup_old_aggregator_errors(days_to_keep INTEGER DEFAULT 14)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM aggregator_errors
    WHERE created_at < NOW() - (days_to_keep || ' days')::INTERVAL
    AND resolved = TRUE;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Function to update daily metrics
CREATE OR REPLACE FUNCTION update_aggregator_daily_metrics(
    p_source_id UUID,
    p_dogs_added INTEGER DEFAULT 0,
    p_dogs_updated INTEGER DEFAULT 0,
    p_sync_duration_ms BIGINT DEFAULT 0,
    p_sync_success BOOLEAN DEFAULT TRUE,
    p_error_count INTEGER DEFAULT 0
) RETURNS VOID AS $$
BEGIN
    INSERT INTO aggregator_metrics (
        source_id, metric_date,
        total_syncs, successful_syncs, failed_syncs,
        total_sync_duration_ms, dogs_added, dogs_updated, error_count
    ) VALUES (
        p_source_id, CURRENT_DATE,
        1,
        CASE WHEN p_sync_success THEN 1 ELSE 0 END,
        CASE WHEN NOT p_sync_success THEN 1 ELSE 0 END,
        p_sync_duration_ms, p_dogs_added, p_dogs_updated, p_error_count
    )
    ON CONFLICT (source_id, metric_date) DO UPDATE SET
        total_syncs = aggregator_metrics.total_syncs + 1,
        successful_syncs = aggregator_metrics.successful_syncs +
            CASE WHEN p_sync_success THEN 1 ELSE 0 END,
        failed_syncs = aggregator_metrics.failed_syncs +
            CASE WHEN NOT p_sync_success THEN 1 ELSE 0 END,
        total_sync_duration_ms = aggregator_metrics.total_sync_duration_ms + p_sync_duration_ms,
        avg_sync_duration_ms = (aggregator_metrics.total_sync_duration_ms + p_sync_duration_ms) /
            (aggregator_metrics.total_syncs + 1),
        dogs_added = aggregator_metrics.dogs_added + p_dogs_added,
        dogs_updated = aggregator_metrics.dogs_updated + p_dogs_updated,
        error_count = aggregator_metrics.error_count + p_error_count,
        updated_at = NOW();
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- GRANTS (uncomment and modify for your user)
-- ============================================================================
-- GRANT ALL ON aggregator_sources TO your_app_user;
-- GRANT ALL ON aggregator_sync_history TO your_app_user;
-- GRANT ALL ON aggregator_errors TO your_app_user;
-- GRANT ALL ON external_dog_mappings TO your_app_user;
-- GRANT ALL ON external_shelter_mappings TO your_app_user;
-- GRANT ALL ON direct_feeds TO your_app_user;
-- GRANT ALL ON duplicate_candidates TO your_app_user;
-- GRANT ALL ON aggregator_rate_limits TO your_app_user;
-- GRANT ALL ON aggregator_metrics TO your_app_user;

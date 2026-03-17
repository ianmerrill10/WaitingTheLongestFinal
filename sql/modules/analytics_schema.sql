-- ============================================================================
-- Migration: analytics_schema.sql
-- Purpose: Create analytics tracking tables for WaitingTheLongest.com
-- Author: Agent 17 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES: 001_extensions.sql (for UUID generation)
--
-- TABLES CREATED:
--   - analytics_events: Raw analytics event data
--   - daily_metrics: Aggregated daily metrics
--   - impact_tracking: Life-saving impact events
--   - social_content: Social media content registry
--   - social_performance: Social media metrics
--
-- ROLLBACK: DROP TABLE IF EXISTS analytics_events, daily_metrics,
--           impact_tracking, social_content, social_performance CASCADE;
-- ============================================================================

-- ============================================================================
-- ANALYTICS EVENTS TABLE
-- Purpose: Store raw analytics events from user interactions
-- ============================================================================
CREATE TABLE IF NOT EXISTS analytics_events (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Event identification
    event_type VARCHAR(50) NOT NULL,          -- PAGE_VIEW, DOG_VIEW, SEARCH, etc.
    entity_type VARCHAR(50),                  -- dog, shelter, user, page, etc.
    entity_id VARCHAR(255),                   -- ID of the entity

    -- User context
    user_id UUID,                             -- User ID if authenticated
    session_id VARCHAR(255),                  -- Session identifier
    visitor_id VARCHAR(255),                  -- Anonymous visitor ID

    -- Source tracking
    source VARCHAR(50) DEFAULT 'web',         -- web, mobile, api, import
    referrer TEXT,                            -- HTTP referrer
    utm_campaign VARCHAR(255),                -- UTM campaign
    utm_source VARCHAR(255),                  -- UTM source
    utm_medium VARCHAR(255),                  -- UTM medium
    page_url TEXT,                            -- Page URL where event occurred

    -- Device context
    device_type VARCHAR(20),                  -- desktop, mobile, tablet
    browser VARCHAR(100),                     -- Browser name
    os VARCHAR(100),                          -- Operating system
    ip_hash VARCHAR(64),                      -- Hashed IP for privacy
    country_code VARCHAR(2),                  -- Geographic country
    region_code VARCHAR(10),                  -- Geographic state/region
    city VARCHAR(100),                        -- Geographic city

    -- Event data
    data JSONB DEFAULT '{}',                  -- Additional event-specific data

    -- Timestamps
    timestamp TIMESTAMP NOT NULL DEFAULT NOW(),
    received_at TIMESTAMP DEFAULT NOW(),

    -- Processing
    is_processed BOOLEAN DEFAULT FALSE,       -- Has been aggregated

    -- Standard timestamps
    created_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for analytics_events
CREATE INDEX IF NOT EXISTS idx_analytics_events_event_type ON analytics_events(event_type);
CREATE INDEX IF NOT EXISTS idx_analytics_events_entity ON analytics_events(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_analytics_events_user ON analytics_events(user_id) WHERE user_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_analytics_events_session ON analytics_events(session_id);
CREATE INDEX IF NOT EXISTS idx_analytics_events_timestamp ON analytics_events(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_analytics_events_date ON analytics_events(DATE(timestamp));
CREATE INDEX IF NOT EXISTS idx_analytics_events_unprocessed ON analytics_events(is_processed) WHERE is_processed = FALSE;

-- Partitioning by month for large-scale data (optional, commented out for initial setup)
-- CREATE TABLE analytics_events_2024_01 PARTITION OF analytics_events
--     FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

COMMENT ON TABLE analytics_events IS 'Raw analytics events from user interactions';
COMMENT ON COLUMN analytics_events.event_type IS 'Type of event (PAGE_VIEW, DOG_VIEW, etc.)';
COMMENT ON COLUMN analytics_events.entity_type IS 'Type of entity this event relates to';
COMMENT ON COLUMN analytics_events.ip_hash IS 'SHA-256 hashed IP address for privacy';

-- ============================================================================
-- DAILY METRICS TABLE
-- Purpose: Aggregated daily metrics for efficient reporting
-- ============================================================================
CREATE TABLE IF NOT EXISTS daily_metrics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Period identification
    period VARCHAR(20) NOT NULL,              -- Date (YYYY-MM-DD) or week (YYYY-W##)
    period_type VARCHAR(20) NOT NULL,         -- daily, weekly, monthly

    -- Dimension (optional for breakdown)
    dimension_type VARCHAR(50) DEFAULT '',    -- dog, shelter, state, or empty for total
    dimension_id VARCHAR(255) DEFAULT '',     -- ID for the dimension

    -- View metrics
    page_views INTEGER DEFAULT 0,
    unique_visitors INTEGER DEFAULT 0,
    dog_views INTEGER DEFAULT 0,
    shelter_views INTEGER DEFAULT 0,
    search_count INTEGER DEFAULT 0,

    -- Engagement metrics
    favorites_added INTEGER DEFAULT 0,
    favorites_removed INTEGER DEFAULT 0,
    shares INTEGER DEFAULT 0,
    contact_clicks INTEGER DEFAULT 0,
    direction_clicks INTEGER DEFAULT 0,

    -- Application metrics
    foster_applications INTEGER DEFAULT 0,
    foster_approved INTEGER DEFAULT 0,
    foster_started INTEGER DEFAULT 0,
    foster_ended INTEGER DEFAULT 0,
    adoption_applications INTEGER DEFAULT 0,

    -- Outcome metrics
    adoptions INTEGER DEFAULT 0,
    rescues INTEGER DEFAULT 0,
    transports INTEGER DEFAULT 0,

    -- User metrics
    new_users INTEGER DEFAULT 0,
    active_users INTEGER DEFAULT 0,
    returning_users INTEGER DEFAULT 0,

    -- Social metrics
    social_views INTEGER DEFAULT 0,
    social_shares INTEGER DEFAULT 0,
    social_engagement INTEGER DEFAULT 0,
    social_referrals INTEGER DEFAULT 0,

    -- Notification metrics
    notifications_sent INTEGER DEFAULT 0,
    notifications_opened INTEGER DEFAULT 0,
    emails_sent INTEGER DEFAULT 0,
    emails_opened INTEGER DEFAULT 0,

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique constraint for upsert
    CONSTRAINT daily_metrics_unique UNIQUE (period, period_type, dimension_type, dimension_id)
);

-- Indexes for daily_metrics
CREATE INDEX IF NOT EXISTS idx_daily_metrics_period ON daily_metrics(period, period_type);
CREATE INDEX IF NOT EXISTS idx_daily_metrics_dimension ON daily_metrics(dimension_type, dimension_id);
CREATE INDEX IF NOT EXISTS idx_daily_metrics_created ON daily_metrics(created_at);

COMMENT ON TABLE daily_metrics IS 'Aggregated metrics by day/week/month for reporting';

-- ============================================================================
-- IMPACT TRACKING TABLE
-- Purpose: Track life-saving impact events (adoptions, rescues, fosters)
-- ============================================================================
CREATE TABLE IF NOT EXISTS impact_tracking (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Impact identification
    impact_type VARCHAR(50) NOT NULL,         -- ADOPTION, FOSTER_PLACEMENT, etc.
    dog_id UUID NOT NULL,
    dog_name VARCHAR(255),

    -- Shelter context
    shelter_id UUID,
    shelter_name VARCHAR(255),
    state_code VARCHAR(2),

    -- Attribution
    attribution JSONB DEFAULT '{}',           -- Source, campaign, content, etc.

    -- Timing
    event_time TIMESTAMP NOT NULL DEFAULT NOW(),
    days_in_shelter INTEGER,                  -- How long dog was in shelter
    days_on_platform INTEGER,                 -- How long on our platform
    hours_to_euthanasia INTEGER,              -- How close to euthanasia

    -- Urgency flags
    was_urgent BOOLEAN DEFAULT FALSE,
    was_critical BOOLEAN DEFAULT FALSE,
    was_on_euthanasia_list BOOLEAN DEFAULT FALSE,

    -- Additional data
    additional_data JSONB DEFAULT '{}',

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for impact_tracking
CREATE INDEX IF NOT EXISTS idx_impact_tracking_type ON impact_tracking(impact_type);
CREATE INDEX IF NOT EXISTS idx_impact_tracking_dog ON impact_tracking(dog_id);
CREATE INDEX IF NOT EXISTS idx_impact_tracking_shelter ON impact_tracking(shelter_id);
CREATE INDEX IF NOT EXISTS idx_impact_tracking_state ON impact_tracking(state_code);
CREATE INDEX IF NOT EXISTS idx_impact_tracking_time ON impact_tracking(event_time DESC);
CREATE INDEX IF NOT EXISTS idx_impact_tracking_urgent ON impact_tracking(was_urgent) WHERE was_urgent = TRUE;
CREATE INDEX IF NOT EXISTS idx_impact_tracking_critical ON impact_tracking(was_critical) WHERE was_critical = TRUE;

COMMENT ON TABLE impact_tracking IS 'Life-saving impact events - every life saved matters';
COMMENT ON COLUMN impact_tracking.attribution IS 'JSON with source, campaign, content, notification that led to this';

-- ============================================================================
-- SOCIAL CONTENT TABLE
-- Purpose: Registry of social media content for tracking
-- ============================================================================
CREATE TABLE IF NOT EXISTS social_content (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Platform identification
    platform VARCHAR(20) NOT NULL,            -- tiktok, instagram, facebook, twitter
    platform_content_id VARCHAR(255),         -- ID on the platform

    -- Content details
    dog_id UUID,                              -- Associated dog
    content_type VARCHAR(50),                 -- video, image, story, reel
    url TEXT,                                 -- URL to content
    caption TEXT,                             -- Caption/description

    -- Timing
    posted_at TIMESTAMP DEFAULT NOW(),

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique constraint
    CONSTRAINT social_content_unique UNIQUE (platform, platform_content_id)
);

-- Indexes for social_content
CREATE INDEX IF NOT EXISTS idx_social_content_platform ON social_content(platform);
CREATE INDEX IF NOT EXISTS idx_social_content_dog ON social_content(dog_id);
CREATE INDEX IF NOT EXISTS idx_social_content_posted ON social_content(posted_at DESC);

COMMENT ON TABLE social_content IS 'Registry of social media content for analytics tracking';

-- ============================================================================
-- SOCIAL PERFORMANCE TABLE
-- Purpose: Metrics for social media content
-- ============================================================================
CREATE TABLE IF NOT EXISTS social_performance (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Content reference
    content_id UUID NOT NULL REFERENCES social_content(id) ON DELETE CASCADE,
    platform VARCHAR(20) NOT NULL,

    -- Engagement metrics
    views INTEGER DEFAULT 0,
    likes INTEGER DEFAULT 0,
    comments INTEGER DEFAULT 0,
    shares INTEGER DEFAULT 0,
    saves INTEGER DEFAULT 0,
    follows INTEGER DEFAULT 0,

    -- Calculated rates
    engagement_rate DECIMAL(5,2) DEFAULT 0,   -- (likes+comments+shares)/views * 100
    share_rate DECIMAL(5,2) DEFAULT 0,        -- shares/views * 100
    save_rate DECIMAL(5,2) DEFAULT 0,         -- saves/views * 100

    -- Platform-specific metrics
    watch_time_seconds INTEGER DEFAULT 0,
    average_watch_percent INTEGER DEFAULT 0,
    profile_visits INTEGER DEFAULT 0,
    link_clicks INTEGER DEFAULT 0,

    -- Conversion metrics (from our platform)
    site_visits INTEGER DEFAULT 0,            -- Visits to our site from this content
    dog_views INTEGER DEFAULT 0,              -- Views of associated dog
    applications INTEGER DEFAULT 0,           -- Applications resulting from content
    adoptions INTEGER DEFAULT 0,              -- Adoptions attributed to content

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique constraint
    CONSTRAINT social_performance_unique UNIQUE (content_id)
);

-- Indexes for social_performance
CREATE INDEX IF NOT EXISTS idx_social_performance_content ON social_performance(content_id);
CREATE INDEX IF NOT EXISTS idx_social_performance_platform ON social_performance(platform);
CREATE INDEX IF NOT EXISTS idx_social_performance_views ON social_performance(views DESC);
CREATE INDEX IF NOT EXISTS idx_social_performance_engagement ON social_performance(engagement_rate DESC);
CREATE INDEX IF NOT EXISTS idx_social_performance_adoptions ON social_performance(adoptions DESC);

COMMENT ON TABLE social_performance IS 'Performance metrics for social media content';

-- ============================================================================
-- VIEWS FOR COMMON QUERIES
-- ============================================================================

-- Real-time analytics summary
CREATE OR REPLACE VIEW analytics_realtime_summary AS
SELECT
    (SELECT COUNT(*) FROM dogs WHERE status = 'available') as active_dogs,
    (SELECT COUNT(*) FROM dogs WHERE urgency_level IN ('high', 'critical')) as urgent_dogs,
    (SELECT COUNT(*) FROM dogs WHERE urgency_level = 'critical') as critical_dogs,
    (SELECT COUNT(DISTINCT COALESCE(user_id::text, session_id))
     FROM analytics_events
     WHERE timestamp >= CURRENT_DATE) as active_users_today,
    (SELECT COUNT(*) FROM foster_placements WHERE status = 'pending') as pending_applications,
    (SELECT COUNT(*) FROM foster_placements WHERE start_date = CURRENT_DATE) as dogs_fostered_today,
    (SELECT COUNT(*) FROM dogs WHERE status = 'adopted' AND updated_at >= CURRENT_DATE) as dogs_adopted_today;

-- Daily event summary
CREATE OR REPLACE VIEW analytics_daily_summary AS
SELECT
    DATE(timestamp) as date,
    COUNT(*) as total_events,
    COUNT(*) FILTER (WHERE event_type = 'PAGE_VIEW') as page_views,
    COUNT(*) FILTER (WHERE event_type = 'DOG_VIEW') as dog_views,
    COUNT(*) FILTER (WHERE event_type = 'SEARCH') as searches,
    COUNT(*) FILTER (WHERE event_type = 'DOG_FAVORITED') as favorites,
    COUNT(*) FILTER (WHERE event_type = 'DOG_SHARED') as shares,
    COUNT(*) FILTER (WHERE event_type = 'ADOPTION_COMPLETED') as adoptions,
    COUNT(DISTINCT COALESCE(user_id::text, session_id)) as unique_visitors
FROM analytics_events
WHERE timestamp >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY DATE(timestamp)
ORDER BY date DESC;

-- Top dogs by views
CREATE OR REPLACE VIEW analytics_top_dogs AS
SELECT
    ae.entity_id as dog_id,
    d.name as dog_name,
    d.shelter_id,
    s.name as shelter_name,
    COUNT(*) as view_count,
    COUNT(*) FILTER (WHERE ae.event_type = 'DOG_FAVORITED') as favorite_count,
    COUNT(*) FILTER (WHERE ae.event_type = 'DOG_SHARED') as share_count
FROM analytics_events ae
JOIN dogs d ON ae.entity_id = d.id::TEXT
LEFT JOIN shelters s ON d.shelter_id = s.id
WHERE ae.event_type IN ('DOG_VIEW', 'DOG_FAVORITED', 'DOG_SHARED')
  AND ae.timestamp >= CURRENT_DATE - INTERVAL '7 days'
GROUP BY ae.entity_id, d.name, d.shelter_id, s.name
ORDER BY view_count DESC
LIMIT 100;

-- Impact summary
CREATE OR REPLACE VIEW analytics_impact_summary AS
SELECT
    DATE_TRUNC('month', event_time) as month,
    COUNT(*) as total_saves,
    COUNT(*) FILTER (WHERE impact_type = 'ADOPTION') as adoptions,
    COUNT(*) FILTER (WHERE impact_type = 'FOSTER_PLACEMENT') as fosters,
    COUNT(*) FILTER (WHERE impact_type = 'RESCUE_PULL') as rescues,
    COUNT(*) FILTER (WHERE was_critical = TRUE) as critical_saves,
    COUNT(*) FILTER (WHERE was_on_euthanasia_list = TRUE) as euthanasia_list_saves,
    AVG(days_in_shelter) as avg_days_in_shelter
FROM impact_tracking
WHERE event_time >= CURRENT_DATE - INTERVAL '1 year'
GROUP BY DATE_TRUNC('month', event_time)
ORDER BY month DESC;

-- Social content performance
CREATE OR REPLACE VIEW analytics_social_summary AS
SELECT
    sp.platform,
    COUNT(*) as content_count,
    SUM(sp.views) as total_views,
    SUM(sp.likes) as total_likes,
    SUM(sp.comments) as total_comments,
    SUM(sp.shares) as total_shares,
    AVG(sp.engagement_rate) as avg_engagement_rate,
    SUM(sp.adoptions) as total_adoptions
FROM social_performance sp
JOIN social_content sc ON sp.content_id = sc.id
WHERE sc.posted_at >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY sp.platform
ORDER BY total_views DESC;

-- ============================================================================
-- CLEANUP FUNCTION
-- Purpose: Remove old analytics events past retention period
-- ============================================================================
CREATE OR REPLACE FUNCTION cleanup_old_analytics_events(retention_days INTEGER DEFAULT 90)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM analytics_events
    WHERE timestamp < NOW() - (retention_days || ' days')::INTERVAL
      AND is_processed = TRUE;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION cleanup_old_analytics_events IS 'Remove processed events older than retention period';

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Update timestamp trigger for daily_metrics
CREATE OR REPLACE FUNCTION update_daily_metrics_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER daily_metrics_update_timestamp
    BEFORE UPDATE ON daily_metrics
    FOR EACH ROW
    EXECUTE FUNCTION update_daily_metrics_timestamp();

-- Update timestamp trigger for social_performance
CREATE OR REPLACE FUNCTION update_social_performance_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER social_performance_update_timestamp
    BEFORE UPDATE ON social_performance
    FOR EACH ROW
    EXECUTE FUNCTION update_social_performance_timestamp();

-- ============================================================================
-- END OF ANALYTICS SCHEMA
-- ============================================================================

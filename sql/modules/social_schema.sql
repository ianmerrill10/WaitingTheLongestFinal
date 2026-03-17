-- ============================================================================
-- SOCIAL MEDIA MODULE - DATABASE SCHEMA
-- ============================================================================
-- Purpose: Schema for social media cross-posting, analytics, and share cards
-- Author: Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
-- Date: 2024-01-28
-- ============================================================================

-- ============================================================================
-- PLATFORM CONNECTIONS TABLE
-- Stores OAuth connections to social media platforms
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_platform_connections (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    platform VARCHAR(50) NOT NULL,

    -- Platform-specific identifiers
    platform_user_id VARCHAR(255),
    platform_username VARCHAR(255),

    -- OAuth tokens (encrypted in production)
    access_token TEXT NOT NULL,
    refresh_token TEXT,
    token_expires_at TIMESTAMP WITH TIME ZONE,

    -- For Facebook/Instagram pages
    page_id VARCHAR(255),
    page_name VARCHAR(255),
    page_access_token TEXT,

    -- For Instagram business accounts
    instagram_business_id VARCHAR(255),

    -- Status
    is_valid BOOLEAN NOT NULL DEFAULT true,
    last_error TEXT,
    last_used_at TIMESTAMP WITH TIME ZONE,

    -- Timestamps
    connected_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    UNIQUE(user_id, platform, page_id)
);

-- Indexes for connections
CREATE INDEX IF NOT EXISTS idx_social_connections_user_id
    ON social_platform_connections(user_id);
CREATE INDEX IF NOT EXISTS idx_social_connections_platform
    ON social_platform_connections(platform);
CREATE INDEX IF NOT EXISTS idx_social_connections_expires
    ON social_platform_connections(token_expires_at)
    WHERE token_expires_at IS NOT NULL;

-- ============================================================================
-- SOCIAL POSTS TABLE
-- Main table for all social media posts
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_posts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Content
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
    user_id UUID REFERENCES users(id) ON DELETE SET NULL,

    -- Post content
    post_type VARCHAR(50) NOT NULL DEFAULT 'adoption_spotlight',
    text_content TEXT NOT NULL,
    hashtags TEXT[], -- Array of hashtags

    -- Media
    image_urls TEXT[],
    video_url TEXT,
    share_card_url TEXT,

    -- Scheduling
    status VARCHAR(50) NOT NULL DEFAULT 'draft',
    scheduled_at TIMESTAMP WITH TIME ZONE,
    published_at TIMESTAMP WITH TIME ZONE,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted_at TIMESTAMP WITH TIME ZONE,

    -- Check constraints
    CONSTRAINT valid_post_type CHECK (post_type IN (
        'adoption_spotlight', 'urgent_appeal', 'waiting_milestone',
        'success_story', 'event', 'shelter_update', 'foster_update',
        'volunteer_spotlight', 'donation_appeal', 'custom'
    )),
    CONSTRAINT valid_status CHECK (status IN (
        'draft', 'pending', 'scheduled', 'posting', 'posted',
        'failed', 'deleted', 'rate_limited'
    ))
);

-- Indexes for posts
CREATE INDEX IF NOT EXISTS idx_social_posts_dog_id ON social_posts(dog_id);
CREATE INDEX IF NOT EXISTS idx_social_posts_shelter_id ON social_posts(shelter_id);
CREATE INDEX IF NOT EXISTS idx_social_posts_user_id ON social_posts(user_id);
CREATE INDEX IF NOT EXISTS idx_social_posts_status ON social_posts(status);
CREATE INDEX IF NOT EXISTS idx_social_posts_scheduled
    ON social_posts(scheduled_at) WHERE status = 'scheduled';
CREATE INDEX IF NOT EXISTS idx_social_posts_created
    ON social_posts(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_social_posts_type ON social_posts(post_type);

-- ============================================================================
-- PLATFORM POST INFO TABLE
-- Per-platform status and IDs for each post
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_platform_posts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    post_id UUID NOT NULL REFERENCES social_posts(id) ON DELETE CASCADE,
    connection_id UUID REFERENCES social_platform_connections(id) ON DELETE SET NULL,

    -- Platform info
    platform VARCHAR(50) NOT NULL,
    platform_post_id VARCHAR(255),
    platform_url TEXT,

    -- Status
    status VARCHAR(50) NOT NULL DEFAULT 'pending',
    error_message TEXT,
    retry_count INTEGER NOT NULL DEFAULT 0,

    -- Timestamps
    posted_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    UNIQUE(post_id, platform),
    CONSTRAINT valid_platform_status CHECK (status IN (
        'pending', 'scheduled', 'posting', 'posted',
        'failed', 'deleted', 'rate_limited'
    ))
);

-- Indexes for platform posts
CREATE INDEX IF NOT EXISTS idx_platform_posts_post_id ON social_platform_posts(post_id);
CREATE INDEX IF NOT EXISTS idx_platform_posts_platform ON social_platform_posts(platform);
CREATE INDEX IF NOT EXISTS idx_platform_posts_status ON social_platform_posts(status);
CREATE INDEX IF NOT EXISTS idx_platform_posts_platform_id
    ON social_platform_posts(platform, platform_post_id);

-- ============================================================================
-- SOCIAL ANALYTICS TABLE
-- Aggregated analytics per post
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_post_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    post_id UUID NOT NULL REFERENCES social_posts(id) ON DELETE CASCADE,

    -- Aggregated metrics
    total_impressions BIGINT NOT NULL DEFAULT 0,
    total_reach BIGINT NOT NULL DEFAULT 0,
    total_engagement BIGINT NOT NULL DEFAULT 0,
    total_likes BIGINT NOT NULL DEFAULT 0,
    total_comments BIGINT NOT NULL DEFAULT 0,
    total_shares BIGINT NOT NULL DEFAULT 0,
    total_saves BIGINT NOT NULL DEFAULT 0,
    total_clicks BIGINT NOT NULL DEFAULT 0,
    total_video_views BIGINT NOT NULL DEFAULT 0,

    -- Calculated metrics
    engagement_rate DECIMAL(5,4) NOT NULL DEFAULT 0,

    -- Adoption impact
    profile_views_from_post INTEGER NOT NULL DEFAULT 0,
    applications_from_post INTEGER NOT NULL DEFAULT 0,
    adoption_attributed BOOLEAN NOT NULL DEFAULT false,

    -- Timestamps
    first_fetched_at TIMESTAMP WITH TIME ZONE,
    last_fetched_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE(post_id)
);

-- Indexes for analytics
CREATE INDEX IF NOT EXISTS idx_post_analytics_post_id ON social_post_analytics(post_id);
CREATE INDEX IF NOT EXISTS idx_post_analytics_engagement ON social_post_analytics(total_engagement DESC);
CREATE INDEX IF NOT EXISTS idx_post_analytics_adoption ON social_post_analytics(adoption_attributed)
    WHERE adoption_attributed = true;

-- ============================================================================
-- PLATFORM ANALYTICS TABLE
-- Per-platform analytics for each post
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_platform_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    post_id UUID NOT NULL REFERENCES social_posts(id) ON DELETE CASCADE,
    platform VARCHAR(50) NOT NULL,
    platform_post_id VARCHAR(255),

    -- Metrics
    impressions BIGINT NOT NULL DEFAULT 0,
    reach BIGINT NOT NULL DEFAULT 0,
    engagement BIGINT NOT NULL DEFAULT 0,
    likes BIGINT NOT NULL DEFAULT 0,
    comments BIGINT NOT NULL DEFAULT 0,
    shares BIGINT NOT NULL DEFAULT 0,
    saves BIGINT NOT NULL DEFAULT 0,
    clicks BIGINT NOT NULL DEFAULT 0,
    video_views BIGINT NOT NULL DEFAULT 0,

    -- Platform-specific metrics (JSON)
    platform_metrics JSONB,

    -- Engagement rate
    engagement_rate DECIMAL(5,4) NOT NULL DEFAULT 0,

    -- Timestamps
    fetched_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE(post_id, platform, fetched_at)
);

-- Indexes for platform analytics
CREATE INDEX IF NOT EXISTS idx_platform_analytics_post_id ON social_platform_analytics(post_id);
CREATE INDEX IF NOT EXISTS idx_platform_analytics_platform ON social_platform_analytics(platform);
CREATE INDEX IF NOT EXISTS idx_platform_analytics_fetched ON social_platform_analytics(fetched_at DESC);

-- ============================================================================
-- DOG ANALYTICS TABLE
-- Aggregated social analytics per dog
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_dog_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,

    -- Post counts
    total_posts INTEGER NOT NULL DEFAULT 0,
    posts_last_7_days INTEGER NOT NULL DEFAULT 0,
    posts_last_30_days INTEGER NOT NULL DEFAULT 0,

    -- Aggregated metrics
    total_impressions BIGINT NOT NULL DEFAULT 0,
    total_reach BIGINT NOT NULL DEFAULT 0,
    total_engagement BIGINT NOT NULL DEFAULT 0,

    -- Best performing
    best_post_id UUID REFERENCES social_posts(id) ON DELETE SET NULL,
    best_engagement INTEGER NOT NULL DEFAULT 0,

    -- Impact
    profile_views_from_social INTEGER NOT NULL DEFAULT 0,
    applications_from_social INTEGER NOT NULL DEFAULT 0,
    adoption_attributed_to_social BOOLEAN NOT NULL DEFAULT false,

    -- Timestamps
    last_post_at TIMESTAMP WITH TIME ZONE,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE(dog_id)
);

-- Indexes for dog analytics
CREATE INDEX IF NOT EXISTS idx_dog_analytics_dog_id ON social_dog_analytics(dog_id);
CREATE INDEX IF NOT EXISTS idx_dog_analytics_engagement ON social_dog_analytics(total_engagement DESC);

-- ============================================================================
-- CROSS-POST TRACKING TABLE
-- Track when posts are cross-posted from one platform to another
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_post_crossposts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_post_id UUID NOT NULL REFERENCES social_posts(id) ON DELETE CASCADE,
    target_post_id UUID NOT NULL REFERENCES social_posts(id) ON DELETE CASCADE,

    source_platform VARCHAR(50) NOT NULL,
    target_platform VARCHAR(50) NOT NULL,

    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE(source_post_id, target_platform)
);

-- ============================================================================
-- SHARE CARDS TABLE
-- Generated share card images
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_share_cards (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,

    -- Card details
    platform VARCHAR(50) NOT NULL,
    style VARCHAR(50) NOT NULL DEFAULT 'standard',

    -- Image
    image_url TEXT NOT NULL,
    image_width INTEGER NOT NULL,
    image_height INTEGER NOT NULL,
    file_size INTEGER,

    -- Content shown
    days_waiting_shown INTEGER,
    urgency_badge VARCHAR(50),

    -- Usage tracking
    times_used INTEGER NOT NULL DEFAULT 0,
    last_used_at TIMESTAMP WITH TIME ZONE,

    -- Timestamps
    generated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE,

    -- Constraints
    CONSTRAINT valid_card_style CHECK (style IN (
        'standard', 'urgent', 'milestone', 'success_story',
        'event', 'comparison', 'carousel_item', 'story'
    ))
);

-- Indexes for share cards
CREATE INDEX IF NOT EXISTS idx_share_cards_dog_id ON social_share_cards(dog_id);
CREATE INDEX IF NOT EXISTS idx_share_cards_platform ON social_share_cards(platform);
CREATE INDEX IF NOT EXISTS idx_share_cards_style ON social_share_cards(style);

-- ============================================================================
-- WORKER TASKS TABLE
-- Background worker task tracking
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_worker_tasks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Task info
    task_type VARCHAR(50) NOT NULL,
    priority INTEGER NOT NULL DEFAULT 5,
    payload JSONB,

    -- Status
    status VARCHAR(50) NOT NULL DEFAULT 'pending',
    error_message TEXT,
    retry_count INTEGER NOT NULL DEFAULT 0,
    max_retries INTEGER NOT NULL DEFAULT 3,

    -- Result
    result_data JSONB,
    execution_time_ms INTEGER,

    -- Timestamps
    scheduled_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    CONSTRAINT valid_task_type CHECK (task_type IN (
        'scheduled_post', 'analytics_sync', 'urgent_dog_check',
        'tiktok_crosspost', 'token_refresh', 'milestone_check', 'cleanup'
    )),
    CONSTRAINT valid_task_status CHECK (status IN (
        'pending', 'running', 'completed', 'failed', 'cancelled'
    ))
);

-- Indexes for worker tasks
CREATE INDEX IF NOT EXISTS idx_worker_tasks_status ON social_worker_tasks(status);
CREATE INDEX IF NOT EXISTS idx_worker_tasks_scheduled
    ON social_worker_tasks(scheduled_at) WHERE status = 'pending';
CREATE INDEX IF NOT EXISTS idx_worker_tasks_type ON social_worker_tasks(task_type);
CREATE INDEX IF NOT EXISTS idx_worker_tasks_completed
    ON social_worker_tasks(completed_at DESC) WHERE completed_at IS NOT NULL;

-- ============================================================================
-- OPTIMAL POST TIMES TABLE
-- Learned optimal posting times per platform
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_optimal_times (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    platform VARCHAR(50) NOT NULL,
    day_of_week INTEGER NOT NULL, -- 0=Sunday, 6=Saturday
    hour INTEGER NOT NULL, -- 0-23

    -- Engagement scores
    engagement_score DECIMAL(5,2) NOT NULL DEFAULT 0,
    sample_count INTEGER NOT NULL DEFAULT 0,

    -- Calculated from data
    avg_engagement DECIMAL(10,2),
    avg_reach DECIMAL(10,2),

    -- Timestamps
    last_calculated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    UNIQUE(platform, day_of_week, hour),
    CONSTRAINT valid_day CHECK (day_of_week >= 0 AND day_of_week <= 6),
    CONSTRAINT valid_hour CHECK (hour >= 0 AND hour <= 23)
);

-- Indexes for optimal times
CREATE INDEX IF NOT EXISTS idx_optimal_times_platform ON social_optimal_times(platform);
CREATE INDEX IF NOT EXISTS idx_optimal_times_score ON social_optimal_times(engagement_score DESC);

-- ============================================================================
-- ANALYTICS CACHE TABLE
-- Cache for fetched analytics
-- ============================================================================

CREATE TABLE IF NOT EXISTS social_analytics_cache (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    post_id UUID NOT NULL,
    platform VARCHAR(50) NOT NULL,
    platform_post_id VARCHAR(255),

    -- Cached data
    analytics_data JSONB NOT NULL,

    -- Timestamps
    cached_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,

    UNIQUE(post_id, platform)
);

-- Indexes for cache
CREATE INDEX IF NOT EXISTS idx_analytics_cache_expires ON social_analytics_cache(expires_at);
CREATE INDEX IF NOT EXISTS idx_analytics_cache_post ON social_analytics_cache(post_id);

-- ============================================================================
-- FUNCTIONS AND TRIGGERS
-- ============================================================================

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_social_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Triggers for updated_at
CREATE TRIGGER update_social_connections_timestamp
    BEFORE UPDATE ON social_platform_connections
    FOR EACH ROW EXECUTE FUNCTION update_social_updated_at();

CREATE TRIGGER update_social_posts_timestamp
    BEFORE UPDATE ON social_posts
    FOR EACH ROW EXECUTE FUNCTION update_social_updated_at();

CREATE TRIGGER update_social_platform_posts_timestamp
    BEFORE UPDATE ON social_platform_posts
    FOR EACH ROW EXECUTE FUNCTION update_social_updated_at();

CREATE TRIGGER update_social_post_analytics_timestamp
    BEFORE UPDATE ON social_post_analytics
    FOR EACH ROW EXECUTE FUNCTION update_social_updated_at();

CREATE TRIGGER update_social_dog_analytics_timestamp
    BEFORE UPDATE ON social_dog_analytics
    FOR EACH ROW EXECUTE FUNCTION update_social_updated_at();

-- ============================================================================
-- Function to update dog analytics when post analytics change
-- ============================================================================

CREATE OR REPLACE FUNCTION update_dog_analytics_from_post()
RETURNS TRIGGER AS $$
BEGIN
    -- Get the dog_id from the post
    DECLARE
        v_dog_id UUID;
    BEGIN
        SELECT dog_id INTO v_dog_id FROM social_posts WHERE id = NEW.post_id;

        IF v_dog_id IS NOT NULL THEN
            -- Update or insert dog analytics
            INSERT INTO social_dog_analytics (dog_id, total_posts, total_impressions,
                                              total_reach, total_engagement, updated_at)
            SELECT
                v_dog_id,
                COUNT(DISTINCT sp.id),
                COALESCE(SUM(spa.total_impressions), 0),
                COALESCE(SUM(spa.total_reach), 0),
                COALESCE(SUM(spa.total_engagement), 0),
                CURRENT_TIMESTAMP
            FROM social_posts sp
            LEFT JOIN social_post_analytics spa ON sp.id = spa.post_id
            WHERE sp.dog_id = v_dog_id AND sp.deleted_at IS NULL
            ON CONFLICT (dog_id) DO UPDATE SET
                total_posts = EXCLUDED.total_posts,
                total_impressions = EXCLUDED.total_impressions,
                total_reach = EXCLUDED.total_reach,
                total_engagement = EXCLUDED.total_engagement,
                updated_at = CURRENT_TIMESTAMP;
        END IF;
    END;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_dog_analytics_on_post_analytics
    AFTER INSERT OR UPDATE ON social_post_analytics
    FOR EACH ROW EXECUTE FUNCTION update_dog_analytics_from_post();

-- ============================================================================
-- SEED DATA - Default optimal posting times
-- ============================================================================

-- Insert default optimal times based on general social media best practices
INSERT INTO social_optimal_times (platform, day_of_week, hour, engagement_score, sample_count)
VALUES
    -- Facebook best times (weekdays lunch and evening)
    ('facebook', 1, 12, 8.5, 100),
    ('facebook', 1, 19, 9.0, 100),
    ('facebook', 2, 12, 8.5, 100),
    ('facebook', 2, 19, 9.2, 100),
    ('facebook', 3, 12, 9.0, 100),
    ('facebook', 3, 15, 8.8, 100),
    ('facebook', 4, 12, 8.5, 100),
    ('facebook', 4, 19, 9.0, 100),
    ('facebook', 5, 12, 8.0, 100),
    ('facebook', 5, 15, 8.5, 100),

    -- Instagram best times (lunch and early evening)
    ('instagram', 1, 11, 8.5, 100),
    ('instagram', 1, 17, 9.0, 100),
    ('instagram', 2, 11, 9.0, 100),
    ('instagram', 2, 14, 8.5, 100),
    ('instagram', 3, 11, 9.2, 100),
    ('instagram', 3, 19, 8.8, 100),
    ('instagram', 4, 11, 8.5, 100),
    ('instagram', 4, 17, 9.0, 100),
    ('instagram', 5, 11, 8.0, 100),
    ('instagram', 5, 14, 8.5, 100),

    -- Twitter best times (morning and lunch)
    ('twitter', 1, 9, 8.5, 100),
    ('twitter', 1, 12, 9.0, 100),
    ('twitter', 2, 9, 9.0, 100),
    ('twitter', 2, 12, 8.8, 100),
    ('twitter', 3, 9, 9.2, 100),
    ('twitter', 3, 12, 9.0, 100),
    ('twitter', 4, 9, 8.5, 100),
    ('twitter', 4, 12, 8.8, 100),
    ('twitter', 5, 9, 8.0, 100),
    ('twitter', 5, 12, 8.5, 100),

    -- Weekend times (generally lower but still good)
    ('facebook', 0, 12, 7.5, 50),
    ('facebook', 6, 12, 7.8, 50),
    ('instagram', 0, 11, 7.5, 50),
    ('instagram', 6, 11, 7.8, 50),
    ('twitter', 0, 10, 7.0, 50),
    ('twitter', 6, 10, 7.2, 50)
ON CONFLICT (platform, day_of_week, hour) DO NOTHING;

-- ============================================================================
-- VIEWS
-- ============================================================================

-- View for post performance summary
CREATE OR REPLACE VIEW v_social_post_performance AS
SELECT
    sp.id AS post_id,
    sp.dog_id,
    d.name AS dog_name,
    sp.post_type,
    sp.status,
    sp.created_at,
    sp.published_at,
    spa.total_impressions,
    spa.total_reach,
    spa.total_engagement,
    spa.engagement_rate,
    spa.adoption_attributed,
    (
        SELECT array_agg(DISTINCT spp.platform)
        FROM social_platform_posts spp
        WHERE spp.post_id = sp.id AND spp.status = 'posted'
    ) AS platforms_posted
FROM social_posts sp
LEFT JOIN dogs d ON sp.dog_id = d.id
LEFT JOIN social_post_analytics spa ON sp.id = spa.post_id
WHERE sp.deleted_at IS NULL;

-- View for dog social media summary
CREATE OR REPLACE VIEW v_dog_social_summary AS
SELECT
    d.id AS dog_id,
    d.name AS dog_name,
    d.intake_date,
    CURRENT_DATE - d.intake_date::date AS days_waiting,
    COALESCE(sda.total_posts, 0) AS total_posts,
    COALESCE(sda.total_impressions, 0) AS total_impressions,
    COALESCE(sda.total_engagement, 0) AS total_engagement,
    sda.last_post_at,
    COALESCE(sda.adoption_attributed_to_social, false) AS adoption_attributed,
    (
        SELECT MAX(generated_at)
        FROM social_share_cards ssc
        WHERE ssc.dog_id = d.id
    ) AS last_share_card_at
FROM dogs d
LEFT JOIN social_dog_analytics sda ON d.id = sda.dog_id
WHERE d.status = 'available';

-- ============================================================================
-- GRANTS (adjust roles as needed)
-- ============================================================================

-- Grant permissions to application role
-- GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO wtl_app;
-- GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO wtl_app;
-- GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO wtl_app;

COMMENT ON TABLE social_platform_connections IS 'OAuth connections to social media platforms';
COMMENT ON TABLE social_posts IS 'Social media posts for cross-posting';
COMMENT ON TABLE social_platform_posts IS 'Per-platform status for each post';
COMMENT ON TABLE social_post_analytics IS 'Aggregated analytics for posts';
COMMENT ON TABLE social_platform_analytics IS 'Per-platform analytics snapshots';
COMMENT ON TABLE social_dog_analytics IS 'Aggregated social analytics per dog';
COMMENT ON TABLE social_share_cards IS 'Generated share card images';
COMMENT ON TABLE social_worker_tasks IS 'Background worker task queue';
COMMENT ON TABLE social_optimal_times IS 'Learned optimal posting times';

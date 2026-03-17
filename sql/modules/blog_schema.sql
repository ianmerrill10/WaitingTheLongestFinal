-- ============================================================================
-- Migration: blog_schema.sql
-- Purpose: Create blog_posts and educational_content tables for DOGBLOG
-- Author: Agent 12 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES: 001_extensions.sql (UUID generation)
--
-- TABLES CREATED:
--   - blog_posts: Main blog posts table
--   - educational_content: Educational articles library
--   - blog_categories: Reference table for categories
--
-- ROLLBACK: DROP TABLE blog_posts, educational_content, blog_categories;
-- ============================================================================

-- ============================================================================
-- BLOG CATEGORIES REFERENCE TABLE
-- Purpose: Define available blog categories
-- ============================================================================
CREATE TABLE IF NOT EXISTS blog_categories (
    id VARCHAR(50) PRIMARY KEY,                    -- Category identifier
    display_name VARCHAR(100) NOT NULL,            -- Human-readable name
    slug VARCHAR(100) NOT NULL UNIQUE,             -- URL-friendly slug
    description TEXT,                              -- Category description
    icon VARCHAR(50),                              -- Icon class name
    color VARCHAR(7),                              -- Hex color code
    priority INTEGER DEFAULT 50,                   -- Display priority (lower = higher)
    is_active BOOLEAN DEFAULT TRUE,                -- Whether category is active
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Insert default categories
INSERT INTO blog_categories (id, display_name, slug, description, icon, color, priority)
VALUES
    ('urgent', 'Urgent Dogs', 'urgent',
     'Dogs in critical situations who need immediate help',
     'icon-alert-triangle', '#dc2626', 1),
    ('longest_waiting', 'Longest Waiting', 'longest-waiting',
     'Dogs who have been waiting the longest for their forever homes',
     'icon-clock', '#f59e0b', 2),
    ('overlooked', 'Overlooked Dogs', 'overlooked',
     'Dogs that often get passed over - seniors, black dogs, large breeds',
     'icon-eye-off', '#8b5cf6', 3),
    ('success_story', 'Success Stories', 'success-stories',
     'Heartwarming stories of dogs who found their forever homes',
     'icon-heart', '#10b981', 5),
    ('educational', 'Educational', 'educational',
     'Helpful articles about dog care, training, health, and nutrition',
     'icon-book-open', '#3b82f6', 6),
    ('new_arrivals', 'New Arrivals', 'new-arrivals',
     'Meet the newest dogs available for adoption',
     'icon-star', '#06b6d4', 4)
ON CONFLICT (id) DO NOTHING;

COMMENT ON TABLE blog_categories IS 'Reference table for blog post categories';

-- ============================================================================
-- BLOG POSTS TABLE
-- Purpose: Store all blog posts for the DOGBLOG content engine
-- ============================================================================
CREATE TABLE IF NOT EXISTS blog_posts (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(255) NOT NULL,                   -- Post title
    slug VARCHAR(255) NOT NULL UNIQUE,             -- URL-friendly slug

    -- Content
    content TEXT NOT NULL,                         -- Full content in Markdown
    excerpt VARCHAR(500),                          -- Short excerpt/summary

    -- Classification
    category VARCHAR(50) NOT NULL                  -- FK to blog_categories
        REFERENCES blog_categories(id) ON DELETE RESTRICT,
    tags TEXT[] DEFAULT '{}',                      -- Array of tags

    -- Media
    featured_image VARCHAR(500),                   -- Hero image URL
    gallery_images TEXT[] DEFAULT '{}',            -- Additional images

    -- Relationships
    dog_id UUID REFERENCES dogs(id)                -- Featured dog (if applicable)
        ON DELETE SET NULL,
    adoption_id UUID REFERENCES foster_placements(id)  -- For success stories
        ON DELETE SET NULL,
    shelter_id UUID REFERENCES shelters(id)        -- Related shelter
        ON DELETE SET NULL,

    -- Publication Status
    status VARCHAR(20) NOT NULL DEFAULT 'draft'    -- draft, scheduled, published, archived
        CHECK (status IN ('draft', 'scheduled', 'published', 'archived')),
    scheduled_at TIMESTAMP,                        -- Scheduled publication time
    published_at TIMESTAMP,                        -- Actual publication time

    -- SEO Metadata
    meta_description VARCHAR(160),                 -- SEO meta description
    og_title VARCHAR(100),                         -- Open Graph title
    og_description VARCHAR(200),                   -- Open Graph description
    og_image VARCHAR(500),                         -- Open Graph image URL

    -- Engagement Metrics
    view_count INTEGER DEFAULT 0,                  -- Number of views
    share_count INTEGER DEFAULT 0,                 -- Number of shares
    like_count INTEGER DEFAULT 0,                  -- Number of likes

    -- Author
    author_id UUID REFERENCES users(id)            -- FK to users (optional)
        ON DELETE SET NULL,
    author_name VARCHAR(100) DEFAULT 'WTL Team',   -- Display name
    is_auto_generated BOOLEAN DEFAULT FALSE,       -- Generated by BlogEngine

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for common queries
CREATE INDEX IF NOT EXISTS idx_blog_posts_status ON blog_posts(status);
CREATE INDEX IF NOT EXISTS idx_blog_posts_category ON blog_posts(category);
CREATE INDEX IF NOT EXISTS idx_blog_posts_published_at ON blog_posts(published_at DESC);
CREATE INDEX IF NOT EXISTS idx_blog_posts_dog_id ON blog_posts(dog_id) WHERE dog_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_blog_posts_shelter_id ON blog_posts(shelter_id) WHERE shelter_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_blog_posts_scheduled ON blog_posts(scheduled_at)
    WHERE status = 'scheduled';
CREATE INDEX IF NOT EXISTS idx_blog_posts_view_count ON blog_posts(view_count DESC);

-- Full-text search index
CREATE INDEX IF NOT EXISTS idx_blog_posts_search ON blog_posts
    USING gin(to_tsvector('english', title || ' ' || COALESCE(content, '') || ' ' || COALESCE(excerpt, '')));

-- Tags array index
CREATE INDEX IF NOT EXISTS idx_blog_posts_tags ON blog_posts USING gin(tags);

-- Comments
COMMENT ON TABLE blog_posts IS 'Blog posts for the DOGBLOG content engine';
COMMENT ON COLUMN blog_posts.slug IS 'URL-friendly identifier (unique)';
COMMENT ON COLUMN blog_posts.content IS 'Full post content in Markdown format';
COMMENT ON COLUMN blog_posts.category IS 'Post category (urgent, success_story, etc.)';
COMMENT ON COLUMN blog_posts.status IS 'Publication status: draft, scheduled, published, archived';
COMMENT ON COLUMN blog_posts.is_auto_generated IS 'True if generated by BlogEngine';

-- ============================================================================
-- EDUCATIONAL CONTENT TABLE
-- Purpose: Store educational articles about dog care
-- ============================================================================
CREATE TABLE IF NOT EXISTS educational_content (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(255) NOT NULL,
    slug VARCHAR(255) NOT NULL UNIQUE,

    -- Content
    content TEXT NOT NULL,                         -- Full content in Markdown
    summary VARCHAR(500),                          -- Short summary
    key_points TEXT[] DEFAULT '{}',                -- Bullet points of key takeaways

    -- Classification
    topic VARCHAR(50) NOT NULL                     -- Primary topic category
        CHECK (topic IN (
            'health', 'training', 'nutrition', 'grooming', 'behavior',
            'adoption_prep', 'home_safety', 'exercise', 'socialization',
            'travel', 'seasonal_care', 'first_time_owner', 'special_needs', 'end_of_life'
        )),
    secondary_topics TEXT[] DEFAULT '{}',          -- Additional related topics
    life_stage VARCHAR(20) DEFAULT 'all'           -- Target life stage
        CHECK (life_stage IN ('puppy', 'young', 'adult', 'senior', 'all')),
    tags TEXT[] DEFAULT '{}',                      -- Search tags

    -- Media
    featured_image VARCHAR(500),                   -- Hero image URL
    images TEXT[] DEFAULT '{}',                    -- Additional images
    video_url VARCHAR(500),                        -- Optional video link

    -- SEO
    meta_description VARCHAR(160),                 -- SEO description
    meta_keywords VARCHAR(255),                    -- SEO keywords

    -- Engagement
    view_count INTEGER DEFAULT 0,                  -- Number of views
    helpful_count INTEGER DEFAULT 0,               -- "Was this helpful?" yes
    not_helpful_count INTEGER DEFAULT 0,           -- "Was this helpful?" no
    helpfulness_score DECIMAL(3,2) DEFAULT 0.0,    -- Computed score (0-1)

    -- Status
    is_published BOOLEAN DEFAULT FALSE,            -- Whether live
    is_featured BOOLEAN DEFAULT FALSE,             -- Whether featured
    display_order INTEGER DEFAULT 0,               -- Order within topic

    -- Author/Reviewer
    author_id UUID REFERENCES users(id) ON DELETE SET NULL,
    author_name VARCHAR(100) DEFAULT 'WTL Team',
    reviewer_id UUID REFERENCES users(id) ON DELETE SET NULL,
    reviewer_name VARCHAR(100),                    -- Expert/vet reviewer

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    published_at TIMESTAMP,                        -- When published
    last_reviewed_at TIMESTAMP                     -- Last expert review
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_educational_topic ON educational_content(topic);
CREATE INDEX IF NOT EXISTS idx_educational_life_stage ON educational_content(life_stage);
CREATE INDEX IF NOT EXISTS idx_educational_published ON educational_content(is_published);
CREATE INDEX IF NOT EXISTS idx_educational_featured ON educational_content(is_featured)
    WHERE is_featured = TRUE;
CREATE INDEX IF NOT EXISTS idx_educational_helpfulness ON educational_content(helpfulness_score DESC)
    WHERE is_published = TRUE;
CREATE INDEX IF NOT EXISTS idx_educational_tags ON educational_content USING gin(tags);

-- Full-text search
CREATE INDEX IF NOT EXISTS idx_educational_search ON educational_content
    USING gin(to_tsvector('english', title || ' ' || COALESCE(content, '') || ' ' || COALESCE(summary, '')));

-- Comments
COMMENT ON TABLE educational_content IS 'Educational articles about dog care';
COMMENT ON COLUMN educational_content.topic IS 'Primary topic: health, training, nutrition, etc.';
COMMENT ON COLUMN educational_content.life_stage IS 'Target dog life stage: puppy, young, adult, senior, all';
COMMENT ON COLUMN educational_content.helpfulness_score IS 'Computed from helpful_count / total votes';

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_blog_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS blog_posts_updated_at ON blog_posts;
CREATE TRIGGER blog_posts_updated_at
    BEFORE UPDATE ON blog_posts
    FOR EACH ROW
    EXECUTE FUNCTION update_blog_updated_at();

DROP TRIGGER IF EXISTS educational_content_updated_at ON educational_content;
CREATE TRIGGER educational_content_updated_at
    BEFORE UPDATE ON educational_content
    FOR EACH ROW
    EXECUTE FUNCTION update_blog_updated_at();

-- ============================================================================
-- TRIGGER: Update helpfulness score on vote
-- ============================================================================
CREATE OR REPLACE FUNCTION update_helpfulness_score()
RETURNS TRIGGER AS $$
BEGIN
    IF (NEW.helpful_count + NEW.not_helpful_count) > 0 THEN
        NEW.helpfulness_score = NEW.helpful_count::DECIMAL /
            (NEW.helpful_count + NEW.not_helpful_count)::DECIMAL;
    ELSE
        NEW.helpfulness_score = 0.0;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS educational_helpfulness_update ON educational_content;
CREATE TRIGGER educational_helpfulness_update
    BEFORE UPDATE OF helpful_count, not_helpful_count ON educational_content
    FOR EACH ROW
    EXECUTE FUNCTION update_helpfulness_score();

-- ============================================================================
-- FUNCTION: Get posts by category with pagination
-- ============================================================================
CREATE OR REPLACE FUNCTION get_posts_by_category(
    p_category VARCHAR(50),
    p_limit INTEGER DEFAULT 20,
    p_offset INTEGER DEFAULT 0
)
RETURNS TABLE (
    id UUID,
    title VARCHAR(255),
    slug VARCHAR(255),
    excerpt VARCHAR(500),
    category VARCHAR(50),
    featured_image VARCHAR(500),
    published_at TIMESTAMP,
    view_count INTEGER,
    author_name VARCHAR(100)
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        bp.id,
        bp.title,
        bp.slug,
        bp.excerpt,
        bp.category,
        bp.featured_image,
        bp.published_at,
        bp.view_count,
        bp.author_name
    FROM blog_posts bp
    WHERE bp.status = 'published'
      AND bp.category = p_category
    ORDER BY bp.published_at DESC
    LIMIT p_limit
    OFFSET p_offset;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION get_posts_by_category IS 'Get published posts by category with pagination';

-- ============================================================================
-- FUNCTION: Search posts full-text
-- ============================================================================
CREATE OR REPLACE FUNCTION search_blog_posts(
    p_query TEXT,
    p_limit INTEGER DEFAULT 20
)
RETURNS TABLE (
    id UUID,
    title VARCHAR(255),
    slug VARCHAR(255),
    excerpt VARCHAR(500),
    category VARCHAR(50),
    featured_image VARCHAR(500),
    published_at TIMESTAMP,
    rank REAL
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        bp.id,
        bp.title,
        bp.slug,
        bp.excerpt,
        bp.category,
        bp.featured_image,
        bp.published_at,
        ts_rank(
            to_tsvector('english', bp.title || ' ' || COALESCE(bp.content, '') || ' ' || COALESCE(bp.excerpt, '')),
            plainto_tsquery('english', p_query)
        ) as rank
    FROM blog_posts bp
    WHERE bp.status = 'published'
      AND to_tsvector('english', bp.title || ' ' || COALESCE(bp.content, '') || ' ' || COALESCE(bp.excerpt, ''))
          @@ plainto_tsquery('english', p_query)
    ORDER BY rank DESC, bp.published_at DESC
    LIMIT p_limit;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION search_blog_posts IS 'Full-text search for published blog posts';

-- ============================================================================
-- VIEW: Published posts with category info
-- ============================================================================
CREATE OR REPLACE VIEW published_posts AS
SELECT
    bp.*,
    bc.display_name as category_display,
    bc.slug as category_slug,
    bc.icon as category_icon,
    bc.color as category_color,
    d.name as dog_name,
    d.breed_primary as dog_breed,
    s.name as shelter_name
FROM blog_posts bp
LEFT JOIN blog_categories bc ON bp.category = bc.id
LEFT JOIN dogs d ON bp.dog_id = d.id
LEFT JOIN shelters s ON bp.shelter_id = s.id
WHERE bp.status = 'published';

COMMENT ON VIEW published_posts IS 'Published blog posts with category and relationship info';

-- ============================================================================
-- VIEW: Educational content summary by topic
-- ============================================================================
CREATE OR REPLACE VIEW educational_topic_summary AS
SELECT
    topic,
    COUNT(*) as article_count,
    SUM(view_count) as total_views,
    AVG(helpfulness_score) as avg_helpfulness,
    MAX(published_at) as latest_article
FROM educational_content
WHERE is_published = TRUE
GROUP BY topic
ORDER BY article_count DESC;

COMMENT ON VIEW educational_topic_summary IS 'Summary statistics for educational content by topic';

-- ============================================================================
-- Grant permissions (adjust for your user)
-- ============================================================================
-- GRANT SELECT, INSERT, UPDATE, DELETE ON blog_posts TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON educational_content TO wtl_app;
-- GRANT SELECT ON blog_categories TO wtl_app;
-- GRANT SELECT ON published_posts TO wtl_app;
-- GRANT SELECT ON educational_topic_summary TO wtl_app;

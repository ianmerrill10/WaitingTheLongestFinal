-- ============================================================================
-- Migration: adoption_support_schema.sql
-- Purpose: Create adoption tracking, post-adoption support, and success story tables
-- Author: Agent - WaitingTheLongest Build System
-- Created: 2024-01-29
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (users table)
-- - 010_dogs.sql (dogs table)
-- - 020_shelters.sql (shelters table)
--
-- TABLES CREATED:
--   - adoptions: Track completed adoptions
--   - adoption_checkins: Post-adoption check-ins and support tracking
--   - success_stories: Published success stories about adopted dogs
--   - resources: Educational resources for adopters
--
-- ROLLBACK: DROP TABLE resources CASCADE; DROP TABLE success_stories CASCADE; DROP TABLE adoption_checkins CASCADE; DROP TABLE adoptions CASCADE;
-- ============================================================================

-- ============================================================================
-- ADOPTIONS TABLE
-- Purpose: Track completed adoptions and post-adoption metrics
-- ============================================================================
CREATE TABLE IF NOT EXISTS adoptions (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Relationships
    dog_id UUID NOT NULL,                        -- FK to dogs
        CONSTRAINT fk_adoptions_dog
        FOREIGN KEY (dog_id) REFERENCES dogs(id) ON DELETE CASCADE,
    adopter_id UUID NOT NULL,                    -- FK to users (adopter)
        CONSTRAINT fk_adoptions_adopter
        FOREIGN KEY (adopter_id) REFERENCES users(id) ON DELETE CASCADE,
    shelter_id UUID NOT NULL,                    -- FK to shelters (where adoption occurred)
        CONSTRAINT fk_adoptions_shelter
        FOREIGN KEY (shelter_id) REFERENCES shelters(id) ON DELETE CASCADE,

    -- Optional: Sponsorship reference
    sponsorship_id UUID,                         -- FK to sponsorships (if sponsorship was used)
        CONSTRAINT fk_adoptions_sponsorship
        FOREIGN KEY (sponsorship_id) REFERENCES sponsorships(id) ON DELETE SET NULL,

    -- Adoption Details
    adoption_date DATE NOT NULL,                 -- Date adoption was finalized
    found_via VARCHAR(100),                      -- How adopter found dog (website, social, referral, etc.)
    was_sponsored BOOLEAN DEFAULT FALSE,         -- Whether adoption was sponsored

    -- Post-Adoption Status
    status VARCHAR(50) NOT NULL DEFAULT 'active' -- active, pending_return, returned
        CHECK (status IN ('active', 'pending_return', 'returned')),

    return_date DATE,                            -- Date dog was returned (if applicable)
    return_reason TEXT,                          -- Reason for return (if applicable)

    -- Engagement Metrics
    last_checkin_at TIMESTAMP WITH TIME ZONE,    -- Last time adopter checked in
    checkin_count INTEGER DEFAULT 0,             -- Number of check-ins conducted

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE adoptions IS 'Records of completed dog adoptions and adoption status';
COMMENT ON COLUMN adoptions.dog_id IS 'Dog that was adopted';
COMMENT ON COLUMN adoptions.adopter_id IS 'User who adopted the dog';
COMMENT ON COLUMN adoptions.shelter_id IS 'Shelter/rescue where adoption occurred';
COMMENT ON COLUMN adoptions.sponsorship_id IS 'If adoption was sponsored, reference to sponsorship';
COMMENT ON COLUMN adoptions.found_via IS 'How adopter found the dog (website, social, referral, other)';
COMMENT ON COLUMN adoptions.status IS 'Status: active, pending_return, returned';
COMMENT ON COLUMN adoptions.checkin_count IS 'Number of post-adoption check-ins completed';

-- Indexes for adoption queries
CREATE INDEX IF NOT EXISTS idx_adoptions_dog_id ON adoptions(dog_id);
CREATE INDEX IF NOT EXISTS idx_adoptions_adopter_id ON adoptions(adopter_id);
CREATE INDEX IF NOT EXISTS idx_adoptions_shelter_id ON adoptions(shelter_id);
CREATE INDEX IF NOT EXISTS idx_adoptions_adoption_date ON adoptions(adoption_date DESC);
CREATE INDEX IF NOT EXISTS idx_adoptions_status ON adoptions(status);
CREATE INDEX IF NOT EXISTS idx_adoptions_created_at ON adoptions(created_at DESC);

-- Index for finding adoptions needing check-in
CREATE INDEX IF NOT EXISTS idx_adoptions_needs_checkin ON adoptions(last_checkin_at)
    WHERE status = 'active' AND checkin_count < 3;

-- ============================================================================
-- ADOPTION_CHECKINS TABLE
-- Purpose: Track post-adoption check-ins and support interactions
-- ============================================================================
CREATE TABLE IF NOT EXISTS adoption_checkins (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Relationship
    adoption_id UUID NOT NULL,                   -- FK to adoptions
        CONSTRAINT fk_adoption_checkins_adoption
        FOREIGN KEY (adoption_id) REFERENCES adoptions(id) ON DELETE CASCADE,

    -- Check-in Details
    checkin_type VARCHAR(50) NOT NULL,           -- email, sms, call, form_submission, in_person
    email_sent_at TIMESTAMP WITH TIME ZONE,      -- When email was sent
    email_opened BOOLEAN DEFAULT FALSE,          -- Whether email was opened
    email_clicked BOOLEAN DEFAULT FALSE,         -- Whether link was clicked

    -- Response Tracking
    responded BOOLEAN DEFAULT FALSE,             -- Whether adopter responded
    response_date TIMESTAMP WITH TIME ZONE,      -- When response was received

    -- Content of Check-in
    how_is_adjustment VARCHAR(500),              -- How is dog adjusting?
    needs_help BOOLEAN DEFAULT FALSE,            -- Does adopter need help/support?
    help_topic VARCHAR(100),                     -- If needs help, what topic?
                                                 -- (behavior, health, training, housing, other)
    comments TEXT,                               -- Additional comments/notes

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE adoption_checkins IS 'Post-adoption check-ins to support adopters and track outcomes';
COMMENT ON COLUMN adoption_checkins.adoption_id IS 'Associated adoption record';
COMMENT ON COLUMN adoption_checkins.checkin_type IS 'Type: email, sms, call, form_submission, in_person';
COMMENT ON COLUMN adoption_checkins.email_opened IS 'Tracked via email service provider';
COMMENT ON COLUMN adoption_checkins.help_topic IS 'Category of support needed: behavior, health, training, housing, other';

-- Indexes for check-in queries
CREATE INDEX IF NOT EXISTS idx_adoption_checkins_adoption_id ON adoption_checkins(adoption_id);
CREATE INDEX IF NOT EXISTS idx_adoption_checkins_created_at ON adoption_checkins(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_adoption_checkins_checkin_type ON adoption_checkins(checkin_type);

-- Index for finding unanswered check-ins
CREATE INDEX IF NOT EXISTS idx_adoption_checkins_unanswered ON adoption_checkins(adoption_id)
    WHERE responded = FALSE;

-- Index for finding adopters needing support
CREATE INDEX IF NOT EXISTS idx_adoption_checkins_needs_help ON adoption_checkins(created_at DESC)
    WHERE needs_help = TRUE;

-- ============================================================================
-- SUCCESS_STORIES TABLE
-- Purpose: Store published success stories of adopted dogs
-- ============================================================================
CREATE TABLE IF NOT EXISTS success_stories (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- References
    adoption_id UUID NOT NULL,                   -- FK to adoptions
        CONSTRAINT fk_success_stories_adoption
        FOREIGN KEY (adoption_id) REFERENCES adoptions(id) ON DELETE CASCADE,
    user_id UUID,                                -- FK to users (author/submitter)
        CONSTRAINT fk_success_stories_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    dog_id UUID NOT NULL,                        -- FK to dogs (for easier querying)
        CONSTRAINT fk_success_stories_dog
        FOREIGN KEY (dog_id) REFERENCES dogs(id) ON DELETE CASCADE,

    -- Story Content
    title VARCHAR(255) NOT NULL,                 -- Story title
    story TEXT NOT NULL,                         -- Full story text
    photo_urls TEXT[] DEFAULT '{}',              -- Array of photo URLs

    -- Metadata
    status VARCHAR(50) NOT NULL DEFAULT 'pending' -- pending, approved, published, rejected, archived
        CHECK (status IN ('pending', 'approved', 'published', 'rejected', 'archived')),
    approved_at TIMESTAMP WITH TIME ZONE,        -- When story was approved
    featured BOOLEAN DEFAULT FALSE,              -- Whether story is featured on homepage

    -- Engagement
    view_count INTEGER DEFAULT 0,                -- Number of views
    share_count INTEGER DEFAULT 0,               -- Number of shares

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE success_stories IS 'Published success stories about adopted dogs';
COMMENT ON COLUMN success_stories.adoption_id IS 'Associated adoption record';
COMMENT ON COLUMN success_stories.user_id IS 'User who submitted the story (often the adopter)';
COMMENT ON COLUMN success_stories.dog_id IS 'Dog in the story (denormalized for easier queries)';
COMMENT ON COLUMN success_stories.story IS 'Full story text in Markdown format';
COMMENT ON COLUMN success_stories.status IS 'Publication status: pending, approved, published, rejected, archived';
COMMENT ON COLUMN success_stories.featured IS 'Whether this story should be featured on homepage';

-- Indexes for success story queries
CREATE INDEX IF NOT EXISTS idx_success_stories_adoption_id ON success_stories(adoption_id);
CREATE INDEX IF NOT EXISTS idx_success_stories_dog_id ON success_stories(dog_id);
CREATE INDEX IF NOT EXISTS idx_success_stories_user_id ON success_stories(user_id) WHERE user_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_success_stories_status ON success_stories(status);
CREATE INDEX IF NOT EXISTS idx_success_stories_published ON success_stories(created_at DESC)
    WHERE status = 'published';
CREATE INDEX IF NOT EXISTS idx_success_stories_featured ON success_stories(created_at DESC)
    WHERE featured = TRUE AND status = 'published';

-- Index for finding pending approvals
CREATE INDEX IF NOT EXISTS idx_success_stories_pending ON success_stories(created_at DESC)
    WHERE status = 'pending';

-- ============================================================================
-- RESOURCES TABLE
-- Purpose: Store educational resources for new and existing adopters
-- ============================================================================
CREATE TABLE IF NOT EXISTS resources (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Content
    title VARCHAR(255) NOT NULL,                 -- Resource title
    slug VARCHAR(255) NOT NULL UNIQUE,           -- URL-friendly slug
    content TEXT NOT NULL,                       -- Full content in Markdown

    -- Classification
    category VARCHAR(100) NOT NULL,              -- e.g., training, health, behavior, housing, nutrition, etc.
    summary VARCHAR(500),                        -- Brief summary

    -- Targeting
    for_new_adopters BOOLEAN DEFAULT FALSE,      -- Recommended for new adopters
    for_dog_age TEXT[] DEFAULT '{}',             -- Target dog ages: puppy, young, adult, senior
    for_special_needs BOOLEAN DEFAULT FALSE,     -- For dogs with special needs

    -- Media
    featured_image VARCHAR(500),                 -- Feature image URL
    video_url VARCHAR(500),                      -- Related video URL (YouTube, etc.)

    -- SEO & Publishing
    meta_description VARCHAR(160),               -- SEO meta description
    is_published BOOLEAN DEFAULT FALSE,          -- Whether resource is publicly visible
    published_at TIMESTAMP WITH TIME ZONE,       -- When published

    -- Engagement
    view_count INTEGER DEFAULT 0,                -- Number of views

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE resources IS 'Educational resources for dog adoption and post-adoption support';
COMMENT ON COLUMN resources.slug IS 'URL-friendly identifier (unique, for routing)';
COMMENT ON COLUMN resources.category IS 'Category: training, health, behavior, housing, nutrition, etc.';
COMMENT ON COLUMN resources.for_new_adopters IS 'Whether this is recommended for first-time adopters';
COMMENT ON COLUMN resources.for_dog_age IS 'Array of dog age groups: puppy, young, adult, senior';
COMMENT ON COLUMN resources.for_special_needs IS 'Whether this resource is for dogs with special needs';
COMMENT ON COLUMN resources.is_published IS 'Whether resource is visible on the website';

-- Indexes for resource queries
CREATE INDEX IF NOT EXISTS idx_resources_slug ON resources(slug);
CREATE INDEX IF NOT EXISTS idx_resources_category ON resources(category);
CREATE INDEX IF NOT EXISTS idx_resources_is_published ON resources(is_published) WHERE is_published = TRUE;
CREATE INDEX IF NOT EXISTS idx_resources_for_new_adopters ON resources(created_at DESC)
    WHERE for_new_adopters = TRUE AND is_published = TRUE;
CREATE INDEX IF NOT EXISTS idx_resources_view_count ON resources(view_count DESC) WHERE is_published = TRUE;

-- Array index for age targeting
CREATE INDEX IF NOT EXISTS idx_resources_for_dog_age ON resources USING gin(for_dog_age);

-- ============================================================================
-- TRIGGER: Auto-update updated_at on adoptions
-- ============================================================================
DROP TRIGGER IF EXISTS update_adoptions_updated_at ON adoptions;
CREATE TRIGGER update_adoptions_updated_at
    BEFORE UPDATE ON adoptions
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on adoption_checkins
-- ============================================================================
DROP TRIGGER IF EXISTS update_adoption_checkins_updated_at ON adoption_checkins;
CREATE TRIGGER update_adoption_checkins_updated_at
    BEFORE UPDATE ON adoption_checkins
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on success_stories
-- ============================================================================
DROP TRIGGER IF EXISTS update_success_stories_updated_at ON success_stories;
CREATE TRIGGER update_success_stories_updated_at
    BEFORE UPDATE ON success_stories
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on resources
-- ============================================================================
DROP TRIGGER IF EXISTS update_resources_updated_at ON resources;
CREATE TRIGGER update_resources_updated_at
    BEFORE UPDATE ON resources
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

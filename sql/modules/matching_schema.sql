-- ============================================================================
-- Migration: matching_schema.sql
-- Purpose: Create lifestyle profiles and dog matching system tables
-- Author: Agent - WaitingTheLongest Build System
-- Created: 2024-01-29
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (users table)
-- - 010_dogs.sql (dogs table)
--
-- TABLES CREATED:
--   - lifestyle_profiles: User preferences and home characteristics
--   - match_scores: Calculated compatibility scores between profiles and dogs
--
-- ROLLBACK: DROP TABLE match_scores CASCADE; DROP TABLE lifestyle_profiles CASCADE;
-- ============================================================================

-- ============================================================================
-- LIFESTYLE_PROFILES TABLE
-- Purpose: Store adopter lifestyle preferences and home characteristics for matching
-- ============================================================================
CREATE TABLE IF NOT EXISTS lifestyle_profiles (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL UNIQUE,                -- FK to users (one profile per adopter)
        CONSTRAINT fk_lifestyle_profiles_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Home Characteristics
    home_type VARCHAR(50),                       -- house, apartment, condo, mobile_home, farm, other
    has_yard BOOLEAN DEFAULT FALSE,              -- Whether property has yard
    yard_size VARCHAR(50),                       -- small, medium, large
    has_fence BOOLEAN DEFAULT FALSE,             -- Whether yard is fenced

    -- Family Composition
    has_children BOOLEAN DEFAULT FALSE,          -- Whether household has children
    children_ages TEXT[] DEFAULT '{}',           -- Array of children's age ranges (e.g., ['0-3', '4-6', '7-12', '13-17'])

    -- Other Pets
    has_other_dogs BOOLEAN DEFAULT FALSE,        -- Whether household has other dogs
    other_dogs_count INTEGER DEFAULT 0,          -- Number of other dogs
    has_cats BOOLEAN DEFAULT FALSE,              -- Whether household has cats
    has_other_pets BOOLEAN DEFAULT FALSE,        -- Whether household has other pets
    other_pets_description TEXT,                 -- Description of other pets (rabbits, birds, etc.)

    -- Lifestyle Factors
    activity_level VARCHAR(50),                  -- sedentary, low, moderate, high, very_high
    hours_home_daily DECIMAL(4,1),               -- Average hours home per day (0-24)
    work_from_home BOOLEAN DEFAULT FALSE,        -- Whether adopter works from home
    travel_frequency VARCHAR(50),                -- never, rarely, occasionally, frequently

    -- Dog Experience & Training
    dog_experience VARCHAR(50),                  -- none, some, experienced, very_experienced
    willing_to_train BOOLEAN DEFAULT FALSE,      -- Willingness to invest in training

    -- Preferences
    preferred_size TEXT[] DEFAULT '{}',          -- Array: toy, small, medium, large, xlarge
    preferred_age TEXT[] DEFAULT '{}',           -- Array: puppy, young, adult, senior
    preferred_energy VARCHAR(50),                -- low, moderate, high

    -- Special Circumstances
    ok_with_special_needs BOOLEAN DEFAULT FALSE, -- Willing to adopt dogs with special needs
    max_adoption_fee DECIMAL(10,2),              -- Maximum willing to spend on adoption fee
    has_breed_restrictions BOOLEAN DEFAULT FALSE, -- Landlord/HOA breed restrictions
    restricted_breeds TEXT[] DEFAULT '{}',       -- Array of breeds they cannot have
    weight_limit_lbs DECIMAL(5,1),               -- Weight limit in pounds (if applicable)

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE lifestyle_profiles IS 'Adopter lifestyle characteristics used for dog matching';
COMMENT ON COLUMN lifestyle_profiles.user_id IS 'Reference to adopter (one-to-one relationship)';
COMMENT ON COLUMN lifestyle_profiles.home_type IS 'Type of residence: house, apartment, condo, mobile_home, farm, other';
COMMENT ON COLUMN lifestyle_profiles.yard_size IS 'Yard size category: small, medium, large';
COMMENT ON COLUMN lifestyle_profiles.children_ages IS 'Array of age ranges: 0-3, 4-6, 7-12, 13-17';
COMMENT ON COLUMN lifestyle_profiles.activity_level IS 'Adopter activity level affecting dog compatibility';
COMMENT ON COLUMN lifestyle_profiles.hours_home_daily IS 'Average hours per day someone is home (0-24)';
COMMENT ON COLUMN lifestyle_profiles.dog_experience IS 'Prior dog ownership experience level';
COMMENT ON COLUMN lifestyle_profiles.preferred_size IS 'Array of acceptable dog sizes';
COMMENT ON COLUMN lifestyle_profiles.preferred_age IS 'Array of acceptable dog age groups';
COMMENT ON COLUMN lifestyle_profiles.ok_with_special_needs IS 'Whether adopter can care for dogs with medical/behavioral needs';
COMMENT ON COLUMN lifestyle_profiles.weight_limit_lbs IS 'Maximum acceptable dog weight in pounds';

-- Indexes for profile lookups
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_user_id ON lifestyle_profiles(user_id);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_home_type ON lifestyle_profiles(home_type);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_activity_level ON lifestyle_profiles(activity_level);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_dog_experience ON lifestyle_profiles(dog_experience);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_has_children ON lifestyle_profiles(has_children);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_work_from_home ON lifestyle_profiles(work_from_home);

-- Array indexes for preference matching
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_preferred_size ON lifestyle_profiles USING gin(preferred_size);
CREATE INDEX IF NOT EXISTS idx_lifestyle_profiles_preferred_age ON lifestyle_profiles USING gin(preferred_age);

-- ============================================================================
-- MATCH_SCORES TABLE
-- Purpose: Store calculated compatibility scores between profiles and dogs
-- ============================================================================
CREATE TABLE IF NOT EXISTS match_scores (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Foreign Keys
    profile_id UUID NOT NULL,                    -- FK to lifestyle_profiles
        CONSTRAINT fk_match_scores_profile
        FOREIGN KEY (profile_id) REFERENCES lifestyle_profiles(id) ON DELETE CASCADE,
    dog_id UUID NOT NULL,                        -- FK to dogs
        CONSTRAINT fk_match_scores_dog
        FOREIGN KEY (dog_id) REFERENCES dogs(id) ON DELETE CASCADE,

    -- Score
    score DECIMAL(5,2) NOT NULL,                 -- Overall match score (0-100)
        CHECK (score >= 0 AND score <= 100),

    -- Detailed Breakdown
    score_breakdown JSONB,                       -- Detailed scoring breakdown
                                                 -- e.g., {
                                                 --   "home_compatibility": 85,
                                                 --   "activity_match": 90,
                                                 --   "experience_level": 75,
                                                 --   "family_compatibility": 80,
                                                 --   "other_pets_compatibility": 95,
                                                 --   "special_needs_match": 88
                                                 -- }

    -- Metadata
    calculated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(), -- When score was calculated
    recalculation_due_at TIMESTAMP WITH TIME ZONE,       -- When score should be recalculated

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),

    -- Constraint: One match score per profile-dog pair
    CONSTRAINT unique_profile_dog_match UNIQUE (profile_id, dog_id)
);

-- Add comments
COMMENT ON TABLE match_scores IS 'Calculated compatibility scores for dog-adopter matching algorithm';
COMMENT ON COLUMN match_scores.score IS 'Overall compatibility score from 0 to 100';
COMMENT ON COLUMN match_scores.score_breakdown IS 'JSON breakdown of scoring factors (home, activity, experience, family, pets, special needs)';
COMMENT ON COLUMN match_scores.calculated_at IS 'Timestamp when this score was calculated';
COMMENT ON COLUMN match_scores.recalculation_due_at IS 'When this score should be recalculated (optional)';

-- Indexes for efficient matching queries
CREATE INDEX IF NOT EXISTS idx_match_scores_profile_id ON match_scores(profile_id);
CREATE INDEX IF NOT EXISTS idx_match_scores_dog_id ON match_scores(dog_id);

-- Index for finding top matches
CREATE INDEX IF NOT EXISTS idx_match_scores_by_profile_desc ON match_scores(profile_id, score DESC);

-- Index for finding potential matches for a dog
CREATE INDEX IF NOT EXISTS idx_match_scores_by_dog_desc ON match_scores(dog_id, score DESC);

-- Index for finding stale scores that need recalculation
CREATE INDEX IF NOT EXISTS idx_match_scores_recalc_due ON match_scores(recalculation_due_at)
    WHERE recalculation_due_at IS NOT NULL;

-- ============================================================================
-- TRIGGER: Auto-update updated_at on lifestyle_profiles
-- ============================================================================
DROP TRIGGER IF EXISTS update_lifestyle_profiles_updated_at ON lifestyle_profiles;
CREATE TRIGGER update_lifestyle_profiles_updated_at
    BEFORE UPDATE ON lifestyle_profiles
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on match_scores
-- ============================================================================
DROP TRIGGER IF EXISTS update_match_scores_updated_at ON match_scores;
CREATE TRIGGER update_match_scores_updated_at
    BEFORE UPDATE ON match_scores
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

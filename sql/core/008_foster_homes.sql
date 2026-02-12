-- ============================================================================
-- Migration: 008_foster_homes.sql
-- Purpose: Create foster_homes table for foster home registration and matching
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--   005_users.sql (users table for FK)
--
-- PLAN REFERENCE: Foster-First Pathway from zany-munching-waffle.md
-- "Research shows fostering makes dogs 14x more likely to be adopted"
--
-- ROLLBACK: DROP TABLE IF EXISTS foster_homes CASCADE;
-- ============================================================================

-- ============================================================================
-- FOSTER_HOMES TABLE
-- Purpose: Register and manage foster home volunteers
-- Foster is the PRIMARY call-to-action - more effective than adoption alone
-- ============================================================================
CREATE TABLE foster_homes (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- USER REFERENCE
    -- ========================================================================
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- ========================================================================
    -- CAPACITY & HOME ENVIRONMENT
    -- ========================================================================
    max_dogs INTEGER DEFAULT 1,                      -- Maximum dogs they can foster at once
    current_dogs INTEGER DEFAULT 0,                  -- Current number of foster dogs

    -- Home features
    has_yard BOOLEAN DEFAULT FALSE,                  -- Fenced yard available
    yard_size VARCHAR(20),                           -- small, medium, large
    has_pool BOOLEAN DEFAULT FALSE,                  -- Pool (safety consideration)
    home_type VARCHAR(30),                           -- house, apartment, condo, townhouse

    -- Other animals in home
    has_other_dogs BOOLEAN DEFAULT FALSE,
    other_dogs_count INTEGER DEFAULT 0,
    other_dogs_info TEXT,                            -- Breeds, ages, temperaments

    has_cats BOOLEAN DEFAULT FALSE,
    cats_count INTEGER DEFAULT 0,

    has_other_pets BOOLEAN DEFAULT FALSE,
    other_pets_info TEXT,                            -- What other pets

    -- Children in home
    has_children BOOLEAN DEFAULT FALSE,
    children_ages TEXT[],                            -- Array of age ranges: ['0-5', '6-12', '13-17']

    -- ========================================================================
    -- PREFERENCES - What types of dogs they can foster
    -- ========================================================================
    -- Age preferences
    ok_with_puppies BOOLEAN DEFAULT TRUE,            -- < 1 year
    ok_with_young BOOLEAN DEFAULT TRUE,              -- 1-3 years
    ok_with_adults BOOLEAN DEFAULT TRUE,             -- 3-7 years
    ok_with_seniors BOOLEAN DEFAULT TRUE,            -- 7+ years

    -- Size preferences (array for multiple selections)
    preferred_sizes TEXT[] DEFAULT ARRAY['small', 'medium', 'large', 'xlarge'],

    -- Special needs willingness
    ok_with_medical BOOLEAN DEFAULT FALSE,           -- Medical needs (medication, recovery)
    ok_with_behavioral BOOLEAN DEFAULT FALSE,        -- Behavioral needs (training required)
    ok_with_heartworm_positive BOOLEAN DEFAULT FALSE,
    ok_with_special_needs BOOLEAN DEFAULT FALSE,     -- General special needs

    -- Experience level
    experience_level VARCHAR(20) DEFAULT 'beginner', -- beginner, intermediate, experienced
    prior_foster_count INTEGER DEFAULT 0,            -- How many dogs they've fostered before
    experience_notes TEXT,                           -- Their experience description

    -- ========================================================================
    -- LOCATION
    -- ========================================================================
    zip_code VARCHAR(10) NOT NULL,
    city VARCHAR(100),
    state_code VARCHAR(2),
    max_transport_miles INTEGER DEFAULT 50,          -- How far they'll travel to pick up

    -- GPS for distance calculations
    latitude DECIMAL(10, 8),
    longitude DECIMAL(11, 8),

    -- ========================================================================
    -- AVAILABILITY
    -- ========================================================================
    is_available BOOLEAN DEFAULT TRUE,               -- Currently accepting fosters
    availability_notes TEXT,                         -- "Available weekends only", etc.
    preferred_duration VARCHAR(50),                  -- short_term, long_term, any
    min_foster_days INTEGER,                         -- Minimum days they can foster
    max_foster_days INTEGER,                         -- Maximum days they can foster

    -- ========================================================================
    -- VERIFICATION & STATUS
    -- ========================================================================
    is_active BOOLEAN DEFAULT TRUE,
    is_verified BOOLEAN DEFAULT FALSE,               -- Completed verification process

    -- Background check
    background_check_date DATE,
    background_check_status VARCHAR(20),             -- pending, passed, failed, expired
    background_check_expires DATE,

    -- Home check
    home_check_date DATE,
    home_check_status VARCHAR(20),                   -- pending, passed, failed
    home_check_notes TEXT,

    -- References
    references_submitted BOOLEAN DEFAULT FALSE,
    references_verified BOOLEAN DEFAULT FALSE,

    -- ========================================================================
    -- STATISTICS
    -- ========================================================================
    fosters_completed INTEGER DEFAULT 0,             -- Total dogs fostered
    fosters_converted_to_adoption INTEGER DEFAULT 0, -- Foster-to-adopt conversions ("foster fails")
    fosters_returned INTEGER DEFAULT 0,              -- Dogs returned early
    total_foster_days INTEGER DEFAULT 0,             -- Total days of fostering
    average_foster_days DECIMAL(5, 1),               -- Average days per foster

    -- Ratings (from shelter feedback)
    rating_average DECIMAL(3, 2),                    -- 1-5 stars
    rating_count INTEGER DEFAULT 0,

    -- ========================================================================
    -- EMERGENCY CONTACT
    -- ========================================================================
    emergency_contact_name VARCHAR(100),
    emergency_contact_phone VARCHAR(20),
    emergency_contact_relationship VARCHAR(50),

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE foster_homes IS 'Foster home registrations. Fostering is 14x more effective than waiting for adoption - this is a critical table for the life-saving mission.';

-- Primary
COMMENT ON COLUMN foster_homes.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN foster_homes.user_id IS 'Foreign key to users table - must have an account to foster';

-- Capacity
COMMENT ON COLUMN foster_homes.max_dogs IS 'Maximum dogs they can foster simultaneously';
COMMENT ON COLUMN foster_homes.current_dogs IS 'Current number of dogs in foster care';
COMMENT ON COLUMN foster_homes.has_yard IS 'TRUE if home has a fenced yard';
COMMENT ON COLUMN foster_homes.yard_size IS 'Yard size: small, medium, large';
COMMENT ON COLUMN foster_homes.has_pool IS 'TRUE if home has a pool (safety consideration)';
COMMENT ON COLUMN foster_homes.home_type IS 'Type of home: house, apartment, condo, townhouse';

-- Other animals
COMMENT ON COLUMN foster_homes.has_other_dogs IS 'TRUE if resident dogs in home';
COMMENT ON COLUMN foster_homes.other_dogs_count IS 'Number of resident dogs';
COMMENT ON COLUMN foster_homes.other_dogs_info IS 'Details about resident dogs';
COMMENT ON COLUMN foster_homes.has_cats IS 'TRUE if cats in home';
COMMENT ON COLUMN foster_homes.cats_count IS 'Number of cats';
COMMENT ON COLUMN foster_homes.has_other_pets IS 'TRUE if other pets in home';
COMMENT ON COLUMN foster_homes.other_pets_info IS 'Details about other pets';

-- Children
COMMENT ON COLUMN foster_homes.has_children IS 'TRUE if children in home';
COMMENT ON COLUMN foster_homes.children_ages IS 'Array of children age ranges';

-- Preferences
COMMENT ON COLUMN foster_homes.ok_with_puppies IS 'Willing to foster puppies (< 1 year)';
COMMENT ON COLUMN foster_homes.ok_with_young IS 'Willing to foster young dogs (1-3 years)';
COMMENT ON COLUMN foster_homes.ok_with_adults IS 'Willing to foster adult dogs (3-7 years)';
COMMENT ON COLUMN foster_homes.ok_with_seniors IS 'Willing to foster senior dogs (7+ years)';
COMMENT ON COLUMN foster_homes.preferred_sizes IS 'Array of acceptable dog sizes';
COMMENT ON COLUMN foster_homes.ok_with_medical IS 'Willing to foster dogs with medical needs';
COMMENT ON COLUMN foster_homes.ok_with_behavioral IS 'Willing to foster dogs with behavioral needs';
COMMENT ON COLUMN foster_homes.ok_with_heartworm_positive IS 'Willing to foster heartworm positive dogs';
COMMENT ON COLUMN foster_homes.ok_with_special_needs IS 'Willing to foster special needs dogs';
COMMENT ON COLUMN foster_homes.experience_level IS 'Foster experience: beginner, intermediate, experienced';
COMMENT ON COLUMN foster_homes.prior_foster_count IS 'Number of dogs fostered previously (outside platform)';
COMMENT ON COLUMN foster_homes.experience_notes IS 'Description of fostering experience';

-- Location
COMMENT ON COLUMN foster_homes.zip_code IS 'ZIP code for location matching';
COMMENT ON COLUMN foster_homes.city IS 'City';
COMMENT ON COLUMN foster_homes.state_code IS 'State code';
COMMENT ON COLUMN foster_homes.max_transport_miles IS 'Maximum miles willing to travel for pickup';
COMMENT ON COLUMN foster_homes.latitude IS 'GPS latitude for distance calculations';
COMMENT ON COLUMN foster_homes.longitude IS 'GPS longitude for distance calculations';

-- Availability
COMMENT ON COLUMN foster_homes.is_available IS 'Currently accepting new fosters';
COMMENT ON COLUMN foster_homes.availability_notes IS 'Notes about availability';
COMMENT ON COLUMN foster_homes.preferred_duration IS 'Preferred foster duration: short_term, long_term, any';
COMMENT ON COLUMN foster_homes.min_foster_days IS 'Minimum days they can commit to fostering';
COMMENT ON COLUMN foster_homes.max_foster_days IS 'Maximum days they can foster';

-- Verification
COMMENT ON COLUMN foster_homes.is_active IS 'Account is active';
COMMENT ON COLUMN foster_homes.is_verified IS 'Completed full verification process';
COMMENT ON COLUMN foster_homes.background_check_date IS 'Date of background check';
COMMENT ON COLUMN foster_homes.background_check_status IS 'Background check status';
COMMENT ON COLUMN foster_homes.background_check_expires IS 'When background check expires';
COMMENT ON COLUMN foster_homes.home_check_date IS 'Date of home check';
COMMENT ON COLUMN foster_homes.home_check_status IS 'Home check status';
COMMENT ON COLUMN foster_homes.home_check_notes IS 'Notes from home check';
COMMENT ON COLUMN foster_homes.references_submitted IS 'References have been submitted';
COMMENT ON COLUMN foster_homes.references_verified IS 'References have been verified';

-- Statistics
COMMENT ON COLUMN foster_homes.fosters_completed IS 'Total number of dogs successfully fostered';
COMMENT ON COLUMN foster_homes.fosters_converted_to_adoption IS 'Number of foster dogs they adopted ("foster fails")';
COMMENT ON COLUMN foster_homes.fosters_returned IS 'Number of dogs returned early';
COMMENT ON COLUMN foster_homes.total_foster_days IS 'Total days spent fostering';
COMMENT ON COLUMN foster_homes.average_foster_days IS 'Average days per foster placement';
COMMENT ON COLUMN foster_homes.rating_average IS 'Average rating from shelter feedback (1-5)';
COMMENT ON COLUMN foster_homes.rating_count IS 'Number of ratings received';

-- Emergency contact
COMMENT ON COLUMN foster_homes.emergency_contact_name IS 'Emergency contact name';
COMMENT ON COLUMN foster_homes.emergency_contact_phone IS 'Emergency contact phone';
COMMENT ON COLUMN foster_homes.emergency_contact_relationship IS 'Relationship to foster';

-- Timestamps
COMMENT ON COLUMN foster_homes.created_at IS 'Registration timestamp';
COMMENT ON COLUMN foster_homes.updated_at IS 'Last update timestamp';

-- ============================================================================
-- UNIQUE CONSTRAINT
-- One foster home profile per user
-- ============================================================================
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_user_unique
    UNIQUE (user_id);

-- ============================================================================
-- INDEXES
-- ============================================================================
-- User lookup
CREATE INDEX idx_foster_homes_user_id ON foster_homes(user_id);

-- Available fosters
CREATE INDEX idx_foster_homes_available ON foster_homes(is_available, is_verified)
    WHERE is_available = TRUE AND is_active = TRUE;

-- Location-based matching
CREATE INDEX idx_foster_homes_zip ON foster_homes(zip_code);
CREATE INDEX idx_foster_homes_state ON foster_homes(state_code);
CREATE INDEX idx_foster_homes_lat_lng ON foster_homes(latitude, longitude)
    WHERE latitude IS NOT NULL AND longitude IS NOT NULL;

-- Capacity filtering
CREATE INDEX idx_foster_homes_capacity ON foster_homes(max_dogs, current_dogs)
    WHERE is_available = TRUE;

-- Special needs willing
CREATE INDEX idx_foster_homes_medical ON foster_homes(ok_with_medical)
    WHERE ok_with_medical = TRUE AND is_available = TRUE;
CREATE INDEX idx_foster_homes_behavioral ON foster_homes(ok_with_behavioral)
    WHERE ok_with_behavioral = TRUE AND is_available = TRUE;

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_foster_homes_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_foster_homes_updated_at
    BEFORE UPDATE ON foster_homes
    FOR EACH ROW
    EXECUTE FUNCTION update_foster_homes_updated_at();

-- ============================================================================
-- TRIGGER: Update user role to 'foster' when foster home is verified
-- ============================================================================
CREATE OR REPLACE FUNCTION update_user_role_on_foster_verify()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.is_verified = TRUE AND (OLD.is_verified IS NULL OR OLD.is_verified = FALSE) THEN
        UPDATE users
        SET role = CASE
            WHEN role IN ('user') THEN 'foster'
            ELSE role  -- Don't downgrade admins
        END
        WHERE id = NEW.user_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_foster_homes_update_user_role
    AFTER UPDATE OF is_verified ON foster_homes
    FOR EACH ROW
    EXECUTE FUNCTION update_user_role_on_foster_verify();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid experience levels
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_experience_level_valid
    CHECK (experience_level IN ('beginner', 'intermediate', 'experienced'));

-- Valid background check status
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_bg_check_status_valid
    CHECK (background_check_status IS NULL OR background_check_status IN ('pending', 'passed', 'failed', 'expired'));

-- Valid home check status
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_home_check_status_valid
    CHECK (home_check_status IS NULL OR home_check_status IN ('pending', 'passed', 'failed'));

-- Valid home types
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_home_type_valid
    CHECK (home_type IS NULL OR home_type IN ('house', 'apartment', 'condo', 'townhouse', 'mobile_home', 'farm'));

-- Valid yard sizes
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_yard_size_valid
    CHECK (yard_size IS NULL OR yard_size IN ('small', 'medium', 'large'));

-- Valid preferred duration
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_preferred_duration_valid
    CHECK (preferred_duration IS NULL OR preferred_duration IN ('short_term', 'long_term', 'any', 'emergency_only'));

-- Non-negative counts
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_max_dogs_valid
    CHECK (max_dogs >= 1);
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_current_dogs_valid
    CHECK (current_dogs >= 0);
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_current_lte_max
    CHECK (current_dogs <= max_dogs);
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_fosters_completed_valid
    CHECK (fosters_completed >= 0);

-- Valid rating
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_rating_valid
    CHECK (rating_average IS NULL OR (rating_average >= 1 AND rating_average <= 5));

-- Valid GPS coordinates
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_latitude_valid
    CHECK (latitude IS NULL OR (latitude >= -90 AND latitude <= 90));
ALTER TABLE foster_homes ADD CONSTRAINT foster_homes_longitude_valid
    CHECK (longitude IS NULL OR (longitude >= -180 AND longitude <= 180));

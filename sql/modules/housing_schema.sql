-- ============================================================================
-- Migration: housing_schema.sql
-- Purpose: Create pet-friendly housing database and submission tables
-- Author: Agent - WaitingTheLongest Build System
-- Created: 2024-01-29
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (users table)
-- - 015_states.sql (states table for state_code)
--
-- TABLES CREATED:
--   - pet_friendly_housing: Directory of pet-friendly rental properties
--   - housing_submissions: User submissions for new housing listings
--
-- ROLLBACK: DROP TABLE housing_submissions CASCADE; DROP TABLE pet_friendly_housing CASCADE;
-- ============================================================================

-- ============================================================================
-- PET_FRIENDLY_HOUSING TABLE
-- Purpose: Directory of pet-friendly rental properties and housing options
-- ============================================================================
CREATE TABLE IF NOT EXISTS pet_friendly_housing (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Property Information
    name VARCHAR(255) NOT NULL,                  -- Property/complex name
    property_type VARCHAR(100) NOT NULL,         -- apartment_complex, rental_company, single_property, community, other
    management_company VARCHAR(255),             -- Property management company name

    -- Location
    address VARCHAR(500) NOT NULL,               -- Street address
    city VARCHAR(100) NOT NULL,                  -- City
    state_code VARCHAR(2) NOT NULL,              -- State code (FK reference pattern)
    zip_code VARCHAR(10) NOT NULL,               -- Zip code
    latitude DECIMAL(10,8),                      -- GPS latitude
    longitude DECIMAL(11,8),                     -- GPS longitude

    -- Contact Information
    phone VARCHAR(20),                           -- Contact phone number
    email VARCHAR(255),                          -- Contact email
    website VARCHAR(500),                        -- Property website

    -- Pet Policies
    dogs_allowed BOOLEAN DEFAULT TRUE,           -- Whether dogs are allowed
    cats_allowed BOOLEAN DEFAULT TRUE,           -- Whether cats are allowed
    max_pets INTEGER,                            -- Maximum number of pets allowed
    weight_limit_lbs DECIMAL(5,1),               -- Weight limit per pet in pounds (NULL = no limit)
    breed_restrictions TEXT[] DEFAULT '{}',      -- Array of restricted breeds (empty = no restrictions)

    -- Fees & Costs
    pet_deposit DECIMAL(10,2),                   -- One-time pet deposit ($)
    monthly_pet_rent DECIMAL(10,2),              -- Monthly pet rent ($)

    -- Amenities
    pet_amenities TEXT[] DEFAULT '{}',           -- Array: dog_park, dog_bed, food_bowls, waste_station, etc.

    -- Additional Information
    notes TEXT,                                  -- Additional notes/policies
    wheelchair_accessible BOOLEAN DEFAULT FALSE, -- ADA compliance
    utilities_included BOOLEAN DEFAULT FALSE,    -- Whether utilities are included in rent

    -- Verification & Quality
    verified BOOLEAN DEFAULT FALSE,              -- Whether listing has been verified
    verified_at TIMESTAMP WITH TIME ZONE,        -- When listing was verified
    verified_by_user_id UUID,                    -- User who verified listing
        CONSTRAINT fk_housing_verified_by_user
        FOREIGN KEY (verified_by_user_id) REFERENCES users(id) ON DELETE SET NULL,

    reported_count INTEGER DEFAULT 0,            -- Number of times reported as inaccurate/problematic
    last_verified_at TIMESTAMP WITH TIME ZONE,   -- Last verification date

    -- Engagement
    view_count INTEGER DEFAULT 0,                -- Number of views
    click_count INTEGER DEFAULT 0,               -- Number of clicks to external link

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE pet_friendly_housing IS 'Directory of verified pet-friendly rental properties and housing';
COMMENT ON COLUMN pet_friendly_housing.property_type IS 'Type: apartment_complex, rental_company, single_property, community, other';
COMMENT ON COLUMN pet_friendly_housing.state_code IS 'Two-letter state code (e.g., CA, TX, NY)';
COMMENT ON COLUMN pet_friendly_housing.weight_limit_lbs IS 'Weight limit per pet (NULL means no limit)';
COMMENT ON COLUMN pet_friendly_housing.breed_restrictions IS 'Array of breeds not allowed (empty array = no restrictions)';
COMMENT ON COLUMN pet_friendly_housing.pet_amenities IS 'Array of amenities: dog_park, dog_bed, food_bowls, waste_station, etc.';
COMMENT ON COLUMN pet_friendly_housing.verified IS 'Whether the listing information has been verified';
COMMENT ON COLUMN pet_friendly_housing.reported_count IS 'Number of user reports of inaccurate information';

-- Indexes for housing searches
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_city_state ON pet_friendly_housing(city, state_code);
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_state_code ON pet_friendly_housing(state_code);
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_property_type ON pet_friendly_housing(property_type);
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_verified ON pet_friendly_housing(verified);
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_created_at ON pet_friendly_housing(created_at DESC);

-- Geospatial index for nearby searches
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_location ON pet_friendly_housing
    USING gist(ll_to_earth(latitude, longitude))
    WHERE latitude IS NOT NULL AND longitude IS NOT NULL;

-- Amenities array index
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_amenities ON pet_friendly_housing USING gin(pet_amenities);

-- Index for finding problematic listings
CREATE INDEX IF NOT EXISTS idx_pet_friendly_housing_reported ON pet_friendly_housing(reported_count DESC)
    WHERE reported_count > 0;

-- ============================================================================
-- HOUSING_SUBMISSIONS TABLE
-- Purpose: Track user submissions for new pet-friendly housing listings
-- ============================================================================
CREATE TABLE IF NOT EXISTS housing_submissions (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Submitter Information
    user_id UUID NOT NULL,                       -- FK to users (who submitted)
        CONSTRAINT fk_housing_submissions_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Reference to Existing Housing (if editing)
    housing_id UUID,                             -- FK to pet_friendly_housing (if updating existing)
        CONSTRAINT fk_housing_submissions_housing
        FOREIGN KEY (housing_id) REFERENCES pet_friendly_housing(id) ON DELETE SET NULL,

    -- Submission Data
    property_name VARCHAR(255) NOT NULL,         -- Property/complex name
    city VARCHAR(100) NOT NULL,                  -- City
    state_code VARCHAR(2) NOT NULL,              -- State code

    dogs_allowed BOOLEAN,                        -- Are dogs allowed?
    notes TEXT,                                  -- Additional notes about the property

    -- Additional contact info from submitter
    submitter_phone VARCHAR(20),                 -- Submitter's contact phone
    submitter_relationship VARCHAR(100),         -- Relationship to property (resident, manager, employee, etc.)

    -- Processing Status
    status VARCHAR(50) NOT NULL DEFAULT 'pending' -- pending, approved, rejected, merged
        CHECK (status IN ('pending', 'approved', 'rejected', 'merged')),

    reviewed_at TIMESTAMP WITH TIME ZONE,        -- When submission was reviewed
    reviewed_by_user_id UUID,                    -- User who reviewed submission
        CONSTRAINT fk_housing_submissions_reviewed_by
        FOREIGN KEY (reviewed_by_user_id) REFERENCES users(id) ON DELETE SET NULL,

    review_notes TEXT,                           -- Notes from reviewer

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE housing_submissions IS 'User submissions for new or updated pet-friendly housing listings';
COMMENT ON COLUMN housing_submissions.user_id IS 'User who submitted the suggestion';
COMMENT ON COLUMN housing_submissions.housing_id IS 'Reference to existing listing (if updating)';
COMMENT ON COLUMN housing_submissions.submitter_relationship IS 'How submitter relates to property (resident, manager, etc.)';
COMMENT ON COLUMN housing_submissions.status IS 'Review status: pending, approved, rejected, merged';
COMMENT ON COLUMN housing_submissions.review_notes IS 'Notes from admin reviewer about decision';

-- Indexes for submission queries
CREATE INDEX IF NOT EXISTS idx_housing_submissions_user_id ON housing_submissions(user_id);
CREATE INDEX IF NOT EXISTS idx_housing_submissions_housing_id ON housing_submissions(housing_id) WHERE housing_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_housing_submissions_status ON housing_submissions(status);
CREATE INDEX IF NOT EXISTS idx_housing_submissions_created_at ON housing_submissions(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_housing_submissions_city_state ON housing_submissions(city, state_code);

-- Index for pending reviews
CREATE INDEX IF NOT EXISTS idx_housing_submissions_pending ON housing_submissions(created_at ASC)
    WHERE status = 'pending';

-- ============================================================================
-- TRIGGER: Auto-update updated_at on pet_friendly_housing
-- ============================================================================
DROP TRIGGER IF EXISTS update_pet_friendly_housing_updated_at ON pet_friendly_housing;
CREATE TRIGGER update_pet_friendly_housing_updated_at
    BEFORE UPDATE ON pet_friendly_housing
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on housing_submissions
-- ============================================================================
DROP TRIGGER IF EXISTS update_housing_submissions_updated_at ON housing_submissions;
CREATE TRIGGER update_housing_submissions_updated_at
    BEFORE UPDATE ON housing_submissions
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

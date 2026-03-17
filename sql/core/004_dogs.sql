-- ============================================================================
-- Migration: 004_dogs.sql
-- Purpose: Create dogs table - the CORE table of the WaitingTheLongest platform
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp, pg_trgm)
--   003_shelters.sql (shelters table for FK)
--
-- MODEL REFERENCE: wtl::core::models::Dog from INTEGRATION_CONTRACTS.md
--
-- THIS IS THE MOST IMPORTANT TABLE IN THE SYSTEM.
-- Every dog in a US shelter who needs a home is represented here.
-- The wait time counter (YY:MM:DD:HH:MM:SS) is calculated from intake_date.
-- The countdown timer (DD:HH:MM:SS) is calculated from euthanasia_date.
--
-- ROLLBACK: DROP TABLE IF EXISTS dogs CASCADE;
-- ============================================================================

-- ============================================================================
-- DOGS TABLE
-- Purpose: Core dog records with all attributes needed for:
--   1. LED wait time counter (time since intake_date)
--   2. Urgency countdown (time until euthanasia_date)
--   3. Search and filtering
--   4. Foster/adoption matching
-- ============================================================================
CREATE TABLE dogs (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- BASIC INFORMATION
    -- ========================================================================
    name VARCHAR(100) NOT NULL,
    shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,

    -- Breed information
    breed_primary VARCHAR(100) NOT NULL,             -- Primary breed (e.g., "Labrador Retriever")
    breed_secondary VARCHAR(100),                    -- Secondary breed for mixes
    breed_mixed BOOLEAN DEFAULT FALSE,               -- TRUE if mixed breed

    -- Physical characteristics
    size VARCHAR(20) NOT NULL DEFAULT 'medium',      -- small, medium, large, xlarge
    age_category VARCHAR(20) NOT NULL DEFAULT 'adult',  -- puppy, young, adult, senior
    age_months INTEGER,                              -- Age in months (calculated or estimated)
    gender VARCHAR(10) NOT NULL,                     -- male, female
    color_primary VARCHAR(50),                       -- Primary coat color
    color_secondary VARCHAR(50),                     -- Secondary coat color
    weight_lbs DECIMAL(5, 1),                        -- Weight in pounds

    -- Fixed/altered status
    is_spayed_neutered BOOLEAN DEFAULT FALSE,
    spay_neuter_date DATE,

    -- ========================================================================
    -- STATUS & AVAILABILITY
    -- ========================================================================
    status VARCHAR(30) NOT NULL DEFAULT 'available', -- See constraint for valid values
    is_available BOOLEAN DEFAULT TRUE,               -- Quick filter for available dogs
    hold_reason VARCHAR(100),                        -- If on hold, why?
    hold_until DATE,                                 -- If on hold, until when?

    -- ========================================================================
    -- DATES - Critical for wait time calculations
    -- ========================================================================
    -- When the dog entered the shelter system
    intake_date TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    intake_type VARCHAR(50),                         -- stray, owner_surrender, transfer, return

    -- When the dog became available for adoption (may differ from intake)
    available_date TIMESTAMP WITH TIME ZONE,

    -- When the dog was adopted/outcome date
    outcome_date TIMESTAMP WITH TIME ZONE,
    outcome_type VARCHAR(50),                        -- adopted, transferred, returned_to_owner, deceased, euthanized

    -- ========================================================================
    -- URGENCY TRACKING - Critical for life-saving prioritization
    -- ========================================================================
    -- Scheduled euthanasia date (if known) - THIS DRIVES THE COUNTDOWN TIMER
    euthanasia_date TIMESTAMP WITH TIME ZONE,

    -- Urgency level (calculated by UrgencyEngine)
    urgency_level VARCHAR(20) DEFAULT 'normal',      -- normal, medium, high, critical

    -- Euthanasia list tracking
    is_on_euthanasia_list BOOLEAN DEFAULT FALSE,     -- TRUE if on shelter's euth list
    euthanasia_list_added_at TIMESTAMP WITH TIME ZONE,

    -- Rescue coordination
    rescue_pull_status VARCHAR(30),                  -- available, pending, confirmed, pulled
    rescue_hold_until TIMESTAMP WITH TIME ZONE,      -- Temporary hold for rescue coordination
    rescue_organization_id UUID,                     -- FK to shelters (rescue pulling)
    rescue_notes TEXT,                               -- Notes for rescue coordination

    -- ========================================================================
    -- MEDIA
    -- ========================================================================
    photo_urls TEXT[] DEFAULT '{}',                  -- Array of photo URLs
    primary_photo_url VARCHAR(500),                  -- Main photo for listings
    video_url VARCHAR(500),                          -- Video URL if available
    thumbnail_url VARCHAR(500),                      -- Thumbnail for quick display

    -- ========================================================================
    -- DESCRIPTION & PERSONALITY
    -- ========================================================================
    description TEXT,                                -- Full description/bio
    short_description VARCHAR(500),                  -- Brief summary for listings

    -- Personality tags (array for flexibility)
    tags TEXT[] DEFAULT '{}',                        -- e.g., ['good_with_kids', 'house_trained', 'leash_trained']

    -- Compatibility flags (structured for filtering)
    good_with_kids BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    house_trained BOOLEAN,
    crate_trained BOOLEAN,
    leash_trained BOOLEAN,

    -- Special needs
    has_special_needs BOOLEAN DEFAULT FALSE,
    special_needs_description TEXT,

    -- Medical
    has_medical_conditions BOOLEAN DEFAULT FALSE,
    medical_conditions TEXT,
    is_heartworm_positive BOOLEAN DEFAULT FALSE,
    heartworm_treatment_status VARCHAR(50),

    -- Behavioral
    has_behavioral_notes BOOLEAN DEFAULT FALSE,
    behavioral_notes TEXT,
    energy_level VARCHAR(20),                        -- low, medium, high

    -- ========================================================================
    -- ADOPTION INFORMATION
    -- ========================================================================
    adoption_fee DECIMAL(8, 2),                      -- Adoption fee in dollars
    fee_includes TEXT,                               -- What the fee includes (spay/neuter, vaccines, etc.)
    is_fee_waived BOOLEAN DEFAULT FALSE,             -- Fee waived for urgent dogs?

    -- ========================================================================
    -- EXTERNAL SYSTEM TRACKING
    -- ========================================================================
    external_id VARCHAR(100),                        -- ID from source system (RescueGroups, etc.)
    external_source VARCHAR(50),                     -- Source system name
    external_url VARCHAR(500),                       -- Direct link to source listing
    last_synced_at TIMESTAMP WITH TIME ZONE,         -- Last sync from external system

    -- ========================================================================
    -- INTERNAL TRACKING
    -- ========================================================================
    view_count INTEGER DEFAULT 0,                    -- Number of times viewed
    favorite_count INTEGER DEFAULT 0,                -- Number of times favorited
    share_count INTEGER DEFAULT 0,                   -- Number of times shared
    inquiry_count INTEGER DEFAULT 0,                 -- Number of adoption inquiries

    -- Last activity tracking
    last_viewed_at TIMESTAMP WITH TIME ZONE,
    last_favorited_at TIMESTAMP WITH TIME ZONE,
    last_shared_at TIMESTAMP WITH TIME ZONE,

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE dogs IS 'Core dog records - the heart of WaitingTheLongest. Each row represents a dog in a shelter who needs a home. Wait time is calculated from intake_date, urgency countdown from euthanasia_date.';

-- Primary identifiers
COMMENT ON COLUMN dogs.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN dogs.name IS 'Dog name as provided by shelter';
COMMENT ON COLUMN dogs.shelter_id IS 'Foreign key to shelters table - which shelter has this dog';

-- Breed information
COMMENT ON COLUMN dogs.breed_primary IS 'Primary breed (e.g., Labrador Retriever, Pit Bull Terrier)';
COMMENT ON COLUMN dogs.breed_secondary IS 'Secondary breed for mixed breeds';
COMMENT ON COLUMN dogs.breed_mixed IS 'TRUE if dog is a mix of breeds';

-- Physical characteristics
COMMENT ON COLUMN dogs.size IS 'Size category: small (<25lbs), medium (25-50lbs), large (50-90lbs), xlarge (90+lbs)';
COMMENT ON COLUMN dogs.age_category IS 'Age category: puppy (<1yr), young (1-3yr), adult (3-7yr), senior (7+yr)';
COMMENT ON COLUMN dogs.age_months IS 'Age in months - used for precise age display';
COMMENT ON COLUMN dogs.gender IS 'Gender: male or female';
COMMENT ON COLUMN dogs.color_primary IS 'Primary coat color';
COMMENT ON COLUMN dogs.color_secondary IS 'Secondary coat color (for multi-colored dogs)';
COMMENT ON COLUMN dogs.weight_lbs IS 'Weight in pounds';
COMMENT ON COLUMN dogs.is_spayed_neutered IS 'TRUE if dog has been spayed/neutered';
COMMENT ON COLUMN dogs.spay_neuter_date IS 'Date of spay/neuter procedure';

-- Status
COMMENT ON COLUMN dogs.status IS 'Current status: available, pending, adopted, foster, hold, transferred, deceased, euthanized';
COMMENT ON COLUMN dogs.is_available IS 'Quick boolean for available dogs - denormalized for performance';
COMMENT ON COLUMN dogs.hold_reason IS 'If status is hold, the reason (e.g., medical, behavioral, rescue_pending)';
COMMENT ON COLUMN dogs.hold_until IS 'If on hold, the expected end date';

-- Dates (CRITICAL)
COMMENT ON COLUMN dogs.intake_date IS 'CRITICAL: Date dog entered shelter - used to calculate wait time counter (YY:MM:DD:HH:MM:SS)';
COMMENT ON COLUMN dogs.intake_type IS 'How dog entered shelter: stray, owner_surrender, transfer, return';
COMMENT ON COLUMN dogs.available_date IS 'Date dog became available for adoption (may be after stray hold period)';
COMMENT ON COLUMN dogs.outcome_date IS 'Date of final outcome (adoption, transfer, etc.)';
COMMENT ON COLUMN dogs.outcome_type IS 'Type of outcome: adopted, transferred, returned_to_owner, deceased, euthanized';

-- Urgency (CRITICAL FOR LIFE-SAVING)
COMMENT ON COLUMN dogs.euthanasia_date IS 'CRITICAL: Scheduled euthanasia date - drives the countdown timer (DD:HH:MM:SS). NULL if unknown.';
COMMENT ON COLUMN dogs.urgency_level IS 'Calculated urgency: normal (no date), medium (<7 days), high (<72 hrs), critical (<24 hrs)';
COMMENT ON COLUMN dogs.is_on_euthanasia_list IS 'TRUE if dog is on shelter euthanasia list - highest priority for rescue';
COMMENT ON COLUMN dogs.euthanasia_list_added_at IS 'When dog was added to euthanasia list';
COMMENT ON COLUMN dogs.rescue_pull_status IS 'Rescue coordination status: available, pending, confirmed, pulled';
COMMENT ON COLUMN dogs.rescue_hold_until IS 'Temporary hold deadline for rescue coordination';
COMMENT ON COLUMN dogs.rescue_organization_id IS 'If being pulled by a rescue, their shelter ID';
COMMENT ON COLUMN dogs.rescue_notes IS 'Notes for rescue coordinators';

-- Media
COMMENT ON COLUMN dogs.photo_urls IS 'Array of photo URLs - all available photos';
COMMENT ON COLUMN dogs.primary_photo_url IS 'Primary photo URL for listings and thumbnails';
COMMENT ON COLUMN dogs.video_url IS 'Video URL if available - videos double adoption rates';
COMMENT ON COLUMN dogs.thumbnail_url IS 'Pre-generated thumbnail URL for fast loading';

-- Description
COMMENT ON COLUMN dogs.description IS 'Full biography/description from shelter';
COMMENT ON COLUMN dogs.short_description IS 'Brief summary for search results and cards';
COMMENT ON COLUMN dogs.tags IS 'Array of tags: good_with_kids, house_trained, leash_trained, crate_trained, etc.';

-- Compatibility
COMMENT ON COLUMN dogs.good_with_kids IS 'TRUE if known to be good with children';
COMMENT ON COLUMN dogs.good_with_dogs IS 'TRUE if known to be good with other dogs';
COMMENT ON COLUMN dogs.good_with_cats IS 'TRUE if known to be good with cats';
COMMENT ON COLUMN dogs.house_trained IS 'TRUE if house trained';
COMMENT ON COLUMN dogs.crate_trained IS 'TRUE if crate trained';
COMMENT ON COLUMN dogs.leash_trained IS 'TRUE if leash trained';

-- Special needs
COMMENT ON COLUMN dogs.has_special_needs IS 'TRUE if dog has special needs (medical, behavioral, etc.)';
COMMENT ON COLUMN dogs.special_needs_description IS 'Description of special needs';
COMMENT ON COLUMN dogs.has_medical_conditions IS 'TRUE if dog has ongoing medical conditions';
COMMENT ON COLUMN dogs.medical_conditions IS 'Description of medical conditions';
COMMENT ON COLUMN dogs.is_heartworm_positive IS 'TRUE if heartworm positive';
COMMENT ON COLUMN dogs.heartworm_treatment_status IS 'Heartworm treatment status: not_started, in_progress, completed';
COMMENT ON COLUMN dogs.has_behavioral_notes IS 'TRUE if there are behavioral notes';
COMMENT ON COLUMN dogs.behavioral_notes IS 'Behavioral assessment notes';
COMMENT ON COLUMN dogs.energy_level IS 'Energy level: low, medium, high';

-- Adoption
COMMENT ON COLUMN dogs.adoption_fee IS 'Adoption fee in dollars';
COMMENT ON COLUMN dogs.fee_includes IS 'What adoption fee includes (spay/neuter, vaccines, microchip, etc.)';
COMMENT ON COLUMN dogs.is_fee_waived IS 'TRUE if fee is waived (for urgent dogs, seniors, etc.)';

-- External tracking
COMMENT ON COLUMN dogs.external_id IS 'ID in external system (RescueGroups, PetFinder, etc.)';
COMMENT ON COLUMN dogs.external_source IS 'Name of external data source';
COMMENT ON COLUMN dogs.external_url IS 'Direct URL to listing in external system';
COMMENT ON COLUMN dogs.last_synced_at IS 'Last time data was synced from external system';

-- Metrics
COMMENT ON COLUMN dogs.view_count IS 'Number of times dog profile was viewed';
COMMENT ON COLUMN dogs.favorite_count IS 'Number of times dog was favorited';
COMMENT ON COLUMN dogs.share_count IS 'Number of times dog was shared';
COMMENT ON COLUMN dogs.inquiry_count IS 'Number of adoption/foster inquiries';
COMMENT ON COLUMN dogs.last_viewed_at IS 'Timestamp of last view';
COMMENT ON COLUMN dogs.last_favorited_at IS 'Timestamp of last favorite';
COMMENT ON COLUMN dogs.last_shared_at IS 'Timestamp of last share';

-- Timestamps
COMMENT ON COLUMN dogs.created_at IS 'Record creation timestamp';
COMMENT ON COLUMN dogs.updated_at IS 'Record last modification timestamp';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Foreign key index
CREATE INDEX idx_dogs_shelter_id ON dogs(shelter_id);

-- Status filtering (most common query)
CREATE INDEX idx_dogs_status ON dogs(status);
CREATE INDEX idx_dogs_is_available ON dogs(is_available) WHERE is_available = TRUE;

-- Urgency filtering (CRITICAL for life-saving features)
CREATE INDEX idx_dogs_urgency_level ON dogs(urgency_level);
CREATE INDEX idx_dogs_euthanasia_date ON dogs(euthanasia_date)
    WHERE euthanasia_date IS NOT NULL;
CREATE INDEX idx_dogs_is_on_euthanasia_list ON dogs(is_on_euthanasia_list)
    WHERE is_on_euthanasia_list = TRUE;

-- Wait time sorting (intake_date is critical)
CREATE INDEX idx_dogs_intake_date ON dogs(intake_date);

-- Search filters
CREATE INDEX idx_dogs_breed_primary ON dogs(breed_primary);
CREATE INDEX idx_dogs_size ON dogs(size);
CREATE INDEX idx_dogs_age_category ON dogs(age_category);
CREATE INDEX idx_dogs_gender ON dogs(gender);

-- Compatibility filtering
CREATE INDEX idx_dogs_good_with_kids ON dogs(good_with_kids) WHERE good_with_kids = TRUE;
CREATE INDEX idx_dogs_good_with_dogs ON dogs(good_with_dogs) WHERE good_with_dogs = TRUE;
CREATE INDEX idx_dogs_good_with_cats ON dogs(good_with_cats) WHERE good_with_cats = TRUE;

-- Special needs filtering
CREATE INDEX idx_dogs_has_special_needs ON dogs(has_special_needs) WHERE has_special_needs = TRUE;

-- Text search (fuzzy matching on name and breed)
CREATE INDEX idx_dogs_name_trgm ON dogs USING gin(name gin_trgm_ops);
CREATE INDEX idx_dogs_breed_trgm ON dogs USING gin(breed_primary gin_trgm_ops);
CREATE INDEX idx_dogs_description_trgm ON dogs USING gin(description gin_trgm_ops)
    WHERE description IS NOT NULL;

-- External ID lookup (for data sync)
CREATE INDEX idx_dogs_external_id ON dogs(external_source, external_id)
    WHERE external_id IS NOT NULL;

-- Composite indexes for common queries
-- "Available dogs at shelter X"
CREATE INDEX idx_dogs_shelter_available ON dogs(shelter_id, is_available)
    WHERE is_available = TRUE;

-- "Urgent dogs that are available"
CREATE INDEX idx_dogs_urgency_available ON dogs(urgency_level, is_available)
    WHERE is_available = TRUE;

-- "Available dogs sorted by wait time"
CREATE INDEX idx_dogs_available_intake ON dogs(intake_date, is_available)
    WHERE is_available = TRUE;

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_dogs_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_dogs_updated_at
    BEFORE UPDATE ON dogs
    FOR EACH ROW
    EXECUTE FUNCTION update_dogs_updated_at();

-- ============================================================================
-- TRIGGER: Auto-update is_available based on status
-- ============================================================================
CREATE OR REPLACE FUNCTION update_dogs_is_available()
RETURNS TRIGGER AS $$
BEGIN
    NEW.is_available = (NEW.status = 'available');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_dogs_is_available
    BEFORE INSERT OR UPDATE OF status ON dogs
    FOR EACH ROW
    EXECUTE FUNCTION update_dogs_is_available();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid status values
ALTER TABLE dogs ADD CONSTRAINT dogs_status_valid
    CHECK (status IN ('available', 'pending', 'adopted', 'foster', 'hold', 'transferred', 'deceased', 'euthanized', 'returned_to_owner'));

-- Valid size values
ALTER TABLE dogs ADD CONSTRAINT dogs_size_valid
    CHECK (size IN ('small', 'medium', 'large', 'xlarge'));

-- Valid age category values
ALTER TABLE dogs ADD CONSTRAINT dogs_age_category_valid
    CHECK (age_category IN ('puppy', 'young', 'adult', 'senior'));

-- Valid gender values
ALTER TABLE dogs ADD CONSTRAINT dogs_gender_valid
    CHECK (gender IN ('male', 'female'));

-- Valid urgency level values
ALTER TABLE dogs ADD CONSTRAINT dogs_urgency_level_valid
    CHECK (urgency_level IN ('normal', 'medium', 'high', 'critical'));

-- Valid intake type values
ALTER TABLE dogs ADD CONSTRAINT dogs_intake_type_valid
    CHECK (intake_type IS NULL OR intake_type IN ('stray', 'owner_surrender', 'transfer', 'return', 'born_in_shelter', 'seized'));

-- Valid outcome type values
ALTER TABLE dogs ADD CONSTRAINT dogs_outcome_type_valid
    CHECK (outcome_type IS NULL OR outcome_type IN ('adopted', 'transferred', 'returned_to_owner', 'deceased', 'euthanized', 'escaped', 'foster'));

-- Valid rescue pull status values
ALTER TABLE dogs ADD CONSTRAINT dogs_rescue_pull_status_valid
    CHECK (rescue_pull_status IS NULL OR rescue_pull_status IN ('available', 'pending', 'confirmed', 'pulled', 'cancelled'));

-- Valid energy level values
ALTER TABLE dogs ADD CONSTRAINT dogs_energy_level_valid
    CHECK (energy_level IS NULL OR energy_level IN ('low', 'medium', 'high'));

-- Age months must be non-negative
ALTER TABLE dogs ADD CONSTRAINT dogs_age_months_valid
    CHECK (age_months IS NULL OR age_months >= 0);

-- Weight must be positive
ALTER TABLE dogs ADD CONSTRAINT dogs_weight_valid
    CHECK (weight_lbs IS NULL OR weight_lbs > 0);

-- Adoption fee must be non-negative
ALTER TABLE dogs ADD CONSTRAINT dogs_adoption_fee_valid
    CHECK (adoption_fee IS NULL OR adoption_fee >= 0);

-- Counts must be non-negative
ALTER TABLE dogs ADD CONSTRAINT dogs_view_count_valid
    CHECK (view_count >= 0);
ALTER TABLE dogs ADD CONSTRAINT dogs_favorite_count_valid
    CHECK (favorite_count >= 0);
ALTER TABLE dogs ADD CONSTRAINT dogs_share_count_valid
    CHECK (share_count >= 0);
ALTER TABLE dogs ADD CONSTRAINT dogs_inquiry_count_valid
    CHECK (inquiry_count >= 0);

-- Intake date must be in the past or present
ALTER TABLE dogs ADD CONSTRAINT dogs_intake_date_valid
    CHECK (intake_date <= NOW() + INTERVAL '1 day');

-- Euthanasia date must be in the future (if set)
-- Note: This is a soft check - dates can be in the past for historical records
-- ALTER TABLE dogs ADD CONSTRAINT dogs_euthanasia_date_valid
--     CHECK (euthanasia_date IS NULL OR euthanasia_date > intake_date);

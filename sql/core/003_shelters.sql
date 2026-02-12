-- ============================================================================
-- Migration: 003_shelters.sql
-- Purpose: Create shelters table for animal shelter/rescue organization tracking
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--   002_states.sql (states table for FK)
--
-- MODEL REFERENCE: wtl::core::models::Shelter from INTEGRATION_CONTRACTS.md
--
-- ROLLBACK: DROP TABLE IF EXISTS shelters CASCADE;
-- ============================================================================

-- ============================================================================
-- SHELTERS TABLE
-- Purpose: Animal shelters, rescues, and organizations that house dogs
-- Includes kill shelter tracking for urgency calculations
-- ============================================================================
CREATE TABLE shelters (
    -- Primary identifier
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Basic information
    name VARCHAR(255) NOT NULL,
    state_code VARCHAR(2) NOT NULL REFERENCES states(code) ON DELETE RESTRICT,

    -- Location details
    city VARCHAR(100) NOT NULL,
    address VARCHAR(255),
    zip_code VARCHAR(10),
    latitude DECIMAL(10, 8),                         -- GPS latitude for distance calculations
    longitude DECIMAL(11, 8),                        -- GPS longitude for distance calculations

    -- Contact information
    phone VARCHAR(20),
    email VARCHAR(255),
    website VARCHAR(500),

    -- Classification
    shelter_type VARCHAR(50) NOT NULL DEFAULT 'municipal',  -- municipal, rescue, private, foster_network

    -- Kill shelter tracking (critical for urgency calculations)
    is_kill_shelter BOOLEAN DEFAULT FALSE,           -- TRUE if euthanasia is practiced
    avg_hold_days INTEGER,                           -- Average days before euthanasia risk
    euthanasia_schedule VARCHAR(100),                -- e.g., "Tues/Thurs/Sat", "End of week"
    accepts_rescue_pulls BOOLEAN DEFAULT TRUE,       -- Whether rescues can pull dogs
    last_euthanasia_list_sync TIMESTAMP WITH TIME ZONE,  -- Last time we synced their list

    -- Urgency scoring
    urgency_multiplier DECIMAL(3, 2) DEFAULT 1.00,   -- Higher = more urgent shelter (max 9.99)

    -- Cached statistics (updated by triggers/functions)
    dog_count INTEGER DEFAULT 0,                     -- Total dogs at shelter
    available_count INTEGER DEFAULT 0,               -- Dogs available for adoption

    -- Verification status
    is_verified BOOLEAN DEFAULT FALSE,               -- Verified by staff/IRS data
    verified_at TIMESTAMP WITH TIME ZONE,
    verified_by VARCHAR(100),                        -- Who verified (user_id or 'system')

    -- Operational status
    is_active BOOLEAN DEFAULT TRUE,                  -- Whether shelter is operational
    deactivated_at TIMESTAMP WITH TIME ZONE,
    deactivation_reason VARCHAR(255),

    -- External identifiers for data sync
    external_id VARCHAR(100),                        -- ID from external system (RescueGroups, etc.)
    external_source VARCHAR(50),                     -- Source system name

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE shelters IS 'Animal shelters, rescues, and organizations. Includes kill shelter tracking for urgency-based prioritization.';

COMMENT ON COLUMN shelters.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN shelters.name IS 'Organization name (e.g., Austin Animal Center)';
COMMENT ON COLUMN shelters.state_code IS 'Two-letter state code - foreign key to states table';
COMMENT ON COLUMN shelters.city IS 'City where shelter is located';
COMMENT ON COLUMN shelters.address IS 'Street address';
COMMENT ON COLUMN shelters.zip_code IS 'ZIP code (5 or 9 digit)';
COMMENT ON COLUMN shelters.latitude IS 'GPS latitude for distance-based searches (-90 to 90)';
COMMENT ON COLUMN shelters.longitude IS 'GPS longitude for distance-based searches (-180 to 180)';
COMMENT ON COLUMN shelters.phone IS 'Primary contact phone number';
COMMENT ON COLUMN shelters.email IS 'Primary contact email';
COMMENT ON COLUMN shelters.website IS 'Organization website URL';
COMMENT ON COLUMN shelters.shelter_type IS 'Classification: municipal (govt), rescue, private, foster_network';
COMMENT ON COLUMN shelters.is_kill_shelter IS 'TRUE if shelter practices euthanasia for space. Critical for urgency calculations.';
COMMENT ON COLUMN shelters.avg_hold_days IS 'Average number of days before a dog becomes at-risk for euthanasia';
COMMENT ON COLUMN shelters.euthanasia_schedule IS 'When euthanasia typically occurs (e.g., Tues/Thurs/Sat)';
COMMENT ON COLUMN shelters.accepts_rescue_pulls IS 'TRUE if rescue organizations can pull dogs from this shelter';
COMMENT ON COLUMN shelters.last_euthanasia_list_sync IS 'Timestamp of last euthanasia list data sync';
COMMENT ON COLUMN shelters.urgency_multiplier IS 'Multiplier for urgency scoring (1.0 = normal, higher = more urgent)';
COMMENT ON COLUMN shelters.dog_count IS 'Cached total dog count - updated by update_shelter_counts()';
COMMENT ON COLUMN shelters.available_count IS 'Cached available dog count - updated by update_shelter_counts()';
COMMENT ON COLUMN shelters.is_verified IS 'TRUE if shelter has been verified (IRS 501c3, manual review, etc.)';
COMMENT ON COLUMN shelters.verified_at IS 'Timestamp when verification occurred';
COMMENT ON COLUMN shelters.verified_by IS 'Who performed verification (user_id or system)';
COMMENT ON COLUMN shelters.is_active IS 'FALSE if shelter is closed or inactive';
COMMENT ON COLUMN shelters.deactivated_at IS 'Timestamp when shelter was deactivated';
COMMENT ON COLUMN shelters.deactivation_reason IS 'Reason for deactivation (closed, merged, etc.)';
COMMENT ON COLUMN shelters.external_id IS 'ID from external data source (RescueGroups, BestFriends, etc.)';
COMMENT ON COLUMN shelters.external_source IS 'Name of external data source';
COMMENT ON COLUMN shelters.created_at IS 'Record creation timestamp';
COMMENT ON COLUMN shelters.updated_at IS 'Record last modification timestamp';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Foreign key index
CREATE INDEX idx_shelters_state_code ON shelters(state_code);

-- Location-based search
CREATE INDEX idx_shelters_zip_code ON shelters(zip_code);
CREATE INDEX idx_shelters_city_state ON shelters(city, state_code);

-- Geospatial indexing (for distance queries)
CREATE INDEX idx_shelters_lat_lng ON shelters(latitude, longitude)
    WHERE latitude IS NOT NULL AND longitude IS NOT NULL;

-- Kill shelter filtering (critical for urgency)
CREATE INDEX idx_shelters_is_kill_shelter ON shelters(is_kill_shelter)
    WHERE is_kill_shelter = TRUE;

-- Active shelter queries
CREATE INDEX idx_shelters_is_active ON shelters(is_active)
    WHERE is_active = TRUE;

-- Shelter type filtering
CREATE INDEX idx_shelters_type ON shelters(shelter_type);

-- External ID lookup (for data sync)
CREATE INDEX idx_shelters_external_id ON shelters(external_source, external_id)
    WHERE external_id IS NOT NULL;

-- Text search on name
CREATE INDEX idx_shelters_name_trgm ON shelters USING gin(name gin_trgm_ops);

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_shelters_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_shelters_updated_at
    BEFORE UPDATE ON shelters
    FOR EACH ROW
    EXECUTE FUNCTION update_shelters_updated_at();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid shelter types
ALTER TABLE shelters ADD CONSTRAINT shelters_type_valid
    CHECK (shelter_type IN ('municipal', 'rescue', 'private', 'foster_network'));

-- Valid GPS coordinates
ALTER TABLE shelters ADD CONSTRAINT shelters_latitude_valid
    CHECK (latitude IS NULL OR (latitude >= -90 AND latitude <= 90));

ALTER TABLE shelters ADD CONSTRAINT shelters_longitude_valid
    CHECK (longitude IS NULL OR (longitude >= -180 AND longitude <= 180));

-- Urgency multiplier range
ALTER TABLE shelters ADD CONSTRAINT shelters_urgency_multiplier_valid
    CHECK (urgency_multiplier >= 0.01 AND urgency_multiplier <= 9.99);

-- Non-negative counts
ALTER TABLE shelters ADD CONSTRAINT shelters_dog_count_non_negative
    CHECK (dog_count >= 0);

ALTER TABLE shelters ADD CONSTRAINT shelters_available_count_non_negative
    CHECK (available_count >= 0);

-- Available count cannot exceed total count
ALTER TABLE shelters ADD CONSTRAINT shelters_available_lte_total
    CHECK (available_count <= dog_count);

-- Valid email format (basic check)
ALTER TABLE shelters ADD CONSTRAINT shelters_email_valid
    CHECK (email IS NULL OR email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

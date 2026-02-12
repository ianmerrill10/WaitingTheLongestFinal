-- ============================================================================
-- Migration: 002_states.sql
-- Purpose: Create states table for geographic organization of shelters and dogs
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES: 001_extensions.sql (uuid-ossp)
--
-- MODEL REFERENCE: wtl::core::models::State from INTEGRATION_CONTRACTS.md
--
-- ROLLBACK: DROP TABLE IF EXISTS states CASCADE;
-- ============================================================================

-- ============================================================================
-- STATES TABLE
-- Purpose: US states for geographic organization
-- Primary key: code (2-letter abbreviation) for efficient joins
-- ============================================================================
CREATE TABLE states (
    -- Primary identifier - using state code instead of UUID for efficiency
    code VARCHAR(2) PRIMARY KEY,

    -- State information
    name VARCHAR(100) NOT NULL,
    region VARCHAR(50),                              -- Geographic region (South, West, Northeast, Midwest)

    -- Status
    is_active BOOLEAN DEFAULT FALSE,                 -- Whether we're actively operating in this state

    -- Cached counts for performance (updated by triggers/functions)
    dog_count INTEGER DEFAULT 0,                     -- Total dogs in state
    shelter_count INTEGER DEFAULT 0,                 -- Total shelters in state

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE states IS 'US states - geographic organization for shelters and dogs. Primary key is 2-letter code for efficient joins.';

COMMENT ON COLUMN states.code IS 'Two-letter state abbreviation (e.g., TX, CA, NY) - used as primary key';
COMMENT ON COLUMN states.name IS 'Full state name (e.g., Texas, California, New York)';
COMMENT ON COLUMN states.region IS 'Geographic region: South, West, Northeast, Midwest, or NULL';
COMMENT ON COLUMN states.is_active IS 'TRUE if platform is actively operating in this state. Pilot states: TX, CA, FL, NY, PA';
COMMENT ON COLUMN states.dog_count IS 'Cached count of total dogs in state - updated by update_state_counts() function';
COMMENT ON COLUMN states.shelter_count IS 'Cached count of total shelters in state - updated by update_state_counts() function';
COMMENT ON COLUMN states.created_at IS 'Timestamp when record was created';
COMMENT ON COLUMN states.updated_at IS 'Timestamp when record was last modified';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Index for filtering active states
CREATE INDEX idx_states_is_active ON states(is_active) WHERE is_active = TRUE;

-- Index for sorting by region
CREATE INDEX idx_states_region ON states(region);

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_states_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_states_updated_at
    BEFORE UPDATE ON states
    FOR EACH ROW
    EXECUTE FUNCTION update_states_updated_at();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Ensure state code is uppercase
ALTER TABLE states ADD CONSTRAINT states_code_uppercase
    CHECK (code = UPPER(code));

-- Ensure counts are non-negative
ALTER TABLE states ADD CONSTRAINT states_dog_count_non_negative
    CHECK (dog_count >= 0);

ALTER TABLE states ADD CONSTRAINT states_shelter_count_non_negative
    CHECK (shelter_count >= 0);

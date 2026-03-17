-- ============================================================================
-- Seed Data: seed_states.sql
-- Purpose: Insert all 50 US states with regions
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   002_states.sql (states table must exist)
--
-- PILOT STATES (is_active = TRUE):
--   TX, CA, FL, NY, PA
--   (Top 5 states by shelter dog population)
--
-- REGIONS:
--   Northeast, Southeast, Midwest, Southwest, West
--
-- ROLLBACK: DELETE FROM states WHERE code IN ('AL', 'AK', ...);
-- ============================================================================

-- ============================================================================
-- INSERT ALL 50 STATES
-- ============================================================================

INSERT INTO states (code, name, region, is_active) VALUES
    -- NORTHEAST REGION
    ('CT', 'Connecticut', 'Northeast', FALSE),
    ('DE', 'Delaware', 'Northeast', FALSE),
    ('MA', 'Massachusetts', 'Northeast', FALSE),
    ('MD', 'Maryland', 'Northeast', FALSE),
    ('ME', 'Maine', 'Northeast', FALSE),
    ('NH', 'New Hampshire', 'Northeast', FALSE),
    ('NJ', 'New Jersey', 'Northeast', FALSE),
    ('NY', 'New York', 'Northeast', TRUE),           -- PILOT STATE
    ('PA', 'Pennsylvania', 'Northeast', TRUE),       -- PILOT STATE
    ('RI', 'Rhode Island', 'Northeast', FALSE),
    ('VT', 'Vermont', 'Northeast', FALSE),

    -- SOUTHEAST REGION
    ('AL', 'Alabama', 'Southeast', FALSE),
    ('AR', 'Arkansas', 'Southeast', FALSE),
    ('FL', 'Florida', 'Southeast', TRUE),            -- PILOT STATE
    ('GA', 'Georgia', 'Southeast', FALSE),
    ('KY', 'Kentucky', 'Southeast', FALSE),
    ('LA', 'Louisiana', 'Southeast', FALSE),
    ('MS', 'Mississippi', 'Southeast', FALSE),
    ('NC', 'North Carolina', 'Southeast', FALSE),
    ('SC', 'South Carolina', 'Southeast', FALSE),
    ('TN', 'Tennessee', 'Southeast', FALSE),
    ('VA', 'Virginia', 'Southeast', FALSE),
    ('WV', 'West Virginia', 'Southeast', FALSE),

    -- MIDWEST REGION
    ('IA', 'Iowa', 'Midwest', FALSE),
    ('IL', 'Illinois', 'Midwest', FALSE),
    ('IN', 'Indiana', 'Midwest', FALSE),
    ('KS', 'Kansas', 'Midwest', FALSE),
    ('MI', 'Michigan', 'Midwest', FALSE),
    ('MN', 'Minnesota', 'Midwest', FALSE),
    ('MO', 'Missouri', 'Midwest', FALSE),
    ('ND', 'North Dakota', 'Midwest', FALSE),
    ('NE', 'Nebraska', 'Midwest', FALSE),
    ('OH', 'Ohio', 'Midwest', FALSE),
    ('SD', 'South Dakota', 'Midwest', FALSE),
    ('WI', 'Wisconsin', 'Midwest', FALSE),

    -- SOUTHWEST REGION
    ('AZ', 'Arizona', 'Southwest', FALSE),
    ('NM', 'New Mexico', 'Southwest', FALSE),
    ('OK', 'Oklahoma', 'Southwest', FALSE),
    ('TX', 'Texas', 'Southwest', TRUE),              -- PILOT STATE

    -- WEST REGION
    ('AK', 'Alaska', 'West', FALSE),
    ('CA', 'California', 'West', TRUE),              -- PILOT STATE
    ('CO', 'Colorado', 'West', FALSE),
    ('HI', 'Hawaii', 'West', FALSE),
    ('ID', 'Idaho', 'West', FALSE),
    ('MT', 'Montana', 'West', FALSE),
    ('NV', 'Nevada', 'West', FALSE),
    ('OR', 'Oregon', 'West', FALSE),
    ('UT', 'Utah', 'West', FALSE),
    ('WA', 'Washington', 'West', FALSE),
    ('WY', 'Wyoming', 'West', FALSE)

ON CONFLICT (code) DO UPDATE SET
    name = EXCLUDED.name,
    region = EXCLUDED.region,
    is_active = EXCLUDED.is_active,
    updated_at = NOW();

-- ============================================================================
-- VERIFICATION QUERIES
-- ============================================================================

-- Verify all 50 states inserted
-- SELECT COUNT(*) AS total_states FROM states;
-- Expected: 50

-- Verify pilot states
-- SELECT code, name, is_active FROM states WHERE is_active = TRUE ORDER BY code;
-- Expected: CA, FL, NY, PA, TX

-- Verify region distribution
-- SELECT region, COUNT(*) AS state_count FROM states GROUP BY region ORDER BY region;
-- Expected: Midwest 12, Northeast 11, Southeast 13, Southwest 4, West 11

-- ============================================================================
-- STATE STATISTICS (from research)
-- ============================================================================

/*
Top 5 states by shelter dog euthanasia (2024 data):
1. Texas (TX)       - Highest volume
2. California (CA)  - High volume, many kill shelters
3. North Carolina (NC) - High euthanasia rate
4. Florida (FL)     - High volume
5. Louisiana (LA)   - High euthanasia rate

These states are prioritized in the pilot program:
- TX: Starting point, highest need
- CA: Large population, many shelters
- FL: High volume, transport hub
- NY: Large adopter population
- PA: Transport destination from South

Future expansion priority:
Phase 2: NC, LA, GA, OH, MI (weeks 5-8)
Phase 3: All remaining states (weeks 9-12)
*/

-- ============================================================================
-- LOG COMPLETION
-- ============================================================================
-- This line can be used to verify seed data was applied
DO $$
BEGIN
    RAISE NOTICE 'Seed states loaded: % total, % pilot states active',
        (SELECT COUNT(*) FROM states),
        (SELECT COUNT(*) FROM states WHERE is_active = TRUE);
END $$;

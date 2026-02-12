-- ============================================================================
-- Migration: 012_views.sql
-- Purpose: Create database views for common query patterns
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   002_states.sql through 004_dogs.sql (core tables)
--
-- VIEWS CREATED:
--   dogs_with_wait_time - Dogs with calculated wait time
--   urgent_dogs - Dogs with urgency status
--   dogs_needing_rescue - Dogs that need rescue pulls
--   shelter_statistics - Aggregate shelter stats
--   state_statistics - Aggregate state stats
--
-- ROLLBACK: DROP VIEW IF EXISTS state_statistics, shelter_statistics, dogs_needing_rescue, urgent_dogs, dogs_with_wait_time CASCADE;
-- ============================================================================

-- ============================================================================
-- VIEW: dogs_with_wait_time
-- Purpose: Dogs with calculated wait time in various formats
-- Used by: LED counter display, search results, dog profiles
-- ============================================================================
CREATE OR REPLACE VIEW dogs_with_wait_time AS
SELECT
    d.id,
    d.name,
    d.shelter_id,
    d.breed_primary,
    d.breed_secondary,
    d.size,
    d.age_category,
    d.age_months,
    d.gender,
    d.status,
    d.is_available,
    d.intake_date,
    d.euthanasia_date,
    d.urgency_level,
    d.is_on_euthanasia_list,
    d.primary_photo_url,
    d.description,
    d.short_description,

    -- Wait time calculations
    NOW() - d.intake_date AS wait_interval,

    -- Total days waiting
    EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400 AS wait_days_total,

    -- Wait time components for YY:MM:DD:HH:MM:SS format
    EXTRACT(YEAR FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_years,
    EXTRACT(MONTH FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_months,
    EXTRACT(DAY FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_days,
    EXTRACT(HOUR FROM (NOW() - d.intake_date))::INTEGER % 24 AS wait_hours,
    EXTRACT(MINUTE FROM (NOW() - d.intake_date))::INTEGER % 60 AS wait_minutes,
    EXTRACT(SECOND FROM (NOW() - d.intake_date))::INTEGER % 60 AS wait_seconds,

    -- Formatted wait time string: YY:MM:DD:HH:MM:SS
    LPAD(EXTRACT(YEAR FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD(EXTRACT(MONTH FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD(EXTRACT(DAY FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(HOUR FROM (NOW() - d.intake_date))::INTEGER % 24)::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(MINUTE FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(SECOND FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0')
    AS wait_time_formatted,

    -- Shelter info
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.is_kill_shelter

FROM dogs d
JOIN shelters s ON d.shelter_id = s.id;

COMMENT ON VIEW dogs_with_wait_time IS 'Dogs with calculated wait time components for LED counter display. Format: YY:MM:DD:HH:MM:SS';

-- ============================================================================
-- VIEW: urgent_dogs
-- Purpose: Dogs with urgency tracking and countdown timers
-- Used by: Urgency alerts, countdown displays, rescue coordination
-- ============================================================================
CREATE OR REPLACE VIEW urgent_dogs AS
SELECT
    d.id,
    d.name,
    d.shelter_id,
    d.breed_primary,
    d.size,
    d.age_category,
    d.gender,
    d.status,
    d.is_available,
    d.intake_date,
    d.euthanasia_date,
    d.urgency_level,
    d.is_on_euthanasia_list,
    d.euthanasia_list_added_at,
    d.rescue_pull_status,
    d.rescue_hold_until,
    d.primary_photo_url,
    d.short_description,

    -- Countdown calculations (only if euthanasia_date is set and in future)
    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN d.euthanasia_date - NOW()
        ELSE NULL
    END AS countdown_interval,

    -- Countdown in total hours
    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600
        ELSE NULL
    END AS countdown_hours_total,

    -- Countdown components for DD:HH:MM:SS format
    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN (EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 86400)::INTEGER
        ELSE NULL
    END AS countdown_days,

    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600)::INTEGER % 24)
        ELSE NULL
    END AS countdown_hours,

    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 60)::INTEGER % 60)
        ELSE NULL
    END AS countdown_minutes,

    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN (EXTRACT(EPOCH FROM (d.euthanasia_date - NOW()))::INTEGER % 60)
        ELSE NULL
    END AS countdown_seconds,

    -- Formatted countdown string: DD:HH:MM:SS
    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN
            LPAD((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 86400)::INTEGER::TEXT, 2, '0') || ':' ||
            LPAD(((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600)::INTEGER % 24)::TEXT, 2, '0') || ':' ||
            LPAD(((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 60)::INTEGER % 60)::TEXT, 2, '0') || ':' ||
            LPAD((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW()))::INTEGER % 60)::TEXT, 2, '0')
        ELSE NULL
    END AS countdown_formatted,

    -- Is this dog in critical time window?
    CASE
        WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
             AND d.euthanasia_date - NOW() < INTERVAL '24 hours'
        THEN TRUE
        ELSE FALSE
    END AS is_critical,

    -- Calculated urgency level based on time remaining
    CASE
        WHEN d.euthanasia_date IS NULL THEN 'normal'
        WHEN d.euthanasia_date <= NOW() THEN 'expired'
        WHEN d.euthanasia_date - NOW() < INTERVAL '24 hours' THEN 'critical'
        WHEN d.euthanasia_date - NOW() < INTERVAL '72 hours' THEN 'high'
        WHEN d.euthanasia_date - NOW() < INTERVAL '7 days' THEN 'medium'
        ELSE 'normal'
    END AS calculated_urgency,

    -- Shelter info
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.is_kill_shelter,
    s.urgency_multiplier,
    s.accepts_rescue_pulls

FROM dogs d
JOIN shelters s ON d.shelter_id = s.id
WHERE d.is_available = TRUE
  AND (d.urgency_level IN ('medium', 'high', 'critical')
       OR d.is_on_euthanasia_list = TRUE
       OR d.euthanasia_date IS NOT NULL);

COMMENT ON VIEW urgent_dogs IS 'Dogs with urgency status and countdown timers. Format: DD:HH:MM:SS. Includes calculated urgency based on time remaining.';

-- ============================================================================
-- VIEW: dogs_needing_rescue
-- Purpose: Dogs available for rescue organization pulls
-- Used by: Rescue coordination, transport network
-- ============================================================================
CREATE OR REPLACE VIEW dogs_needing_rescue AS
SELECT
    d.id,
    d.name,
    d.shelter_id,
    d.breed_primary,
    d.size,
    d.age_category,
    d.gender,
    d.intake_date,
    d.euthanasia_date,
    d.urgency_level,
    d.is_on_euthanasia_list,
    d.rescue_pull_status,
    d.rescue_hold_until,
    d.rescue_notes,
    d.has_special_needs,
    d.special_needs_description,
    d.has_medical_conditions,
    d.medical_conditions,
    d.is_heartworm_positive,
    d.primary_photo_url,
    d.short_description,

    -- Wait time
    EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400 AS wait_days,

    -- Shelter details
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.phone AS shelter_phone,
    s.email AS shelter_email,
    s.accepts_rescue_pulls,
    s.is_kill_shelter,
    s.urgency_multiplier,

    -- Priority score (higher = more urgent)
    CASE
        WHEN d.urgency_level = 'critical' THEN 100
        WHEN d.urgency_level = 'high' THEN 75
        WHEN d.urgency_level = 'medium' THEN 50
        ELSE 25
    END +
    CASE WHEN d.is_on_euthanasia_list THEN 50 ELSE 0 END +
    CASE WHEN s.is_kill_shelter THEN 25 ELSE 0 END +
    (EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400 / 10)::INTEGER  -- +1 per 10 days waiting
    AS rescue_priority_score

FROM dogs d
JOIN shelters s ON d.shelter_id = s.id
WHERE d.is_available = TRUE
  AND s.accepts_rescue_pulls = TRUE
  AND (d.rescue_pull_status IS NULL OR d.rescue_pull_status = 'available')
ORDER BY rescue_priority_score DESC;

COMMENT ON VIEW dogs_needing_rescue IS 'Dogs available for rescue organization pulls, sorted by priority score. Higher score = more urgent.';

-- ============================================================================
-- VIEW: shelter_statistics
-- Purpose: Aggregate statistics per shelter
-- Used by: Shelter profiles, admin dashboard, reporting
-- ============================================================================
CREATE OR REPLACE VIEW shelter_statistics AS
SELECT
    s.id,
    s.name,
    s.city,
    s.state_code,
    s.shelter_type,
    s.is_kill_shelter,
    s.urgency_multiplier,
    s.is_active,

    -- Dog counts
    COUNT(d.id) AS total_dogs,
    COUNT(d.id) FILTER (WHERE d.is_available = TRUE) AS available_dogs,
    COUNT(d.id) FILTER (WHERE d.status = 'adopted') AS adopted_dogs,
    COUNT(d.id) FILTER (WHERE d.status = 'foster') AS foster_dogs,

    -- Urgency breakdown
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'critical' AND d.is_available = TRUE) AS critical_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'high' AND d.is_available = TRUE) AS high_urgency_dogs,
    COUNT(d.id) FILTER (WHERE d.is_on_euthanasia_list = TRUE AND d.is_available = TRUE) AS on_euthanasia_list,

    -- Wait time statistics (available dogs only)
    AVG(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400)
        FILTER (WHERE d.is_available = TRUE) AS avg_wait_days,
    MAX(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400)
        FILTER (WHERE d.is_available = TRUE) AS max_wait_days,
    MIN(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400)
        FILTER (WHERE d.is_available = TRUE) AS min_wait_days,

    -- Longest waiting dog
    (SELECT d2.id FROM dogs d2
     WHERE d2.shelter_id = s.id AND d2.is_available = TRUE
     ORDER BY d2.intake_date ASC LIMIT 1) AS longest_waiting_dog_id,

    -- Age breakdown (available dogs)
    COUNT(d.id) FILTER (WHERE d.age_category = 'puppy' AND d.is_available = TRUE) AS puppies,
    COUNT(d.id) FILTER (WHERE d.age_category = 'young' AND d.is_available = TRUE) AS young_dogs,
    COUNT(d.id) FILTER (WHERE d.age_category = 'adult' AND d.is_available = TRUE) AS adult_dogs,
    COUNT(d.id) FILTER (WHERE d.age_category = 'senior' AND d.is_available = TRUE) AS senior_dogs,

    -- Size breakdown (available dogs)
    COUNT(d.id) FILTER (WHERE d.size = 'small' AND d.is_available = TRUE) AS small_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'medium' AND d.is_available = TRUE) AS medium_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'large' AND d.is_available = TRUE) AS large_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'xlarge' AND d.is_available = TRUE) AS xlarge_dogs,

    -- Special needs
    COUNT(d.id) FILTER (WHERE d.has_special_needs = TRUE AND d.is_available = TRUE) AS special_needs_dogs,
    COUNT(d.id) FILTER (WHERE d.is_heartworm_positive = TRUE AND d.is_available = TRUE) AS heartworm_positive_dogs

FROM shelters s
LEFT JOIN dogs d ON s.id = d.shelter_id
GROUP BY s.id;

COMMENT ON VIEW shelter_statistics IS 'Aggregate statistics per shelter including dog counts, urgency breakdown, wait times, and demographics.';

-- ============================================================================
-- VIEW: state_statistics
-- Purpose: Aggregate statistics per state
-- Used by: State pages, regional analysis, reporting
-- ============================================================================
CREATE OR REPLACE VIEW state_statistics AS
SELECT
    st.code,
    st.name,
    st.region,
    st.is_active,

    -- Shelter counts
    COUNT(DISTINCT s.id) AS total_shelters,
    COUNT(DISTINCT s.id) FILTER (WHERE s.is_active = TRUE) AS active_shelters,
    COUNT(DISTINCT s.id) FILTER (WHERE s.is_kill_shelter = TRUE AND s.is_active = TRUE) AS kill_shelters,

    -- Dog counts
    COUNT(d.id) AS total_dogs,
    COUNT(d.id) FILTER (WHERE d.is_available = TRUE) AS available_dogs,

    -- Urgency breakdown
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'critical' AND d.is_available = TRUE) AS critical_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'high' AND d.is_available = TRUE) AS high_urgency_dogs,
    COUNT(d.id) FILTER (WHERE d.is_on_euthanasia_list = TRUE AND d.is_available = TRUE) AS on_euthanasia_list,

    -- Wait time statistics
    AVG(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400)
        FILTER (WHERE d.is_available = TRUE) AS avg_wait_days,
    MAX(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400)
        FILTER (WHERE d.is_available = TRUE) AS max_wait_days,

    -- Age breakdown
    COUNT(d.id) FILTER (WHERE d.age_category = 'puppy' AND d.is_available = TRUE) AS puppies,
    COUNT(d.id) FILTER (WHERE d.age_category = 'young' AND d.is_available = TRUE) AS young_dogs,
    COUNT(d.id) FILTER (WHERE d.age_category = 'adult' AND d.is_available = TRUE) AS adult_dogs,
    COUNT(d.id) FILTER (WHERE d.age_category = 'senior' AND d.is_available = TRUE) AS senior_dogs,

    -- Size breakdown
    COUNT(d.id) FILTER (WHERE d.size = 'small' AND d.is_available = TRUE) AS small_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'medium' AND d.is_available = TRUE) AS medium_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'large' AND d.is_available = TRUE) AS large_dogs,
    COUNT(d.id) FILTER (WHERE d.size = 'xlarge' AND d.is_available = TRUE) AS xlarge_dogs

FROM states st
LEFT JOIN shelters s ON st.code = s.state_code
LEFT JOIN dogs d ON s.id = d.shelter_id
GROUP BY st.code;

COMMENT ON VIEW state_statistics IS 'Aggregate statistics per state including shelter counts, dog counts, urgency breakdown, and demographics.';

-- ============================================================================
-- VIEW: foster_home_availability
-- Purpose: Available foster homes with capacity info
-- Used by: Foster matching algorithm
-- ============================================================================
CREATE OR REPLACE VIEW foster_home_availability AS
SELECT
    fh.id,
    fh.user_id,
    fh.zip_code,
    fh.city,
    fh.state_code,
    fh.latitude,
    fh.longitude,
    fh.max_dogs,
    fh.current_dogs,
    fh.max_dogs - fh.current_dogs AS available_capacity,
    fh.preferred_sizes,
    fh.ok_with_puppies,
    fh.ok_with_seniors,
    fh.ok_with_medical,
    fh.ok_with_behavioral,
    fh.ok_with_heartworm_positive,
    fh.ok_with_special_needs,
    fh.experience_level,
    fh.has_yard,
    fh.has_other_dogs,
    fh.has_cats,
    fh.has_children,
    fh.max_transport_miles,
    fh.fosters_completed,
    fh.rating_average,

    -- User info
    u.display_name,
    u.email

FROM foster_homes fh
JOIN users u ON fh.user_id = u.id
WHERE fh.is_available = TRUE
  AND fh.is_verified = TRUE
  AND fh.is_active = TRUE
  AND fh.current_dogs < fh.max_dogs
  AND u.is_active = TRUE;

COMMENT ON VIEW foster_home_availability IS 'Available foster homes with capacity, used for foster matching.';

-- ============================================================================
-- REFRESH NOTES
-- ============================================================================

-- These are regular views (not materialized) so they always show current data.
--
-- For high-traffic production use, consider:
--
-- 1. Creating materialized views for statistics:
--    CREATE MATERIALIZED VIEW shelter_statistics_mat AS SELECT * FROM shelter_statistics;
--    REFRESH MATERIALIZED VIEW shelter_statistics_mat;
--
-- 2. Setting up periodic refresh via pg_cron or application worker:
--    SELECT cron.schedule('refresh-stats', '0/5 * * * *', 'REFRESH MATERIALIZED VIEW shelter_statistics_mat');
--
-- 3. Adding indexes to materialized views for faster queries.

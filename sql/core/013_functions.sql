-- ============================================================================
-- Migration: 013_functions.sql
-- Purpose: Create utility functions for common operations
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   002_states.sql through 004_dogs.sql (core tables)
--
-- FUNCTIONS CREATED:
--   calculate_wait_time(dog_id) - Calculate wait time for a dog
--   calculate_countdown(dog_id) - Calculate countdown to euthanasia
--   update_shelter_counts(shelter_id) - Update cached shelter statistics
--   update_state_counts(state_code) - Update cached state statistics
--   update_all_shelter_counts() - Update all shelter statistics
--   update_all_state_counts() - Update all state statistics
--   cleanup_old_error_logs() - Already in 010_debug_schema.sql
--   get_urgent_dogs_for_alert(radius_miles, zip) - Find urgent dogs for alerts
--
-- ROLLBACK: See individual DROP FUNCTION statements
-- ============================================================================

-- ============================================================================
-- FUNCTION: update_updated_at_column
-- Purpose: Common trigger function to auto-update updated_at on row change
-- Used by many module tables
-- ============================================================================
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- FUNCTION: calculate_wait_time
-- Purpose: Calculate wait time components for a dog
-- Returns: JSONB with years, months, days, hours, minutes, seconds, formatted
-- ============================================================================
CREATE OR REPLACE FUNCTION calculate_wait_time(p_dog_id UUID)
RETURNS JSONB AS $$
DECLARE
    v_intake_date TIMESTAMP WITH TIME ZONE;
    v_interval INTERVAL;
    v_years INTEGER;
    v_months INTEGER;
    v_days INTEGER;
    v_hours INTEGER;
    v_minutes INTEGER;
    v_seconds INTEGER;
    v_formatted TEXT;
BEGIN
    -- Get intake date
    SELECT intake_date INTO v_intake_date
    FROM dogs
    WHERE id = p_dog_id;

    IF v_intake_date IS NULL THEN
        RETURN NULL;
    END IF;

    -- Calculate interval
    v_interval := NOW() - v_intake_date;

    -- Extract components
    v_years := EXTRACT(YEAR FROM AGE(NOW(), v_intake_date))::INTEGER;
    v_months := EXTRACT(MONTH FROM AGE(NOW(), v_intake_date))::INTEGER;
    v_days := EXTRACT(DAY FROM AGE(NOW(), v_intake_date))::INTEGER;
    v_hours := (EXTRACT(HOUR FROM v_interval)::INTEGER) % 24;
    v_minutes := (EXTRACT(MINUTE FROM v_interval)::INTEGER) % 60;
    v_seconds := (EXTRACT(SECOND FROM v_interval)::INTEGER) % 60;

    -- Format as YY:MM:DD:HH:MM:SS
    v_formatted := LPAD(v_years::TEXT, 2, '0') || ':' ||
                   LPAD(v_months::TEXT, 2, '0') || ':' ||
                   LPAD(v_days::TEXT, 2, '0') || ':' ||
                   LPAD(v_hours::TEXT, 2, '0') || ':' ||
                   LPAD(v_minutes::TEXT, 2, '0') || ':' ||
                   LPAD(v_seconds::TEXT, 2, '0');

    RETURN jsonb_build_object(
        'years', v_years,
        'months', v_months,
        'days', v_days,
        'hours', v_hours,
        'minutes', v_minutes,
        'seconds', v_seconds,
        'formatted', v_formatted,
        'total_days', EXTRACT(EPOCH FROM v_interval) / 86400
    );
END;
$$ LANGUAGE plpgsql STABLE;

COMMENT ON FUNCTION calculate_wait_time IS 'Calculate wait time for a dog. Returns JSONB with components and formatted string (YY:MM:DD:HH:MM:SS).';

-- ============================================================================
-- FUNCTION: calculate_countdown
-- Purpose: Calculate countdown to euthanasia for a dog
-- Returns: JSONB with days, hours, minutes, seconds, formatted, is_critical
-- ============================================================================
CREATE OR REPLACE FUNCTION calculate_countdown(p_dog_id UUID)
RETURNS JSONB AS $$
DECLARE
    v_euthanasia_date TIMESTAMP WITH TIME ZONE;
    v_interval INTERVAL;
    v_total_seconds NUMERIC;
    v_days INTEGER;
    v_hours INTEGER;
    v_minutes INTEGER;
    v_seconds INTEGER;
    v_formatted TEXT;
    v_is_critical BOOLEAN;
BEGIN
    -- Get euthanasia date
    SELECT euthanasia_date INTO v_euthanasia_date
    FROM dogs
    WHERE id = p_dog_id;

    -- Return null if no euthanasia date or already passed
    IF v_euthanasia_date IS NULL OR v_euthanasia_date <= NOW() THEN
        RETURN NULL;
    END IF;

    -- Calculate interval
    v_interval := v_euthanasia_date - NOW();
    v_total_seconds := EXTRACT(EPOCH FROM v_interval);

    -- Extract components
    v_days := (v_total_seconds / 86400)::INTEGER;
    v_hours := ((v_total_seconds / 3600)::INTEGER) % 24;
    v_minutes := ((v_total_seconds / 60)::INTEGER) % 60;
    v_seconds := v_total_seconds::INTEGER % 60;

    -- Format as DD:HH:MM:SS
    v_formatted := LPAD(v_days::TEXT, 2, '0') || ':' ||
                   LPAD(v_hours::TEXT, 2, '0') || ':' ||
                   LPAD(v_minutes::TEXT, 2, '0') || ':' ||
                   LPAD(v_seconds::TEXT, 2, '0');

    -- Is critical if less than 24 hours
    v_is_critical := v_total_seconds < 86400;

    RETURN jsonb_build_object(
        'days', v_days,
        'hours', v_hours,
        'minutes', v_minutes,
        'seconds', v_seconds,
        'formatted', v_formatted,
        'is_critical', v_is_critical,
        'total_hours', v_total_seconds / 3600,
        'urgency_level', CASE
            WHEN v_total_seconds < 86400 THEN 'critical'
            WHEN v_total_seconds < 259200 THEN 'high'
            WHEN v_total_seconds < 604800 THEN 'medium'
            ELSE 'normal'
        END
    );
END;
$$ LANGUAGE plpgsql STABLE;

COMMENT ON FUNCTION calculate_countdown IS 'Calculate countdown to euthanasia. Returns JSONB with components, formatted string (DD:HH:MM:SS), and criticality.';

-- ============================================================================
-- FUNCTION: update_shelter_counts
-- Purpose: Update cached dog counts for a shelter
-- ============================================================================
CREATE OR REPLACE FUNCTION update_shelter_counts(p_shelter_id UUID)
RETURNS VOID AS $$
BEGIN
    UPDATE shelters
    SET dog_count = (
            SELECT COUNT(*) FROM dogs
            WHERE shelter_id = p_shelter_id
        ),
        available_count = (
            SELECT COUNT(*) FROM dogs
            WHERE shelter_id = p_shelter_id AND is_available = TRUE
        ),
        updated_at = NOW()
    WHERE id = p_shelter_id;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION update_shelter_counts IS 'Update cached dog_count and available_count for a shelter.';

-- ============================================================================
-- FUNCTION: update_state_counts
-- Purpose: Update cached counts for a state
-- ============================================================================
CREATE OR REPLACE FUNCTION update_state_counts(p_state_code VARCHAR(2))
RETURNS VOID AS $$
BEGIN
    UPDATE states
    SET shelter_count = (
            SELECT COUNT(*) FROM shelters
            WHERE state_code = p_state_code AND is_active = TRUE
        ),
        dog_count = (
            SELECT COUNT(*) FROM dogs d
            JOIN shelters s ON d.shelter_id = s.id
            WHERE s.state_code = p_state_code AND d.is_available = TRUE
        ),
        updated_at = NOW()
    WHERE code = p_state_code;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION update_state_counts IS 'Update cached shelter_count and dog_count for a state.';

-- ============================================================================
-- FUNCTION: update_all_shelter_counts
-- Purpose: Update cached counts for all shelters
-- ============================================================================
CREATE OR REPLACE FUNCTION update_all_shelter_counts()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    WITH updated AS (
        UPDATE shelters s
        SET dog_count = COALESCE(counts.total, 0),
            available_count = COALESCE(counts.available, 0),
            updated_at = NOW()
        FROM (
            SELECT
                shelter_id,
                COUNT(*) AS total,
                COUNT(*) FILTER (WHERE is_available = TRUE) AS available
            FROM dogs
            GROUP BY shelter_id
        ) counts
        WHERE s.id = counts.shelter_id
        RETURNING s.id
    )
    SELECT COUNT(*) INTO v_count FROM updated;

    RETURN v_count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION update_all_shelter_counts IS 'Update cached counts for all shelters. Returns number of shelters updated.';

-- ============================================================================
-- FUNCTION: update_all_state_counts
-- Purpose: Update cached counts for all states
-- ============================================================================
CREATE OR REPLACE FUNCTION update_all_state_counts()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    WITH updated AS (
        UPDATE states st
        SET shelter_count = COALESCE(counts.shelters, 0),
            dog_count = COALESCE(counts.dogs, 0),
            updated_at = NOW()
        FROM (
            SELECT
                s.state_code,
                COUNT(DISTINCT s.id) AS shelters,
                COUNT(d.id) FILTER (WHERE d.is_available = TRUE) AS dogs
            FROM shelters s
            LEFT JOIN dogs d ON s.id = d.shelter_id
            WHERE s.is_active = TRUE
            GROUP BY s.state_code
        ) counts
        WHERE st.code = counts.state_code
        RETURNING st.code
    )
    SELECT COUNT(*) INTO v_count FROM updated;

    RETURN v_count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION update_all_state_counts IS 'Update cached counts for all states. Returns number of states updated.';

-- ============================================================================
-- FUNCTION: get_urgent_dogs_for_alert
-- Purpose: Find urgent dogs near a location for alert notifications
-- ============================================================================
CREATE OR REPLACE FUNCTION get_urgent_dogs_for_alert(
    p_zip_code VARCHAR(10),
    p_radius_miles INTEGER DEFAULT 100,
    p_urgency_levels TEXT[] DEFAULT ARRAY['critical', 'high']
)
RETURNS TABLE (
    dog_id UUID,
    dog_name VARCHAR(100),
    breed VARCHAR(100),
    urgency_level VARCHAR(20),
    countdown_formatted TEXT,
    hours_remaining NUMERIC,
    shelter_name VARCHAR(255),
    shelter_city VARCHAR(100),
    shelter_state VARCHAR(2),
    distance_miles NUMERIC
) AS $$
BEGIN
    -- Note: For accurate distance calculation, you would use PostGIS
    -- This is a simplified version using ZIP code matching

    RETURN QUERY
    SELECT
        d.id AS dog_id,
        d.name AS dog_name,
        d.breed_primary AS breed,
        d.urgency_level,
        CASE
            WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
            THEN LPAD((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 86400)::INTEGER::TEXT, 2, '0') || ':' ||
                 LPAD(((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600)::INTEGER % 24)::TEXT, 2, '0') || ':' ||
                 LPAD(((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 60)::INTEGER % 60)::TEXT, 2, '0') || ':' ||
                 LPAD((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW()))::INTEGER % 60)::TEXT, 2, '0')
            ELSE NULL
        END AS countdown_formatted,
        CASE
            WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
            THEN EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600
            ELSE NULL
        END AS hours_remaining,
        s.name AS shelter_name,
        s.city AS shelter_city,
        s.state_code AS shelter_state,
        -- Simplified distance - would use PostGIS for real distance
        CASE
            WHEN s.zip_code = p_zip_code THEN 0
            WHEN LEFT(s.zip_code, 3) = LEFT(p_zip_code, 3) THEN 25
            ELSE 50
        END::NUMERIC AS distance_miles
    FROM dogs d
    JOIN shelters s ON d.shelter_id = s.id
    WHERE d.is_available = TRUE
      AND d.urgency_level = ANY(p_urgency_levels)
      AND s.is_active = TRUE
    ORDER BY
        CASE d.urgency_level
            WHEN 'critical' THEN 1
            WHEN 'high' THEN 2
            WHEN 'medium' THEN 3
            ELSE 4
        END,
        d.euthanasia_date ASC NULLS LAST;
END;
$$ LANGUAGE plpgsql STABLE;

COMMENT ON FUNCTION get_urgent_dogs_for_alert IS 'Find urgent dogs for alert notifications. Note: Simplified distance calculation - use PostGIS for production.';

-- ============================================================================
-- FUNCTION: recalculate_dog_urgency
-- Purpose: Update urgency level for a dog based on euthanasia date
-- ============================================================================
CREATE OR REPLACE FUNCTION recalculate_dog_urgency(p_dog_id UUID)
RETURNS VARCHAR(20) AS $$
DECLARE
    v_euthanasia_date TIMESTAMP WITH TIME ZONE;
    v_new_urgency VARCHAR(20);
BEGIN
    SELECT euthanasia_date INTO v_euthanasia_date
    FROM dogs
    WHERE id = p_dog_id;

    IF v_euthanasia_date IS NULL THEN
        v_new_urgency := 'normal';
    ELSIF v_euthanasia_date <= NOW() THEN
        v_new_urgency := 'normal'; -- Expired
    ELSIF v_euthanasia_date - NOW() < INTERVAL '24 hours' THEN
        v_new_urgency := 'critical';
    ELSIF v_euthanasia_date - NOW() < INTERVAL '72 hours' THEN
        v_new_urgency := 'high';
    ELSIF v_euthanasia_date - NOW() < INTERVAL '7 days' THEN
        v_new_urgency := 'medium';
    ELSE
        v_new_urgency := 'normal';
    END IF;

    UPDATE dogs
    SET urgency_level = v_new_urgency,
        updated_at = NOW()
    WHERE id = p_dog_id
      AND urgency_level != v_new_urgency;

    RETURN v_new_urgency;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION recalculate_dog_urgency IS 'Recalculate and update urgency level for a dog based on euthanasia date.';

-- ============================================================================
-- FUNCTION: recalculate_all_dog_urgencies
-- Purpose: Update urgency levels for all dogs with euthanasia dates
-- ============================================================================
CREATE OR REPLACE FUNCTION recalculate_all_dog_urgencies()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    WITH updated AS (
        UPDATE dogs
        SET urgency_level = CASE
                WHEN euthanasia_date IS NULL THEN 'normal'
                WHEN euthanasia_date <= NOW() THEN 'normal'
                WHEN euthanasia_date - NOW() < INTERVAL '24 hours' THEN 'critical'
                WHEN euthanasia_date - NOW() < INTERVAL '72 hours' THEN 'high'
                WHEN euthanasia_date - NOW() < INTERVAL '7 days' THEN 'medium'
                ELSE 'normal'
            END,
            updated_at = NOW()
        WHERE is_available = TRUE
        RETURNING id
    )
    SELECT COUNT(*) INTO v_count FROM updated;

    RETURN v_count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION recalculate_all_dog_urgencies IS 'Recalculate urgency levels for all available dogs. Run periodically (e.g., hourly).';

-- ============================================================================
-- FUNCTION: increment_dog_view_count
-- Purpose: Thread-safe increment of view count
-- ============================================================================
CREATE OR REPLACE FUNCTION increment_dog_view_count(p_dog_id UUID)
RETURNS INTEGER AS $$
DECLARE
    v_new_count INTEGER;
BEGIN
    UPDATE dogs
    SET view_count = view_count + 1,
        last_viewed_at = NOW()
    WHERE id = p_dog_id
    RETURNING view_count INTO v_new_count;

    RETURN COALESCE(v_new_count, 0);
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION increment_dog_view_count IS 'Thread-safe increment of dog view count. Returns new count.';

-- ============================================================================
-- FUNCTION: increment_dog_share_count
-- Purpose: Thread-safe increment of share count
-- ============================================================================
CREATE OR REPLACE FUNCTION increment_dog_share_count(p_dog_id UUID)
RETURNS INTEGER AS $$
DECLARE
    v_new_count INTEGER;
BEGIN
    UPDATE dogs
    SET share_count = share_count + 1,
        last_shared_at = NOW()
    WHERE id = p_dog_id
    RETURNING share_count INTO v_new_count;

    RETURN COALESCE(v_new_count, 0);
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION increment_dog_share_count IS 'Thread-safe increment of dog share count. Returns new count.';

-- ============================================================================
-- FUNCTION: get_longest_waiting_dogs
-- Purpose: Get dogs with longest wait times
-- ============================================================================
CREATE OR REPLACE FUNCTION get_longest_waiting_dogs(
    p_limit INTEGER DEFAULT 10,
    p_state_code VARCHAR(2) DEFAULT NULL
)
RETURNS TABLE (
    dog_id UUID,
    dog_name VARCHAR(100),
    breed VARCHAR(100),
    wait_days NUMERIC,
    wait_formatted TEXT,
    shelter_name VARCHAR(255),
    shelter_city VARCHAR(100),
    shelter_state VARCHAR(2),
    primary_photo_url VARCHAR(500)
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        d.id AS dog_id,
        d.name AS dog_name,
        d.breed_primary AS breed,
        EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400 AS wait_days,
        LPAD(EXTRACT(YEAR FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
        LPAD(EXTRACT(MONTH FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
        LPAD(EXTRACT(DAY FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
        LPAD((EXTRACT(HOUR FROM (NOW() - d.intake_date))::INTEGER % 24)::TEXT, 2, '0') || ':' ||
        LPAD((EXTRACT(MINUTE FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0') || ':' ||
        LPAD((EXTRACT(SECOND FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0')
        AS wait_formatted,
        s.name AS shelter_name,
        s.city AS shelter_city,
        s.state_code AS shelter_state,
        d.primary_photo_url
    FROM dogs d
    JOIN shelters s ON d.shelter_id = s.id
    WHERE d.is_available = TRUE
      AND s.is_active = TRUE
      AND (p_state_code IS NULL OR s.state_code = p_state_code)
    ORDER BY d.intake_date ASC
    LIMIT p_limit;
END;
$$ LANGUAGE plpgsql STABLE;

COMMENT ON FUNCTION get_longest_waiting_dogs IS 'Get dogs with longest wait times, optionally filtered by state.';

-- ============================================================================
-- SCHEDULED MAINTENANCE FUNCTIONS
-- These would be called by pg_cron or application workers
-- ============================================================================

-- Example pg_cron schedule (if using pg_cron extension):
--
-- Update urgency levels every hour
-- SELECT cron.schedule('update-urgencies', '0 * * * *', 'SELECT recalculate_all_dog_urgencies()');
--
-- Update shelter counts every 15 minutes
-- SELECT cron.schedule('update-shelter-counts', '0/15 * * * *', 'SELECT update_all_shelter_counts()');
--
-- Update state counts every 15 minutes
-- SELECT cron.schedule('update-state-counts', '0/15 * * * *', 'SELECT update_all_state_counts()');
--
-- Cleanup old error logs daily at 3am
-- SELECT cron.schedule('cleanup-errors', '0 3 * * *', 'SELECT cleanup_old_error_logs(30)');
--
-- Cleanup expired sessions daily at 4am
-- SELECT cron.schedule('cleanup-sessions', '0 4 * * *', 'SELECT cleanup_expired_sessions()');

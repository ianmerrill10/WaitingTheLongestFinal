-- Ported from WaitingTheLongestRedo/sql/core/012_views.sql

-- Dogs with calculated wait time
CREATE OR REPLACE VIEW dogs_with_wait_time AS
SELECT
    d.*,
    NOW() - d.intake_date AS wait_interval,
    EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400 AS wait_days_total,
    EXTRACT(YEAR FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_years,
    EXTRACT(MONTH FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_months,
    EXTRACT(DAY FROM AGE(NOW(), d.intake_date))::INTEGER AS wait_days,
    EXTRACT(HOUR FROM (NOW() - d.intake_date))::INTEGER % 24 AS wait_hours,
    EXTRACT(MINUTE FROM (NOW() - d.intake_date))::INTEGER % 60 AS wait_minutes,
    EXTRACT(SECOND FROM (NOW() - d.intake_date))::INTEGER % 60 AS wait_seconds,
    LPAD(EXTRACT(YEAR FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD(EXTRACT(MONTH FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD(EXTRACT(DAY FROM AGE(NOW(), d.intake_date))::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(HOUR FROM (NOW() - d.intake_date))::INTEGER % 24)::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(MINUTE FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0') || ':' ||
    LPAD((EXTRACT(SECOND FROM (NOW() - d.intake_date))::INTEGER % 60)::TEXT, 2, '0')
    AS wait_time_formatted,
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.is_kill_shelter
FROM dogs d
JOIN shelters s ON d.shelter_id = s.id;

-- Urgent dogs with countdown
CREATE OR REPLACE VIEW urgent_dogs AS
SELECT
    d.*,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN (EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 86400)::INTEGER ELSE NULL END AS countdown_days,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600)::INTEGER % 24) ELSE NULL END AS countdown_hours,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 60)::INTEGER % 60) ELSE NULL END AS countdown_minutes,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN (EXTRACT(EPOCH FROM (d.euthanasia_date - NOW()))::INTEGER % 60) ELSE NULL END AS countdown_seconds,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600 ELSE NULL END AS countdown_hours_total,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
             AND d.euthanasia_date - NOW() < INTERVAL '24 hours'
        THEN TRUE ELSE FALSE END AS is_critical,
    CASE
        WHEN d.euthanasia_date IS NULL THEN 'normal'
        WHEN d.euthanasia_date <= NOW() THEN 'expired'
        WHEN d.euthanasia_date - NOW() < INTERVAL '24 hours' THEN 'critical'
        WHEN d.euthanasia_date - NOW() < INTERVAL '72 hours' THEN 'high'
        WHEN d.euthanasia_date - NOW() < INTERVAL '7 days' THEN 'medium'
        ELSE 'normal'
    END AS calculated_urgency,
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.is_kill_shelter,
    s.accepts_rescue_pulls
FROM dogs d
JOIN shelters s ON d.shelter_id = s.id
WHERE d.is_available = TRUE
  AND (d.urgency_level IN ('medium', 'high', 'critical')
       OR d.is_on_euthanasia_list = TRUE
       OR d.euthanasia_date IS NOT NULL);

-- Shelter statistics
CREATE OR REPLACE VIEW shelter_statistics AS
SELECT
    s.id, s.name, s.city, s.state_code, s.shelter_type, s.is_kill_shelter, s.is_active,
    COUNT(d.id) AS total_dogs,
    COUNT(d.id) FILTER (WHERE d.is_available = TRUE) AS available_dogs,
    COUNT(d.id) FILTER (WHERE d.status = 'adopted') AS adopted_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'critical' AND d.is_available = TRUE) AS critical_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'high' AND d.is_available = TRUE) AS high_urgency_dogs,
    COUNT(d.id) FILTER (WHERE d.is_on_euthanasia_list = TRUE AND d.is_available = TRUE) AS on_euthanasia_list,
    AVG(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400) FILTER (WHERE d.is_available = TRUE) AS avg_wait_days,
    MAX(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400) FILTER (WHERE d.is_available = TRUE) AS max_wait_days
FROM shelters s
LEFT JOIN dogs d ON s.id = d.shelter_id
GROUP BY s.id;

-- State statistics
CREATE OR REPLACE VIEW state_statistics AS
SELECT
    st.code, st.name, st.region,
    COUNT(DISTINCT s.id) AS total_shelters,
    COUNT(DISTINCT s.id) FILTER (WHERE s.is_active = TRUE) AS active_shelters,
    COUNT(d.id) AS total_dogs,
    COUNT(d.id) FILTER (WHERE d.is_available = TRUE) AS available_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'critical' AND d.is_available = TRUE) AS critical_dogs,
    COUNT(d.id) FILTER (WHERE d.urgency_level = 'high' AND d.is_available = TRUE) AS high_urgency_dogs,
    AVG(EXTRACT(EPOCH FROM (NOW() - d.intake_date)) / 86400) FILTER (WHERE d.is_available = TRUE) AS avg_wait_days
FROM states st
LEFT JOIN shelters s ON st.code = s.state_code
LEFT JOIN dogs d ON s.id = d.shelter_id
GROUP BY st.code;

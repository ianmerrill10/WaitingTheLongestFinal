-- WaitingTheLongest.com - Complete Database Setup
-- Run this in the Supabase SQL Editor to create all tables, views, and indexes.
-- This consolidates all 6 migration files in the correct order.

-- ==========================================
-- PREREQUISITES: Enable required extensions
-- ==========================================
CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- ==========================================
-- 001: States reference table
-- ==========================================
CREATE TABLE IF NOT EXISTS states (
    code VARCHAR(2) PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    region VARCHAR(20) NOT NULL DEFAULT 'Unknown',
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

COMMENT ON TABLE states IS 'US states reference table for shelter/dog location';
CREATE INDEX IF NOT EXISTS idx_states_region ON states(region);

-- ==========================================
-- 002: Shelters/Organizations table
-- ==========================================
CREATE TABLE IF NOT EXISTS shelters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(200) NOT NULL,
    slug VARCHAR(200),
    shelter_type VARCHAR(30) NOT NULL DEFAULT 'rescue',
    address VARCHAR(300),
    city VARCHAR(100) NOT NULL,
    state_code VARCHAR(2) NOT NULL REFERENCES states(code),
    zip_code VARCHAR(10),
    county VARCHAR(100),
    latitude DECIMAL(10, 7),
    longitude DECIMAL(10, 7),
    phone VARCHAR(30),
    email VARCHAR(200),
    website VARCHAR(500),
    facebook_url VARCHAR(500),
    description TEXT,
    is_kill_shelter BOOLEAN DEFAULT FALSE,
    urgency_multiplier DECIMAL(3, 2) DEFAULT 1.00,
    accepts_rescue_pulls BOOLEAN DEFAULT TRUE,
    is_verified BOOLEAN DEFAULT FALSE,
    is_active BOOLEAN DEFAULT TRUE,
    ein VARCHAR(20),
    external_id VARCHAR(100),
    external_source VARCHAR(50),
    total_animals INTEGER DEFAULT 0,
    last_synced_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

ALTER TABLE shelters ADD CONSTRAINT shelters_type_valid
    CHECK (shelter_type IN ('municipal', 'private', 'rescue', 'foster_network', 'humane_society', 'spca', 'other'));

CREATE UNIQUE INDEX IF NOT EXISTS idx_shelters_name_state ON shelters(name, state_code);
CREATE INDEX IF NOT EXISTS idx_shelters_state ON shelters(state_code);
CREATE INDEX IF NOT EXISTS idx_shelters_city_state ON shelters(city, state_code);
CREATE INDEX IF NOT EXISTS idx_shelters_zip ON shelters(zip_code) WHERE zip_code IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_shelters_active ON shelters(is_active) WHERE is_active = TRUE;
CREATE INDEX IF NOT EXISTS idx_shelters_ein ON shelters(ein) WHERE ein IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_shelters_external ON shelters(external_source, external_id) WHERE external_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_shelters_name_trgm ON shelters USING gin(name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_shelters_slug ON shelters(slug) WHERE slug IS NOT NULL;

CREATE OR REPLACE FUNCTION update_shelters_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_shelters_updated_at ON shelters;
CREATE TRIGGER trg_shelters_updated_at
    BEFORE UPDATE ON shelters
    FOR EACH ROW
    EXECUTE FUNCTION update_shelters_updated_at();

COMMENT ON TABLE shelters IS 'Shelter, rescue, and animal welfare organizations. No breeders or pet stores.';

-- ==========================================
-- 003: Dogs table - THE CORE TABLE
-- ==========================================
CREATE TABLE IF NOT EXISTS dogs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(100) NOT NULL,
    shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
    breed_primary VARCHAR(100) NOT NULL,
    breed_secondary VARCHAR(100),
    breed_mixed BOOLEAN DEFAULT FALSE,
    size VARCHAR(20) NOT NULL DEFAULT 'medium',
    age_category VARCHAR(20) NOT NULL DEFAULT 'adult',
    age_months INTEGER,
    gender VARCHAR(10) NOT NULL,
    color_primary VARCHAR(50),
    color_secondary VARCHAR(50),
    weight_lbs DECIMAL(5, 1),
    is_spayed_neutered BOOLEAN DEFAULT FALSE,
    status VARCHAR(30) NOT NULL DEFAULT 'available',
    is_available BOOLEAN DEFAULT TRUE,
    hold_reason VARCHAR(100),
    hold_until DATE,
    intake_date TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    intake_type VARCHAR(50),
    available_date TIMESTAMPTZ,
    outcome_date TIMESTAMPTZ,
    outcome_type VARCHAR(50),
    euthanasia_date TIMESTAMPTZ,
    urgency_level VARCHAR(20) DEFAULT 'normal',
    is_on_euthanasia_list BOOLEAN DEFAULT FALSE,
    euthanasia_list_added_at TIMESTAMPTZ,
    rescue_pull_status VARCHAR(30),
    rescue_hold_until TIMESTAMPTZ,
    rescue_organization_id UUID,
    rescue_notes TEXT,
    photo_urls TEXT[] DEFAULT '{}',
    primary_photo_url VARCHAR(500),
    video_url VARCHAR(500),
    thumbnail_url VARCHAR(500),
    description TEXT,
    short_description VARCHAR(500),
    tags TEXT[] DEFAULT '{}',
    good_with_kids BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    house_trained BOOLEAN,
    crate_trained BOOLEAN,
    leash_trained BOOLEAN,
    has_special_needs BOOLEAN DEFAULT FALSE,
    special_needs_description TEXT,
    has_medical_conditions BOOLEAN DEFAULT FALSE,
    medical_conditions TEXT,
    is_heartworm_positive BOOLEAN DEFAULT FALSE,
    heartworm_treatment_status VARCHAR(50),
    has_behavioral_notes BOOLEAN DEFAULT FALSE,
    behavioral_notes TEXT,
    energy_level VARCHAR(20),
    adoption_fee DECIMAL(8, 2),
    fee_includes TEXT,
    is_fee_waived BOOLEAN DEFAULT FALSE,
    external_id VARCHAR(100),
    external_source VARCHAR(50),
    external_url VARCHAR(500),
    last_synced_at TIMESTAMPTZ,
    view_count INTEGER DEFAULT 0,
    favorite_count INTEGER DEFAULT 0,
    share_count INTEGER DEFAULT 0,
    inquiry_count INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

ALTER TABLE dogs ADD CONSTRAINT dogs_status_valid
    CHECK (status IN ('available', 'pending', 'adopted', 'foster', 'hold', 'transferred', 'deceased', 'euthanized', 'returned_to_owner'));
ALTER TABLE dogs ADD CONSTRAINT dogs_size_valid CHECK (size IN ('small', 'medium', 'large', 'xlarge'));
ALTER TABLE dogs ADD CONSTRAINT dogs_age_valid CHECK (age_category IN ('puppy', 'young', 'adult', 'senior'));
ALTER TABLE dogs ADD CONSTRAINT dogs_gender_valid CHECK (gender IN ('male', 'female'));
ALTER TABLE dogs ADD CONSTRAINT dogs_urgency_valid CHECK (urgency_level IN ('normal', 'medium', 'high', 'critical'));
ALTER TABLE dogs ADD CONSTRAINT dogs_energy_valid CHECK (energy_level IS NULL OR energy_level IN ('low', 'medium', 'high'));

CREATE INDEX IF NOT EXISTS idx_dogs_shelter ON dogs(shelter_id);
CREATE INDEX IF NOT EXISTS idx_dogs_status ON dogs(status);
CREATE INDEX IF NOT EXISTS idx_dogs_available ON dogs(is_available) WHERE is_available = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_urgency ON dogs(urgency_level);
CREATE INDEX IF NOT EXISTS idx_dogs_euth_date ON dogs(euthanasia_date) WHERE euthanasia_date IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_dogs_euth_list ON dogs(is_on_euthanasia_list) WHERE is_on_euthanasia_list = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_intake ON dogs(intake_date);
CREATE INDEX IF NOT EXISTS idx_dogs_breed ON dogs(breed_primary);
CREATE INDEX IF NOT EXISTS idx_dogs_size ON dogs(size);
CREATE INDEX IF NOT EXISTS idx_dogs_age ON dogs(age_category);
CREATE INDEX IF NOT EXISTS idx_dogs_gender ON dogs(gender);
CREATE INDEX IF NOT EXISTS idx_dogs_kids ON dogs(good_with_kids) WHERE good_with_kids = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_dogs ON dogs(good_with_dogs) WHERE good_with_dogs = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_cats ON dogs(good_with_cats) WHERE good_with_cats = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_special ON dogs(has_special_needs) WHERE has_special_needs = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_name_trgm ON dogs USING gin(name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_dogs_breed_trgm ON dogs USING gin(breed_primary gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_dogs_external ON dogs(external_source, external_id) WHERE external_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_dogs_shelter_avail ON dogs(shelter_id, is_available) WHERE is_available = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_urgency_avail ON dogs(urgency_level, is_available) WHERE is_available = TRUE;
CREATE INDEX IF NOT EXISTS idx_dogs_avail_intake ON dogs(intake_date, is_available) WHERE is_available = TRUE;

CREATE OR REPLACE FUNCTION update_dogs_updated_at()
RETURNS TRIGGER AS $$
BEGIN NEW.updated_at = NOW(); RETURN NEW; END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_dogs_updated_at ON dogs;
CREATE TRIGGER trg_dogs_updated_at BEFORE UPDATE ON dogs
    FOR EACH ROW EXECUTE FUNCTION update_dogs_updated_at();

CREATE OR REPLACE FUNCTION update_dogs_is_available()
RETURNS TRIGGER AS $$
BEGIN NEW.is_available = (NEW.status = 'available'); RETURN NEW; END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_dogs_is_available ON dogs;
CREATE TRIGGER trg_dogs_is_available BEFORE INSERT OR UPDATE OF status ON dogs
    FOR EACH ROW EXECUTE FUNCTION update_dogs_is_available();

COMMENT ON TABLE dogs IS 'Core dog records. Wait time from intake_date. Countdown from euthanasia_date.';

-- ==========================================
-- 004: Breeds reference table
-- ==========================================
CREATE TABLE IF NOT EXISTS breeds (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    breed_group VARCHAR(50),
    size_typical VARCHAR(20),
    energy_level VARCHAR(20),
    grooming_needs VARCHAR(20),
    temperament TEXT,
    good_with_children BOOLEAN,
    good_with_other_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    apartment_suitable BOOLEAN,
    exercise_needs VARCHAR(20),
    shedding_level VARCHAR(20),
    life_expectancy_min INTEGER,
    life_expectancy_max INTEGER,
    weight_min_lbs INTEGER,
    weight_max_lbs INTEGER,
    description TEXT,
    image_url VARCHAR(500),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_breeds_name ON breeds(name);
CREATE INDEX IF NOT EXISTS idx_breeds_group ON breeds(breed_group) WHERE breed_group IS NOT NULL;

COMMENT ON TABLE breeds IS 'Dog breed reference data with attributes for matching and filtering.';

-- ==========================================
-- 005: Views (wait time, urgency, statistics)
-- ==========================================
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
    s.name AS shelter_name,
    s.city AS shelter_city,
    s.state_code AS shelter_state,
    s.is_kill_shelter
FROM dogs d
JOIN shelters s ON d.shelter_id = s.id;

CREATE OR REPLACE VIEW urgent_dogs AS
SELECT
    d.*,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN (EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 86400)::INTEGER ELSE NULL END AS countdown_days,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600)::INTEGER % 24) ELSE NULL END AS countdown_hours,
    CASE WHEN d.euthanasia_date IS NOT NULL AND d.euthanasia_date > NOW()
        THEN ((EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 60)::INTEGER % 60) ELSE NULL END AS countdown_minutes,
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

-- ==========================================
-- 006: Users, Favorites, Reports, Stories
-- ==========================================
CREATE TABLE IF NOT EXISTS favorites (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(user_id, dog_id)
);

CREATE INDEX IF NOT EXISTS idx_favorites_user ON favorites(user_id);
CREATE INDEX IF NOT EXISTS idx_favorites_dog ON favorites(dog_id);

CREATE TABLE IF NOT EXISTS euthanasia_reports (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
    source VARCHAR(50) NOT NULL,
    source_url VARCHAR(500),
    raw_text TEXT,
    parsed_dog_name VARCHAR(100),
    parsed_breed VARCHAR(100),
    parsed_euthanasia_date TIMESTAMPTZ,
    parsed_shelter_name VARCHAR(200),
    confidence DECIMAL(3, 2) NOT NULL DEFAULT 0.0,
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    reviewed_by UUID,
    reviewed_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

ALTER TABLE euthanasia_reports ADD CONSTRAINT euth_reports_status_valid
    CHECK (status IN ('pending', 'approved', 'rejected', 'auto_applied'));

CREATE INDEX IF NOT EXISTS idx_euth_reports_status ON euthanasia_reports(status);
CREATE INDEX IF NOT EXISTS idx_euth_reports_dog ON euthanasia_reports(dog_id) WHERE dog_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS rescue_notifications (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    rescue_org_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
    notification_type VARCHAR(20) NOT NULL DEFAULT 'email',
    status VARCHAR(20) NOT NULL DEFAULT 'sent',
    response VARCHAR(20),
    sent_at TIMESTAMPTZ DEFAULT NOW(),
    responded_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_rescue_notif_dog ON rescue_notifications(dog_id);

CREATE TABLE IF NOT EXISTS success_stories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    user_id UUID,
    dog_name VARCHAR(100) NOT NULL,
    title VARCHAR(200) NOT NULL,
    story TEXT NOT NULL,
    photo_urls TEXT[] DEFAULT '{}',
    is_approved BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_stories_approved ON success_stories(is_approved) WHERE is_approved = TRUE;

-- ==========================================
-- DONE! All tables, views, indexes created.
-- ==========================================

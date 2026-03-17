-- Shelters/Rescues/Organizations table
CREATE TABLE shelters (
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

-- Constraints
ALTER TABLE shelters ADD CONSTRAINT shelters_type_valid
    CHECK (shelter_type IN ('municipal', 'private', 'rescue', 'foster_network', 'humane_society', 'spca', 'other'));

-- Unique constraint for deduplication
CREATE UNIQUE INDEX idx_shelters_name_state ON shelters(name, state_code);

-- Indexes
CREATE INDEX idx_shelters_state ON shelters(state_code);
CREATE INDEX idx_shelters_city_state ON shelters(city, state_code);
CREATE INDEX idx_shelters_zip ON shelters(zip_code) WHERE zip_code IS NOT NULL;
CREATE INDEX idx_shelters_active ON shelters(is_active) WHERE is_active = TRUE;
CREATE INDEX idx_shelters_ein ON shelters(ein) WHERE ein IS NOT NULL;
CREATE INDEX idx_shelters_external ON shelters(external_source, external_id) WHERE external_id IS NOT NULL;
CREATE INDEX idx_shelters_name_trgm ON shelters USING gin(name gin_trgm_ops);
CREATE INDEX idx_shelters_slug ON shelters(slug) WHERE slug IS NOT NULL;

-- Trigger: auto-update updated_at
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

COMMENT ON TABLE shelters IS 'Shelter, rescue, and animal welfare organizations. No breeders or pet stores.';

-- Dogs table - THE CORE TABLE
-- Ported from WaitingTheLongestRedo/sql/core/004_dogs.sql
CREATE TABLE dogs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Basic info
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

    -- Status
    status VARCHAR(30) NOT NULL DEFAULT 'available',
    is_available BOOLEAN DEFAULT TRUE,
    hold_reason VARCHAR(100),
    hold_until DATE,

    -- Dates (critical for wait time)
    intake_date TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    intake_type VARCHAR(50),
    available_date TIMESTAMPTZ,
    outcome_date TIMESTAMPTZ,
    outcome_type VARCHAR(50),

    -- Urgency tracking (critical for life-saving)
    euthanasia_date TIMESTAMPTZ,
    urgency_level VARCHAR(20) DEFAULT 'normal',
    is_on_euthanasia_list BOOLEAN DEFAULT FALSE,
    euthanasia_list_added_at TIMESTAMPTZ,
    rescue_pull_status VARCHAR(30),
    rescue_hold_until TIMESTAMPTZ,
    rescue_organization_id UUID,
    rescue_notes TEXT,

    -- Media
    photo_urls TEXT[] DEFAULT '{}',
    primary_photo_url VARCHAR(500),
    video_url VARCHAR(500),
    thumbnail_url VARCHAR(500),

    -- Description
    description TEXT,
    short_description VARCHAR(500),
    tags TEXT[] DEFAULT '{}',

    -- Compatibility
    good_with_kids BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    house_trained BOOLEAN,
    crate_trained BOOLEAN,
    leash_trained BOOLEAN,

    -- Special needs / medical
    has_special_needs BOOLEAN DEFAULT FALSE,
    special_needs_description TEXT,
    has_medical_conditions BOOLEAN DEFAULT FALSE,
    medical_conditions TEXT,
    is_heartworm_positive BOOLEAN DEFAULT FALSE,
    heartworm_treatment_status VARCHAR(50),
    has_behavioral_notes BOOLEAN DEFAULT FALSE,
    behavioral_notes TEXT,
    energy_level VARCHAR(20),

    -- Adoption
    adoption_fee DECIMAL(8, 2),
    fee_includes TEXT,
    is_fee_waived BOOLEAN DEFAULT FALSE,

    -- External tracking
    external_id VARCHAR(100),
    external_source VARCHAR(50),
    external_url VARCHAR(500),
    last_synced_at TIMESTAMPTZ,

    -- Metrics
    view_count INTEGER DEFAULT 0,
    favorite_count INTEGER DEFAULT 0,
    share_count INTEGER DEFAULT 0,
    inquiry_count INTEGER DEFAULT 0,

    -- Timestamps
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Constraints
ALTER TABLE dogs ADD CONSTRAINT dogs_status_valid
    CHECK (status IN ('available', 'pending', 'adopted', 'foster', 'hold', 'transferred', 'deceased', 'euthanized', 'returned_to_owner'));
ALTER TABLE dogs ADD CONSTRAINT dogs_size_valid CHECK (size IN ('small', 'medium', 'large', 'xlarge'));
ALTER TABLE dogs ADD CONSTRAINT dogs_age_valid CHECK (age_category IN ('puppy', 'young', 'adult', 'senior'));
ALTER TABLE dogs ADD CONSTRAINT dogs_gender_valid CHECK (gender IN ('male', 'female'));
ALTER TABLE dogs ADD CONSTRAINT dogs_urgency_valid CHECK (urgency_level IN ('normal', 'medium', 'high', 'critical'));
ALTER TABLE dogs ADD CONSTRAINT dogs_energy_valid CHECK (energy_level IS NULL OR energy_level IN ('low', 'medium', 'high'));

-- Core indexes
CREATE INDEX idx_dogs_shelter ON dogs(shelter_id);
CREATE INDEX idx_dogs_status ON dogs(status);
CREATE INDEX idx_dogs_available ON dogs(is_available) WHERE is_available = TRUE;
CREATE INDEX idx_dogs_urgency ON dogs(urgency_level);
CREATE INDEX idx_dogs_euth_date ON dogs(euthanasia_date) WHERE euthanasia_date IS NOT NULL;
CREATE INDEX idx_dogs_euth_list ON dogs(is_on_euthanasia_list) WHERE is_on_euthanasia_list = TRUE;
CREATE INDEX idx_dogs_intake ON dogs(intake_date);
CREATE INDEX idx_dogs_breed ON dogs(breed_primary);
CREATE INDEX idx_dogs_size ON dogs(size);
CREATE INDEX idx_dogs_age ON dogs(age_category);
CREATE INDEX idx_dogs_gender ON dogs(gender);

-- Compatibility indexes
CREATE INDEX idx_dogs_kids ON dogs(good_with_kids) WHERE good_with_kids = TRUE;
CREATE INDEX idx_dogs_dogs ON dogs(good_with_dogs) WHERE good_with_dogs = TRUE;
CREATE INDEX idx_dogs_cats ON dogs(good_with_cats) WHERE good_with_cats = TRUE;
CREATE INDEX idx_dogs_special ON dogs(has_special_needs) WHERE has_special_needs = TRUE;

-- Text search
CREATE INDEX idx_dogs_name_trgm ON dogs USING gin(name gin_trgm_ops);
CREATE INDEX idx_dogs_breed_trgm ON dogs USING gin(breed_primary gin_trgm_ops);

-- External lookup
CREATE INDEX idx_dogs_external ON dogs(external_source, external_id) WHERE external_id IS NOT NULL;

-- Composite indexes
CREATE INDEX idx_dogs_shelter_avail ON dogs(shelter_id, is_available) WHERE is_available = TRUE;
CREATE INDEX idx_dogs_urgency_avail ON dogs(urgency_level, is_available) WHERE is_available = TRUE;
CREATE INDEX idx_dogs_avail_intake ON dogs(intake_date, is_available) WHERE is_available = TRUE;

-- Triggers
CREATE OR REPLACE FUNCTION update_dogs_updated_at()
RETURNS TRIGGER AS $$
BEGIN NEW.updated_at = NOW(); RETURN NEW; END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_dogs_updated_at BEFORE UPDATE ON dogs
    FOR EACH ROW EXECUTE FUNCTION update_dogs_updated_at();

CREATE OR REPLACE FUNCTION update_dogs_is_available()
RETURNS TRIGGER AS $$
BEGIN NEW.is_available = (NEW.status = 'available'); RETURN NEW; END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_dogs_is_available BEFORE INSERT OR UPDATE OF status ON dogs
    FOR EACH ROW EXECUTE FUNCTION update_dogs_is_available();

COMMENT ON TABLE dogs IS 'Core dog records. Wait time from intake_date (YY:MM:DD:HH:MM:SS). Countdown from euthanasia_date (DD:HH:MM:SS).';

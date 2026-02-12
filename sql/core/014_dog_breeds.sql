-- ============================================================================
-- 014_dog_breeds.sql
-- Dog breeds reference/lookup table
-- Used by matching service and seed data
-- ============================================================================

CREATE TABLE IF NOT EXISTS dog_breeds (
    id              SERIAL PRIMARY KEY,
    name            VARCHAR(100) NOT NULL UNIQUE,
    breed_group     VARCHAR(50),
    height_imperial VARCHAR(50),
    weight_imperial VARCHAR(50),
    life_span       VARCHAR(50),
    temperament     TEXT,
    energy_level    VARCHAR(20),
    exercise_needs  VARCHAR(20),
    grooming_needs  VARCHAR(20),
    good_with_kids  BOOLEAN DEFAULT TRUE,
    good_with_dogs  BOOLEAN DEFAULT TRUE,
    good_with_cats  BOOLEAN DEFAULT FALSE,
    apartment_friendly BOOLEAN DEFAULT FALSE,
    first_time_owner_friendly BOOLEAN DEFAULT FALSE,
    image_url       TEXT,
    description     TEXT,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

COMMENT ON TABLE dog_breeds IS 'Reference table of dog breeds with characteristics for matching';
COMMENT ON COLUMN dog_breeds.name IS 'Breed name (unique)';
COMMENT ON COLUMN dog_breeds.breed_group IS 'AKC breed group (e.g., Sporting, Herding, Working)';
COMMENT ON COLUMN dog_breeds.energy_level IS 'Energy level: Low, Medium, High, Very High';
COMMENT ON COLUMN dog_breeds.exercise_needs IS 'Exercise needs: Low, Medium, High, Very High';
COMMENT ON COLUMN dog_breeds.grooming_needs IS 'Grooming needs: Low, Medium, High, Very High';

CREATE INDEX idx_dog_breeds_name ON dog_breeds(name);
CREATE INDEX idx_dog_breeds_group ON dog_breeds(breed_group);

-- Dog breeds reference table
CREATE TABLE breeds (
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

CREATE INDEX idx_breeds_name ON breeds(name);
CREATE INDEX idx_breeds_group ON breeds(breed_group) WHERE breed_group IS NOT NULL;

COMMENT ON TABLE breeds IS 'Dog breed reference data with attributes for matching and filtering.';

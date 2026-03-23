-- Migration 013: Shelter Master List enhancements
-- Adds denormalized dog counts for fast sort/filter, city search index, county index

-- Denormalized dog counts on shelters (updated periodically by scraper agent)
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS available_dog_count INTEGER DEFAULT 0;
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS urgent_dog_count INTEGER DEFAULT 0;

-- Indexes for sort-by-dog-count queries
CREATE INDEX IF NOT EXISTS idx_shelters_dog_count ON shelters(available_dog_count DESC);
CREATE INDEX IF NOT EXISTS idx_shelters_urgent_count ON shelters(urgent_dog_count DESC);

-- City trigram index for fuzzy city search (pg_trgm already enabled from migration 002)
CREATE INDEX IF NOT EXISTS idx_shelters_city_trgm ON shelters USING gin(city gin_trgm_ops);

-- County index for county filter
CREATE INDEX IF NOT EXISTS idx_shelters_county ON shelters(county) WHERE county IS NOT NULL;

-- Function to refresh shelter dog counts (call after scrape cycles or on schedule)
CREATE OR REPLACE FUNCTION refresh_shelter_dog_counts() RETURNS void AS $$
BEGIN
  UPDATE shelters s SET
    available_dog_count = COALESCE(sub.avail, 0),
    urgent_dog_count = COALESCE(sub.urgent, 0)
  FROM (
    SELECT
      d.shelter_id,
      COUNT(*) FILTER (WHERE d.is_available = true) AS avail,
      COUNT(*) FILTER (WHERE d.is_available = true AND d.urgency_level IN ('critical', 'high')) AS urgent
    FROM dogs d
    GROUP BY d.shelter_id
  ) sub
  WHERE s.id = sub.shelter_id
    AND (s.available_dog_count != COALESCE(sub.avail, 0) OR s.urgent_dog_count != COALESCE(sub.urgent, 0));

  -- Zero out shelters with no dogs
  UPDATE shelters SET available_dog_count = 0, urgent_dog_count = 0
  WHERE (available_dog_count > 0 OR urgent_dog_count > 0)
    AND id NOT IN (SELECT DISTINCT shelter_id FROM dogs WHERE is_available = true);
END;
$$ LANGUAGE plpgsql;

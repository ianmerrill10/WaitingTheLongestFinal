-- Migration 014: Cross-platform deduplication tracking
-- Tracks when dog records from different sources are merged

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS dedup_merged_from JSONB DEFAULT '[]'::jsonb;

-- Index for finding merged records
CREATE INDEX IF NOT EXISTS idx_dogs_dedup ON dogs USING gin(dedup_merged_from) WHERE dedup_merged_from != '[]'::jsonb;

-- Index for dedup matching: name + shelter_id + breed
CREATE INDEX IF NOT EXISTS idx_dogs_name_shelter_breed ON dogs(shelter_id, name, breed_primary) WHERE is_available = true;

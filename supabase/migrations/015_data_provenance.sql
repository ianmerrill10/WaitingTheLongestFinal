-- Migration 015: Data Provenance for Legal Defensibility
-- Adds source tracking columns for complete audit trail of every dog listing.
-- Each dog must have a documented, verifiable chain of data origin.

-- Array of verifiable source links with check history
-- Structure: [{url, source, checked_at, status_code, description}]
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS source_links JSONB DEFAULT '[]'::jsonb;

-- Preserve the original intake_date before any capping/adjustment
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS original_intake_date TIMESTAMPTZ;

-- How the data was extracted (e.g., "rescuegroups_api_v5", "html_scraper_cheerio")
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS source_extraction_method VARCHAR(50);

-- Computed credibility score 0-100
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS credibility_score INTEGER DEFAULT 0;

-- Denormalized from shelter: whether shelter has verified nonprofit status
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS source_nonprofit_verified BOOLEAN DEFAULT FALSE;

-- Index for credibility-based queries
CREATE INDEX IF NOT EXISTS idx_dogs_credibility ON dogs(credibility_score) WHERE is_available = TRUE;

-- Backfill original_intake_date from current intake_date for all existing dogs
UPDATE dogs SET original_intake_date = intake_date WHERE original_intake_date IS NULL;

-- Backfill source_extraction_method from external_source
UPDATE dogs SET source_extraction_method = 'rescuegroups_api_v5'
WHERE external_source = 'rescuegroups' AND source_extraction_method IS NULL;

UPDATE dogs SET source_extraction_method = 'html_scraper_cheerio'
WHERE external_source LIKE 'scraper_%' AND source_extraction_method IS NULL;

-- Backfill source_nonprofit_verified from shelter join
UPDATE dogs d SET source_nonprofit_verified = s.is_verified
FROM shelters s WHERE d.shelter_id = s.id AND d.source_nonprofit_verified = FALSE AND s.is_verified = TRUE;

-- Backfill source_links for RescueGroups dogs that have external_url
UPDATE dogs SET source_links = jsonb_build_array(
  jsonb_build_object(
    'url', external_url,
    'source', 'rescuegroups_api_v5',
    'checked_at', last_synced_at::text,
    'status_code', NULL,
    'description', 'RescueGroups.org animal listing page'
  )
)
WHERE external_source = 'rescuegroups'
  AND external_url IS NOT NULL
  AND (source_links IS NULL OR source_links = '[]'::jsonb);

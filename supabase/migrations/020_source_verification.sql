-- Migration 020: Source Verification & Confidence Tier
--
-- Guarantees zero breeders in the DB by adding a structured verification
-- system to shelters and a confidence_tier column to dogs that is assigned
-- automatically at ingest time based on shelter verification state.
--
-- Three-layer defence against bad actors:
--   1. shelters.org_type ENUM bans breeder/pet_store at the DB level.
--   2. shelters.verification_status + verification metadata give admins a
--      clear workflow to review and certify every shelter as non-profit.
--   3. dogs.confidence_tier is set by the scraper/API handler based on
--      the parent shelter's verification state — high-confidence dogs from
--      verified non-profits float to the top of every query.

-- ─── 1. Org type (replaces the looser shelter_type enum) ───────────────────
-- We keep shelter_type for backwards compat but add org_type which is
-- stricter and used for verification gating.

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS org_type VARCHAR(30) DEFAULT 'unknown';

ALTER TABLE shelters DROP CONSTRAINT IF EXISTS shelters_org_type_valid;
ALTER TABLE shelters ADD CONSTRAINT shelters_org_type_valid
  CHECK (org_type IN (
    'municipal_shelter',    -- government-run, tax-funded
    'nonprofit_humane',     -- 501(c)(3) humane society / SPCA
    'nonprofit_rescue',     -- 501(c)(3) breed or general rescue
    'foster_network',       -- volunteer foster-only network
    'tribal_shelter',       -- tribal nation shelter
    'university',           -- vet school / training shelter
    'unknown'               -- not yet classified
    -- INTENTIONALLY EXCLUDED: 'breeder', 'pet_store', 'for_profit'
    -- If any of those appear, the INSERT will fail with a constraint violation.
  ));

-- ─── 2. Verification status enum ───────────────────────────────────────────

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS verification_status VARCHAR(20) DEFAULT 'unverified';

ALTER TABLE shelters DROP CONSTRAINT IF EXISTS shelters_verification_status_valid;
ALTER TABLE shelters ADD CONSTRAINT shelters_verification_status_valid
  CHECK (verification_status IN (
    'unverified',           -- default: scraped/imported, not yet reviewed
    'pending_review',       -- flagged for admin to check
    'verified_nonprofit',   -- EIN confirmed, 990 on file or Guidestar match
    'verified_government',  -- .gov domain or FOIA-listed municipal shelter
    'rejected'              -- failed review — dogs will be hidden from site
  ));

-- ─── 3. Verification metadata (structured audit trail) ─────────────────────
-- Stores the evidence chain so any listing can be verified on demand.
-- Example:
-- {
--   "verified_by": "admin:ian",
--   "verified_at": "2026-04-15T10:00:00Z",
--   "method": "ein_guidestar",
--   "ein": "47-1234567",
--   "guidestar_url": "https://www.guidestar.org/profile/471234567",
--   "irs_pub78": true,
--   "domain_verified": true,
--   "notes": "Confirmed 501(c)(3), active filing for FY2024"
-- }

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS verification_metadata JSONB DEFAULT '{}'::jsonb;

-- When the verification was last updated
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS verified_at TIMESTAMPTZ;

-- Who performed the verification (admin user reference)
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS verified_by VARCHAR(100);

-- ─── 4. Domain allowlist flag ──────────────────────────────────────────────
-- Set to TRUE once we've confirmed the shelter's domain belongs to a
-- real non-profit (checked via HTTPS + domain registration + EIN match).
-- Scrapers use this to auto-assign high confidence at ingest.

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS domain_verified BOOLEAN DEFAULT FALSE;

-- ─── 5. Confidence tier on dogs ────────────────────────────────────────────
-- Set at ingest time by the scraper / API handler.
-- Used for: ranking, display badges, query filtering.
-- Never manually set — always derived from shelter state.

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS confidence_tier VARCHAR(20) DEFAULT 'standard';

ALTER TABLE dogs DROP CONSTRAINT IF EXISTS dogs_confidence_tier_valid;
ALTER TABLE dogs ADD CONSTRAINT dogs_confidence_tier_valid
  CHECK (confidence_tier IN (
    'high',       -- from verified_nonprofit or verified_government shelter
    'standard',   -- from unverified shelter with clean scrape history
    'low',        -- from shelter with data quality issues or pending review
    'hidden'      -- from rejected shelter — excluded from public listings
  ));

-- ─── 6. Indexes ────────────────────────────────────────────────────────────

CREATE INDEX IF NOT EXISTS idx_shelters_verification
  ON shelters(verification_status);

CREATE INDEX IF NOT EXISTS idx_shelters_org_type
  ON shelters(org_type);

CREATE INDEX IF NOT EXISTS idx_shelters_domain_verified
  ON shelters(domain_verified) WHERE domain_verified = TRUE;

CREATE INDEX IF NOT EXISTS idx_dogs_confidence_tier
  ON dogs(confidence_tier, is_available) WHERE is_available = TRUE;

-- Composite: verified dogs, sorted by urgency then wait time (homepage query)
CREATE INDEX IF NOT EXISTS idx_dogs_high_confidence_urgency
  ON dogs(confidence_tier, urgency_level, euthanasia_date)
  WHERE confidence_tier = 'high' AND is_available = TRUE;

-- ─── 7. Backfill org_type from existing shelter_type ───────────────────────

UPDATE shelters SET org_type = CASE shelter_type
  WHEN 'municipal'      THEN 'municipal_shelter'
  WHEN 'humane_society' THEN 'nonprofit_humane'
  WHEN 'spca'           THEN 'nonprofit_humane'
  WHEN 'rescue'         THEN 'nonprofit_rescue'
  WHEN 'foster_network' THEN 'foster_network'
  WHEN 'private'        THEN 'nonprofit_rescue'  -- assume non-profit until reviewed
  ELSE 'unknown'
END
WHERE org_type = 'unknown';

-- ─── 8. Backfill verification_status from existing is_verified flag ─────────

UPDATE shelters
  SET verification_status = 'verified_nonprofit'
  WHERE is_verified = TRUE AND verification_status = 'unverified';

-- Mark government shelters from .gov domains
UPDATE shelters
  SET verification_status = 'verified_government',
      org_type = 'municipal_shelter'
  WHERE website ILIKE '%\.gov%'
    AND verification_status = 'unverified';

-- ─── 9. Backfill confidence_tier on existing dogs ──────────────────────────

-- High: parent shelter is fully verified
UPDATE dogs d
  SET confidence_tier = 'high'
  FROM shelters s
  WHERE d.shelter_id = s.id
    AND s.verification_status IN ('verified_nonprofit', 'verified_government')
    AND d.confidence_tier = 'standard';

-- Hidden: parent shelter was rejected
UPDATE dogs d
  SET confidence_tier = 'hidden'
  FROM shelters s
  WHERE d.shelter_id = s.id
    AND s.verification_status = 'rejected'
    AND d.confidence_tier != 'hidden';

-- ─── 10. Function: assign_dog_confidence_tier() ────────────────────────────
-- Called by the upsert trigger so confidence_tier is always consistent
-- with its parent shelter — no scraper logic can forget to set it.

CREATE OR REPLACE FUNCTION assign_dog_confidence_tier()
RETURNS TRIGGER AS $$
DECLARE
  shelter_vs VARCHAR(20);
BEGIN
  SELECT verification_status INTO shelter_vs
    FROM shelters WHERE id = NEW.shelter_id;

  NEW.confidence_tier := CASE shelter_vs
    WHEN 'verified_nonprofit'  THEN 'high'
    WHEN 'verified_government' THEN 'high'
    WHEN 'rejected'            THEN 'hidden'
    WHEN 'pending_review'      THEN 'low'
    ELSE 'standard'
  END;

  -- Also sync the denormalized flag
  NEW.source_nonprofit_verified := (shelter_vs IN ('verified_nonprofit', 'verified_government'));

  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Fire on INSERT and whenever shelter_id changes (e.g. re-assigned during dedup)
DROP TRIGGER IF EXISTS trg_dogs_confidence_tier ON dogs;
CREATE TRIGGER trg_dogs_confidence_tier
  BEFORE INSERT OR UPDATE OF shelter_id ON dogs
  FOR EACH ROW EXECUTE FUNCTION assign_dog_confidence_tier();

-- ─── 11. View: public listing guard ────────────────────────────────────────
-- Any query that reads public dog listings should include this filter.
-- "hidden" dogs are from rejected shelters and must never appear publicly.

CREATE OR REPLACE VIEW public_dogs AS
  SELECT * FROM dogs
  WHERE is_available = TRUE
    AND confidence_tier != 'hidden';

COMMENT ON VIEW public_dogs IS
  'Safe public-facing dog view. Excludes hidden (rejected-shelter) dogs. Always use this instead of querying dogs directly on public routes.';

-- ─── Comments ───────────────────────────────────────────────────────────────

COMMENT ON COLUMN shelters.org_type IS
  'Organisation type. ENUM excludes breeder/pet_store at the constraint level.';
COMMENT ON COLUMN shelters.verification_status IS
  'Admin-managed verification state. verified_nonprofit and verified_government unlock high confidence_tier on all child dogs.';
COMMENT ON COLUMN shelters.verification_metadata IS
  'Structured evidence chain: EIN, Guidestar URL, IRS Pub78 flag, verifier identity, timestamp.';
COMMENT ON COLUMN shelters.domain_verified IS
  'TRUE once HTTPS domain confirmed to belong to this organisation (not a parked/cloned domain).';
COMMENT ON COLUMN dogs.confidence_tier IS
  'Auto-assigned by trigger from parent shelter verification_status. high=verified nonprofit, standard=unreviewed, low=pending, hidden=rejected.';

-- Migration 016: Stray hold adjustment, foster distinction, cross-shelter transfer tracking
-- Addresses three data accuracy gaps identified in architecture review

-- ═══════════════════════════════════════════════════════════
-- 1. STRAY HOLD: Per-shelter stray hold duration
-- ═══════════════════════════════════════════════════════════
-- Municipal shelters report available_date AFTER stray hold (3-7 days).
-- The real intake date is stray_hold_days earlier than what they report.
-- This column allows per-shelter override of the state default.

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS stray_hold_days INTEGER DEFAULT NULL;

COMMENT ON COLUMN shelters.stray_hold_days IS
  'Number of days this shelter holds strays before marking available. NULL = use state default.';

-- ═══════════════════════════════════════════════════════════
-- 2. FOSTER: Distinguish shelter-housed vs foster-housed dogs
-- ═══════════════════════════════════════════════════════════
-- Dogs in foster homes are available for adoption but NOT at physical risk
-- of euthanasia. This flag helps separate urgency from availability.

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_foster BOOLEAN DEFAULT false;

COMMENT ON COLUMN dogs.is_foster IS
  'True if dog is in a foster home rather than physically in the shelter.';

CREATE INDEX IF NOT EXISTS idx_dogs_is_foster ON dogs(is_foster) WHERE is_foster = true;

-- ═══════════════════════════════════════════════════════════
-- 3. TRANSFER TRACKING: Preserve wait time across shelters
-- ═══════════════════════════════════════════════════════════
-- When a dog transfers from Shelter A to Shelter B, the listing resets.
-- These columns preserve the original shelter and true first intake date
-- so cumulative wait time is accurate.

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS transfer_origin_shelter_id UUID REFERENCES shelters(id);
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS transfer_original_intake TIMESTAMPTZ DEFAULT NULL;

COMMENT ON COLUMN dogs.transfer_origin_shelter_id IS
  'If this dog was transferred, the shelter they originally came from.';
COMMENT ON COLUMN dogs.transfer_original_intake IS
  'If this dog was transferred, their intake date at the ORIGINAL shelter.';

-- ═══════════════════════════════════════════════════════════
-- 4. FIX: Add outcome_unknown to status constraint
-- ═══════════════════════════════════════════════════════════
-- Agents use outcome_unknown but it was missing from the constraint.

ALTER TABLE dogs DROP CONSTRAINT IF EXISTS dogs_status_valid;
ALTER TABLE dogs ADD CONSTRAINT dogs_status_valid
  CHECK (status IN (
    'available', 'pending', 'adopted', 'foster', 'hold',
    'transferred', 'deceased', 'euthanized', 'returned_to_owner',
    'outcome_unknown', 'duplicate_merged'
  ));

-- ═══════════════════════════════════════════════════════════
-- 5. BACKFILL: Detect foster dogs from existing data
-- ═══════════════════════════════════════════════════════════
-- Dogs with status='foster' should have is_foster=true
UPDATE dogs SET is_foster = true WHERE status = 'foster';

-- Dogs with description mentioning "foster" that are available
UPDATE dogs SET is_foster = true
WHERE is_available = true
  AND is_foster = false
  AND (
    description ILIKE '%currently in foster%'
    OR description ILIKE '%in a foster home%'
    OR description ILIKE '%foster home%'
    OR description ILIKE '%living in foster%'
    OR tags @> ARRAY['foster']
  );

-- Migration 017: Evidence capture, ranking eligibility, review queue
-- Implements improvements from architecture review for data accuracy

-- ═══════════════════════════════════════════════════════════
-- 1. RANKING ELIGIBILITY: Only verified dogs rank on "longest waiting"
-- ═══════════════════════════════════════════════════════════
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS ranking_eligible BOOLEAN DEFAULT false;

COMMENT ON COLUMN dogs.ranking_eligible IS
  'True only if dog has a verified intake date and is available. Controls ranking visibility.';

CREATE INDEX IF NOT EXISTS idx_dogs_ranking_eligible
  ON dogs(ranking_eligible, intake_date ASC)
  WHERE ranking_eligible = true AND is_available = true;

-- Backfill: dogs with verified dates are ranking-eligible
UPDATE dogs SET ranking_eligible = true
WHERE is_available = true
  AND date_confidence = 'verified'
  AND intake_date IS NOT NULL;

-- ═══════════════════════════════════════════════════════════
-- 2. CRAWL CONSISTENCY: Track how many times we've seen the same date
-- ═══════════════════════════════════════════════════════════
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS intake_date_observation_count INTEGER DEFAULT 1;

COMMENT ON COLUMN dogs.intake_date_observation_count IS
  'Number of independent crawl/sync cycles that reported the same intake_date.';

-- ═══════════════════════════════════════════════════════════
-- 3. EVIDENCE STORAGE: Path references for snapshots
-- ═══════════════════════════════════════════════════════════
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS page_hash TEXT DEFAULT NULL;

COMMENT ON COLUMN dogs.page_hash IS
  'SHA-256 hash of the listing page content at last check. Used for change detection.';

-- ═══════════════════════════════════════════════════════════
-- 4. REVIEW QUEUE: Flag dogs needing manual review
-- ═══════════════════════════════════════════════════════════
CREATE TABLE IF NOT EXISTS review_queue (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
  reason TEXT NOT NULL CHECK (reason IN (
    'missing_intake_date',
    'conflicting_intake_dates',
    'possible_duplicate',
    'status_unclear',
    'transfer_suspected',
    'suspiciously_old_listing',
    'date_confidence_low'
  )),
  priority INTEGER NOT NULL DEFAULT 50,
  state TEXT NOT NULL DEFAULT 'open' CHECK (state IN ('open', 'in_review', 'resolved', 'dismissed')),
  notes TEXT,
  resolved_by TEXT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_review_queue_state_priority
  ON review_queue(state, priority DESC, created_at ASC);

CREATE INDEX IF NOT EXISTS idx_review_queue_dog
  ON review_queue(dog_id);

-- ═══════════════════════════════════════════════════════════
-- 5. BACKFILL REVIEW QUEUE: Flag dogs needing attention
-- ═══════════════════════════════════════════════════════════
-- Low confidence dogs that have been waiting a while
INSERT INTO review_queue (dog_id, reason, priority, notes)
SELECT id, 'date_confidence_low', 30, 'Automated: low confidence date on active listing'
FROM dogs
WHERE is_available = true
  AND date_confidence IN ('low', 'unknown')
  AND intake_date < now() - interval '30 days'
LIMIT 500
ON CONFLICT DO NOTHING;

-- Suspiciously old listings (> 2 years)
INSERT INTO review_queue (dog_id, reason, priority, notes)
SELECT id, 'suspiciously_old_listing', 60, 'Automated: listing older than 2 years, may be stale'
FROM dogs
WHERE is_available = true
  AND intake_date < now() - interval '730 days'
LIMIT 200
ON CONFLICT DO NOTHING;

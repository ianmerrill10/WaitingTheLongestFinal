-- Migration 009: Add verification status tracking to dogs table
-- Tracks whether each dog has been verified as still available in RescueGroups

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS last_verified_at TIMESTAMPTZ;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS verification_status TEXT DEFAULT 'unverified';
-- verification_status values: 'verified', 'unverified', 'not_found', 'pending', 'stale'

-- Index for finding unverified/stale dogs efficiently
CREATE INDEX IF NOT EXISTS idx_dogs_verification_status ON dogs(verification_status) WHERE is_available = true;
CREATE INDEX IF NOT EXISTS idx_dogs_last_verified_at ON dogs(last_verified_at ASC NULLS FIRST) WHERE is_available = true;

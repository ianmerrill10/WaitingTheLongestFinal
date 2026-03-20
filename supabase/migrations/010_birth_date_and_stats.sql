-- Migration 010: Birth date, courtesy listing, and daily stats
-- Run this in Supabase SQL Editor

-- Add birth_date columns to dogs
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS birth_date TIMESTAMPTZ;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_birth_date_exact BOOLEAN DEFAULT NULL;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_courtesy_listing BOOLEAN DEFAULT FALSE;

CREATE INDEX IF NOT EXISTS idx_dogs_birth_date ON dogs(birth_date) WHERE birth_date IS NOT NULL;

-- Daily stats table for tracking national averages over time
CREATE TABLE IF NOT EXISTS daily_stats (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  stat_date DATE NOT NULL UNIQUE,
  total_available_dogs INTEGER NOT NULL DEFAULT 0,
  high_confidence_dogs INTEGER NOT NULL DEFAULT 0,
  avg_wait_days_all DECIMAL(10,2),
  avg_wait_days_high_confidence DECIMAL(10,2),
  median_wait_days DECIMAL(10,2),
  max_wait_days INTEGER,
  dogs_with_birth_date INTEGER DEFAULT 0,
  verification_coverage_pct DECIMAL(5,2),
  date_accuracy_pct DECIMAL(5,2),
  dogs_added_today INTEGER DEFAULT 0,
  dogs_adopted_today INTEGER DEFAULT 0,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_daily_stats_date ON daily_stats(stat_date DESC);

-- Enable RLS on daily_stats (read-only for anon)
ALTER TABLE daily_stats ENABLE ROW LEVEL SECURITY;
CREATE POLICY "daily_stats_read" ON daily_stats FOR SELECT USING (true);
CREATE POLICY "daily_stats_write" ON daily_stats FOR ALL USING (true) WITH CHECK (true);

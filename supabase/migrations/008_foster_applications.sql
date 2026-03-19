-- ============================================================
-- Migration 008: Foster Applications
-- ============================================================

CREATE TABLE IF NOT EXISTS foster_applications (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  first_name TEXT NOT NULL,
  last_name TEXT NOT NULL,
  email TEXT NOT NULL,
  zip_code TEXT NOT NULL,
  housing_type TEXT NOT NULL,
  dog_experience TEXT NOT NULL,
  preferred_size TEXT DEFAULT 'any',
  notes TEXT,
  status TEXT NOT NULL DEFAULT 'pending',
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Index for querying by status and recency
CREATE INDEX idx_foster_applications_status ON foster_applications(status);
CREATE INDEX idx_foster_applications_created ON foster_applications(created_at DESC);
CREATE INDEX idx_foster_applications_zip ON foster_applications(zip_code);

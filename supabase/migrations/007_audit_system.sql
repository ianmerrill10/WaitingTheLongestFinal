-- 007: Audit System - Data integrity tracking and automated repair logging
-- Adds date confidence tracking to dogs, audit runs and logs tables

-- Add date tracking columns to dogs table
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS date_confidence TEXT DEFAULT 'unknown'
  CHECK (date_confidence IN ('verified', 'high', 'medium', 'low', 'unknown'));
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS date_source TEXT DEFAULT NULL;
ALTER TABLE dogs ADD COLUMN IF NOT EXISTS last_audited_at TIMESTAMPTZ DEFAULT NULL;

-- Audit runs: each scheduled or manual audit creates a run
CREATE TABLE IF NOT EXISTS audit_runs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  run_type TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'running' CHECK (status IN ('running', 'completed', 'failed')),
  started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  completed_at TIMESTAMPTZ,
  stats JSONB DEFAULT '{}',
  errors JSONB DEFAULT '[]',
  trigger_source TEXT DEFAULT 'cron',
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Audit logs: individual findings per run
CREATE TABLE IF NOT EXISTS audit_logs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  run_id UUID REFERENCES audit_runs(id) ON DELETE CASCADE,
  audit_type TEXT NOT NULL,
  entity_type TEXT NOT NULL DEFAULT 'dog',
  entity_id UUID,
  severity TEXT NOT NULL DEFAULT 'info' CHECK (severity IN ('info', 'warning', 'error', 'critical')),
  message TEXT NOT NULL,
  details JSONB DEFAULT '{}',
  action_taken TEXT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Indexes for efficient querying
CREATE INDEX IF NOT EXISTS idx_audit_logs_run_id ON audit_logs(run_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_severity ON audit_logs(severity);
CREATE INDEX IF NOT EXISTS idx_audit_logs_entity ON audit_logs(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_type ON audit_logs(audit_type);
CREATE INDEX IF NOT EXISTS idx_audit_logs_created ON audit_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_runs_status ON audit_runs(status);
CREATE INDEX IF NOT EXISTS idx_audit_runs_created ON audit_runs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_dogs_date_confidence ON dogs(date_confidence);
CREATE INDEX IF NOT EXISTS idx_dogs_last_audited ON dogs(last_audited_at);

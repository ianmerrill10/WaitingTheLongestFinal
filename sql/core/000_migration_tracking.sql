-- Migration Version Tracking
-- Tracks which migrations have been applied to the database
-- This should be the FIRST migration run

CREATE TABLE IF NOT EXISTS schema_migrations (
    id SERIAL PRIMARY KEY,
    version VARCHAR(50) NOT NULL UNIQUE,
    filename VARCHAR(255) NOT NULL,
    description TEXT,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    applied_by VARCHAR(100) DEFAULT current_user,
    checksum VARCHAR(64),
    execution_time_ms INTEGER
);

CREATE INDEX IF NOT EXISTS idx_schema_migrations_version ON schema_migrations(version);

-- Record this migration
INSERT INTO schema_migrations (version, filename, description)
VALUES ('000', '000_migration_tracking.sql', 'Migration version tracking table')
ON CONFLICT (version) DO NOTHING;

-- Function to record a migration
CREATE OR REPLACE FUNCTION record_migration(
    p_version VARCHAR(50),
    p_filename VARCHAR(255),
    p_description TEXT DEFAULT NULL,
    p_execution_time_ms INTEGER DEFAULT NULL
) RETURNS VOID AS $$
BEGIN
    INSERT INTO schema_migrations (version, filename, description, execution_time_ms)
    VALUES (p_version, p_filename, p_description, p_execution_time_ms)
    ON CONFLICT (version) DO UPDATE
    SET applied_at = NOW(), execution_time_ms = p_execution_time_ms;
END;
$$ LANGUAGE plpgsql;

-- Function to check if a migration has been applied
CREATE OR REPLACE FUNCTION is_migration_applied(p_version VARCHAR(50))
RETURNS BOOLEAN AS $$
BEGIN
    RETURN EXISTS (SELECT 1 FROM schema_migrations WHERE version = p_version);
END;
$$ LANGUAGE plpgsql;

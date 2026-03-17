-- ============================================================================
-- Migration: 001_extensions.sql
-- Purpose: Enable required PostgreSQL extensions for the WaitingTheLongest platform
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES: None (initial migration)
--
-- EXTENSIONS ENABLED:
--   uuid-ossp    - UUID generation functions
--   pg_trgm      - Trigram text search for fuzzy matching
--   pgcrypto     - Cryptographic functions for password hashing
--
-- ROLLBACK: DROP EXTENSION IF EXISTS pgcrypto, pg_trgm, "uuid-ossp";
-- ============================================================================

-- ============================================================================
-- UUID GENERATION
-- Purpose: Provides uuid_generate_v4() and gen_random_uuid() for primary keys
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

COMMENT ON EXTENSION "uuid-ossp" IS 'Functions to generate universally unique identifiers (UUIDs)';

-- ============================================================================
-- TRIGRAM TEXT SEARCH
-- Purpose: Enables fuzzy text search for dog names, breeds, descriptions
-- Used for: Autocomplete, "did you mean?" suggestions, typo-tolerant search
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS pg_trgm;

COMMENT ON EXTENSION pg_trgm IS 'Text similarity measurement and index searching based on trigrams';

-- ============================================================================
-- CRYPTOGRAPHIC FUNCTIONS
-- Purpose: Provides crypt() and gen_salt() for secure password hashing
-- Used for: User password storage with bcrypt
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS pgcrypto;

COMMENT ON EXTENSION pgcrypto IS 'Cryptographic functions including password hashing';

-- ============================================================================
-- CUBE + EARTH DISTANCE
-- Purpose: Provides ll_to_earth(), earth_distance() for geo queries
-- Used for: Finding nearby shelters, housing, transport routes
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS cube;
CREATE EXTENSION IF NOT EXISTS earthdistance;

COMMENT ON EXTENSION cube IS 'Data type for multidimensional cubes';
COMMENT ON EXTENSION earthdistance IS 'Calculate great-circle distances on the surface of the Earth';

-- ============================================================================
-- PG_STAT_STATEMENTS
-- Purpose: Query performance monitoring and slow query analysis
-- Used for: Identifying slow queries, optimizing database performance
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS pg_stat_statements;

COMMENT ON EXTENSION pg_stat_statements IS 'Track execution statistics of all SQL statements';

-- ============================================================================
-- AUTOMATIC TIMESTAMP TRIGGER FUNCTION
-- Purpose: Automatically update updated_at column on row modification
-- Usage: CREATE TRIGGER ... BEFORE UPDATE ON table EXECUTE FUNCTION update_updated_at();
-- ============================================================================
CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- VERIFICATION
-- Run this query to confirm extensions are installed:
-- SELECT extname, extversion FROM pg_extension WHERE extname IN ('uuid-ossp', 'pg_trgm', 'pgcrypto', 'pg_stat_statements');
-- ============================================================================

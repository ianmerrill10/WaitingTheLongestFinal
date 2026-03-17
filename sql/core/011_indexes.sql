-- ============================================================================
-- Migration: 011_indexes.sql
-- Purpose: Additional performance and text search indexes
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (pg_trgm for text search)
--   002_states.sql through 010_debug_schema.sql
--
-- NOTE: Many indexes are created inline with tables. This file contains:
--   1. Complex composite indexes for common query patterns
--   2. Full-text search configurations
--   3. Additional trigram indexes for fuzzy search
--   4. Partial indexes for performance optimization
--
-- ROLLBACK: See individual DROP INDEX statements
-- ============================================================================

-- ============================================================================
-- FULL-TEXT SEARCH CONFIGURATION
-- ============================================================================
-- Create custom text search configuration for dog descriptions
-- This helps with searching descriptions that include breed-specific terms

-- Note: Using default 'english' configuration for now
-- Custom configuration can be added later for breed-specific dictionaries

-- ============================================================================
-- DOGS TABLE - ADVANCED INDEXES
-- ============================================================================

-- Composite index for the main search query pattern:
-- "Available dogs in state X, sorted by wait time"
CREATE INDEX IF NOT EXISTS idx_dogs_search_by_state ON dogs(
    is_available,
    intake_date
) INCLUDE (shelter_id)
WHERE is_available = TRUE;

COMMENT ON INDEX idx_dogs_search_by_state IS 'Optimizes: Available dogs sorted by wait time, includes shelter_id for joining';

-- Composite index for urgency-based queries:
-- "Critical/High urgency dogs that are available"
CREATE INDEX IF NOT EXISTS idx_dogs_urgent_available ON dogs(
    urgency_level,
    euthanasia_date,
    is_available
)
WHERE is_available = TRUE AND urgency_level IN ('critical', 'high');

COMMENT ON INDEX idx_dogs_urgent_available IS 'Optimizes: Finding urgent dogs for emergency alerts';

-- Composite index for breed search:
-- "Available dogs of breed X"
CREATE INDEX IF NOT EXISTS idx_dogs_breed_search ON dogs(
    breed_primary,
    is_available,
    intake_date DESC
)
WHERE is_available = TRUE;

COMMENT ON INDEX idx_dogs_breed_search IS 'Optimizes: Searching by breed with wait time sort';

-- Composite index for size + age filter (common combination):
CREATE INDEX IF NOT EXISTS idx_dogs_size_age ON dogs(
    size,
    age_category,
    is_available
)
WHERE is_available = TRUE;

COMMENT ON INDEX idx_dogs_size_age IS 'Optimizes: Filtering by size and age category';

-- GIN index for tags array search
CREATE INDEX IF NOT EXISTS idx_dogs_tags ON dogs USING gin(tags)
WHERE tags IS NOT NULL AND array_length(tags, 1) > 0;

COMMENT ON INDEX idx_dogs_tags IS 'Optimizes: Searching by tags (good_with_kids, house_trained, etc.)';

-- Combined trigram index for name + breed fuzzy search
CREATE INDEX IF NOT EXISTS idx_dogs_name_breed_combined_trgm ON dogs
USING gin((name || ' ' || breed_primary) gin_trgm_ops)
WHERE is_available = TRUE;

COMMENT ON INDEX idx_dogs_name_breed_combined_trgm IS 'Optimizes: Fuzzy search across name and breed';

-- Partial index for dogs on euthanasia list (critical for life-saving)
CREATE INDEX IF NOT EXISTS idx_dogs_euthanasia_list_urgent ON dogs(
    euthanasia_date ASC NULLS LAST,
    shelter_id
)
WHERE is_on_euthanasia_list = TRUE AND is_available = TRUE;

COMMENT ON INDEX idx_dogs_euthanasia_list_urgent IS 'Optimizes: Finding dogs on euthanasia list sorted by date';

-- Index for external sync operations
CREATE INDEX IF NOT EXISTS idx_dogs_sync_status ON dogs(
    external_source,
    last_synced_at
)
WHERE external_id IS NOT NULL;

COMMENT ON INDEX idx_dogs_sync_status IS 'Optimizes: Finding dogs that need to be synced from external sources';

-- Index for popularity/engagement metrics
CREATE INDEX IF NOT EXISTS idx_dogs_popular ON dogs(
    view_count DESC,
    favorite_count DESC
)
WHERE is_available = TRUE;

COMMENT ON INDEX idx_dogs_popular IS 'Optimizes: Finding most popular/viewed dogs';

-- ============================================================================
-- SHELTERS TABLE - ADVANCED INDEXES
-- ============================================================================

-- Composite index for geographic search with kill shelter filter
CREATE INDEX IF NOT EXISTS idx_shelters_geo_kill ON shelters(
    state_code,
    is_kill_shelter,
    is_active
)
WHERE is_active = TRUE;

COMMENT ON INDEX idx_shelters_geo_kill IS 'Optimizes: Finding active kill shelters by state';

-- Index for shelters with urgent dogs
CREATE INDEX IF NOT EXISTS idx_shelters_urgent_capacity ON shelters(
    urgency_multiplier DESC,
    dog_count DESC
)
WHERE is_active = TRUE AND is_kill_shelter = TRUE;

COMMENT ON INDEX idx_shelters_urgent_capacity IS 'Optimizes: Finding high-urgency shelters with dogs';

-- ============================================================================
-- USERS TABLE - ADVANCED INDEXES
-- ============================================================================

-- Index for notification queries
CREATE INDEX IF NOT EXISTS idx_users_notification_enabled ON users(
    is_active,
    email_verified
)
WHERE is_active = TRUE AND email_verified = TRUE;

COMMENT ON INDEX idx_users_notification_enabled IS 'Optimizes: Finding users eligible for notifications';

-- GIN index for searching within notification preferences
CREATE INDEX IF NOT EXISTS idx_users_notif_prefs ON users
USING gin(notification_preferences);

COMMENT ON INDEX idx_users_notif_prefs IS 'Optimizes: Querying notification preferences JSONB';

-- GIN index for searching within search preferences
CREATE INDEX IF NOT EXISTS idx_users_search_prefs ON users
USING gin(search_preferences);

COMMENT ON INDEX idx_users_search_prefs IS 'Optimizes: Querying search preferences JSONB';

-- ============================================================================
-- FOSTER_HOMES TABLE - ADVANCED INDEXES
-- ============================================================================

-- Composite index for foster matching
CREATE INDEX IF NOT EXISTS idx_foster_matching ON foster_homes(
    is_available,
    is_verified,
    is_active,
    max_dogs,
    current_dogs
)
WHERE is_available = TRUE AND is_verified = TRUE AND is_active = TRUE;

COMMENT ON INDEX idx_foster_matching IS 'Optimizes: Finding available verified foster homes with capacity';

-- Index for foster homes willing to take special needs
CREATE INDEX IF NOT EXISTS idx_foster_special_needs ON foster_homes(
    zip_code,
    max_transport_miles
)
WHERE is_available = TRUE AND is_verified = TRUE AND ok_with_special_needs = TRUE;

COMMENT ON INDEX idx_foster_special_needs IS 'Optimizes: Finding foster homes for special needs dogs';

-- Index for urgent foster matching (will take medical/behavioral)
CREATE INDEX IF NOT EXISTS idx_foster_urgent ON foster_homes(
    state_code,
    ok_with_medical,
    ok_with_behavioral
)
WHERE is_available = TRUE AND is_verified = TRUE AND is_active = TRUE;

COMMENT ON INDEX idx_foster_urgent IS 'Optimizes: Finding fosters for urgent/medical/behavioral dogs';

-- ============================================================================
-- FOSTER_PLACEMENTS TABLE - ADVANCED INDEXES
-- ============================================================================

-- Index for tracking active placements by shelter
CREATE INDEX IF NOT EXISTS idx_placements_shelter_active ON foster_placements(
    shelter_id,
    status
)
WHERE status = 'active';

COMMENT ON INDEX idx_placements_shelter_active IS 'Optimizes: Finding active placements by shelter';

-- Index for follow-up scheduling
CREATE INDEX IF NOT EXISTS idx_placements_followup ON foster_placements(
    next_checkin_date,
    status
)
WHERE status = 'active' AND next_checkin_date IS NOT NULL;

COMMENT ON INDEX idx_placements_followup IS 'Optimizes: Finding placements due for check-in';

-- Index for outcome analysis
CREATE INDEX IF NOT EXISTS idx_placements_outcome_analysis ON foster_placements(
    outcome,
    shelter_id,
    actual_end_date
)
WHERE outcome IS NOT NULL;

COMMENT ON INDEX idx_placements_outcome_analysis IS 'Optimizes: Analyzing foster outcomes by shelter';

-- ============================================================================
-- FAVORITES TABLE - ADVANCED INDEXES
-- ============================================================================

-- Index for user's favorite dogs with notification preferences
CREATE INDEX IF NOT EXISTS idx_favorites_notify ON favorites(
    dog_id,
    notify_on_status_change
)
WHERE notify_on_status_change = TRUE;

COMMENT ON INDEX idx_favorites_notify IS 'Optimizes: Finding users to notify when dog status changes';

-- ============================================================================
-- SESSIONS TABLE - ADVANCED INDEXES
-- ============================================================================

-- Index for session validation (most frequent operation)
CREATE INDEX IF NOT EXISTS idx_sessions_validate ON sessions(
    access_token,
    is_active,
    access_token_expires_at
)
WHERE is_active = TRUE;

COMMENT ON INDEX idx_sessions_validate IS 'Optimizes: Token validation (most frequent auth operation)';

-- ============================================================================
-- CROSS-TABLE QUERY OPTIMIZATIONS
-- ============================================================================

-- Create materialized view indexes would go here if needed
-- For now, we rely on the view definitions in 012_views.sql

-- ============================================================================
-- MAINTENANCE NOTES
-- ============================================================================

/*
INDEX MAINTENANCE RECOMMENDATIONS:

1. Run ANALYZE after bulk data imports:
   ANALYZE dogs;
   ANALYZE shelters;

2. Monitor index usage with:
   SELECT schemaname, tablename, indexname, idx_scan, idx_tup_read
   FROM pg_stat_user_indexes
   ORDER BY idx_scan DESC;

3. Find unused indexes:
   SELECT schemaname, tablename, indexname
   FROM pg_stat_user_indexes
   WHERE idx_scan = 0;

4. Reindex periodically (during low traffic):
   REINDEX TABLE dogs;

5. Monitor bloat with:
   SELECT tablename, pg_size_pretty(pg_total_relation_size(tablename::text))
   FROM pg_tables WHERE schemaname = 'public';
*/

-- ============================================================================
-- VERIFICATION QUERY
-- List all indexes created by this migration system
-- ============================================================================
/*
SELECT
    schemaname,
    tablename,
    indexname,
    indexdef
FROM pg_indexes
WHERE schemaname = 'public'
ORDER BY tablename, indexname;
*/

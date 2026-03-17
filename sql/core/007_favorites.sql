-- ============================================================================
-- Migration: 007_favorites.sql
-- Purpose: Create favorites table for user-dog favorite relationships
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--   004_dogs.sql (dogs table for FK)
--   005_users.sql (users table for FK)
--
-- ROLLBACK: DROP TABLE IF EXISTS favorites CASCADE;
-- ============================================================================

-- ============================================================================
-- FAVORITES TABLE
-- Purpose: Track which dogs users have favorited/saved
-- Used for:
--   1. User's saved dogs list
--   2. Favorite count on dog profiles
--   3. Notifications when favorited dogs have updates
-- ============================================================================
CREATE TABLE favorites (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- RELATIONSHIPS
    -- ========================================================================
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,

    -- ========================================================================
    -- METADATA
    -- ========================================================================
    -- User notes about this dog
    notes TEXT,

    -- Notification preferences for this favorite
    notify_on_status_change BOOLEAN DEFAULT TRUE,    -- When status changes (adopted, etc.)
    notify_on_price_change BOOLEAN DEFAULT TRUE,     -- When adoption fee changes
    notify_on_urgency_change BOOLEAN DEFAULT TRUE,   -- When urgency increases

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE favorites IS 'User-dog favorite relationships. Tracks which dogs users have saved to their favorites list.';

COMMENT ON COLUMN favorites.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN favorites.user_id IS 'Foreign key to users table - who favorited';
COMMENT ON COLUMN favorites.dog_id IS 'Foreign key to dogs table - which dog was favorited';
COMMENT ON COLUMN favorites.notes IS 'User private notes about this dog';
COMMENT ON COLUMN favorites.notify_on_status_change IS 'Notify user when dog status changes';
COMMENT ON COLUMN favorites.notify_on_price_change IS 'Notify user when adoption fee changes';
COMMENT ON COLUMN favorites.notify_on_urgency_change IS 'Notify user when dog urgency level increases';
COMMENT ON COLUMN favorites.created_at IS 'When the favorite was added';
COMMENT ON COLUMN favorites.updated_at IS 'Last update timestamp';

-- ============================================================================
-- UNIQUE CONSTRAINT
-- A user can only favorite a dog once
-- ============================================================================
ALTER TABLE favorites ADD CONSTRAINT favorites_user_dog_unique
    UNIQUE (user_id, dog_id);

-- ============================================================================
-- INDEXES
-- ============================================================================
-- User's favorites list
CREATE INDEX idx_favorites_user_id ON favorites(user_id);

-- Dog's favorited count
CREATE INDEX idx_favorites_dog_id ON favorites(dog_id);

-- Recent favorites
CREATE INDEX idx_favorites_created_at ON favorites(created_at DESC);

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_favorites_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_favorites_updated_at
    BEFORE UPDATE ON favorites
    FOR EACH ROW
    EXECUTE FUNCTION update_favorites_updated_at();

-- ============================================================================
-- TRIGGER: Update dog's favorite_count when favorites change
-- ============================================================================
CREATE OR REPLACE FUNCTION update_dog_favorite_count()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        UPDATE dogs
        SET favorite_count = favorite_count + 1,
            last_favorited_at = NOW()
        WHERE id = NEW.dog_id;
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        UPDATE dogs
        SET favorite_count = GREATEST(0, favorite_count - 1)
        WHERE id = OLD.dog_id;
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_favorites_update_dog_count
    AFTER INSERT OR DELETE ON favorites
    FOR EACH ROW
    EXECUTE FUNCTION update_dog_favorite_count();

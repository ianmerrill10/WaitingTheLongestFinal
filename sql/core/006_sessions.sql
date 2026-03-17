-- ============================================================================
-- Migration: 006_sessions.sql
-- Purpose: Create sessions table for user session/token management
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--   005_users.sql (users table for FK)
--
-- MODEL REFERENCE: AuthToken from INTEGRATION_CONTRACTS.md
--
-- ROLLBACK: DROP TABLE IF EXISTS sessions CASCADE;
-- ============================================================================

-- ============================================================================
-- SESSIONS TABLE
-- Purpose: Store active user sessions and authentication tokens
-- Supports both JWT-style tokens and traditional session management
-- ============================================================================
CREATE TABLE sessions (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- USER REFERENCE
    -- ========================================================================
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- ========================================================================
    -- TOKEN MANAGEMENT
    -- ========================================================================
    -- Access token (short-lived)
    access_token VARCHAR(500) NOT NULL UNIQUE,
    access_token_expires_at TIMESTAMP WITH TIME ZONE NOT NULL,

    -- Refresh token (long-lived, for getting new access tokens)
    refresh_token VARCHAR(500) UNIQUE,
    refresh_token_expires_at TIMESTAMP WITH TIME ZONE,

    -- Token metadata
    token_type VARCHAR(20) DEFAULT 'bearer',         -- bearer, api_key, etc.

    -- ========================================================================
    -- SESSION CONTEXT
    -- ========================================================================
    -- Device/client information
    user_agent TEXT,                                 -- Browser/app user agent
    ip_address VARCHAR(45),                          -- IPv4 or IPv6
    device_id VARCHAR(255),                          -- For mobile apps
    device_type VARCHAR(50),                         -- web, ios, android

    -- Location (optional, for security)
    login_location VARCHAR(255),                     -- City, Region, Country

    -- ========================================================================
    -- STATUS
    -- ========================================================================
    is_active BOOLEAN DEFAULT TRUE,
    revoked_at TIMESTAMP WITH TIME ZONE,
    revoked_reason VARCHAR(100),                     -- logout, password_change, admin_action, expired

    -- ========================================================================
    -- ACTIVITY TRACKING
    -- ========================================================================
    last_activity_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    activity_count INTEGER DEFAULT 0,

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE sessions IS 'User sessions and authentication tokens. Supports token-based auth with access/refresh token pairs.';

COMMENT ON COLUMN sessions.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN sessions.user_id IS 'Foreign key to users table';
COMMENT ON COLUMN sessions.access_token IS 'Short-lived access token for API requests';
COMMENT ON COLUMN sessions.access_token_expires_at IS 'When access token expires (typically 15-60 minutes)';
COMMENT ON COLUMN sessions.refresh_token IS 'Long-lived refresh token for getting new access tokens';
COMMENT ON COLUMN sessions.refresh_token_expires_at IS 'When refresh token expires (typically 7-30 days)';
COMMENT ON COLUMN sessions.token_type IS 'Token type: bearer (default), api_key';
COMMENT ON COLUMN sessions.user_agent IS 'Browser/app user agent string';
COMMENT ON COLUMN sessions.ip_address IS 'Client IP address at login';
COMMENT ON COLUMN sessions.device_id IS 'Unique device identifier (for mobile apps)';
COMMENT ON COLUMN sessions.device_type IS 'Device type: web, ios, android';
COMMENT ON COLUMN sessions.login_location IS 'Approximate location at login (for security alerts)';
COMMENT ON COLUMN sessions.is_active IS 'FALSE if session has been revoked';
COMMENT ON COLUMN sessions.revoked_at IS 'When session was revoked';
COMMENT ON COLUMN sessions.revoked_reason IS 'Why session was revoked: logout, password_change, admin_action, expired';
COMMENT ON COLUMN sessions.last_activity_at IS 'Last API activity timestamp';
COMMENT ON COLUMN sessions.activity_count IS 'Number of API requests in this session';
COMMENT ON COLUMN sessions.created_at IS 'Session creation timestamp (login time)';
COMMENT ON COLUMN sessions.updated_at IS 'Last update timestamp';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Token lookups (most common operation)
CREATE INDEX idx_sessions_access_token ON sessions(access_token)
    WHERE is_active = TRUE;
CREATE INDEX idx_sessions_refresh_token ON sessions(refresh_token)
    WHERE is_active = TRUE AND refresh_token IS NOT NULL;

-- User's active sessions
CREATE INDEX idx_sessions_user_active ON sessions(user_id, is_active)
    WHERE is_active = TRUE;

-- Expired session cleanup
CREATE INDEX idx_sessions_expires_at ON sessions(access_token_expires_at)
    WHERE is_active = TRUE;

-- Device tracking
CREATE INDEX idx_sessions_device_id ON sessions(device_id)
    WHERE device_id IS NOT NULL;

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_sessions_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_sessions_updated_at
    BEFORE UPDATE ON sessions
    FOR EACH ROW
    EXECUTE FUNCTION update_sessions_updated_at();

-- ============================================================================
-- FUNCTION: Revoke session
-- Usage: SELECT revoke_session(session_id, 'reason');
-- ============================================================================
CREATE OR REPLACE FUNCTION revoke_session(
    p_session_id UUID,
    p_reason VARCHAR(100) DEFAULT 'logout'
)
RETURNS BOOLEAN AS $$
DECLARE
    rows_affected INTEGER;
BEGIN
    UPDATE sessions
    SET is_active = FALSE,
        revoked_at = NOW(),
        revoked_reason = p_reason
    WHERE id = p_session_id AND is_active = TRUE;

    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected > 0;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION revoke_session IS 'Revoke a specific session by ID. Returns TRUE if session was active and is now revoked.';

-- ============================================================================
-- FUNCTION: Revoke all user sessions
-- Usage: SELECT revoke_all_user_sessions(user_id, 'reason');
-- ============================================================================
CREATE OR REPLACE FUNCTION revoke_all_user_sessions(
    p_user_id UUID,
    p_reason VARCHAR(100) DEFAULT 'password_change'
)
RETURNS INTEGER AS $$
DECLARE
    rows_affected INTEGER;
BEGIN
    UPDATE sessions
    SET is_active = FALSE,
        revoked_at = NOW(),
        revoked_reason = p_reason
    WHERE user_id = p_user_id AND is_active = TRUE;

    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION revoke_all_user_sessions IS 'Revoke all active sessions for a user. Use after password change. Returns count of revoked sessions.';

-- ============================================================================
-- FUNCTION: Cleanup expired sessions
-- Usage: SELECT cleanup_expired_sessions();
-- Called periodically by a cron job or worker
-- ============================================================================
CREATE OR REPLACE FUNCTION cleanup_expired_sessions()
RETURNS INTEGER AS $$
DECLARE
    rows_deleted INTEGER;
BEGIN
    -- Delete sessions that have been inactive for 30 days
    -- or were revoked more than 7 days ago
    DELETE FROM sessions
    WHERE (is_active = FALSE AND revoked_at < NOW() - INTERVAL '7 days')
       OR (is_active = TRUE AND access_token_expires_at < NOW() - INTERVAL '30 days');

    GET DIAGNOSTICS rows_deleted = ROW_COUNT;
    RETURN rows_deleted;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION cleanup_expired_sessions IS 'Remove old expired and revoked sessions. Call periodically to keep table size manageable.';

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid token types
ALTER TABLE sessions ADD CONSTRAINT sessions_token_type_valid
    CHECK (token_type IN ('bearer', 'api_key'));

-- Valid device types
ALTER TABLE sessions ADD CONSTRAINT sessions_device_type_valid
    CHECK (device_type IS NULL OR device_type IN ('web', 'ios', 'android', 'api'));

-- Valid revoked reasons
ALTER TABLE sessions ADD CONSTRAINT sessions_revoked_reason_valid
    CHECK (revoked_reason IS NULL OR revoked_reason IN ('logout', 'password_change', 'admin_action', 'expired', 'security', 'user_request'));

-- Activity count non-negative
ALTER TABLE sessions ADD CONSTRAINT sessions_activity_count_valid
    CHECK (activity_count >= 0);

-- Expiration dates must be in the future at creation
-- Note: This is validated at application level

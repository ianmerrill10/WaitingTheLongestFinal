-- ============================================================================
-- Migration: 005_users.sql
-- Purpose: Create users table for user account management
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp, pgcrypto)
--   003_shelters.sql (shelters table for shelter_admin FK)
--
-- MODEL REFERENCE: wtl::core::models::User from INTEGRATION_CONTRACTS.md
--
-- ROLLBACK: DROP TABLE IF EXISTS users CASCADE;
-- ============================================================================

-- ============================================================================
-- USERS TABLE
-- Purpose: User accounts for adopters, fosters, shelter admins, and system admins
-- Includes role-based access control and preference storage
-- ============================================================================
CREATE TABLE users (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- AUTHENTICATION
    -- ========================================================================
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,             -- bcrypt hash via pgcrypto
    email_verified BOOLEAN DEFAULT FALSE,
    email_verification_token VARCHAR(255),
    email_verification_sent_at TIMESTAMP WITH TIME ZONE,

    -- Password reset
    password_reset_token VARCHAR(255),
    password_reset_expires_at TIMESTAMP WITH TIME ZONE,

    -- ========================================================================
    -- PROFILE INFORMATION
    -- ========================================================================
    display_name VARCHAR(100) NOT NULL,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    phone VARCHAR(20),
    zip_code VARCHAR(10),

    -- Profile photo
    avatar_url VARCHAR(500),

    -- ========================================================================
    -- ROLE & PERMISSIONS
    -- ========================================================================
    role VARCHAR(30) NOT NULL DEFAULT 'user',        -- user, foster, shelter_admin, admin, super_admin

    -- Shelter association (for shelter_admin role)
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,

    -- ========================================================================
    -- PREFERENCES (JSONB for flexibility)
    -- ========================================================================
    -- Notification preferences
    notification_preferences JSONB DEFAULT '{
        "email": {
            "enabled": true,
            "urgent_dogs": true,
            "new_matches": true,
            "favorites_updates": true,
            "weekly_digest": true,
            "foster_opportunities": false
        },
        "push": {
            "enabled": true,
            "urgent_dogs": true,
            "new_matches": true,
            "favorites_updates": true
        },
        "sms": {
            "enabled": false,
            "urgent_dogs": false,
            "critical_alerts_only": true
        },
        "quiet_hours": {
            "enabled": false,
            "start": "22:00",
            "end": "07:00"
        }
    }'::jsonb,

    -- Search preferences (saved search criteria)
    search_preferences JSONB DEFAULT '{
        "breeds": [],
        "sizes": [],
        "age_categories": [],
        "distance_miles": 50,
        "good_with_kids": null,
        "good_with_dogs": null,
        "good_with_cats": null,
        "include_special_needs": true,
        "urgency_levels": ["normal", "medium", "high", "critical"]
    }'::jsonb,

    -- ========================================================================
    -- STATUS & TRACKING
    -- ========================================================================
    is_active BOOLEAN DEFAULT TRUE,
    deactivated_at TIMESTAMP WITH TIME ZONE,
    deactivation_reason VARCHAR(255),

    -- Login tracking
    last_login_at TIMESTAMP WITH TIME ZONE,
    last_login_ip VARCHAR(45),                       -- IPv6 can be up to 45 chars
    login_count INTEGER DEFAULT 0,
    failed_login_attempts INTEGER DEFAULT 0,
    locked_until TIMESTAMP WITH TIME ZONE,

    -- Terms acceptance
    terms_accepted_at TIMESTAMP WITH TIME ZONE,
    terms_version VARCHAR(20),
    privacy_accepted_at TIMESTAMP WITH TIME ZONE,
    privacy_version VARCHAR(20),

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE users IS 'User accounts for the platform. Supports multiple roles: user (adopter), foster, shelter_admin, admin, super_admin.';

-- Authentication
COMMENT ON COLUMN users.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN users.email IS 'User email address - must be unique. Used for login.';
COMMENT ON COLUMN users.password_hash IS 'bcrypt password hash - never store plaintext passwords';
COMMENT ON COLUMN users.email_verified IS 'TRUE if user has verified their email address';
COMMENT ON COLUMN users.email_verification_token IS 'Token for email verification link';
COMMENT ON COLUMN users.email_verification_sent_at IS 'When verification email was sent';
COMMENT ON COLUMN users.password_reset_token IS 'Token for password reset link';
COMMENT ON COLUMN users.password_reset_expires_at IS 'When password reset token expires';

-- Profile
COMMENT ON COLUMN users.display_name IS 'Public display name shown in comments, favorites, etc.';
COMMENT ON COLUMN users.first_name IS 'First name (for personalization)';
COMMENT ON COLUMN users.last_name IS 'Last name (for legal documents, applications)';
COMMENT ON COLUMN users.phone IS 'Phone number (for urgent alerts if enabled)';
COMMENT ON COLUMN users.zip_code IS 'ZIP code for location-based features';
COMMENT ON COLUMN users.avatar_url IS 'URL to profile photo';

-- Role
COMMENT ON COLUMN users.role IS 'User role: user (adopter), foster (registered foster), shelter_admin, admin, super_admin';
COMMENT ON COLUMN users.shelter_id IS 'For shelter_admin role, which shelter they manage';

-- Preferences
COMMENT ON COLUMN users.notification_preferences IS 'JSONB: email/push/sms notification settings, quiet hours';
COMMENT ON COLUMN users.search_preferences IS 'JSONB: saved search filters (breeds, sizes, location radius, etc.)';

-- Status
COMMENT ON COLUMN users.is_active IS 'FALSE if account is deactivated';
COMMENT ON COLUMN users.deactivated_at IS 'When account was deactivated';
COMMENT ON COLUMN users.deactivation_reason IS 'Reason for deactivation (user_request, admin_action, etc.)';
COMMENT ON COLUMN users.last_login_at IS 'Timestamp of last successful login';
COMMENT ON COLUMN users.last_login_ip IS 'IP address of last login (for security)';
COMMENT ON COLUMN users.login_count IS 'Total number of successful logins';
COMMENT ON COLUMN users.failed_login_attempts IS 'Failed login attempts since last success - reset on success';
COMMENT ON COLUMN users.locked_until IS 'If set, account is locked until this time (brute force protection)';

-- Legal
COMMENT ON COLUMN users.terms_accepted_at IS 'When user accepted terms of service';
COMMENT ON COLUMN users.terms_version IS 'Version of terms accepted';
COMMENT ON COLUMN users.privacy_accepted_at IS 'When user accepted privacy policy';
COMMENT ON COLUMN users.privacy_version IS 'Version of privacy policy accepted';

-- Timestamps
COMMENT ON COLUMN users.created_at IS 'Account creation timestamp';
COMMENT ON COLUMN users.updated_at IS 'Last profile update timestamp';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Email lookup (login)
CREATE UNIQUE INDEX idx_users_email ON users(LOWER(email));

-- Role filtering
CREATE INDEX idx_users_role ON users(role);

-- Shelter admin lookup
CREATE INDEX idx_users_shelter_id ON users(shelter_id)
    WHERE shelter_id IS NOT NULL;

-- Active users
CREATE INDEX idx_users_is_active ON users(is_active)
    WHERE is_active = TRUE;

-- Token lookups
CREATE INDEX idx_users_email_verification_token ON users(email_verification_token)
    WHERE email_verification_token IS NOT NULL;
CREATE INDEX idx_users_password_reset_token ON users(password_reset_token)
    WHERE password_reset_token IS NOT NULL;

-- Location-based queries
CREATE INDEX idx_users_zip_code ON users(zip_code)
    WHERE zip_code IS NOT NULL;

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_users_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_users_updated_at
    BEFORE UPDATE ON users
    FOR EACH ROW
    EXECUTE FUNCTION update_users_updated_at();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid role values
ALTER TABLE users ADD CONSTRAINT users_role_valid
    CHECK (role IN ('user', 'foster', 'shelter_admin', 'admin', 'super_admin'));

-- Shelter ID required for shelter_admin role
-- Note: This is enforced at application level to allow role changes

-- Valid email format
ALTER TABLE users ADD CONSTRAINT users_email_valid
    CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

-- Non-negative counters
ALTER TABLE users ADD CONSTRAINT users_login_count_valid
    CHECK (login_count >= 0);
ALTER TABLE users ADD CONSTRAINT users_failed_login_attempts_valid
    CHECK (failed_login_attempts >= 0);

-- ============================================================================
-- HELPER FUNCTION: Hash password
-- Usage: SELECT hash_password('plain_text_password');
-- ============================================================================
CREATE OR REPLACE FUNCTION hash_password(password TEXT)
RETURNS TEXT AS $$
BEGIN
    RETURN crypt(password, gen_salt('bf', 10));
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

COMMENT ON FUNCTION hash_password IS 'Hash a password using bcrypt with cost factor 10. Use for creating/updating passwords.';

-- ============================================================================
-- HELPER FUNCTION: Verify password
-- Usage: SELECT verify_password('plain_text_password', password_hash);
-- ============================================================================
CREATE OR REPLACE FUNCTION verify_password(password TEXT, hash TEXT)
RETURNS BOOLEAN AS $$
BEGIN
    RETURN crypt(password, hash) = hash;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

COMMENT ON FUNCTION verify_password IS 'Verify a password against a bcrypt hash. Returns TRUE if match.';

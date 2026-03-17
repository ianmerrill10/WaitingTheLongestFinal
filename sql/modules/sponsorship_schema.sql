-- ============================================================================
-- Migration: sponsorship_schema.sql
-- Purpose: Create dog sponsorship and corporate sponsor tables
-- Author: Agent - WaitingTheLongest Build System
-- Created: 2024-01-29
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (users table)
-- - 010_dogs.sql (dogs table)
-- - 015_states.sql (states table for state_code)
--
-- TABLES CREATED:
--   - sponsors: Individual and anonymous sponsors
--   - sponsorships: Individual sponsorship records
--   - corporate_sponsors: Corporate/business sponsors
--
-- ROLLBACK: DROP TABLE sponsorships CASCADE; DROP TABLE sponsors CASCADE; DROP TABLE corporate_sponsors CASCADE;
-- ============================================================================

-- ============================================================================
-- SPONSORS TABLE
-- Purpose: Store individual sponsor information (registered users and anonymous)
-- ============================================================================
CREATE TABLE IF NOT EXISTS sponsors (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- User Association (nullable for anonymous sponsors)
    user_id UUID UNIQUE,                         -- FK to users (NULL for anonymous)
        CONSTRAINT fk_sponsors_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,

    -- Sponsor Information
    name VARCHAR(255) NOT NULL,                  -- Sponsor's name
    email VARCHAR(255) NOT NULL,                 -- Sponsor's email (required for contact)
    is_anonymous BOOLEAN DEFAULT FALSE,          -- Whether sponsor wishes to remain anonymous

    -- Display Information
    display_name VARCHAR(255),                   -- Name to display publicly (different from name if anonymous)
    logo_url VARCHAR(500),                       -- Optional sponsor logo/image URL

    -- Statistics
    total_sponsored DECIMAL(12,2) DEFAULT 0,     -- Total amount sponsored ($)
    dogs_sponsored INTEGER DEFAULT 0,            -- Number of dogs sponsored

    -- Metadata
    phone VARCHAR(20),                           -- Optional contact phone
    preferred_contact VARCHAR(50),               -- email, phone, both, none
    communication_preferences TEXT[] DEFAULT '{}', -- Array: newsletters, updates, annual_report

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE sponsors IS 'Individual sponsors for dog adoptions (registered users and anonymous)';
COMMENT ON COLUMN sponsors.user_id IS 'Reference to user (NULL for anonymous sponsors)';
COMMENT ON COLUMN sponsors.is_anonymous IS 'If true, display_name should be used instead of name';
COMMENT ON COLUMN sponsors.total_sponsored IS 'Cumulative sponsorship amount in dollars';
COMMENT ON COLUMN sponsors.dogs_sponsored IS 'Number of unique dogs sponsored by this sponsor';
COMMENT ON COLUMN sponsors.communication_preferences IS 'Array of preferences: newsletters, updates, annual_report';

-- Indexes for sponsor lookups
CREATE INDEX IF NOT EXISTS idx_sponsors_user_id ON sponsors(user_id) WHERE user_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_sponsors_email ON sponsors(email);
CREATE INDEX IF NOT EXISTS idx_sponsors_is_anonymous ON sponsors(is_anonymous);
CREATE INDEX IF NOT EXISTS idx_sponsors_created_at ON sponsors(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_sponsors_total_sponsored ON sponsors(total_sponsored DESC);

-- ============================================================================
-- SPONSORSHIPS TABLE
-- Purpose: Individual sponsorship transactions linking sponsors to dogs
-- ============================================================================
CREATE TABLE IF NOT EXISTS sponsorships (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Foreign Keys
    sponsor_id UUID NOT NULL,                    -- FK to sponsors
        CONSTRAINT fk_sponsorships_sponsor
        FOREIGN KEY (sponsor_id) REFERENCES sponsors(id) ON DELETE CASCADE,
    dog_id UUID NOT NULL,                        -- FK to dogs
        CONSTRAINT fk_sponsorships_dog
        FOREIGN KEY (dog_id) REFERENCES dogs(id) ON DELETE CASCADE,

    -- Optional: Reference to adopter who used this sponsorship
    adopter_id UUID,                             -- FK to users (adopter who received sponsorship)
        CONSTRAINT fk_sponsorships_adopter
        FOREIGN KEY (adopter_id) REFERENCES users(id) ON DELETE SET NULL,

    -- Sponsorship Details
    amount DECIMAL(10,2) NOT NULL,               -- Sponsorship amount ($)
        CHECK (amount > 0),
    covers_full_fee BOOLEAN DEFAULT FALSE,       -- Whether this sponsorship covers full adoption fee

    -- Payment Information
    payment_status VARCHAR(50) NOT NULL          -- pending, completed, failed, refunded
        DEFAULT 'pending'
        CHECK (payment_status IN ('pending', 'completed', 'failed', 'refunded')),
    payment_id VARCHAR(255),                     -- External payment processor ID (Stripe, PayPal, etc.)
    payment_processor VARCHAR(50),               -- Which processor: stripe, paypal, manual, etc.

    -- Sponsorship Status
    status VARCHAR(50) NOT NULL DEFAULT 'active' -- active, used, inactive, canceled
        CHECK (status IN ('active', 'used', 'inactive', 'canceled')),

    -- Usage Tracking
    used_at TIMESTAMP WITH TIME ZONE,            -- When sponsorship was used/applied to adoption
    used_by_adopter_id UUID,                     -- Which adopter used this sponsorship
        CONSTRAINT fk_sponsorships_used_by_adopter
        FOREIGN KEY (used_by_adopter_id) REFERENCES users(id) ON DELETE SET NULL,

    -- Communication
    message TEXT,                                -- Optional message from sponsor to adopter

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE sponsorships IS 'Individual sponsorship transactions linking sponsors to dogs';
COMMENT ON COLUMN sponsorships.sponsor_id IS 'Sponsor providing the sponsorship';
COMMENT ON COLUMN sponsorships.dog_id IS 'Dog being sponsored';
COMMENT ON COLUMN sponsorships.adopter_id IS 'Adopter who ultimately adopted the dog (if used)';
COMMENT ON COLUMN sponsorships.amount IS 'Dollar amount of sponsorship';
COMMENT ON COLUMN sponsorships.covers_full_fee IS 'Whether this covers the complete adoption fee';
COMMENT ON COLUMN sponsorships.payment_status IS 'Payment processing status (pending/completed/failed/refunded)';
COMMENT ON COLUMN sponsorships.status IS 'Sponsorship status (active/used/inactive/canceled)';
COMMENT ON COLUMN sponsorships.used_at IS 'Timestamp when sponsorship was applied to adoption';
COMMENT ON COLUMN sponsorships.message IS 'Optional message from sponsor (e.g., dedication, encouragement)';

-- Indexes for sponsorship queries
CREATE INDEX IF NOT EXISTS idx_sponsorships_sponsor_id ON sponsorships(sponsor_id);
CREATE INDEX IF NOT EXISTS idx_sponsorships_dog_id ON sponsorships(dog_id);
CREATE INDEX IF NOT EXISTS idx_sponsorships_adopter_id ON sponsorships(adopter_id) WHERE adopter_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_sponsorships_status ON sponsorships(status);
CREATE INDEX IF NOT EXISTS idx_sponsorships_payment_status ON sponsorships(payment_status);

-- Composite indexes for common queries
CREATE INDEX IF NOT EXISTS idx_sponsorships_dog_status ON sponsorships(dog_id, status);
CREATE INDEX IF NOT EXISTS idx_sponsorships_sponsor_status ON sponsorships(sponsor_id, status);

-- Index for finding unused active sponsorships
CREATE INDEX IF NOT EXISTS idx_sponsorships_active_unused ON sponsorships(dog_id, status, created_at DESC)
    WHERE status = 'active' AND used_at IS NULL;

-- ============================================================================
-- CORPORATE_SPONSORS TABLE
-- Purpose: Store corporate and business sponsor information
-- ============================================================================
CREATE TABLE IF NOT EXISTS corporate_sponsors (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Company Information
    company_name VARCHAR(255) NOT NULL,          -- Corporate sponsor name
    logo_url VARCHAR(500),                       -- Company logo URL
    website VARCHAR(255),                        -- Company website
    contact_email VARCHAR(255) NOT NULL,         -- Primary contact email
    contact_name VARCHAR(255),                   -- Primary contact person name
    contact_phone VARCHAR(20),                   -- Primary contact phone

    -- Sponsorship Details
    monthly_budget DECIMAL(12,2),                -- Monthly sponsorship budget ($)
    dogs_per_month INTEGER,                      -- Target number of dogs to sponsor per month

    -- Geographic Focus
    states TEXT[] DEFAULT '{}',                  -- Array of states where they sponsor (state codes)

    -- Statistics
    total_sponsored DECIMAL(12,2) DEFAULT 0,     -- Total amount sponsored to date ($)
    total_dogs INTEGER DEFAULT 0,                -- Total number of dogs sponsored to date

    -- Status & Metadata
    is_active BOOLEAN DEFAULT TRUE,              -- Whether sponsorship is currently active
    sponsorship_start_date DATE,                 -- When sponsorship program started
    sponsorship_end_date DATE,                   -- When sponsorship program ends (if applicable)

    -- Marketing/Branding
    public_description TEXT,                     -- Public description of corporate sponsorship
    social_media_handles JSONB,                  -- JSON: {twitter, facebook, instagram, linkedin}
    is_featured BOOLEAN DEFAULT FALSE,           -- Featured corporate sponsor on website

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE corporate_sponsors IS 'Corporate and business sponsors of dog adoptions';
COMMENT ON COLUMN corporate_sponsors.monthly_budget IS 'Monthly sponsorship budget in dollars';
COMMENT ON COLUMN corporate_sponsors.dogs_per_month IS 'Target number of dogs to sponsor monthly';
COMMENT ON COLUMN corporate_sponsors.states IS 'Array of state codes where they prefer to sponsor';
COMMENT ON COLUMN corporate_sponsors.total_sponsored IS 'Cumulative sponsorship amount to date';
COMMENT ON COLUMN corporate_sponsors.social_media_handles IS 'JSON object with social media handles';

-- Indexes for corporate sponsor queries
CREATE INDEX IF NOT EXISTS idx_corporate_sponsors_company_name ON corporate_sponsors(company_name);
CREATE INDEX IF NOT EXISTS idx_corporate_sponsors_is_active ON corporate_sponsors(is_active);
CREATE INDEX IF NOT EXISTS idx_corporate_sponsors_is_featured ON corporate_sponsors(is_featured) WHERE is_featured = TRUE;
CREATE INDEX IF NOT EXISTS idx_corporate_sponsors_created_at ON corporate_sponsors(created_at DESC);

-- Array index for state searching
CREATE INDEX IF NOT EXISTS idx_corporate_sponsors_states ON corporate_sponsors USING gin(states);

-- ============================================================================
-- TRIGGER: Auto-update updated_at on sponsors
-- ============================================================================
DROP TRIGGER IF EXISTS update_sponsors_updated_at ON sponsors;
CREATE TRIGGER update_sponsors_updated_at
    BEFORE UPDATE ON sponsors
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on sponsorships
-- ============================================================================
DROP TRIGGER IF EXISTS update_sponsorships_updated_at ON sponsorships;
CREATE TRIGGER update_sponsorships_updated_at
    BEFORE UPDATE ON sponsorships
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on corporate_sponsors
-- ============================================================================
DROP TRIGGER IF EXISTS update_corporate_sponsors_updated_at ON corporate_sponsors;
CREATE TRIGGER update_corporate_sponsors_updated_at
    BEFORE UPDATE ON corporate_sponsors
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

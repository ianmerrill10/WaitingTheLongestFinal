-- ============================================================================
-- Migration: transport_schema.sql
-- Purpose: Create dog transport coordination system tables
-- Author: Agent - WaitingTheLongest Build System
-- Created: 2024-01-29
--
-- DEPENDENCIES:
-- - 001_extensions.sql (UUID generation)
-- - 005_users.sql (users table)
-- - 010_dogs.sql (dogs table)
-- - 020_shelters.sql (shelters table)
-- - 015_states.sql (states table for state_code)
--
-- TABLES CREATED:
--   - transport_volunteers: Volunteer drivers for dog transport
--   - transport_requests: Requests for dog transportation
--   - transport_legs: Individual legs of multi-leg transport routes
--
-- ROLLBACK: DROP TABLE transport_legs CASCADE; DROP TABLE transport_requests CASCADE; DROP TABLE transport_volunteers CASCADE;
-- ============================================================================

-- ============================================================================
-- TRANSPORT_VOLUNTEERS TABLE
-- Purpose: Store information about volunteer drivers available for transport
-- ============================================================================
CREATE TABLE IF NOT EXISTS transport_volunteers (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- User Information
    user_id UUID NOT NULL UNIQUE,                -- FK to users
        CONSTRAINT fk_transport_volunteers_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Contact Information
    name VARCHAR(255) NOT NULL,                  -- Volunteer name
    email VARCHAR(255) NOT NULL,                 -- Email
    phone VARCHAR(20) NOT NULL,                  -- Phone number

    -- Location & Range
    home_state_code VARCHAR(2) NOT NULL,         -- Home state
    home_zip VARCHAR(10),                        -- Home zip code
    max_distance_miles DECIMAL(6,1),             -- Maximum distance willing to drive per trip
    willing_to_cross_state BOOLEAN DEFAULT FALSE, -- Willing to drive across state lines
    coverage_states TEXT[] DEFAULT '{}',         -- Array of states they can cover

    -- Vehicle Information
    vehicle_type VARCHAR(50),                    -- sedan, suv, truck, van, other
    can_transport_large BOOLEAN DEFAULT FALSE,   -- Can transport large/xl dogs
    has_crate BOOLEAN DEFAULT FALSE,             -- Has pet crate/carrier

    -- Availability
    available_weekdays BOOLEAN DEFAULT FALSE,    -- Available Monday-Friday
    available_weekends BOOLEAN DEFAULT FALSE,    -- Available Saturday-Sunday

    -- Statistics & Verification
    transports_completed INTEGER DEFAULT 0,      -- Number of completed transports
    miles_driven DECIMAL(10,1) DEFAULT 0,        -- Total miles driven
    is_active BOOLEAN DEFAULT TRUE,              -- Currently accepting transports

    -- Safety & Background
    background_check_date DATE,                  -- Date of background check
    background_check_status VARCHAR(50),         -- pending, passed, failed, expired
    emergency_contact_name VARCHAR(255),         -- Emergency contact name
    emergency_contact_phone VARCHAR(20),         -- Emergency contact phone

    -- Communication
    communication_preference VARCHAR(50),        -- email, sms, phone, app
    notes TEXT,                                  -- Additional notes about volunteer

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE transport_volunteers IS 'Volunteer drivers available for transporting dogs between shelters';
COMMENT ON COLUMN transport_volunteers.max_distance_miles IS 'Maximum distance willing to drive per trip';
COMMENT ON COLUMN transport_volunteers.coverage_states IS 'Array of state codes they can cover';
COMMENT ON COLUMN transport_volunteers.vehicle_type IS 'Type of vehicle: sedan, suv, truck, van, other';
COMMENT ON COLUMN transport_volunteers.background_check_status IS 'Status: pending, passed, failed, expired';

-- Indexes for volunteer queries
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_user_id ON transport_volunteers(user_id);
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_state ON transport_volunteers(home_state_code);
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_is_active ON transport_volunteers(is_active);
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_created_at ON transport_volunteers(created_at DESC);

-- Array index for state coverage
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_coverage_states ON transport_volunteers USING gin(coverage_states);

-- Index for finding available volunteers
CREATE INDEX IF NOT EXISTS idx_transport_volunteers_available ON transport_volunteers(is_active, available_weekdays)
    WHERE is_active = TRUE;

-- ============================================================================
-- TRANSPORT_REQUESTS TABLE
-- Purpose: Requests for dog transportation between locations
-- ============================================================================
CREATE TABLE IF NOT EXISTS transport_requests (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Dog & Requester
    dog_id UUID NOT NULL,                        -- FK to dogs
        CONSTRAINT fk_transport_requests_dog
        FOREIGN KEY (dog_id) REFERENCES dogs(id) ON DELETE CASCADE,
    requester_id UUID NOT NULL,                  -- FK to users (who requested transport)
        CONSTRAINT fk_transport_requests_requester
        FOREIGN KEY (requester_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Pickup Location
    pickup_shelter_id UUID,                      -- FK to shelters (if pickup from shelter)
        CONSTRAINT fk_transport_requests_pickup_shelter
        FOREIGN KEY (pickup_shelter_id) REFERENCES shelters(id) ON DELETE SET NULL,
    pickup_address VARCHAR(500),                 -- Street address for pickup
    pickup_city VARCHAR(100),                    -- City for pickup
    pickup_state_code VARCHAR(2),                -- State code for pickup
    pickup_zip VARCHAR(10),                      -- Zip for pickup
    pickup_date_time TIMESTAMP WITH TIME ZONE,   -- Specific pickup time (if known)

    -- Destination Location
    destination_address VARCHAR(500),            -- Street address for destination
    destination_city VARCHAR(100),               -- City for destination
    destination_state_code VARCHAR(2),           -- State code for destination
    destination_zip VARCHAR(10),                 -- Zip for destination
    destination_date_time TIMESTAMP WITH TIME ZONE, -- Desired arrival time (if known)

    -- Route Information
    distance_miles DECIMAL(8,2),                 -- Calculated distance for transport
    preferred_date DATE,                         -- Preferred date for transport
    flexible_dates BOOLEAN DEFAULT FALSE,        -- Whether dates are flexible
    required_arrival_by DATE,                    -- Hard deadline for arrival

    -- Assigned Transport
    volunteer_id UUID,                           -- FK to transport_volunteers (assigned driver)
        CONSTRAINT fk_transport_requests_volunteer
        FOREIGN KEY (volunteer_id) REFERENCES transport_volunteers(id) ON DELETE SET NULL,

    -- Status
    status VARCHAR(50) NOT NULL DEFAULT 'open'  -- open, assigned, in_progress, completed, canceled
        CHECK (status IN ('open', 'assigned', 'in_progress', 'completed', 'canceled')),

    -- Completion
    completed_at TIMESTAMP WITH TIME ZONE,       -- When transport was completed
    actual_arrival_time TIMESTAMP WITH TIME ZONE, -- Actual arrival time

    -- Additional Info
    notes TEXT,                                  -- Special instructions or notes
    dog_condition_notes TEXT,                    -- Notes about dog's condition
    carrier_provided BOOLEAN DEFAULT FALSE,      -- Whether WTL provided carrier
    requires_overnight BOOLEAN DEFAULT FALSE,    -- Whether overnight stop needed

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE transport_requests IS 'Requests for transporting dogs between locations';
COMMENT ON COLUMN transport_requests.dog_id IS 'Dog being transported';
COMMENT ON COLUMN transport_requests.requester_id IS 'User requesting the transport';
COMMENT ON COLUMN transport_requests.pickup_shelter_id IS 'Shelter pickup location (if applicable)';
COMMENT ON COLUMN transport_requests.distance_miles IS 'Total distance for the transport route';
COMMENT ON COLUMN transport_requests.preferred_date IS 'Preferred date (flexible if flexible_dates=true)';
COMMENT ON COLUMN transport_requests.status IS 'Status: open, assigned, in_progress, completed, canceled';

-- Indexes for request queries
CREATE INDEX IF NOT EXISTS idx_transport_requests_dog_id ON transport_requests(dog_id);
CREATE INDEX IF NOT EXISTS idx_transport_requests_requester_id ON transport_requests(requester_id);
CREATE INDEX IF NOT EXISTS idx_transport_requests_volunteer_id ON transport_requests(volunteer_id) WHERE volunteer_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_transport_requests_status ON transport_requests(status);
CREATE INDEX IF NOT EXISTS idx_transport_requests_created_at ON transport_requests(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_transport_requests_preferred_date ON transport_requests(preferred_date);

-- Index for finding open requests
CREATE INDEX IF NOT EXISTS idx_transport_requests_open ON transport_requests(created_at DESC)
    WHERE status = 'open';

-- Index for finding upcoming transports
CREATE INDEX IF NOT EXISTS idx_transport_requests_upcoming ON transport_requests(preferred_date)
    WHERE status IN ('open', 'assigned');

-- ============================================================================
-- TRANSPORT_LEGS TABLE
-- Purpose: Store individual legs of multi-leg transport routes
-- ============================================================================
CREATE TABLE IF NOT EXISTS transport_legs (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Relationships
    request_id UUID NOT NULL,                    -- FK to transport_requests
        CONSTRAINT fk_transport_legs_request
        FOREIGN KEY (request_id) REFERENCES transport_requests(id) ON DELETE CASCADE,
    volunteer_id UUID NOT NULL,                  -- FK to transport_volunteers (driver for this leg)
        CONSTRAINT fk_transport_legs_volunteer
        FOREIGN KEY (volunteer_id) REFERENCES transport_volunteers(id) ON DELETE CASCADE,

    -- Leg Information
    leg_order INTEGER NOT NULL,                  -- Order of this leg (1, 2, 3, etc.)
        CHECK (leg_order >= 1),

    pickup_location VARCHAR(500) NOT NULL,       -- Pickup location address
    dropoff_location VARCHAR(500) NOT NULL,      -- Dropoff location address
    distance_miles DECIMAL(8,2),                 -- Distance for this leg

    -- Status
    status VARCHAR(50) NOT NULL DEFAULT 'pending' -- pending, in_progress, completed, canceled
        CHECK (status IN ('pending', 'in_progress', 'completed', 'canceled')),

    -- Timing
    scheduled_pickup_time TIMESTAMP WITH TIME ZONE, -- Scheduled pickup time
    actual_pickup_time TIMESTAMP WITH TIME ZONE,    -- Actual pickup time
    scheduled_dropoff_time TIMESTAMP WITH TIME ZONE, -- Scheduled dropoff time
    actual_dropoff_time TIMESTAMP WITH TIME ZONE,   -- Actual dropoff time
    completed_at TIMESTAMP WITH TIME ZONE,          -- When leg was completed

    -- Notes
    notes TEXT,                                  -- Notes specific to this leg

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Add comments
COMMENT ON TABLE transport_legs IS 'Individual legs of multi-leg transport routes';
COMMENT ON COLUMN transport_legs.request_id IS 'Parent transport request';
COMMENT ON COLUMN transport_legs.volunteer_id IS 'Volunteer driver for this leg';
COMMENT ON COLUMN transport_legs.leg_order IS 'Order of this leg in the route (1=first, 2=second, etc.)';
COMMENT ON COLUMN transport_legs.status IS 'Status: pending, in_progress, completed, canceled';

-- Indexes for leg queries
CREATE INDEX IF NOT EXISTS idx_transport_legs_request_id ON transport_legs(request_id);
CREATE INDEX IF NOT EXISTS idx_transport_legs_volunteer_id ON transport_legs(volunteer_id);
CREATE INDEX IF NOT EXISTS idx_transport_legs_status ON transport_legs(status);
CREATE INDEX IF NOT EXISTS idx_transport_legs_created_at ON transport_legs(created_at DESC);

-- Composite index for finding pending legs for a request
CREATE INDEX IF NOT EXISTS idx_transport_legs_request_leg ON transport_legs(request_id, leg_order)
    WHERE status IN ('pending', 'in_progress');

-- ============================================================================
-- TRIGGER: Auto-update updated_at on transport_volunteers
-- ============================================================================
DROP TRIGGER IF EXISTS update_transport_volunteers_updated_at ON transport_volunteers;
CREATE TRIGGER update_transport_volunteers_updated_at
    BEFORE UPDATE ON transport_volunteers
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on transport_requests
-- ============================================================================
DROP TRIGGER IF EXISTS update_transport_requests_updated_at ON transport_requests;
CREATE TRIGGER update_transport_requests_updated_at
    BEFORE UPDATE ON transport_requests
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- TRIGGER: Auto-update updated_at on transport_legs
-- ============================================================================
DROP TRIGGER IF EXISTS update_transport_legs_updated_at ON transport_legs;
CREATE TRIGGER update_transport_legs_updated_at
    BEFORE UPDATE ON transport_legs
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- Migration: notifications_schema.sql
-- Purpose: Create notification system tables for push/email/SMS notifications
-- Author: Agent 14 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp, pg_trgm)
--   005_users.sql (users table)
--   004_dogs.sql (dogs table)
--   003_shelters.sql (shelters table)
--
-- TABLES CREATED:
--   - notification_preferences: User notification preferences
--   - device_tokens: Push notification device tokens
--   - notifications: All notifications sent
--   - notification_queue: Pending notifications for async processing
--   - urgent_alert_log: History of urgent broadcasts
--   - notification_stats: Daily aggregated statistics
--
-- ROLLBACK:
--   DROP TABLE IF EXISTS notification_stats CASCADE;
--   DROP TABLE IF EXISTS urgent_alert_log CASCADE;
--   DROP TABLE IF EXISTS notification_queue CASCADE;
--   DROP TABLE IF EXISTS notifications CASCADE;
--   DROP TABLE IF EXISTS device_tokens CASCADE;
--   DROP TABLE IF EXISTS notification_preferences CASCADE;
-- ============================================================================

-- ============================================================================
-- NOTIFICATION TYPES ENUM
-- Purpose: Define all notification types for type safety
-- ============================================================================
DO $$ BEGIN
    CREATE TYPE notification_type AS ENUM (
        'critical_alert',
        'high_urgency',
        'foster_needed_urgent',
        'perfect_match',
        'good_match',
        'dog_update',
        'foster_followup',
        'success_story',
        'new_blog_post',
        'tip_of_the_day',
        'system_alert'
    );
EXCEPTION
    WHEN duplicate_object THEN NULL;
END $$;

COMMENT ON TYPE notification_type IS 'All possible notification types in the system';

-- ============================================================================
-- NOTIFICATION PREFERENCES TABLE
-- Purpose: Store user preferences for receiving notifications
-- ============================================================================
CREATE TABLE IF NOT EXISTS notification_preferences (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- Notification Type Preferences
    critical_alerts_enabled BOOLEAN DEFAULT TRUE,     -- Cannot be disabled (life-saving)
    high_urgency_enabled BOOLEAN DEFAULT TRUE,        -- <72 hours urgent dogs
    foster_needed_enabled BOOLEAN DEFAULT TRUE,       -- Foster requests
    match_alerts_enabled BOOLEAN DEFAULT TRUE,        -- Dog matches
    dog_updates_enabled BOOLEAN DEFAULT TRUE,         -- Favorites updates
    success_stories_enabled BOOLEAN DEFAULT TRUE,     -- Adoption success stories
    tips_enabled BOOLEAN DEFAULT TRUE,                -- Tips of the day
    blog_enabled BOOLEAN DEFAULT TRUE,                -- New blog posts
    system_alerts_enabled BOOLEAN DEFAULT TRUE,       -- System notifications

    -- Channel Preferences
    push_enabled BOOLEAN DEFAULT TRUE,                -- Push notifications
    email_enabled BOOLEAN DEFAULT TRUE,               -- Email notifications
    sms_enabled BOOLEAN DEFAULT FALSE,                -- SMS (requires phone)
    email_digest_enabled BOOLEAN DEFAULT TRUE,        -- Daily/weekly digests
    digest_frequency VARCHAR(10) DEFAULT 'daily'      -- 'daily' or 'weekly'
        CHECK (digest_frequency IN ('daily', 'weekly')),

    -- Quiet Hours
    quiet_hours_enabled BOOLEAN DEFAULT FALSE,
    quiet_hours_start TIME DEFAULT '22:00',           -- Start of quiet period (local time)
    quiet_hours_end TIME DEFAULT '07:00',             -- End of quiet period
    timezone VARCHAR(50) DEFAULT 'America/New_York',  -- User's timezone

    -- Location-Based
    location_enabled BOOLEAN DEFAULT TRUE,            -- Use location for alerts
    alert_radius_miles INTEGER DEFAULT 50,            -- Radius for urgent alerts
    home_latitude DECIMAL(10, 7),                     -- Home location
    home_longitude DECIMAL(10, 7),

    -- Frequency Limits
    max_daily_notifications INTEGER DEFAULT 50,       -- Rate limit
    max_urgent_per_day INTEGER DEFAULT 10,            -- Urgent alerts limit

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique constraint
    CONSTRAINT unique_user_preferences UNIQUE (user_id)
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_notification_prefs_user ON notification_preferences(user_id);
CREATE INDEX IF NOT EXISTS idx_notification_prefs_location ON notification_preferences(home_latitude, home_longitude)
    WHERE home_latitude IS NOT NULL AND home_longitude IS NOT NULL;

-- Comments
COMMENT ON TABLE notification_preferences IS 'User preferences for notification delivery';
COMMENT ON COLUMN notification_preferences.critical_alerts_enabled IS 'Critical alerts (<24h) always enabled - life-saving';
COMMENT ON COLUMN notification_preferences.quiet_hours_enabled IS 'Respect quiet hours (does not apply to critical)';
COMMENT ON COLUMN notification_preferences.alert_radius_miles IS 'Geographic radius for urgent alerts';

-- ============================================================================
-- DEVICE TOKENS TABLE
-- Purpose: Store FCM/APNs device tokens for push notifications
-- ============================================================================
CREATE TABLE IF NOT EXISTS device_tokens (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- Token Info
    token TEXT NOT NULL,                              -- FCM/APNs token
    platform VARCHAR(20) NOT NULL                     -- ios, android, web
        CHECK (platform IN ('ios', 'android', 'web')),
    device_name VARCHAR(100),                         -- Human-readable name
    device_model VARCHAR(100),                        -- Device model

    -- Status
    is_active BOOLEAN DEFAULT TRUE,                   -- Token still valid
    last_used_at TIMESTAMP,                           -- Last push sent
    invalidated_at TIMESTAMP,                         -- When marked invalid
    invalidation_reason VARCHAR(100),                 -- Reason for invalidation

    -- FCM Topics
    subscribed_topics TEXT[] DEFAULT '{}',            -- Topics this device is subscribed to

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- Unique token per user
    CONSTRAINT unique_device_token UNIQUE (user_id, token)
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_device_tokens_user ON device_tokens(user_id);
CREATE INDEX IF NOT EXISTS idx_device_tokens_active ON device_tokens(is_active, user_id)
    WHERE is_active = TRUE;
CREATE INDEX IF NOT EXISTS idx_device_tokens_platform ON device_tokens(platform);

-- Comments
COMMENT ON TABLE device_tokens IS 'Push notification device tokens (FCM/APNs)';
COMMENT ON COLUMN device_tokens.token IS 'Firebase Cloud Messaging or APNs token';
COMMENT ON COLUMN device_tokens.invalidated_at IS 'Marked invalid when FCM reports token expired';

-- ============================================================================
-- NOTIFICATIONS TABLE
-- Purpose: Store all notifications sent to users
-- ============================================================================
CREATE TABLE IF NOT EXISTS notifications (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- Notification Content
    type notification_type NOT NULL,
    title VARCHAR(255) NOT NULL,
    body TEXT NOT NULL,
    icon VARCHAR(255),                                -- Icon URL or class
    image_url VARCHAR(500),                           -- Rich notification image
    action_url VARCHAR(500),                          -- Deep link URL
    data JSONB DEFAULT '{}',                          -- Custom payload

    -- Related Entities
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,

    -- Delivery Status
    status VARCHAR(20) DEFAULT 'pending'
        CHECK (status IN ('pending', 'sent', 'delivered', 'failed', 'expired')),
    priority INTEGER DEFAULT 5                        -- 1 = highest, 10 = lowest
        CHECK (priority BETWEEN 1 AND 10),

    -- Channel Delivery
    channels_attempted TEXT[] DEFAULT '{}',           -- Channels we tried
    channels_succeeded TEXT[] DEFAULT '{}',           -- Channels that succeeded
    push_sent_at TIMESTAMP,
    email_sent_at TIMESTAMP,
    sms_sent_at TIMESTAMP,
    websocket_sent_at TIMESTAMP,

    -- Engagement
    read_at TIMESTAMP,                                -- When user read notification
    opened_at TIMESTAMP,                              -- When notification was opened
    clicked_at TIMESTAMP,                             -- When action was clicked
    action_taken VARCHAR(100),                        -- Which action was taken
    dismissed_at TIMESTAMP,                           -- When dismissed without action

    -- Error Tracking
    error_message TEXT,                               -- Last error if failed
    retry_count INTEGER DEFAULT 0,                    -- Number of delivery attempts
    last_retry_at TIMESTAMP,

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP                              -- Auto-expire old notifications
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications(user_id);
CREATE INDEX IF NOT EXISTS idx_notifications_user_unread ON notifications(user_id, read_at)
    WHERE read_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_notifications_type ON notifications(type);
CREATE INDEX IF NOT EXISTS idx_notifications_status ON notifications(status);
CREATE INDEX IF NOT EXISTS idx_notifications_created ON notifications(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_notifications_dog ON notifications(dog_id)
    WHERE dog_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_notifications_shelter ON notifications(shelter_id)
    WHERE shelter_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_notifications_expires ON notifications(expires_at)
    WHERE expires_at IS NOT NULL AND status != 'expired';

-- Comments
COMMENT ON TABLE notifications IS 'All notifications sent to users across all channels';
COMMENT ON COLUMN notifications.type IS 'Notification type determines priority and delivery';
COMMENT ON COLUMN notifications.priority IS 'Delivery priority: 1 = critical/immediate, 10 = low';
COMMENT ON COLUMN notifications.channels_attempted IS 'Array of channels delivery was attempted on';
COMMENT ON COLUMN notifications.action_url IS 'Deep link URL when notification is tapped';

-- ============================================================================
-- NOTIFICATION QUEUE TABLE
-- Purpose: Queue for async notification processing
-- ============================================================================
CREATE TABLE IF NOT EXISTS notification_queue (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Notification Reference
    notification_id UUID NOT NULL REFERENCES notifications(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,

    -- Queue Status
    status VARCHAR(20) DEFAULT 'pending'
        CHECK (status IN ('pending', 'processing', 'completed', 'failed', 'expired')),
    scheduled_at TIMESTAMP DEFAULT NOW(),             -- When to process
    priority INTEGER DEFAULT 5                        -- Processing priority
        CHECK (priority BETWEEN 1 AND 10),

    -- Processing
    started_at TIMESTAMP,                             -- When processing began
    completed_at TIMESTAMP,                           -- When finished
    retry_count INTEGER DEFAULT 0,
    max_retries INTEGER DEFAULT 5,
    next_retry_at TIMESTAMP,                          -- For exponential backoff

    -- Error Handling
    error_message TEXT,
    error_code VARCHAR(50),

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_notification_queue_pending ON notification_queue(scheduled_at, priority)
    WHERE status = 'pending';
CREATE INDEX IF NOT EXISTS idx_notification_queue_processing ON notification_queue(started_at)
    WHERE status = 'processing';
CREATE INDEX IF NOT EXISTS idx_notification_queue_retry ON notification_queue(next_retry_at)
    WHERE status = 'failed' AND retry_count < max_retries;
CREATE INDEX IF NOT EXISTS idx_notification_queue_user ON notification_queue(user_id);

-- Comments
COMMENT ON TABLE notification_queue IS 'Queue for async notification delivery with retry support';
COMMENT ON COLUMN notification_queue.scheduled_at IS 'When notification should be processed';
COMMENT ON COLUMN notification_queue.next_retry_at IS 'Exponential backoff: when to retry failed delivery';

-- ============================================================================
-- URGENT ALERT LOG TABLE
-- Purpose: Track urgent broadcast operations
-- ============================================================================
CREATE TABLE IF NOT EXISTS urgent_alert_log (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Alert Info
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
    alert_type VARCHAR(50) NOT NULL                   -- critical, high_urgency, foster_needed
        CHECK (alert_type IN ('critical', 'high_urgency', 'foster_needed', 'custom')),

    -- Targeting
    radius_miles INTEGER,
    latitude DECIMAL(10, 7),
    longitude DECIMAL(10, 7),
    state_code VARCHAR(2),

    -- Results
    users_targeted INTEGER DEFAULT 0,                 -- Users in target area
    users_notified INTEGER DEFAULT 0,                 -- Actually notified
    users_skipped INTEGER DEFAULT 0,                  -- Skipped (preferences, limits)
    fosters_notified INTEGER DEFAULT 0,               -- Foster homes notified
    rescues_notified INTEGER DEFAULT 0,               -- Rescue orgs notified

    -- Channels Used
    push_sent INTEGER DEFAULT 0,
    email_sent INTEGER DEFAULT 0,
    sms_sent INTEGER DEFAULT 0,

    -- Engagement (updated over time)
    total_opened INTEGER DEFAULT 0,
    total_clicked INTEGER DEFAULT 0,
    total_inquiries INTEGER DEFAULT 0,               -- Led to inquiries

    -- Processing
    duration_ms INTEGER,                              -- How long blast took
    triggered_by UUID REFERENCES users(id)            -- Admin who triggered
        ON DELETE SET NULL,
    is_automated BOOLEAN DEFAULT FALSE,               -- UrgencyWorker trigger

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW()
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_urgent_alert_dog ON urgent_alert_log(dog_id);
CREATE INDEX IF NOT EXISTS idx_urgent_alert_created ON urgent_alert_log(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_urgent_alert_type ON urgent_alert_log(alert_type);

-- Comments
COMMENT ON TABLE urgent_alert_log IS 'History of urgent broadcast operations';
COMMENT ON COLUMN urgent_alert_log.alert_type IS 'Type of urgent alert: critical (<24h), high_urgency (<72h), foster_needed';
COMMENT ON COLUMN urgent_alert_log.is_automated IS 'True if triggered by UrgencyWorker, false if manual';

-- ============================================================================
-- NOTIFICATION STATS TABLE
-- Purpose: Daily aggregated notification statistics
-- ============================================================================
CREATE TABLE IF NOT EXISTS notification_stats (
    -- Identity
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    date DATE NOT NULL,

    -- Volume
    total_sent INTEGER DEFAULT 0,
    total_delivered INTEGER DEFAULT 0,
    total_failed INTEGER DEFAULT 0,
    total_expired INTEGER DEFAULT 0,

    -- By Type
    critical_alerts_sent INTEGER DEFAULT 0,
    urgent_alerts_sent INTEGER DEFAULT 0,
    match_alerts_sent INTEGER DEFAULT 0,
    update_alerts_sent INTEGER DEFAULT 0,
    other_sent INTEGER DEFAULT 0,

    -- By Channel
    push_sent INTEGER DEFAULT 0,
    push_delivered INTEGER DEFAULT 0,
    email_sent INTEGER DEFAULT 0,
    email_delivered INTEGER DEFAULT 0,
    sms_sent INTEGER DEFAULT 0,
    sms_delivered INTEGER DEFAULT 0,

    -- Engagement
    total_opened INTEGER DEFAULT 0,
    total_clicked INTEGER DEFAULT 0,
    total_dismissed INTEGER DEFAULT 0,

    -- Calculated Rates (updated by trigger)
    delivery_rate DECIMAL(5, 4) DEFAULT 0,            -- delivered / sent
    open_rate DECIMAL(5, 4) DEFAULT 0,                -- opened / delivered
    click_rate DECIMAL(5, 4) DEFAULT 0,               -- clicked / opened

    -- Timestamps
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),

    -- One row per date
    CONSTRAINT unique_stats_date UNIQUE (date)
);

-- Index
CREATE INDEX IF NOT EXISTS idx_notification_stats_date ON notification_stats(date DESC);

-- Comments
COMMENT ON TABLE notification_stats IS 'Daily aggregated notification statistics';
COMMENT ON COLUMN notification_stats.delivery_rate IS 'Delivered / Sent ratio';
COMMENT ON COLUMN notification_stats.open_rate IS 'Opened / Delivered ratio';

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamps
-- ============================================================================
CREATE OR REPLACE FUNCTION update_notification_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS notification_preferences_updated_at ON notification_preferences;
CREATE TRIGGER notification_preferences_updated_at
    BEFORE UPDATE ON notification_preferences
    FOR EACH ROW
    EXECUTE FUNCTION update_notification_updated_at();

DROP TRIGGER IF EXISTS device_tokens_updated_at ON device_tokens;
CREATE TRIGGER device_tokens_updated_at
    BEFORE UPDATE ON device_tokens
    FOR EACH ROW
    EXECUTE FUNCTION update_notification_updated_at();

DROP TRIGGER IF EXISTS notifications_updated_at ON notifications;
CREATE TRIGGER notifications_updated_at
    BEFORE UPDATE ON notifications
    FOR EACH ROW
    EXECUTE FUNCTION update_notification_updated_at();

DROP TRIGGER IF EXISTS notification_queue_updated_at ON notification_queue;
CREATE TRIGGER notification_queue_updated_at
    BEFORE UPDATE ON notification_queue
    FOR EACH ROW
    EXECUTE FUNCTION update_notification_updated_at();

-- ============================================================================
-- TRIGGER: Auto-expire old notifications
-- ============================================================================
CREATE OR REPLACE FUNCTION expire_old_notifications()
RETURNS TRIGGER AS $$
BEGIN
    -- Only check if expires_at is set and we haven't already marked as expired
    IF NEW.expires_at IS NOT NULL AND NEW.expires_at <= NOW() AND NEW.status != 'expired' THEN
        NEW.status = 'expired';
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS notifications_auto_expire ON notifications;
CREATE TRIGGER notifications_auto_expire
    BEFORE UPDATE ON notifications
    FOR EACH ROW
    EXECUTE FUNCTION expire_old_notifications();

-- ============================================================================
-- FUNCTION: Get unread notification count
-- ============================================================================
CREATE OR REPLACE FUNCTION get_unread_notification_count(p_user_id UUID)
RETURNS INTEGER AS $$
DECLARE
    count INTEGER;
BEGIN
    SELECT COUNT(*) INTO count
    FROM notifications
    WHERE user_id = p_user_id
      AND read_at IS NULL
      AND status IN ('sent', 'delivered');

    RETURN count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION get_unread_notification_count IS 'Get count of unread notifications for a user';

-- ============================================================================
-- FUNCTION: Find users for urgent broadcast
-- ============================================================================
CREATE OR REPLACE FUNCTION find_users_for_urgent_broadcast(
    p_latitude DECIMAL(10, 7),
    p_longitude DECIMAL(10, 7),
    p_radius_miles INTEGER,
    p_dog_id UUID DEFAULT NULL
)
RETURNS TABLE (
    user_id UUID,
    distance_miles DECIMAL
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        np.user_id,
        -- Haversine formula for distance in miles
        (3959 * acos(
            cos(radians(p_latitude)) *
            cos(radians(np.home_latitude)) *
            cos(radians(np.home_longitude) - radians(p_longitude)) +
            sin(radians(p_latitude)) *
            sin(radians(np.home_latitude))
        ))::DECIMAL as distance_miles
    FROM notification_preferences np
    INNER JOIN users u ON np.user_id = u.id
    WHERE u.is_active = TRUE
      AND np.critical_alerts_enabled = TRUE
      AND np.home_latitude IS NOT NULL
      AND np.home_longitude IS NOT NULL
      AND (3959 * acos(
            cos(radians(p_latitude)) *
            cos(radians(np.home_latitude)) *
            cos(radians(np.home_longitude) - radians(p_longitude)) +
            sin(radians(p_latitude)) *
            sin(radians(np.home_latitude))
          )) <= p_radius_miles
    ORDER BY distance_miles;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION find_users_for_urgent_broadcast IS 'Find users within radius for urgent broadcast using Haversine formula';

-- ============================================================================
-- FUNCTION: Check quiet hours
-- ============================================================================
CREATE OR REPLACE FUNCTION is_in_quiet_hours(p_user_id UUID)
RETURNS BOOLEAN AS $$
DECLARE
    prefs notification_preferences%ROWTYPE;
    user_local_time TIME;
BEGIN
    SELECT * INTO prefs FROM notification_preferences WHERE user_id = p_user_id;

    IF NOT FOUND OR NOT prefs.quiet_hours_enabled THEN
        RETURN FALSE;
    END IF;

    -- Get current time in user's timezone
    user_local_time := (NOW() AT TIME ZONE prefs.timezone)::TIME;

    -- Handle overnight quiet hours (e.g., 22:00 to 07:00)
    IF prefs.quiet_hours_start > prefs.quiet_hours_end THEN
        RETURN user_local_time >= prefs.quiet_hours_start OR user_local_time < prefs.quiet_hours_end;
    ELSE
        RETURN user_local_time >= prefs.quiet_hours_start AND user_local_time < prefs.quiet_hours_end;
    END IF;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION is_in_quiet_hours IS 'Check if user is currently in quiet hours';

-- ============================================================================
-- FUNCTION: Update notification stats
-- ============================================================================
CREATE OR REPLACE FUNCTION update_daily_notification_stats()
RETURNS VOID AS $$
DECLARE
    today DATE := CURRENT_DATE;
BEGIN
    INSERT INTO notification_stats (date)
    VALUES (today)
    ON CONFLICT (date) DO NOTHING;

    UPDATE notification_stats SET
        total_sent = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today),
        total_delivered = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND status = 'delivered'),
        total_failed = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND status = 'failed'),
        total_opened = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND opened_at IS NOT NULL),
        total_clicked = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND clicked_at IS NOT NULL),
        critical_alerts_sent = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND type = 'critical_alert'),
        urgent_alerts_sent = (SELECT COUNT(*) FROM notifications WHERE DATE(created_at) = today AND type = 'high_urgency'),
        delivery_rate = CASE
            WHEN total_sent > 0 THEN total_delivered::DECIMAL / total_sent
            ELSE 0
        END,
        open_rate = CASE
            WHEN total_delivered > 0 THEN total_opened::DECIMAL / total_delivered
            ELSE 0
        END,
        click_rate = CASE
            WHEN total_opened > 0 THEN total_clicked::DECIMAL / total_opened
            ELSE 0
        END,
        updated_at = NOW()
    WHERE date = today;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION update_daily_notification_stats IS 'Update daily aggregated notification statistics';

-- ============================================================================
-- VIEW: User notification summary
-- ============================================================================
CREATE OR REPLACE VIEW user_notification_summary AS
SELECT
    u.id as user_id,
    u.email,
    u.display_name,
    np.push_enabled,
    np.email_enabled,
    np.sms_enabled,
    np.quiet_hours_enabled,
    np.alert_radius_miles,
    COUNT(n.id) as total_notifications,
    COUNT(n.id) FILTER (WHERE n.read_at IS NULL) as unread_count,
    MAX(n.created_at) as last_notification_at,
    COUNT(dt.id) as registered_devices
FROM users u
LEFT JOIN notification_preferences np ON u.id = np.user_id
LEFT JOIN notifications n ON u.id = n.user_id
LEFT JOIN device_tokens dt ON u.id = dt.user_id AND dt.is_active = TRUE
WHERE u.is_active = TRUE
GROUP BY u.id, u.email, u.display_name, np.push_enabled, np.email_enabled,
         np.sms_enabled, np.quiet_hours_enabled, np.alert_radius_miles;

COMMENT ON VIEW user_notification_summary IS 'Summary of user notification settings and activity';

-- ============================================================================
-- VIEW: Recent urgent alerts
-- ============================================================================
CREATE OR REPLACE VIEW recent_urgent_alerts AS
SELECT
    ual.*,
    d.name as dog_name,
    d.breed_primary as dog_breed,
    s.name as shelter_name,
    s.state_code as shelter_state,
    u.display_name as triggered_by_name
FROM urgent_alert_log ual
LEFT JOIN dogs d ON ual.dog_id = d.id
LEFT JOIN shelters s ON ual.shelter_id = s.id
LEFT JOIN users u ON ual.triggered_by = u.id
ORDER BY ual.created_at DESC;

COMMENT ON VIEW recent_urgent_alerts IS 'Recent urgent alert broadcasts with dog and shelter info';

-- ============================================================================
-- CREATE DEFAULT PREFERENCES FOR EXISTING USERS
-- ============================================================================
INSERT INTO notification_preferences (user_id)
SELECT id FROM users
WHERE NOT EXISTS (
    SELECT 1 FROM notification_preferences np WHERE np.user_id = users.id
)
ON CONFLICT (user_id) DO NOTHING;

-- ============================================================================
-- Grant permissions (adjust for your user)
-- ============================================================================
-- GRANT SELECT, INSERT, UPDATE, DELETE ON notification_preferences TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON device_tokens TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON notifications TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE, DELETE ON notification_queue TO wtl_app;
-- GRANT SELECT, INSERT ON urgent_alert_log TO wtl_app;
-- GRANT SELECT, INSERT, UPDATE ON notification_stats TO wtl_app;
-- GRANT SELECT ON user_notification_summary TO wtl_app;
-- GRANT SELECT ON recent_urgent_alerts TO wtl_app;

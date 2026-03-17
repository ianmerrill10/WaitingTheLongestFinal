/**
 * @file scheduler_schema.sql
 * @brief Database schema for scheduler and worker system
 *
 * PURPOSE:
 * Defines tables for scheduled jobs, job queue, worker status tracking,
 * job execution history, and health monitoring.
 *
 * TABLES:
 * - scheduled_jobs: Recurring and one-time scheduled jobs
 * - job_queue: Active jobs waiting for processing
 * - job_history: Completed/failed job execution records
 * - worker_status: Current state of all workers
 * - health_metrics: System health check results
 * - health_alerts: Active and resolved health alerts
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

-- ============================================================================
-- ENUMS
-- ============================================================================

-- Job status enum
DO $$ BEGIN
    CREATE TYPE job_status AS ENUM (
        'pending',
        'scheduled',
        'running',
        'completed',
        'failed',
        'cancelled',
        'dead_letter'
    );
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- Job priority enum
DO $$ BEGIN
    CREATE TYPE job_priority AS ENUM (
        'low',
        'normal',
        'high',
        'critical'
    );
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- Worker status enum
DO $$ BEGIN
    CREATE TYPE worker_status_enum AS ENUM (
        'stopped',
        'starting',
        'running',
        'paused',
        'stopping',
        'error'
    );
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- Health status enum
DO $$ BEGIN
    CREATE TYPE health_status AS ENUM (
        'healthy',
        'degraded',
        'unhealthy',
        'unknown'
    );
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- Recurrence type enum
DO $$ BEGIN
    CREATE TYPE recurrence_type AS ENUM (
        'once',
        'interval',
        'cron',
        'daily',
        'weekly',
        'monthly'
    );
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- ============================================================================
-- SCHEDULED JOBS TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS scheduled_jobs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    description TEXT,
    handler_name VARCHAR(255) NOT NULL,
    payload JSONB DEFAULT '{}'::jsonb,

    -- Scheduling
    recurrence recurrence_type NOT NULL DEFAULT 'once',
    cron_expression VARCHAR(100),
    interval_seconds INTEGER,
    next_run_at TIMESTAMP WITH TIME ZONE,
    last_run_at TIMESTAMP WITH TIME ZONE,

    -- Status
    enabled BOOLEAN NOT NULL DEFAULT true,
    status job_status NOT NULL DEFAULT 'scheduled',
    priority job_priority NOT NULL DEFAULT 'normal',

    -- Retry configuration
    max_retries INTEGER NOT NULL DEFAULT 3,
    retry_count INTEGER NOT NULL DEFAULT 0,
    retry_delay_seconds INTEGER NOT NULL DEFAULT 60,

    -- Timeout
    timeout_seconds INTEGER NOT NULL DEFAULT 300,

    -- Tracking
    run_count INTEGER NOT NULL DEFAULT 0,
    success_count INTEGER NOT NULL DEFAULT 0,
    failure_count INTEGER NOT NULL DEFAULT 0,
    last_error TEXT,

    -- Metadata
    tags TEXT[] DEFAULT '{}',
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT valid_cron_or_interval CHECK (
        (recurrence = 'cron' AND cron_expression IS NOT NULL) OR
        (recurrence = 'interval' AND interval_seconds IS NOT NULL) OR
        (recurrence NOT IN ('cron', 'interval'))
    )
);

-- Indexes for scheduled_jobs
CREATE INDEX IF NOT EXISTS idx_scheduled_jobs_next_run
    ON scheduled_jobs(next_run_at) WHERE enabled = true;
CREATE INDEX IF NOT EXISTS idx_scheduled_jobs_status
    ON scheduled_jobs(status);
CREATE INDEX IF NOT EXISTS idx_scheduled_jobs_handler
    ON scheduled_jobs(handler_name);
CREATE INDEX IF NOT EXISTS idx_scheduled_jobs_enabled
    ON scheduled_jobs(enabled);
CREATE INDEX IF NOT EXISTS idx_scheduled_jobs_tags
    ON scheduled_jobs USING GIN(tags);

-- ============================================================================
-- JOB QUEUE TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS job_queue (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    scheduled_job_id UUID REFERENCES scheduled_jobs(id) ON DELETE SET NULL,
    handler_name VARCHAR(255) NOT NULL,
    payload JSONB DEFAULT '{}'::jsonb,

    -- Priority and status
    priority job_priority NOT NULL DEFAULT 'normal',
    status job_status NOT NULL DEFAULT 'pending',

    -- Processing info
    worker_id VARCHAR(255),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,

    -- Retry handling
    attempt INTEGER NOT NULL DEFAULT 0,
    max_attempts INTEGER NOT NULL DEFAULT 3,
    next_retry_at TIMESTAMP WITH TIME ZONE,

    -- Error tracking
    last_error TEXT,
    error_details JSONB,

    -- Timeout
    timeout_seconds INTEGER NOT NULL DEFAULT 300,
    deadline_at TIMESTAMP WITH TIME ZONE,

    -- Metadata
    correlation_id UUID,
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for job_queue
CREATE INDEX IF NOT EXISTS idx_job_queue_status_priority
    ON job_queue(status, priority DESC) WHERE status = 'pending';
CREATE INDEX IF NOT EXISTS idx_job_queue_handler
    ON job_queue(handler_name);
CREATE INDEX IF NOT EXISTS idx_job_queue_worker
    ON job_queue(worker_id) WHERE worker_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_job_queue_retry
    ON job_queue(next_retry_at) WHERE status = 'pending' AND next_retry_at IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_job_queue_deadline
    ON job_queue(deadline_at) WHERE status = 'running';
CREATE INDEX IF NOT EXISTS idx_job_queue_correlation
    ON job_queue(correlation_id) WHERE correlation_id IS NOT NULL;

-- ============================================================================
-- JOB HISTORY TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS job_history (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_queue_id UUID,
    scheduled_job_id UUID,
    handler_name VARCHAR(255) NOT NULL,
    payload JSONB DEFAULT '{}'::jsonb,

    -- Execution info
    status job_status NOT NULL,
    worker_id VARCHAR(255),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    duration_ms INTEGER,

    -- Result
    result JSONB,
    items_processed INTEGER DEFAULT 0,

    -- Error info
    error_message TEXT,
    error_details JSONB,
    stack_trace TEXT,

    -- Attempt info
    attempt INTEGER NOT NULL DEFAULT 1,

    -- Metadata
    correlation_id UUID,
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for job_history
CREATE INDEX IF NOT EXISTS idx_job_history_handler_date
    ON job_history(handler_name, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_job_history_status
    ON job_history(status);
CREATE INDEX IF NOT EXISTS idx_job_history_created
    ON job_history(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_job_history_scheduled_job
    ON job_history(scheduled_job_id) WHERE scheduled_job_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_job_history_correlation
    ON job_history(correlation_id) WHERE correlation_id IS NOT NULL;

-- Partition job_history by month for performance
-- (In production, consider implementing table partitioning)

-- ============================================================================
-- WORKER STATUS TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS worker_status (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    worker_name VARCHAR(255) NOT NULL UNIQUE,
    description TEXT,

    -- Status
    status worker_status_enum NOT NULL DEFAULT 'stopped',
    is_healthy BOOLEAN NOT NULL DEFAULT true,

    -- Execution info
    interval_seconds INTEGER NOT NULL DEFAULT 60,
    last_execution_at TIMESTAMP WITH TIME ZONE,
    next_execution_at TIMESTAMP WITH TIME ZONE,

    -- Statistics
    total_executions INTEGER NOT NULL DEFAULT 0,
    successful_executions INTEGER NOT NULL DEFAULT 0,
    failed_executions INTEGER NOT NULL DEFAULT 0,
    total_items_processed INTEGER NOT NULL DEFAULT 0,
    avg_execution_time_ms DOUBLE PRECISION DEFAULT 0,

    -- Error tracking
    consecutive_failures INTEGER NOT NULL DEFAULT 0,
    last_error TEXT,
    last_error_at TIMESTAMP WITH TIME ZONE,

    -- Metadata
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Index for worker_status
CREATE INDEX IF NOT EXISTS idx_worker_status_status
    ON worker_status(status);
CREATE INDEX IF NOT EXISTS idx_worker_status_health
    ON worker_status(is_healthy);

-- ============================================================================
-- HEALTH METRICS TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS health_metrics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    overall_status health_status NOT NULL,
    healthy_count INTEGER NOT NULL DEFAULT 0,
    degraded_count INTEGER NOT NULL DEFAULT 0,
    unhealthy_count INTEGER NOT NULL DEFAULT 0,

    -- Component details
    details JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    check_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Index for health_metrics
CREATE INDEX IF NOT EXISTS idx_health_metrics_time
    ON health_metrics(check_time DESC);
CREATE INDEX IF NOT EXISTS idx_health_metrics_status
    ON health_metrics(overall_status);

-- ============================================================================
-- HEALTH ALERTS TABLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS health_alerts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    component VARCHAR(255) NOT NULL,
    status health_status NOT NULL,
    message TEXT NOT NULL,

    -- Resolution
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolution_message TEXT,

    -- Notification tracking
    notified BOOLEAN NOT NULL DEFAULT false,
    notified_at TIMESTAMP WITH TIME ZONE,
    notification_channels TEXT[] DEFAULT '{}',

    -- Acknowledgment
    acknowledged BOOLEAN NOT NULL DEFAULT false,
    acknowledged_by VARCHAR(255),
    acknowledged_at TIMESTAMP WITH TIME ZONE,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for health_alerts
CREATE INDEX IF NOT EXISTS idx_health_alerts_component
    ON health_alerts(component);
CREATE INDEX IF NOT EXISTS idx_health_alerts_active
    ON health_alerts(component, status) WHERE resolved_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_health_alerts_created
    ON health_alerts(created_at DESC);

-- ============================================================================
-- URGENCY ALERTS TABLE (for UrgencyUpdateWorker)
-- ============================================================================

CREATE TABLE IF NOT EXISTS urgency_alerts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    alert_type VARCHAR(50) NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'active',

    -- Notification tracking
    notifications_sent INTEGER NOT NULL DEFAULT 0,
    last_notification_at TIMESTAMP WITH TIME ZONE,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    resolved_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT unique_active_alert UNIQUE (dog_id, alert_type, status)
);

-- Indexes for urgency_alerts
CREATE INDEX IF NOT EXISTS idx_urgency_alerts_dog
    ON urgency_alerts(dog_id);
CREATE INDEX IF NOT EXISTS idx_urgency_alerts_active
    ON urgency_alerts(status) WHERE status = 'active';
CREATE INDEX IF NOT EXISTS idx_urgency_alerts_created
    ON urgency_alerts(created_at DESC);

-- ============================================================================
-- CONTENT POSTS TABLE (for ContentGenerationWorker)
-- ============================================================================

CREATE TABLE IF NOT EXISTS content_posts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    post_type VARCHAR(50) NOT NULL,
    title VARCHAR(500),
    content TEXT NOT NULL,

    -- Scheduling
    status VARCHAR(50) NOT NULL DEFAULT 'draft',
    scheduled_at TIMESTAMP WITH TIME ZONE,
    published_at TIMESTAMP WITH TIME ZONE,

    -- Metadata
    tags TEXT[] DEFAULT '{}',
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for content_posts
CREATE INDEX IF NOT EXISTS idx_content_posts_type
    ON content_posts(post_type);
CREATE INDEX IF NOT EXISTS idx_content_posts_status
    ON content_posts(status);
CREATE INDEX IF NOT EXISTS idx_content_posts_scheduled
    ON content_posts(scheduled_at) WHERE status = 'scheduled';

-- NOTE: social_posts table is defined in social_schema.sql (authoritative version)

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Apply to tables
DROP TRIGGER IF EXISTS update_scheduled_jobs_updated_at ON scheduled_jobs;
CREATE TRIGGER update_scheduled_jobs_updated_at
    BEFORE UPDATE ON scheduled_jobs
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

DROP TRIGGER IF EXISTS update_job_queue_updated_at ON job_queue;
CREATE TRIGGER update_job_queue_updated_at
    BEFORE UPDATE ON job_queue
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

DROP TRIGGER IF EXISTS update_worker_status_updated_at ON worker_status;
CREATE TRIGGER update_worker_status_updated_at
    BEFORE UPDATE ON worker_status
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

DROP TRIGGER IF EXISTS update_content_posts_updated_at ON content_posts;
CREATE TRIGGER update_content_posts_updated_at
    BEFORE UPDATE ON content_posts
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- NOTE: social_posts trigger is defined in social_schema.sql

-- ============================================================================
-- HELPER FUNCTIONS
-- ============================================================================

-- Function to get next pending job
CREATE OR REPLACE FUNCTION get_next_pending_job(p_worker_id VARCHAR)
RETURNS TABLE (
    job_id UUID,
    handler_name VARCHAR,
    payload JSONB
) AS $$
DECLARE
    v_job_id UUID;
BEGIN
    -- Lock and claim the next job
    UPDATE job_queue
    SET status = 'running',
        worker_id = p_worker_id,
        started_at = NOW(),
        deadline_at = NOW() + (timeout_seconds || ' seconds')::interval
    WHERE id = (
        SELECT id FROM job_queue
        WHERE status = 'pending'
        AND (next_retry_at IS NULL OR next_retry_at <= NOW())
        ORDER BY
            CASE priority
                WHEN 'critical' THEN 0
                WHEN 'high' THEN 1
                WHEN 'normal' THEN 2
                WHEN 'low' THEN 3
            END,
            created_at ASC
        LIMIT 1
        FOR UPDATE SKIP LOCKED
    )
    RETURNING id INTO v_job_id;

    IF v_job_id IS NOT NULL THEN
        RETURN QUERY
        SELECT jq.id, jq.handler_name, jq.payload
        FROM job_queue jq
        WHERE jq.id = v_job_id;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Function to complete a job
CREATE OR REPLACE FUNCTION complete_job(
    p_job_id UUID,
    p_result JSONB DEFAULT NULL,
    p_items_processed INTEGER DEFAULT 0
)
RETURNS VOID AS $$
DECLARE
    v_job job_queue%ROWTYPE;
BEGIN
    -- Get job details
    SELECT * INTO v_job FROM job_queue WHERE id = p_job_id;

    IF v_job.id IS NULL THEN
        RAISE EXCEPTION 'Job not found: %', p_job_id;
    END IF;

    -- Record in history
    INSERT INTO job_history (
        job_queue_id, scheduled_job_id, handler_name, payload,
        status, worker_id, started_at, completed_at,
        duration_ms, result, items_processed, attempt,
        correlation_id, metadata
    ) VALUES (
        v_job.id, v_job.scheduled_job_id, v_job.handler_name, v_job.payload,
        'completed', v_job.worker_id, v_job.started_at, NOW(),
        EXTRACT(EPOCH FROM (NOW() - v_job.started_at)) * 1000,
        p_result, p_items_processed, v_job.attempt,
        v_job.correlation_id, v_job.metadata
    );

    -- Update scheduled job if linked
    IF v_job.scheduled_job_id IS NOT NULL THEN
        UPDATE scheduled_jobs
        SET run_count = run_count + 1,
            success_count = success_count + 1,
            last_run_at = NOW(),
            retry_count = 0,
            last_error = NULL
        WHERE id = v_job.scheduled_job_id;
    END IF;

    -- Remove from queue
    DELETE FROM job_queue WHERE id = p_job_id;
END;
$$ LANGUAGE plpgsql;

-- Function to fail a job
CREATE OR REPLACE FUNCTION fail_job(
    p_job_id UUID,
    p_error_message TEXT,
    p_error_details JSONB DEFAULT NULL,
    p_stack_trace TEXT DEFAULT NULL
)
RETURNS VOID AS $$
DECLARE
    v_job job_queue%ROWTYPE;
BEGIN
    -- Get job details
    SELECT * INTO v_job FROM job_queue WHERE id = p_job_id;

    IF v_job.id IS NULL THEN
        RAISE EXCEPTION 'Job not found: %', p_job_id;
    END IF;

    -- Record in history
    INSERT INTO job_history (
        job_queue_id, scheduled_job_id, handler_name, payload,
        status, worker_id, started_at, completed_at,
        duration_ms, error_message, error_details, stack_trace,
        attempt, correlation_id, metadata
    ) VALUES (
        v_job.id, v_job.scheduled_job_id, v_job.handler_name, v_job.payload,
        'failed', v_job.worker_id, v_job.started_at, NOW(),
        EXTRACT(EPOCH FROM (NOW() - v_job.started_at)) * 1000,
        p_error_message, p_error_details, p_stack_trace,
        v_job.attempt, v_job.correlation_id, v_job.metadata
    );

    -- Check if should retry
    IF v_job.attempt < v_job.max_attempts THEN
        -- Schedule retry
        UPDATE job_queue
        SET status = 'pending',
            attempt = attempt + 1,
            last_error = p_error_message,
            error_details = p_error_details,
            worker_id = NULL,
            started_at = NULL,
            next_retry_at = NOW() + (power(2, v_job.attempt) * interval '1 minute')
        WHERE id = p_job_id;
    ELSE
        -- Move to dead letter
        UPDATE job_queue
        SET status = 'dead_letter',
            last_error = p_error_message,
            error_details = p_error_details,
            completed_at = NOW()
        WHERE id = p_job_id;

        -- Update scheduled job if linked
        IF v_job.scheduled_job_id IS NOT NULL THEN
            UPDATE scheduled_jobs
            SET run_count = run_count + 1,
                failure_count = failure_count + 1,
                last_run_at = NOW(),
                last_error = p_error_message
            WHERE id = v_job.scheduled_job_id;
        END IF;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up stale jobs
CREATE OR REPLACE FUNCTION cleanup_stale_jobs()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    -- Find jobs that have exceeded their deadline
    WITH stale_jobs AS (
        SELECT id
        FROM job_queue
        WHERE status = 'running'
        AND deadline_at < NOW()
    )
    UPDATE job_queue
    SET status = 'pending',
        worker_id = NULL,
        started_at = NULL,
        attempt = attempt + 1,
        last_error = 'Job timed out'
    WHERE id IN (SELECT id FROM stale_jobs);

    GET DIAGNOSTICS v_count = ROW_COUNT;
    RETURN v_count;
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- COMMENTS
-- ============================================================================

COMMENT ON TABLE scheduled_jobs IS 'Stores recurring and one-time scheduled jobs';
COMMENT ON TABLE job_queue IS 'Active jobs waiting for or being processed by workers';
COMMENT ON TABLE job_history IS 'Historical record of completed and failed job executions';
COMMENT ON TABLE worker_status IS 'Current state and statistics of background workers';
COMMENT ON TABLE health_metrics IS 'System health check results over time';
COMMENT ON TABLE health_alerts IS 'Active and resolved system health alerts';
COMMENT ON TABLE urgency_alerts IS 'Alerts for dogs becoming critical urgency';
COMMENT ON TABLE content_posts IS 'Generated content posts (roundups, articles)';
-- social_posts comments are in social_schema.sql

COMMENT ON FUNCTION get_next_pending_job IS 'Atomically claims the next pending job for a worker';
COMMENT ON FUNCTION complete_job IS 'Marks a job as successfully completed';
COMMENT ON FUNCTION fail_job IS 'Marks a job as failed, handling retries';
COMMENT ON FUNCTION cleanup_stale_jobs IS 'Resets jobs that have timed out';

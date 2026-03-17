-- ============================================================================
-- Migration: 010_debug_schema.sql
-- Purpose: Create debug/error tracking tables for self-healing system
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--
-- PLAN REFERENCE: Debug System Contract from INTEGRATION_CONTRACTS.md
-- Supports WTL_CAPTURE_ERROR, WTL_CAPTURE_CRITICAL macros and SelfHealing class
--
-- TABLES:
--   error_logs - All captured errors
--   healing_actions - Self-healing attempts and outcomes
--   circuit_breaker_states - Circuit breaker status for external services
--   health_checks - System health check results
--
-- ROLLBACK: DROP TABLE IF EXISTS health_checks, circuit_breaker_states, healing_actions, error_logs CASCADE;
-- ============================================================================

-- ============================================================================
-- ERROR_LOGS TABLE
-- Purpose: Capture all errors for debugging and pattern analysis
-- Used by: WTL_CAPTURE_ERROR and WTL_CAPTURE_CRITICAL macros
-- ============================================================================
CREATE TABLE error_logs (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- ERROR CLASSIFICATION
    -- ========================================================================
    error_category VARCHAR(50) NOT NULL,             -- DATABASE, EXTERNAL_API, VALIDATION, AUTH, SYSTEM, WEBSOCKET
    severity VARCHAR(20) NOT NULL DEFAULT 'error',   -- debug, info, warning, error, critical
    error_code VARCHAR(50),                          -- Application-specific error code

    -- ========================================================================
    -- ERROR DETAILS
    -- ========================================================================
    message TEXT NOT NULL,                           -- Error message
    stack_trace TEXT,                                -- Stack trace if available
    exception_type VARCHAR(255),                     -- Exception class name

    -- ========================================================================
    -- CONTEXT
    -- ========================================================================
    context JSONB DEFAULT '{}',                      -- Additional context (dog_id, operation, etc.)
    request_id VARCHAR(100),                         -- Request/correlation ID for tracing
    session_id UUID,                                 -- User session if applicable
    user_id UUID,                                    -- User ID if authenticated

    -- ========================================================================
    -- SOURCE INFORMATION
    -- ========================================================================
    source_file VARCHAR(500),                        -- Source file where error occurred
    source_line INTEGER,                             -- Line number
    source_function VARCHAR(255),                    -- Function/method name
    component VARCHAR(100),                          -- Component name (DogService, AuthController, etc.)

    -- ========================================================================
    -- ENVIRONMENT
    -- ========================================================================
    hostname VARCHAR(255),                           -- Server hostname
    environment VARCHAR(20),                         -- development, staging, production
    version VARCHAR(50),                             -- Application version

    -- ========================================================================
    -- RESOLUTION TRACKING
    -- ========================================================================
    is_resolved BOOLEAN DEFAULT FALSE,
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolved_by VARCHAR(100),
    resolution_notes TEXT,

    -- Was this auto-healed?
    was_auto_healed BOOLEAN DEFAULT FALSE,
    healing_action_id UUID,                          -- FK to healing_actions

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    occurred_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE error_logs IS 'Central error logging table for the debug system. Captures all errors from WTL_CAPTURE_ERROR and WTL_CAPTURE_CRITICAL macros.';

COMMENT ON COLUMN error_logs.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN error_logs.error_category IS 'Category: DATABASE, EXTERNAL_API, VALIDATION, AUTH, SYSTEM, WEBSOCKET, NETWORK';
COMMENT ON COLUMN error_logs.severity IS 'Severity level: debug, info, warning, error, critical';
COMMENT ON COLUMN error_logs.error_code IS 'Application-specific error code for programmatic handling';
COMMENT ON COLUMN error_logs.message IS 'Human-readable error message';
COMMENT ON COLUMN error_logs.stack_trace IS 'Full stack trace if available';
COMMENT ON COLUMN error_logs.exception_type IS 'Exception class/type name';
COMMENT ON COLUMN error_logs.context IS 'JSONB with additional context (IDs, parameters, etc.)';
COMMENT ON COLUMN error_logs.request_id IS 'Request/correlation ID for distributed tracing';
COMMENT ON COLUMN error_logs.session_id IS 'Session ID if user was authenticated';
COMMENT ON COLUMN error_logs.user_id IS 'User ID if authenticated';
COMMENT ON COLUMN error_logs.source_file IS 'Source file path where error occurred';
COMMENT ON COLUMN error_logs.source_line IS 'Line number in source file';
COMMENT ON COLUMN error_logs.source_function IS 'Function or method name';
COMMENT ON COLUMN error_logs.component IS 'Component/service name';
COMMENT ON COLUMN error_logs.hostname IS 'Server hostname';
COMMENT ON COLUMN error_logs.environment IS 'Environment: development, staging, production';
COMMENT ON COLUMN error_logs.version IS 'Application version';
COMMENT ON COLUMN error_logs.is_resolved IS 'TRUE if error has been resolved/acknowledged';
COMMENT ON COLUMN error_logs.resolved_at IS 'When error was resolved';
COMMENT ON COLUMN error_logs.resolved_by IS 'Who resolved (user_id or system)';
COMMENT ON COLUMN error_logs.resolution_notes IS 'Notes about resolution';
COMMENT ON COLUMN error_logs.was_auto_healed IS 'TRUE if self-healing system handled this';
COMMENT ON COLUMN error_logs.healing_action_id IS 'FK to healing_actions if auto-healed';
COMMENT ON COLUMN error_logs.occurred_at IS 'When the error occurred';
COMMENT ON COLUMN error_logs.created_at IS 'When the log record was created';

-- ============================================================================
-- HEALING_ACTIONS TABLE
-- Purpose: Track self-healing attempts and their outcomes
-- Used by: SelfHealing::executeWithHealing
-- ============================================================================
CREATE TABLE healing_actions (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- ERROR REFERENCE
    -- ========================================================================
    error_log_id UUID REFERENCES error_logs(id) ON DELETE SET NULL,
    error_category VARCHAR(50) NOT NULL,

    -- ========================================================================
    -- ACTION DETAILS
    -- ========================================================================
    action_type VARCHAR(100) NOT NULL,               -- retry, fallback, circuit_break, restart, etc.
    action_name VARCHAR(255) NOT NULL,               -- Human-readable action name
    action_parameters JSONB DEFAULT '{}',            -- Parameters used for the action

    -- ========================================================================
    -- OUTCOME
    -- ========================================================================
    was_successful BOOLEAN,
    failure_reason TEXT,

    -- Retry tracking
    attempt_number INTEGER DEFAULT 1,
    max_attempts INTEGER,

    -- Performance
    execution_time_ms INTEGER,

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    started_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE healing_actions IS 'Tracks self-healing actions taken by the system. Records what was tried and whether it succeeded.';

COMMENT ON COLUMN healing_actions.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN healing_actions.error_log_id IS 'Reference to the error that triggered this action';
COMMENT ON COLUMN healing_actions.error_category IS 'Category of error being healed';
COMMENT ON COLUMN healing_actions.action_type IS 'Type of healing action: retry, fallback, circuit_break, restart, reconnect';
COMMENT ON COLUMN healing_actions.action_name IS 'Human-readable action description';
COMMENT ON COLUMN healing_actions.action_parameters IS 'JSONB of parameters used';
COMMENT ON COLUMN healing_actions.was_successful IS 'TRUE if healing succeeded';
COMMENT ON COLUMN healing_actions.failure_reason IS 'If failed, why';
COMMENT ON COLUMN healing_actions.attempt_number IS 'Which attempt this was (1, 2, 3...)';
COMMENT ON COLUMN healing_actions.max_attempts IS 'Maximum attempts allowed';
COMMENT ON COLUMN healing_actions.execution_time_ms IS 'How long the action took';
COMMENT ON COLUMN healing_actions.started_at IS 'When action started';
COMMENT ON COLUMN healing_actions.completed_at IS 'When action completed';
COMMENT ON COLUMN healing_actions.created_at IS 'Record creation timestamp';

-- ============================================================================
-- CIRCUIT_BREAKER_STATES TABLE
-- Purpose: Track circuit breaker status for external services
-- Prevents cascading failures when external services are down
-- ============================================================================
CREATE TABLE circuit_breaker_states (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- SERVICE IDENTIFICATION
    -- ========================================================================
    service_name VARCHAR(100) NOT NULL UNIQUE,       -- rescuegroups_api, database, redis, etc.
    service_type VARCHAR(50),                        -- api, database, cache, queue

    -- ========================================================================
    -- STATE
    -- ========================================================================
    state VARCHAR(20) NOT NULL DEFAULT 'closed',     -- closed (normal), open (blocking), half_open (testing)

    -- ========================================================================
    -- FAILURE TRACKING
    -- ========================================================================
    failure_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    last_failure_at TIMESTAMP WITH TIME ZONE,
    last_success_at TIMESTAMP WITH TIME ZONE,
    last_failure_reason TEXT,

    -- ========================================================================
    -- THRESHOLDS
    -- ========================================================================
    failure_threshold INTEGER DEFAULT 5,             -- Failures before opening
    success_threshold INTEGER DEFAULT 3,             -- Successes to close from half-open
    timeout_seconds INTEGER DEFAULT 60,              -- How long to stay open

    -- ========================================================================
    -- STATE TRANSITIONS
    -- ========================================================================
    opened_at TIMESTAMP WITH TIME ZONE,              -- When circuit was opened
    half_opened_at TIMESTAMP WITH TIME ZONE,         -- When moved to half-open
    closed_at TIMESTAMP WITH TIME ZONE,              -- When closed (recovered)

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE circuit_breaker_states IS 'Circuit breaker status for external services. Prevents cascading failures by temporarily blocking calls to failing services.';

COMMENT ON COLUMN circuit_breaker_states.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN circuit_breaker_states.service_name IS 'Unique service identifier';
COMMENT ON COLUMN circuit_breaker_states.service_type IS 'Type of service: api, database, cache, queue';
COMMENT ON COLUMN circuit_breaker_states.state IS 'Circuit state: closed (normal), open (blocking), half_open (testing)';
COMMENT ON COLUMN circuit_breaker_states.failure_count IS 'Recent failure count';
COMMENT ON COLUMN circuit_breaker_states.success_count IS 'Recent success count';
COMMENT ON COLUMN circuit_breaker_states.last_failure_at IS 'When last failure occurred';
COMMENT ON COLUMN circuit_breaker_states.last_success_at IS 'When last success occurred';
COMMENT ON COLUMN circuit_breaker_states.last_failure_reason IS 'Reason for last failure';
COMMENT ON COLUMN circuit_breaker_states.failure_threshold IS 'Failures before circuit opens';
COMMENT ON COLUMN circuit_breaker_states.success_threshold IS 'Successes needed to close from half-open';
COMMENT ON COLUMN circuit_breaker_states.timeout_seconds IS 'Seconds to wait before half-opening';
COMMENT ON COLUMN circuit_breaker_states.opened_at IS 'When circuit was opened';
COMMENT ON COLUMN circuit_breaker_states.half_opened_at IS 'When circuit moved to half-open';
COMMENT ON COLUMN circuit_breaker_states.closed_at IS 'When circuit recovered (closed)';
COMMENT ON COLUMN circuit_breaker_states.created_at IS 'Record creation timestamp';
COMMENT ON COLUMN circuit_breaker_states.updated_at IS 'Last update timestamp';

-- ============================================================================
-- HEALTH_CHECKS TABLE
-- Purpose: Record health check results for monitoring
-- ============================================================================
CREATE TABLE health_checks (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- CHECK IDENTIFICATION
    -- ========================================================================
    check_name VARCHAR(100) NOT NULL,                -- database, redis, rescuegroups_api, etc.
    check_type VARCHAR(50),                          -- connectivity, query, latency, quota

    -- ========================================================================
    -- RESULT
    -- ========================================================================
    is_healthy BOOLEAN NOT NULL,
    status_code INTEGER,                             -- HTTP status or custom code
    status_message VARCHAR(255),

    -- ========================================================================
    -- METRICS
    -- ========================================================================
    response_time_ms INTEGER,
    details JSONB DEFAULT '{}',                      -- Additional details (queries/sec, memory, etc.)

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    checked_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE health_checks IS 'Health check results for system monitoring. Records periodic health checks of all system components.';

COMMENT ON COLUMN health_checks.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN health_checks.check_name IS 'Name of the health check';
COMMENT ON COLUMN health_checks.check_type IS 'Type: connectivity, query, latency, quota';
COMMENT ON COLUMN health_checks.is_healthy IS 'TRUE if check passed';
COMMENT ON COLUMN health_checks.status_code IS 'HTTP status or custom status code';
COMMENT ON COLUMN health_checks.status_message IS 'Status message';
COMMENT ON COLUMN health_checks.response_time_ms IS 'Response time in milliseconds';
COMMENT ON COLUMN health_checks.details IS 'JSONB with additional metrics';
COMMENT ON COLUMN health_checks.checked_at IS 'When the check was performed';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Error logs indexes
CREATE INDEX idx_error_logs_occurred_at ON error_logs(occurred_at DESC);
CREATE INDEX idx_error_logs_category ON error_logs(error_category);
CREATE INDEX idx_error_logs_severity ON error_logs(severity);
CREATE INDEX idx_error_logs_unresolved ON error_logs(is_resolved, occurred_at DESC)
    WHERE is_resolved = FALSE;
CREATE INDEX idx_error_logs_component ON error_logs(component)
    WHERE component IS NOT NULL;
CREATE INDEX idx_error_logs_request_id ON error_logs(request_id)
    WHERE request_id IS NOT NULL;

-- Healing actions indexes
CREATE INDEX idx_healing_actions_error ON healing_actions(error_log_id);
CREATE INDEX idx_healing_actions_started ON healing_actions(started_at DESC);
CREATE INDEX idx_healing_actions_outcome ON healing_actions(was_successful);

-- Circuit breaker indexes
CREATE INDEX idx_circuit_breaker_service ON circuit_breaker_states(service_name);
CREATE INDEX idx_circuit_breaker_state ON circuit_breaker_states(state);

-- Health checks indexes
CREATE INDEX idx_health_checks_name_time ON health_checks(check_name, checked_at DESC);
CREATE INDEX idx_health_checks_unhealthy ON health_checks(check_name, checked_at DESC)
    WHERE is_healthy = FALSE;

-- ============================================================================
-- TRIGGER: Auto-update circuit_breaker_states.updated_at
-- ============================================================================
CREATE OR REPLACE FUNCTION update_circuit_breaker_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_circuit_breaker_updated_at
    BEFORE UPDATE ON circuit_breaker_states
    FOR EACH ROW
    EXECUTE FUNCTION update_circuit_breaker_updated_at();

-- ============================================================================
-- FUNCTION: Cleanup old error logs (30 day retention)
-- Usage: SELECT cleanup_old_error_logs();
-- Called periodically by a cron job or worker
-- ============================================================================
CREATE OR REPLACE FUNCTION cleanup_old_error_logs(retention_days INTEGER DEFAULT 30)
RETURNS TABLE(
    errors_deleted INTEGER,
    healings_deleted INTEGER,
    health_checks_deleted INTEGER
) AS $$
DECLARE
    v_errors_deleted INTEGER;
    v_healings_deleted INTEGER;
    v_health_checks_deleted INTEGER;
    v_cutoff_date TIMESTAMP WITH TIME ZONE;
BEGIN
    v_cutoff_date := NOW() - (retention_days || ' days')::INTERVAL;

    -- Delete old error logs (resolved or older than retention)
    DELETE FROM error_logs
    WHERE occurred_at < v_cutoff_date
       OR (is_resolved = TRUE AND resolved_at < v_cutoff_date - INTERVAL '7 days');
    GET DIAGNOSTICS v_errors_deleted = ROW_COUNT;

    -- Delete old healing actions
    DELETE FROM healing_actions
    WHERE created_at < v_cutoff_date;
    GET DIAGNOSTICS v_healings_deleted = ROW_COUNT;

    -- Delete old health checks (keep less - only 7 days)
    DELETE FROM health_checks
    WHERE checked_at < NOW() - INTERVAL '7 days';
    GET DIAGNOSTICS v_health_checks_deleted = ROW_COUNT;

    RETURN QUERY SELECT v_errors_deleted, v_healings_deleted, v_health_checks_deleted;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION cleanup_old_error_logs IS 'Remove old error logs, healing actions, and health checks. Default 30 day retention for errors, 7 days for health checks.';

-- ============================================================================
-- FUNCTION: Record circuit breaker failure
-- ============================================================================
CREATE OR REPLACE FUNCTION record_circuit_failure(
    p_service_name VARCHAR(100),
    p_failure_reason TEXT DEFAULT NULL
)
RETURNS circuit_breaker_states AS $$
DECLARE
    v_result circuit_breaker_states;
BEGIN
    INSERT INTO circuit_breaker_states (service_name, state, failure_count, last_failure_at, last_failure_reason)
    VALUES (p_service_name, 'closed', 1, NOW(), p_failure_reason)
    ON CONFLICT (service_name) DO UPDATE SET
        failure_count = circuit_breaker_states.failure_count + 1,
        last_failure_at = NOW(),
        last_failure_reason = COALESCE(p_failure_reason, circuit_breaker_states.last_failure_reason),
        state = CASE
            WHEN circuit_breaker_states.failure_count + 1 >= circuit_breaker_states.failure_threshold
            THEN 'open'
            ELSE circuit_breaker_states.state
        END,
        opened_at = CASE
            WHEN circuit_breaker_states.failure_count + 1 >= circuit_breaker_states.failure_threshold
                 AND circuit_breaker_states.state = 'closed'
            THEN NOW()
            ELSE circuit_breaker_states.opened_at
        END
    RETURNING * INTO v_result;

    RETURN v_result;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION record_circuit_failure IS 'Record a failure for a service. Automatically opens circuit if threshold exceeded.';

-- ============================================================================
-- FUNCTION: Record circuit breaker success
-- ============================================================================
CREATE OR REPLACE FUNCTION record_circuit_success(
    p_service_name VARCHAR(100)
)
RETURNS circuit_breaker_states AS $$
DECLARE
    v_result circuit_breaker_states;
BEGIN
    INSERT INTO circuit_breaker_states (service_name, state, success_count, last_success_at)
    VALUES (p_service_name, 'closed', 1, NOW())
    ON CONFLICT (service_name) DO UPDATE SET
        success_count = CASE
            WHEN circuit_breaker_states.state = 'half_open'
            THEN circuit_breaker_states.success_count + 1
            ELSE 1
        END,
        failure_count = CASE
            WHEN circuit_breaker_states.state = 'closed' THEN 0
            ELSE circuit_breaker_states.failure_count
        END,
        last_success_at = NOW(),
        state = CASE
            WHEN circuit_breaker_states.state = 'half_open'
                 AND circuit_breaker_states.success_count + 1 >= circuit_breaker_states.success_threshold
            THEN 'closed'
            ELSE circuit_breaker_states.state
        END,
        closed_at = CASE
            WHEN circuit_breaker_states.state = 'half_open'
                 AND circuit_breaker_states.success_count + 1 >= circuit_breaker_states.success_threshold
            THEN NOW()
            ELSE circuit_breaker_states.closed_at
        END
    RETURNING * INTO v_result;

    RETURN v_result;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION record_circuit_success IS 'Record a success for a service. Closes circuit if success threshold reached in half-open state.';

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid error categories
ALTER TABLE error_logs ADD CONSTRAINT error_logs_category_valid
    CHECK (error_category IN ('DATABASE', 'EXTERNAL_API', 'VALIDATION', 'AUTH', 'SYSTEM', 'WEBSOCKET', 'NETWORK', 'BUSINESS_LOGIC'));

-- Valid severity levels
ALTER TABLE error_logs ADD CONSTRAINT error_logs_severity_valid
    CHECK (severity IN ('debug', 'info', 'warning', 'error', 'critical'));

-- Valid environments
ALTER TABLE error_logs ADD CONSTRAINT error_logs_environment_valid
    CHECK (environment IS NULL OR environment IN ('development', 'staging', 'production', 'test'));

-- Valid action types
ALTER TABLE healing_actions ADD CONSTRAINT healing_actions_type_valid
    CHECK (action_type IN ('retry', 'fallback', 'circuit_break', 'restart', 'reconnect', 'cache_clear', 'queue_retry', 'manual'));

-- Valid circuit breaker states
ALTER TABLE circuit_breaker_states ADD CONSTRAINT circuit_breaker_state_valid
    CHECK (state IN ('closed', 'open', 'half_open'));

-- Non-negative values
ALTER TABLE error_logs ADD CONSTRAINT error_logs_source_line_valid
    CHECK (source_line IS NULL OR source_line > 0);
ALTER TABLE healing_actions ADD CONSTRAINT healing_actions_attempt_valid
    CHECK (attempt_number > 0);
ALTER TABLE healing_actions ADD CONSTRAINT healing_actions_execution_time_valid
    CHECK (execution_time_ms IS NULL OR execution_time_ms >= 0);
ALTER TABLE circuit_breaker_states ADD CONSTRAINT circuit_breaker_counts_valid
    CHECK (failure_count >= 0 AND success_count >= 0);
ALTER TABLE health_checks ADD CONSTRAINT health_checks_response_time_valid
    CHECK (response_time_ms IS NULL OR response_time_ms >= 0);

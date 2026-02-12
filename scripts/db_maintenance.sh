#!/bin/bash
# WaitingTheLongest.com - Database Maintenance Script
# Runs VACUUM ANALYZE and reports table statistics
# Usage: ./scripts/db_maintenance.sh
# Schedule: Run weekly via cron

set -euo pipefail

CONTAINER_NAME="wtl-db"
DB_NAME="${DB_NAME:-waitingthelongest}"
DB_USER="${DB_USER:-postgres}"

# Load environment
if [ -f .env ]; then
    export $(grep -v '^#' .env | xargs)
fi

echo "============================================="
echo " WaitingTheLongest Database Maintenance"
echo " $(date)"
echo "============================================="

echo ""
echo "--- Running VACUUM ANALYZE ---"
docker exec "${CONTAINER_NAME}" psql -U "${DB_USER}" -d "${DB_NAME}" -c "VACUUM ANALYZE;"
echo "VACUUM ANALYZE complete."

echo ""
echo "--- Table Statistics ---"
docker exec "${CONTAINER_NAME}" psql -U "${DB_USER}" -d "${DB_NAME}" -c "
SELECT
    schemaname || '.' || relname AS table_name,
    n_live_tup AS live_rows,
    n_dead_tup AS dead_rows,
    CASE WHEN n_live_tup > 0
        THEN round(100.0 * n_dead_tup / n_live_tup, 1)
        ELSE 0
    END AS dead_pct,
    last_vacuum::date AS last_vacuum,
    last_analyze::date AS last_analyze,
    pg_size_pretty(pg_total_relation_size(relid)) AS total_size
FROM pg_stat_user_tables
ORDER BY n_live_tup DESC
LIMIT 30;
"

echo ""
echo "--- Database Size ---"
docker exec "${CONTAINER_NAME}" psql -U "${DB_USER}" -d "${DB_NAME}" -c "
SELECT pg_size_pretty(pg_database_size('${DB_NAME}')) AS database_size;
"

echo ""
echo "--- Index Usage ---"
docker exec "${CONTAINER_NAME}" psql -U "${DB_USER}" -d "${DB_NAME}" -c "
SELECT
    schemaname || '.' || relname AS table_name,
    indexrelname AS index_name,
    idx_scan AS times_used,
    pg_size_pretty(pg_relation_size(indexrelid)) AS index_size
FROM pg_stat_user_indexes
WHERE idx_scan = 0
ORDER BY pg_relation_size(indexrelid) DESC
LIMIT 20;
"

echo ""
echo "--- Slow Queries (if pg_stat_statements available) ---"
docker exec "${CONTAINER_NAME}" psql -U "${DB_USER}" -d "${DB_NAME}" -c "
SELECT
    substring(query, 1, 80) AS query_preview,
    calls,
    round(total_exec_time::numeric, 2) AS total_ms,
    round(mean_exec_time::numeric, 2) AS avg_ms,
    rows
FROM pg_stat_statements
WHERE dbid = (SELECT oid FROM pg_database WHERE datname = '${DB_NAME}')
ORDER BY mean_exec_time DESC
LIMIT 10;
" 2>/dev/null || echo "pg_stat_statements not available (enable in postgresql.conf)"

echo ""
echo "Maintenance complete at $(date)"

#!/bin/bash
# WaitingTheLongest.com - Database Backup Script
# Usage: ./scripts/backup_db.sh [daily|weekly|manual]
# Designed to be run via cron or manually

set -euo pipefail

BACKUP_TYPE="${1:-daily}"
BACKUP_DIR="/app/backups/${BACKUP_TYPE}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="${BACKUP_DIR}/wtl_${BACKUP_TYPE}_${TIMESTAMP}.sql.gz"
CONTAINER_NAME="wtl-db"

# Load environment
if [ -f .env ]; then
    export $(grep -v '^#' .env | xargs)
fi

DB_NAME="${DB_NAME:-waitingthelongest}"
DB_USER="${DB_USER:-postgres}"
RETENTION_DAILY=7
RETENTION_WEEKLY=4

# Create backup directory
mkdir -p "${BACKUP_DIR}"

echo "[$(date)] Starting ${BACKUP_TYPE} backup..."

# Run pg_dump inside the container
docker exec "${CONTAINER_NAME}" pg_dump -U "${DB_USER}" -d "${DB_NAME}" --format=plain | gzip > "${BACKUP_FILE}"

BACKUP_SIZE=$(du -h "${BACKUP_FILE}" | cut -f1)
echo "[$(date)] Backup complete: ${BACKUP_FILE} (${BACKUP_SIZE})"

# Cleanup old backups
if [ "${BACKUP_TYPE}" = "daily" ]; then
    find "${BACKUP_DIR}" -name "*.sql.gz" -mtime +${RETENTION_DAILY} -delete
    echo "[$(date)] Cleaned backups older than ${RETENTION_DAILY} days"
elif [ "${BACKUP_TYPE}" = "weekly" ]; then
    find "${BACKUP_DIR}" -name "*.sql.gz" -mtime +$((RETENTION_WEEKLY * 7)) -delete
    echo "[$(date)] Cleaned backups older than ${RETENTION_WEEKLY} weeks"
fi

echo "[$(date)] Backup job finished successfully"

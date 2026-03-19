#!/bin/bash
# Weekly Backup Script for WaitingTheLongest.com
# Exports all database tables and site code, zips them, uploads to Google Drive
#
# Prerequisites:
#   1. Install rclone: https://rclone.org/install/
#   2. Configure Google Drive remote: rclone config
#      - Name: gdrive
#      - Type: drive
#      - Scope: drive.file
#   3. Set CRON_SECRET env var or pass it as first argument
#   4. Add to crontab: 0 4 * * 0 /path/to/backup-to-drive.sh
#
# Usage: ./backup-to-drive.sh [CRON_SECRET]

set -euo pipefail

SITE_URL="${SITE_URL:-https://waitingthelongest.vercel.app}"
CRON_SECRET="${1:-${CRON_SECRET:-}}"
BACKUP_DIR="/tmp/wtl-backup-$(date +%Y%m%d)"
GDRIVE_REMOTE="gdrive"
GDRIVE_FOLDER="WaitingTheLongest-Backups"
DATE_STAMP=$(date +%Y-%m-%d)

if [ -z "$CRON_SECRET" ]; then
  echo "ERROR: CRON_SECRET required. Pass as argument or set env var."
  exit 1
fi

echo "=== WaitingTheLongest.com Backup - $DATE_STAMP ==="

# Create backup directory
rm -rf "$BACKUP_DIR"
mkdir -p "$BACKUP_DIR/database"

# Step 1: Export each database table
echo "Exporting database tables..."
AUTH="Authorization: Bearer $CRON_SECRET"

# Get manifest (table counts)
curl -sf "$SITE_URL/api/backup" -H "$AUTH" > "$BACKUP_DIR/database/manifest.json"
echo "  Manifest saved"

# Export each table page by page
for TABLE in dogs shelters states breeds audit_logs audit_runs; do
  echo "  Exporting $TABLE..."
  PAGE=1
  HAS_MORE=true

  while [ "$HAS_MORE" = "true" ]; do
    RESPONSE=$(curl -sf "$SITE_URL/api/backup?table=$TABLE&page=$PAGE&limit=5000" -H "$AUTH")
    echo "$RESPONSE" > "$BACKUP_DIR/database/${TABLE}_page${PAGE}.json"

    HAS_MORE=$(echo "$RESPONSE" | python3 -c "import sys,json; print(str(json.load(sys.stdin).get('has_more', False)).lower())" 2>/dev/null || echo "false")
    RECORDS=$(echo "$RESPONSE" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d.get('data',[])))" 2>/dev/null || echo "0")
    echo "    Page $PAGE: $RECORDS records"

    PAGE=$((PAGE + 1))

    # Safety limit
    if [ "$PAGE" -gt 100 ]; then
      echo "    WARNING: Hit page limit (100)"
      break
    fi
  done
done

# Step 2: Git repo snapshot
echo "Creating code snapshot..."
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ -d "$REPO_DIR/.git" ]; then
  git -C "$REPO_DIR" archive --format=tar.gz HEAD > "$BACKUP_DIR/site-code.tar.gz"
  echo "  Code archive created"
else
  echo "  WARNING: Not a git repo, skipping code backup"
fi

# Step 3: Create final zip
echo "Creating backup archive..."
ZIP_FILE="/tmp/wtl-backup-${DATE_STAMP}.zip"
cd /tmp
zip -r "$ZIP_FILE" "$(basename $BACKUP_DIR)" -q
BACKUP_SIZE=$(du -h "$ZIP_FILE" | cut -f1)
echo "  Archive: $ZIP_FILE ($BACKUP_SIZE)"

# Step 4: Upload to Google Drive
echo "Uploading to Google Drive..."
if command -v rclone &> /dev/null; then
  rclone mkdir "$GDRIVE_REMOTE:$GDRIVE_FOLDER" 2>/dev/null || true
  rclone copy "$ZIP_FILE" "$GDRIVE_REMOTE:$GDRIVE_FOLDER/" --progress
  echo "  Uploaded to $GDRIVE_REMOTE:$GDRIVE_FOLDER/"

  # Clean up old backups (keep last 12 weeks)
  echo "Cleaning old backups..."
  rclone ls "$GDRIVE_REMOTE:$GDRIVE_FOLDER/" | sort | head -n -12 | while read SIZE FILE; do
    echo "  Removing old backup: $FILE"
    rclone delete "$GDRIVE_REMOTE:$GDRIVE_FOLDER/$FILE" 2>/dev/null || true
  done
else
  echo "  WARNING: rclone not installed. Backup saved locally at: $ZIP_FILE"
  echo "  Install rclone and configure Google Drive to enable automatic uploads."
  echo "  See: https://rclone.org/drive/"
fi

# Cleanup
rm -rf "$BACKUP_DIR"
echo ""
echo "=== Backup Complete ==="
echo "Date: $DATE_STAMP"
echo "Size: $BACKUP_SIZE"
echo "Location: $GDRIVE_REMOTE:$GDRIVE_FOLDER/wtl-backup-${DATE_STAMP}.zip"

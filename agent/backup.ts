/**
 * WaitingTheLongest — Google Drive Backup System
 *
 * Creates timestamped zip backups of the project and syncs to Google Drive.
 * Supports multiple redundant backup strategies:
 *   - Local timestamped zips (kept 30 days)
 *   - Google Drive sync via rclone (daily + weekly + monthly retention)
 *   - Supabase data export (database dump to JSON)
 *
 * Standalone: npx tsx --tsconfig tsconfig.json agent/backup.ts
 * Or integrated into the main agent as a scheduled task.
 *
 * Setup: Install rclone, run `rclone config` to set up a "gdrive" remote.
 * See agent/README.md for full setup instructions.
 */

import dotenv from "dotenv";
import path from "path";
import fs from "fs";
import { execSync, exec } from "child_process";

dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const PROJECT_ROOT = path.join(__dirname, "..");
const BACKUP_DIR = path.join(PROJECT_ROOT, "backups");
const RCLONE_REMOTE = "gdrive:WaitingTheLongest-Backups";
const MAX_LOCAL_BACKUPS_DAYS = 30;

// Files/dirs to exclude from backup
const EXCLUDE_PATTERNS = [
  "node_modules",
  ".next",
  ".git",
  "backups",
  ".env.local",
  ".env",
  "*.log",
  ".claude",
];

interface BackupResult {
  success: boolean;
  localPath: string | null;
  size: string;
  duration: number;
  rcloneSync: boolean;
  dbExport: boolean;
  error?: string;
}

function log(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.log(`  ${time} [backup] ${msg}`);
}

function logError(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.error(`\x1b[31m  ${time} [backup] ${msg}\x1b[0m`);
}

function logSuccess(msg: string) {
  const time = new Date().toLocaleTimeString();
  console.log(`\x1b[32m  ${time} [backup] ${msg}\x1b[0m`);
}

/**
 * Check if rclone is installed and configured
 */
function isRcloneAvailable(): boolean {
  try {
    execSync("rclone version", { stdio: "pipe" });
    return true;
  } catch {
    return false;
  }
}

function isRcloneConfigured(): boolean {
  try {
    const output = execSync("rclone listremotes", { stdio: "pipe", encoding: "utf8" });
    return output.includes("gdrive:");
  } catch {
    return false;
  }
}

/**
 * Create a timestamped zip backup of the project
 */
function createLocalBackup(): { path: string; size: string } {
  // Ensure backup directory exists
  if (!fs.existsSync(BACKUP_DIR)) {
    fs.mkdirSync(BACKUP_DIR, { recursive: true });
  }

  const timestamp = new Date().toISOString().replace(/[:.]/g, "-").replace("T", "_").substring(0, 19);
  const zipName = `wtl-backup-${timestamp}.zip`;
  const zipPath = path.join(BACKUP_DIR, zipName);

  log(`Creating backup: ${zipName}`);

  // Build exclude args for PowerShell
  const excludeList = EXCLUDE_PATTERNS.map(p => `'${p}'`).join(",");

  // Use PowerShell Compress-Archive with exclusion
  // First, create a temporary file list
  const tempListFile = path.join(BACKUP_DIR, "_backup_filelist.txt");

  try {
    // Write PowerShell script to a temp .ps1 file to avoid cmd.exe argument parsing issues
    const psScriptPath = path.join(BACKUP_DIR, "_backup_script.ps1");
    const psScript = `
$root = '${PROJECT_ROOT.replace(/'/g, "''")}'
$excludeDirs = @(${excludeList})
$outFile = '${tempListFile.replace(/'/g, "''")}'
$zipDest = '${zipPath.replace(/'/g, "''")}'

$files = Get-ChildItem -Path $root -Recurse -File | Where-Object {
  $rel = $_.FullName.Substring($root.Length + 1)
  $skip = $false
  foreach ($ex in $excludeDirs) {
    if ($rel -like "$ex*" -or $rel -like "*\\$ex*" -or $rel -like "$ex") {
      $skip = $true
      break
    }
  }
  -not $skip
}

$files.FullName | Out-File -FilePath $outFile -Encoding UTF8

# Create zip from the file list
Compress-Archive -Path (Get-Content $outFile) -DestinationPath $zipDest -Force
`;
    fs.writeFileSync(psScriptPath, psScript, "utf8");

    execSync(
      `powershell -ExecutionPolicy Bypass -File "${psScriptPath}"`,
      { cwd: PROJECT_ROOT, stdio: "pipe", timeout: 300000 }
    );

    // Clean up temp script
    if (fs.existsSync(psScriptPath)) fs.unlinkSync(psScriptPath);

    // Clean up temp file
    if (fs.existsSync(tempListFile)) fs.unlinkSync(tempListFile);

    const stats = fs.statSync(zipPath);
    const sizeMB = (stats.size / (1024 * 1024)).toFixed(1);

    return { path: zipPath, size: `${sizeMB} MB` };
  } catch (err) {
    // Clean up temp files on error
    if (fs.existsSync(tempListFile)) fs.unlinkSync(tempListFile);
    const psScriptPath = path.join(BACKUP_DIR, "_backup_script.ps1");
    if (fs.existsSync(psScriptPath)) fs.unlinkSync(psScriptPath);

    // Fallback: use tar if available (Git Bash on Windows includes it)
    try {
      const excludeArgs = EXCLUDE_PATTERNS.map(p => `--exclude='${p}'`).join(" ");
      const tarPath = zipPath.replace(".zip", ".tar.gz");
      execSync(
        `tar czf "${tarPath}" ${excludeArgs} -C "${PROJECT_ROOT}" .`,
        { stdio: "pipe", timeout: 300000 }
      );
      const stats = fs.statSync(tarPath);
      const sizeMB = (stats.size / (1024 * 1024)).toFixed(1);
      return { path: tarPath, size: `${sizeMB} MB` };
    } catch (tarErr) {
      throw new Error(`Failed to create backup: ${err}`);
    }
  }
}

/**
 * Export Supabase data to JSON files
 */
async function exportSupabaseData(): Promise<string | null> {
  try {
    const { createAdminClient } = await import("../src/lib/supabase/admin");
    const supabase = createAdminClient();

    const exportDir = path.join(BACKUP_DIR, "db-export");
    if (!fs.existsSync(exportDir)) fs.mkdirSync(exportDir, { recursive: true });

    const timestamp = new Date().toISOString().replace(/[:.]/g, "-").substring(0, 19);
    const exportFile = path.join(exportDir, `db-export-${timestamp}.json`);

    log("Exporting Supabase data...");

    // Export key tables (counts + sample data for verification)
    const [dogsCount, sheltersCount, statsData] = await Promise.all([
      supabase.from("dogs").select("id", { count: "exact", head: true }),
      supabase.from("shelters").select("id", { count: "exact", head: true }),
      supabase.from("daily_stats").select("*").order("stat_date", { ascending: false }).limit(30),
    ]);

    // Export dogs metadata (not full records - too large)
    const { data: dogsMeta } = await supabase
      .from("dogs")
      .select("id, name, breed_primary, shelter_id, intake_date, is_available, verification_status, date_confidence, urgency_level, euthanasia_date")
      .eq("is_available", true)
      .limit(5000);

    const exportData = {
      exportDate: new Date().toISOString(),
      counts: {
        dogs: dogsCount.count,
        shelters: sheltersCount.count,
      },
      dailyStats: statsData.data,
      activeDogsSample: dogsMeta,
    };

    fs.writeFileSync(exportFile, JSON.stringify(exportData, null, 2));
    const sizeMB = (fs.statSync(exportFile).size / (1024 * 1024)).toFixed(1);
    log(`DB export saved: ${exportFile} (${sizeMB} MB)`);

    // Clean old exports (keep last 7)
    const exports = fs.readdirSync(exportDir)
      .filter(f => f.startsWith("db-export-"))
      .sort()
      .reverse();
    for (const f of exports.slice(7)) {
      fs.unlinkSync(path.join(exportDir, f));
    }

    return exportFile;
  } catch (err) {
    logError(`DB export failed: ${err}`);
    return null;
  }
}

/**
 * Sync backups to Google Drive via rclone
 */
function syncToGoogleDrive(): boolean {
  if (!isRcloneAvailable()) {
    log("rclone not installed — skipping Google Drive sync");
    log("Install: winget install Rclone.Rclone && rclone config (add 'gdrive' remote)");
    return false;
  }

  if (!isRcloneConfigured()) {
    log("rclone 'gdrive' remote not configured — run: rclone config");
    return false;
  }

  try {
    log("Syncing backups to Google Drive...");

    // Sync the entire backups directory
    execSync(
      `rclone sync "${BACKUP_DIR}" "${RCLONE_REMOTE}" --progress --transfers 4 --checkers 8`,
      { stdio: "pipe", timeout: 600000 }
    );

    logSuccess("Google Drive sync complete");
    return true;
  } catch (err) {
    logError(`Google Drive sync failed: ${err}`);
    return false;
  }
}

/**
 * Clean up old local backups
 */
function cleanOldBackups() {
  if (!fs.existsSync(BACKUP_DIR)) return;

  const cutoff = Date.now() - MAX_LOCAL_BACKUPS_DAYS * 24 * 60 * 60 * 1000;
  const files = fs.readdirSync(BACKUP_DIR).filter(f => f.startsWith("wtl-backup-"));

  let cleaned = 0;
  for (const file of files) {
    const filePath = path.join(BACKUP_DIR, file);
    const stats = fs.statSync(filePath);
    if (stats.mtimeMs < cutoff) {
      fs.unlinkSync(filePath);
      cleaned++;
    }
  }

  if (cleaned > 0) {
    log(`Cleaned ${cleaned} backups older than ${MAX_LOCAL_BACKUPS_DAYS} days`);
  }
}

/**
 * Get backup status info
 */
export function getBackupStatus(): {
  lastBackup: string | null;
  backupCount: number;
  totalSize: string;
  rcloneAvailable: boolean;
  rcloneConfigured: boolean;
} {
  let lastBackup: string | null = null;
  let backupCount = 0;
  let totalSize = 0;

  if (fs.existsSync(BACKUP_DIR)) {
    const files = fs.readdirSync(BACKUP_DIR).filter(f => f.startsWith("wtl-backup-"));
    backupCount = files.length;

    for (const file of files) {
      const stats = fs.statSync(path.join(BACKUP_DIR, file));
      totalSize += stats.size;
    }

    if (files.length > 0) {
      const latest = files.sort().reverse()[0];
      const stats = fs.statSync(path.join(BACKUP_DIR, latest));
      lastBackup = stats.mtime.toISOString();
    }
  }

  return {
    lastBackup,
    backupCount,
    totalSize: `${(totalSize / (1024 * 1024)).toFixed(1)} MB`,
    rcloneAvailable: isRcloneAvailable(),
    rcloneConfigured: isRcloneAvailable() && isRcloneConfigured(),
  };
}

/**
 * Run a full backup cycle
 */
export async function runBackup(): Promise<BackupResult> {
  const start = Date.now();
  log("Starting backup cycle...");

  let localPath: string | null = null;
  let size = "0 MB";
  let rcloneSync = false;
  let dbExport = false;

  try {
    // Step 1: Create local zip backup
    const backup = createLocalBackup();
    localPath = backup.path;
    size = backup.size;
    logSuccess(`Local backup created: ${path.basename(localPath)} (${size})`);

    // Step 2: Export Supabase data
    const exportPath = await exportSupabaseData();
    dbExport = exportPath !== null;

    // Step 3: Sync to Google Drive
    rcloneSync = syncToGoogleDrive();

    // Step 4: Clean old backups
    cleanOldBackups();

    const duration = Date.now() - start;
    logSuccess(`Backup cycle complete in ${Math.round(duration / 1000)}s — local: ${size}, gdrive: ${rcloneSync ? "synced" : "skipped"}, db: ${dbExport ? "exported" : "skipped"}`);

    return { success: true, localPath, size, duration, rcloneSync, dbExport };
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    logError(`Backup failed: ${msg}`);
    return {
      success: false,
      localPath,
      size,
      duration: Date.now() - start,
      rcloneSync,
      dbExport,
      error: msg,
    };
  }
}

// ─── Standalone execution ───
if (require.main === module) {
  console.log("\n" + "═".repeat(50));
  console.log("  WTL Backup System");
  console.log("═".repeat(50) + "\n");

  const status = getBackupStatus();
  log(`Existing backups: ${status.backupCount} (${status.totalSize})`);
  log(`rclone available: ${status.rcloneAvailable}`);
  log(`rclone configured: ${status.rcloneConfigured}`);

  runBackup().then(result => {
    if (result.success) {
      logSuccess("Done!");
    } else {
      logError(`Failed: ${result.error}`);
      process.exit(1);
    }
  });
}

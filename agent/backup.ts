/**
 * WaitingTheLongest — Google Drive Backup System
 *
 * Creates timestamped zip backups of the project and syncs to Google Drive.
 * Supports multiple redundant backup strategies:
 *   - Local timestamped zips (kept 30 days)
 *   - Google Drive sync via rclone with redundant versioning:
 *     - daily/   — last 7 days
 *     - weekly/  — last 4 weeks (Sunday snapshots)
 *     - monthly/ — last 12 months (1st of month snapshots)
 *   - Git bundle (full repo history in a single file)
 *   - Supabase data export (database dump to JSON)
 *
 * Standalone: npx tsx --tsconfig tsconfig.json agent/backup.ts
 * Or integrated into the main agent as a scheduled task.
 *
 * Setup: Install rclone, run `rclone config` to set up a "gdrive" remote.
 */

import dotenv from "dotenv";
import path from "path";
import fs from "fs";
import { execSync } from "child_process";

dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

const PROJECT_ROOT = path.join(__dirname, "..");
const BACKUP_DIR = path.join(PROJECT_ROOT, "backups");
const RCLONE_REMOTE = "gdrive:WaitingTheLongest-Backups";
const MAX_LOCAL_BACKUPS_DAYS = 30;

// Redundant versioning retention
const RETENTION = {
  daily: 7,    // keep last 7 daily backups
  weekly: 4,   // keep last 4 weekly backups (Sundays)
  monthly: 12, // keep last 12 monthly backups (1st of month)
};

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
  gitBundle: string | null;
  size: string;
  duration: number;
  rcloneSync: boolean;
  dbExport: boolean;
  versioningTier: "daily" | "weekly" | "monthly";
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
 * Find rclone binary — checks PATH then common Windows install locations
 */
function findRclone(): string | null {
  // Try PATH first
  try {
    execSync("rclone version", { stdio: "pipe" });
    return "rclone";
  } catch { /* not in PATH */ }

  // Check common Windows locations
  const home = process.env.USERPROFILE || process.env.HOME || "";
  const candidates = [
    path.join(home, "bin", "rclone.exe"),
    path.join(home, "AppData", "Local", "Programs", "rclone", "rclone.exe"),
    "C:\\Program Files\\rclone\\rclone.exe",
    "C:\\ProgramData\\chocolatey\\bin\\rclone.exe",
  ];

  // Also check winget packages folder dynamically
  const wingetPkgs = path.join(home, "AppData", "Local", "Microsoft", "WinGet", "Packages");
  if (fs.existsSync(wingetPkgs)) {
    try {
      const dirs = fs.readdirSync(wingetPkgs).filter(d => d.toLowerCase().includes("rclone"));
      for (const dir of dirs) {
        const pkgDir = path.join(wingetPkgs, dir);
        const subDirs = fs.readdirSync(pkgDir).filter(d => d.startsWith("rclone-"));
        for (const sub of subDirs) {
          candidates.push(path.join(pkgDir, sub, "rclone.exe"));
        }
      }
    } catch { /* ignore */ }
  }

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      try {
        execSync(`"${candidate}" version`, { stdio: "pipe" });
        return candidate;
      } catch { /* binary exists but doesn't work */ }
    }
  }

  return null;
}

let _rclonePath: string | null | undefined;
function getRclonePath(): string | null {
  if (_rclonePath === undefined) _rclonePath = findRclone();
  return _rclonePath;
}

function rcloneExec(args: string, options?: { timeout?: number }): string {
  const bin = getRclonePath();
  if (!bin) throw new Error("rclone not found");
  return execSync(`"${bin}" ${args}`, {
    stdio: "pipe",
    encoding: "utf8",
    timeout: options?.timeout ?? 120000,
  });
}

function isRcloneAvailable(): boolean {
  return getRclonePath() !== null;
}

function isRcloneConfigured(): boolean {
  try {
    const output = rcloneExec("listremotes");
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
 * Create a git bundle (full repo history in a single file)
 */
function createGitBundle(): string | null {
  try {
    const timestamp = new Date().toISOString().replace(/[:.]/g, "-").replace("T", "_").substring(0, 19);
    const bundleName = `wtl-repo-${timestamp}.bundle`;
    const bundlePath = path.join(BACKUP_DIR, bundleName);

    log("Creating git bundle...");
    execSync(`git bundle create "${bundlePath}" --all`, {
      cwd: PROJECT_ROOT,
      stdio: "pipe",
      timeout: 120000,
    });

    const sizeMB = (fs.statSync(bundlePath).size / (1024 * 1024)).toFixed(1);
    logSuccess(`Git bundle created: ${bundleName} (${sizeMB} MB)`);

    // Keep only last 3 bundles locally
    const bundles = fs.readdirSync(BACKUP_DIR)
      .filter(f => f.startsWith("wtl-repo-") && f.endsWith(".bundle"))
      .sort()
      .reverse();
    for (const f of bundles.slice(3)) {
      fs.unlinkSync(path.join(BACKUP_DIR, f));
    }

    return bundlePath;
  } catch (err) {
    logError(`Git bundle failed: ${err}`);
    return null;
  }
}

/**
 * Determine which versioning tier this backup belongs to
 */
function getVersioningTier(): "daily" | "weekly" | "monthly" {
  const now = new Date();
  if (now.getDate() === 1) return "monthly";
  if (now.getDay() === 0) return "weekly"; // Sunday
  return "daily";
}

/**
 * Sync backups to Google Drive with redundant versioning
 *
 * Folder structure on Google Drive:
 *   WaitingTheLongest-Backups/
 *     latest/        — always the most recent backup
 *     daily/         — last 7 days
 *     weekly/        — last 4 weeks
 *     monthly/       — last 12 months
 *     git-bundles/   — full repo bundles
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
    log("Syncing to Google Drive with redundant versioning...");

    const now = new Date();
    const dateStr = now.toISOString().substring(0, 10); // YYYY-MM-DD
    const tier = getVersioningTier();

    // 1. Copy latest backup to the appropriate tier folder
    const latestZip = fs.readdirSync(BACKUP_DIR)
      .filter(f => f.startsWith("wtl-backup-") && f.endsWith(".zip"))
      .sort()
      .reverse()[0];

    if (!latestZip) {
      logError("No zip backup found to sync");
      return false;
    }

    const latestZipPath = path.join(BACKUP_DIR, latestZip);

    // Upload to latest/ (always)
    rcloneExec(
      `copyto "${latestZipPath}" "${RCLONE_REMOTE}/latest/wtl-backup-latest.zip"`,
      { timeout: 600000 }
    );
    log("  uploaded to latest/");

    // Upload to tier folder with date stamp
    rcloneExec(
      `copyto "${latestZipPath}" "${RCLONE_REMOTE}/${tier}/wtl-backup-${dateStr}.zip"`,
      { timeout: 600000 }
    );
    log(`  uploaded to ${tier}/ (${dateStr})`);

    // Upload DB export if it exists
    const dbExportDir = path.join(BACKUP_DIR, "db-export");
    if (fs.existsSync(dbExportDir)) {
      const latestExport = fs.readdirSync(dbExportDir)
        .filter(f => f.startsWith("db-export-"))
        .sort()
        .reverse()[0];
      if (latestExport) {
        rcloneExec(
          `copyto "${path.join(dbExportDir, latestExport)}" "${RCLONE_REMOTE}/latest/db-export-latest.json"`,
          { timeout: 300000 }
        );
        rcloneExec(
          `copyto "${path.join(dbExportDir, latestExport)}" "${RCLONE_REMOTE}/${tier}/db-export-${dateStr}.json"`,
          { timeout: 300000 }
        );
        log("  uploaded db export");
      }
    }

    // Upload git bundle
    const latestBundle = fs.readdirSync(BACKUP_DIR)
      .filter(f => f.startsWith("wtl-repo-") && f.endsWith(".bundle"))
      .sort()
      .reverse()[0];
    if (latestBundle) {
      rcloneExec(
        `copyto "${path.join(BACKUP_DIR, latestBundle)}" "${RCLONE_REMOTE}/git-bundles/wtl-repo-${dateStr}.bundle"`,
        { timeout: 600000 }
      );
      log("  uploaded git bundle");
    }

    // 2. Enforce retention — delete old files in each tier
    enforceRetention("daily", RETENTION.daily);
    enforceRetention("weekly", RETENTION.weekly);
    enforceRetention("monthly", RETENTION.monthly);
    enforceRetention("git-bundles", RETENTION.monthly); // keep bundles on monthly schedule

    logSuccess(`Google Drive sync complete (tier: ${tier})`);
    return true;
  } catch (err) {
    logError(`Google Drive sync failed: ${err}`);
    return false;
  }
}

/**
 * Enforce retention policy on a Google Drive folder
 */
function enforceRetention(folder: string, keepCount: number) {
  try {
    const listing = rcloneExec(`lsf "${RCLONE_REMOTE}/${folder}/" --format "p" 2>/dev/null || true`);
    const files = listing.trim().split("\n").filter(f => f.length > 0).sort().reverse();

    if (files.length > keepCount) {
      const toDelete = files.slice(keepCount);
      for (const file of toDelete) {
        rcloneExec(`deletefile "${RCLONE_REMOTE}/${folder}/${file}"`);
      }
      log(`  ${folder}: pruned ${toDelete.length} old files (keeping ${keepCount})`);
    }
  } catch {
    // Folder may not exist yet — that's fine
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
  bundleCount: number;
  totalSize: string;
  rcloneAvailable: boolean;
  rcloneConfigured: boolean;
  rclonePath: string | null;
  nextTier: string;
} {
  let lastBackup: string | null = null;
  let backupCount = 0;
  let bundleCount = 0;
  let totalSize = 0;

  if (fs.existsSync(BACKUP_DIR)) {
    const zips = fs.readdirSync(BACKUP_DIR).filter(f => f.startsWith("wtl-backup-"));
    const bundles = fs.readdirSync(BACKUP_DIR).filter(f => f.startsWith("wtl-repo-") && f.endsWith(".bundle"));
    backupCount = zips.length;
    bundleCount = bundles.length;

    for (const file of [...zips, ...bundles]) {
      const stats = fs.statSync(path.join(BACKUP_DIR, file));
      totalSize += stats.size;
    }

    if (zips.length > 0) {
      const latest = zips.sort().reverse()[0];
      const stats = fs.statSync(path.join(BACKUP_DIR, latest));
      lastBackup = stats.mtime.toISOString();
    }
  }

  return {
    lastBackup,
    backupCount,
    bundleCount,
    totalSize: `${(totalSize / (1024 * 1024)).toFixed(1)} MB`,
    rcloneAvailable: isRcloneAvailable(),
    rcloneConfigured: isRcloneAvailable() && isRcloneConfigured(),
    rclonePath: getRclonePath(),
    nextTier: getVersioningTier(),
  };
}

/**
 * Run a full backup cycle
 */
export async function runBackup(): Promise<BackupResult> {
  const start = Date.now();
  const tier = getVersioningTier();
  log(`Starting backup cycle (tier: ${tier})...`);

  let localPath: string | null = null;
  let gitBundlePath: string | null = null;
  let size = "0 MB";
  let rcloneSync = false;
  let dbExport = false;

  try {
    // Step 1: Create local zip backup
    const backup = createLocalBackup();
    localPath = backup.path;
    size = backup.size;
    logSuccess(`Local backup created: ${path.basename(localPath)} (${size})`);

    // Step 2: Create git bundle
    gitBundlePath = createGitBundle();

    // Step 3: Export Supabase data
    const exportPath = await exportSupabaseData();
    dbExport = exportPath !== null;

    // Step 4: Sync to Google Drive with versioning
    rcloneSync = syncToGoogleDrive();

    // Step 5: Clean old local backups
    cleanOldBackups();

    const duration = Date.now() - start;
    logSuccess(`Backup cycle complete in ${Math.round(duration / 1000)}s — local: ${size}, git: ${gitBundlePath ? "bundled" : "skipped"}, gdrive: ${rcloneSync ? "synced" : "skipped"}, db: ${dbExport ? "exported" : "skipped"}, tier: ${tier}`);

    return { success: true, localPath, gitBundle: gitBundlePath, size, duration, rcloneSync, dbExport, versioningTier: tier };
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    logError(`Backup failed: ${msg}`);
    return {
      success: false,
      localPath,
      gitBundle: gitBundlePath,
      size,
      duration: Date.now() - start,
      rcloneSync,
      dbExport,
      versioningTier: tier,
      error: msg,
    };
  }
}

// ─── Standalone execution ───
if (require.main === module) {
  console.log("\n" + "═".repeat(50));
  console.log("  WTL Backup System — Redundant Versioning");
  console.log("═".repeat(50) + "\n");

  const status = getBackupStatus();
  log(`Local backups: ${status.backupCount} zips, ${status.bundleCount} bundles (${status.totalSize})`);
  log(`Last backup: ${status.lastBackup || "never"}`);
  log(`rclone path: ${status.rclonePath || "not found"}`);
  log(`rclone configured: ${status.rcloneConfigured}`);
  log(`Next backup tier: ${status.nextTier}`);
  log(`Retention: daily=${RETENTION.daily}, weekly=${RETENTION.weekly}, monthly=${RETENTION.monthly}`);

  // Support --setup flag to configure rclone
  if (process.argv.includes("--setup")) {
    if (!isRcloneAvailable()) {
      logError("rclone not installed. Run: winget install Rclone.Rclone");
      process.exit(1);
    }
    log("Launching rclone config — follow the prompts to add a 'gdrive' remote...");
    const { execSync: es } = require("child_process");
    const bin = getRclonePath();
    es(`"${bin}" config`, { stdio: "inherit" });
    process.exit(0);
  }

  // Support --status flag to just show status
  if (process.argv.includes("--status")) {
    process.exit(0);
  }

  runBackup().then(result => {
    if (result.success) {
      logSuccess("Done!");
    } else {
      logError(`Failed: ${result.error}`);
      process.exit(1);
    }
  });
}

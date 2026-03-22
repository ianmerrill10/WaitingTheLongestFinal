# WaitingTheLongest — Agent System

Three autonomous agents that keep the site healthy, accurate, and backed up 24/7.

## Quick Start

```bash
# Start all agents (run each in a separate terminal)
npm run agent           # Data agent (port 3847)
npm run agent:improve   # Improvement agent (port 3848)
npm run agent:backup    # One-time backup
```

Or manually:
```bash
cd C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal
npx tsx --tsconfig tsconfig.json agent/index.ts
```

## 1. Data Agent (`agent/index.ts`) — Port 3847

Main agent managing dog data accuracy. Dashboard at **http://localhost:3847**

### Features
- **Real-time dog verification slideshow** — see each dog being verified with photo, name, breed, shelter, wait time, and outcome
- **Error detection & auto-correction** — finds and fixes data inconsistencies on the fly
- **Circuit breaker** — stops tasks after 5 consecutive errors, retries after 30 min
- **Rate limit protection** — auto-pauses on 429 responses
- **Structured logging** with colored console output and SSE streaming
- **Shelter name cache** — pre-loads 5000 shelters for fast enrichment

### Scheduled Tasks

| Task | Interval | Description |
|------|----------|-------------|
| verify | 30 min | Verify 150 dogs against RescueGroups API |
| sync | 60 min | Sync 10 pages of new dogs from RescueGroups |
| errorFix | 3 hours | Scan for and auto-correct data errors |
| urgency | 2 hours | Update euthanasia urgency countdowns |
| audit | 4 hours | Quick data quality audit |
| stats | 6 hours | Compute daily stats snapshot |
| backup | 6 hours | Create zip backup + Google Drive sync |
| fullAudit | 24 hours | Full 9-check comprehensive audit |

**Throughput:** ~7,200 dogs/day (18x faster than Vercel crons)

### Auto-Corrections
1. Future intake dates → capped to today
2. Intake before birth date → set to birth date
3. Available but status='adopted' → mark unavailable
4. Missing primary photo → extract from photo_urls
5. Negative age → set to 0
6. Expired euthanasia dates (3+ days past) → clear
7. Duplicate external IDs → flagged

### Controls
- **Pause/Resume:** Dashboard button or `POST /api/pause` / `POST /api/resume`
- **Trigger task:** Dashboard button or `POST /api/trigger` with `{"task":"verify"}`
- **Metrics:** `GET /api/metrics`
- **Health:** `GET /health`
- **Stop:** Press Ctrl+C (graceful shutdown, waits up to 30s)

## 2. Improvement Agent (`agent/improve.ts`) — Port 3848

Perpetual code quality scanner. Dashboard at **http://localhost:3848**

### 10 Automated Checkers
1. **TypeScript Errors** — `tsc --noEmit`
2. **ESLint Issues** — JSON output parsing
3. **`any` Usage** — Explicit `any` types in source
4. **TODO/FIXME Comments** — Incomplete work items
5. **Console.log** — Debug output in production code
6. **Large Files** — Files over 400 lines
7. **API Error Handling** — Routes missing try/catch
8. **Security Patterns** — Hardcoded passwords, keys, tokens
9. **Missing Loading States** — Pages without loading.tsx
10. **npm Audit** — Dependency vulnerabilities

### Dashboard Features
- Category + severity breakdown bar charts
- Filterable findings list with fix/dismiss/reopen buttons
- File path and line number for each finding
- Auto-fix suggestions
- Scan progress + agent log

State persists to `agent/improve-state.json` between runs. Scans every 4 hours.

## 3. Backup System (`agent/backup.ts`)

Integrated into the data agent (every 6 hours) or run standalone.

### What Gets Backed Up
- Full project zip (excludes node_modules, .next, .git)
- Supabase data export (dogs metadata, daily stats)

### Retention: 30 days local, unlimited Google Drive

### Google Drive Setup

```bash
winget install Rclone.Rclone
rclone config
# New remote → name: gdrive → type: Google Drive → follow OAuth
rclone ls gdrive:   # Test connection
```

Without rclone, local backups still work in `backups/`.

## File Structure

```
agent/
├── index.ts              # Data agent (port 3847)
├── dashboard.html        # Data agent dashboard
├── backup.ts             # Backup system
├── improve.ts            # Improvement agent (port 3848)
├── improve-dashboard.html # Improvement dashboard
├── improve-state.json    # Persisted findings (auto-generated)
└── README.md             # This file
```

## Running as Background Services

```bash
# Option 1: pm2
npm install -g pm2
pm2 start "npm run agent" --name wtl-data-agent
pm2 start "npm run agent:improve" --name wtl-improve-agent
pm2 save && pm2 startup

# Option 2: Windows Task Scheduler
# Create task → trigger: at startup → action: npm run agent
```

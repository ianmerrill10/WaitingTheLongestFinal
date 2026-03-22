# WTL Local Agent

Persistent Node.js process that manages dog data accuracy 24/7. Runs on your local Windows PC, connects directly to Supabase + RescueGroups.

## Quick Start

```bash
cd C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal
npx tsx agent/index.ts
```

Open **http://localhost:3847** in your browser to see the real-time dashboard.

## What It Does

| Task | Interval | Description |
|------|----------|-------------|
| verify | 30 min | Verify 150 dogs against RescueGroups API (captures availableDate, foundDate, killDate) |
| sync | 60 min | Sync 10 pages from RescueGroups, auto-rotates through all 350 pages |
| urgency | 2 hours | Update euthanasia urgency levels |
| audit | 4 hours | Quick audit (dates, staleness, statuses, photos) |
| stats | 6 hours | Compute daily_stats snapshot |
| fullAudit | 24 hours | Full 9-check audit |

**Throughput:** ~7,200 dogs/day (18x faster than Vercel crons)

## Dashboard

The browser dashboard at localhost:3847 shows:
- Live log stream with color-coded entries
- Stats: uptime, dogs/hr, verified count, errors
- Progress bar for current operation
- Task queue with next run times
- Database health (verified %, date accuracy, urgent dogs)

## Controls

- **Pause:** Click "Pause" button on dashboard, or `POST http://localhost:3847/api/pause`
- **Resume:** Click "Resume" button, or `POST http://localhost:3847/api/resume`
- **Trigger task:** Click trigger buttons, or `POST http://localhost:3847/api/trigger` with `{"task":"verify"}`
- **Metrics API:** `GET http://localhost:3847/api/metrics`

## Stop

Press `Ctrl+C`. The agent waits for the current task to finish (up to 30 seconds) before shutting down.

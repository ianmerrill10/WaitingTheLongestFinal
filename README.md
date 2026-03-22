# WaitingTheLongest.com

Real-time dog rescue platform showing shelter dogs waiting for adoption. Every second matters.

**Motto:** "Stop a Clock, Save a Life"

**Live:** [waitingthelongest.com](https://waitingthelongest.com)

## Mission

Find all adoptable dogs in the United States from shelters, rescues, and non-profit agencies. Show real-time LED wait time counters and euthanasia countdown timers so visitors can help save dogs before time runs out. Every dog listed for free. Every shelter integration for free.

## Tech Stack

- **Framework:** Next.js 14 (App Router) with TypeScript
- **Database:** Supabase (PostgreSQL + Realtime + Auth)
- **Styling:** Tailwind CSS (mobile-first)
- **Hosting:** Vercel
- **Domain:** waitingthelongest.com (DNS via IONOS)
- **Local Agents:** Node.js/tsx processes for 24/7 data management

## Quick Start

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Start local data agent (port 3847)
npm run agent

# Start improvement scanner (port 3848)
npm run agent:improve
```

## Architecture

```
src/
  app/              Next.js App Router pages + API routes
    api/            REST API endpoints (crons, admin, v1 partner API)
    admin/          Admin dashboards (data, revenue, social, outreach)
    partners/       Partner/shelter portal
    dogs/           Dog listing and detail pages
    breeds/         Breed directory
    shelters/       Shelter directory
    states/         State-by-state browsing
    urgent/         Urgent/euthanasia listings
  components/       React components (counters, dogs, layout, UI)
  lib/              Core business logic
    rescuegroups/   RescueGroups API client, mapper, sync
    verification/   Dog listing verification engine
    audit/          Data quality audit system
    shelter-api/    Partner API auth, CSV parser, webhooks
    shelter-crm/    Shelter relationship management
    outreach/       Email outreach and automation
    social/         Social media content generation
    monetization/   Affiliate tracking and recommendations
    stats/          Daily statistics computation
    supabase/       Database clients (admin, server, client)
    utils/          Shared utilities
  types/            TypeScript type definitions
  data/             Seed data scripts

agent/              Local autonomous agents
  index.ts          Data agent (verification, sync, error correction)
  dashboard.html    Real-time data agent dashboard
  improve.ts        Code quality improvement scanner
  improve-dashboard.html  Improvement agent GUI
  backup.ts         Google Drive backup system

supabase/
  migrations/       Database schema (001-011)

sql/                Additional SQL modules
```

## Key Features

- **LED Wait Time Counters** — Real-time YY:MM:DD:HH:MM:SS display for every dog
- **Euthanasia Countdown** — DD:HH:MM:SS countdown to scheduled euthanasia
- **Urgency System** — CRITICAL (<24h), HIGH (<72h), MEDIUM (<7d), NORMAL (>7d)
- **Verified Wait Times** — Cross-referenced with RescueGroups API for accuracy
- **Real-time Verification Agent** — 24/7 dog slideshow showing live verification
- **Error Auto-Correction** — Detects and fixes data inconsistencies automatically
- **Partner API (v1)** — REST API with key auth for shelter integrations
- **Shelter CRM** — Onboarding, contacts, communications, API keys
- **Social Media Automation** — Multi-platform content generation
- **Affiliate Monetization** — Amazon Associates product recommendations
- **Admin Dashboards** — Data health, revenue, social analytics, outreach
- **Google Drive Backup** — Scheduled redundant backups via rclone

## Data Sources

- **RescueGroups.org API v5** — Primary source (search + individual animal endpoints)
- **TheDogAPI** — Breed data enrichment
- **Direct shelter feeds** — CSV/JSON imports via partner API
- **44,947 organization database** — Shelter directory

## Database

- 16 enterprise tables (dogs, shelters, states, breeds, daily_stats, audit_runs, etc.)
- ~33,000+ dogs, 44,947 shelters, 50 states, 170 breeds
- Migrations 001-011 in `supabase/migrations/`

## Local Agents

See [agent/README.md](agent/README.md) for full documentation.

| Agent | Port | Purpose |
|-------|------|---------|
| Data Agent | 3847 | Verify dogs, sync data, fix errors, run audits, backup |
| Improvement Agent | 3848 | Scan code quality, find bugs, suggest improvements |

## Documentation

- [CLAUDE.md](CLAUDE.md) — Project rules and conventions
- [agent/README.md](agent/README.md) — Agent system documentation
- [SITE_WIKI.md](SITE_WIKI.md) — Site feature wiki
- [CODING_STANDARDS.md](CODING_STANDARDS.md) — Code conventions
- [VERIFIED_WAIT_TIME_AUDIT.md](VERIFIED_WAIT_TIME_AUDIT.md) — Wait time accuracy audit

## License

All rights reserved.

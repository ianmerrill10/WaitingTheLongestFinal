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
- **Hosting:** Vercel (paid account)
- **Domain:** waitingthelongest.com (DNS via IONOS)
- **Local Agents:** Node.js/tsx processes for 24/7 data management

## Quick Start

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Start all local agents
npm run agent:all

# Start individual agents
npm run agent          # Data agent (port 3847)
npm run agent:scraper  # Scraper agent (port 3849)
npm run agent:freshness # Freshness agent (port 3850)
npm run agent:intel    # Intelligence agent (port 3851)
npm run agent:improve  # Improvement agent (port 3848)

# Run tests
npx tsx --test tests/*.test.ts

# Production build (requires extra memory)
NODE_OPTIONS=--max-old-space-size=4096 npx next build
```

## Architecture

```
src/
  app/              Next.js App Router (42 pages + 48 API routes)
    api/            REST API endpoints
      cron/         Scheduled jobs (sync-dogs, update-urgency, sync-gov-data, etc.)
      admin/        Admin endpoints (dashboard, scrapers, fix-wait-times, etc.)
      v1/           Partner API (dogs CRUD, bulk, import, webhooks, account)
      partners/     Partner portal (login, register, keys, webhooks)
      social/       Social media (generate, schedule, publish)
      outreach/     Shelter outreach (process, webhook)
    admin/          Admin pages (dashboard, review-queue)
    partners/       Partner/shelter portal pages
    dogs/           Dog listing and detail pages
    breeds/         Breed directory (170 breeds)
    shelters/       Shelter directory (~45K shelters)
    states/         State-by-state browsing (50 states)
    urgent/         Urgent/euthanasia listings
    overlooked/     Overlooked Angels (seniors, special needs)
    legal/          Legal pages (privacy, terms, DMCA)
  components/       React components (29 total)
    counters/       LED wait time + countdown timers + national average
    dogs/           Dog cards, grids, listings
    layout/         Header, Footer, BottomTabs
    shelters/       Shelter cards, table, listings, export
    ui/             Badges, pagination, sharing, toast, interest form
  lib/              Core business logic (17 modules)
    rescuegroups/   RescueGroups API v5 client, mapper, sync
    verification/   Dog listing verification engine
    audit/          Data quality audit system (8 checks)
    scrapers/       50-state universal scraper (8 platform adapters)
    connectors/     Government open data sync (Socrata API)
    shelter-api/    Partner API auth, rate limiting, CSV parser
    shelter-crm/    Shelter relationship management
    outreach/       Email outreach and automation
    social/         Social media content generation
    monetization/   Affiliate tracking and recommendations
    stats/          Daily statistics computation
    auth/           Admin/partner auth utilities
    supabase/       Database clients (admin, server, client)
    utils/          Shared utilities (rate-limit, wait-time, credibility-score)
  types/            TypeScript type definitions
  data/             Seed data and reference tables

agent/              Local autonomous agents
  index.ts          Data agent — port 3847 (verification, sync, error correction)
  scraper.ts        Scraper agent — port 3849 (50-state universal scraper)
  freshness.ts      Freshness agent — port 3850 (expired dog verification)
  intelligence.ts   Intelligence agent — port 3851 (risk scoring, anomalies)
  improve.ts        Improvement agent — port 3848 (code quality scanner)
  backup.ts         Backup system (zip + git bundle + Google Drive via rclone)
  start-all.ts      Master launcher (starts all agents with auto-restart)

supabase/
  migrations/       Database schema (001-017)

tests/              Test files (node:test + tsx)
docs/               Documentation and archives
```

## Key Features

- **LED Wait Time Counters** — Real-time YY:MM:DD:HH:MM:SS display for every dog
- **Euthanasia Countdown** — DD:HH:MM:SS countdown to scheduled euthanasia
- **Urgency System** — CRITICAL (<24h), HIGH (<72h), MEDIUM (<7d), NORMAL (>7d)
- **Verified Wait Times** — Cross-referenced with RescueGroups API, ranked by `ranking_eligible` flag
- **50-State Scraper** — 8 platform adapters (ShelterLuv, ShelterBuddy, Petango, Chameleon, etc.)
- **Government Data Sync** — Socrata open data from Austin TX, Bloomington IN, Norfolk VA
- **5 Autonomous Agents** — Data, Scraper, Freshness, Intelligence, Improvement
- **Partner API (v1)** — REST API with key auth, rate limiting, CSV import, webhooks
- **Shelter CRM** — Onboarding, contacts, communications, API keys
- **Social Media Automation** — Multi-platform content generation and scheduling
- **Affiliate Monetization** — Amazon Associates breed-specific product recommendations
- **Admin Dashboards** — Data health, revenue, social analytics, outreach, scraper status
- **Data Provenance** — Credibility scoring, intake date observation counting, page hash detection
- **Security Hardened** — CRON_SECRET auth on all routes, rate limiting, input sanitization

## Data Sources

- **RescueGroups.org API v5** — Primary source (~33K dogs via search + individual endpoints)
- **50-State Scraper** — 8 platform adapters for direct shelter website scraping
- **Government Open Data** — Socrata API (Austin TX, Bloomington IN, Norfolk VA)
- **TheDogAPI** — Breed data enrichment
- **Direct shelter feeds** — CSV/JSON imports via partner API
- **~45K organization database** — Comprehensive shelter directory

## Database

- **17 migrations** (001-017) in `supabase/migrations/`
- ~57K dogs, ~45K shelters, 50 states, 170 breeds
- Key tables: dogs, shelters, states, breeds, daily_stats, audit_runs, audit_logs, foster_applications, shelter_communications, shelter_api_keys, webhook_endpoints, affiliate_products, affiliate_clicks, outreach_targets, social_posts

## Local Agents

| Agent | Port | Purpose |
|-------|------|---------|
| Data Agent | 3847 | Verify dogs, sync data, fix errors, run audits |
| Improvement Agent | 3848 | Scan code quality, find bugs, suggest improvements |
| Scraper Agent | 3849 | 50-state universal shelter website scraper |
| Freshness Agent | 3850 | Expired dog verification, stale listing cleanup |
| Intelligence Agent | 3851 | Shelter risk scoring, anomaly detection, scraper optimization |

## Cron Jobs

| Schedule | Endpoint | Purpose |
|----------|----------|---------|
| 6 AM daily | `/api/cron/sync-dogs` | Sync dogs from RescueGroups + run verification batch |
| 6 PM daily | `/api/cron/update-urgency` | Recalculate urgency levels, verify dogs, run audits |

## Documentation

- [CLAUDE.md](CLAUDE.md) — Project rules and conventions
- [SITE_WIKI.md](SITE_WIKI.md) — Site feature wiki and architecture reference
- [docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md) — Environment variable reference
- [VERIFIED_WAIT_TIME_AUDIT.md](VERIFIED_WAIT_TIME_AUDIT.md) — Wait time accuracy audit
- [DATA_SOURCES_STRATEGY.md](DATA_SOURCES_STRATEGY.md) — Data sourcing strategy
- [BUSINESS_PLAN.md](BUSINESS_PLAN.md) — Business plan

## License

All rights reserved.

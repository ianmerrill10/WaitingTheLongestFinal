# WaitingTheLongest.com — Site Wiki

> Last updated: 2026-03-29
> Live URL: https://waitingthelongest.com
> Vercel alias: https://waitingthelongest.vercel.app

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [All Routes & Pages](#all-routes--pages)
3. [API Endpoints](#api-endpoints)
4. [Database Schema](#database-schema)
5. [Data Pipeline](#data-pipeline)
6. [Local Agents](#local-agents)
7. [Environment Variables](#environment-variables)
8. [Deployment & Crons](#deployment--crons)
9. [Security](#security)
10. [Debugging & Monitoring](#debugging--monitoring)
11. [Key Components](#key-components)

---

## Architecture Overview

```
User Browser
    │
    ▼
Vercel Edge Network (CDN + SSL)
    │
    ├── Middleware (password gate, partner auth)
    │
    ▼
Next.js 14 (App Router) — 42 pages + 48 API routes
    ├── Static pages (SSG): /about, /foster, /dogs, /search, /legal/*
    ├── Dynamic pages (SSR): /, /urgent, /overlooked, /states/*, /shelters/*, /dogs/*
    ├── Admin pages: /admin/dashboard, /admin/review-queue
    ├── Partner pages: /partners/dashboard/*, /partners/login, /partners/register
    └── API routes: /api/dogs, /api/cron/*, /api/v1/*, /api/admin/*, /api/social/*
         │
         ▼
    Supabase (PostgreSQL)
         ├── 50 states
         ├── ~45K shelters
         ├── ~57K dogs (~33K RG + scraper + gov data)
         └── 170 breeds

Local Agents (5 processes)
    ├── Data Agent (port 3847) — verification, sync, audits
    ├── Improvement Agent (port 3848) — code quality scanner
    ├── Scraper Agent (port 3849) — 50-state shelter scraper
    ├── Freshness Agent (port 3850) — expired dog verification
    └── Intelligence Agent (port 3851) — risk scoring, anomalies
```

**Tech Stack:**

- **Framework:** Next.js 14 (App Router) + TypeScript
- **Database:** Supabase (PostgreSQL + pg_trgm)
- **Styling:** Tailwind CSS (mobile-first)
- **Hosting:** Vercel (paid account)
- **Domain:** waitingthelongest.com (DNS via IONOS)
- **Data Sources:** RescueGroups API v5, 50-state scraper, government Socrata API

---

## All Routes & Pages

### Public Pages

| Route | Type | Description |
|-------|------|-------------|
| `/` | SSR | Homepage — stats, urgent dogs, longest waiting, overlooked, state grid |
| `/dogs` | SSG+Client | Browse all dogs — search, filter, sort, paginate |
| `/dogs/[id]` | SSR | Dog detail — photos, LED counter, description, shelter info, interest form |
| `/dogs/[id]/sources` | SSR | Dog data sources and provenance |
| `/urgent` | SSR | Urgent dogs — critical/high/medium sections with countdown timers |
| `/overlooked` | SSR | Overlooked Angels — senior, black, pit bull, bonded, special needs, long-timer dogs |
| `/states` | SSR | States directory — all 50 states with dog/shelter counts |
| `/states/[code]` | SSR | State detail — dogs and shelters in a specific state |
| `/breeds` | SSR | Breed directory — all 170 breeds |
| `/breeds/[slug]` | SSR | Breed detail — breed info and available dogs |
| `/shelters` | SSR | Shelter directory — searchable, filterable, grid/table toggle |
| `/shelters/[id]` | SSR | Shelter detail — info, stats, available dogs, JSON-LD structured data |
| `/search` | SSG+Client | Search landing with filters |
| `/saved` | SSG+Client | User saved/favorited dogs |
| `/about` | SSG | About/mission page |
| `/foster` | SSG | Foster program — application form with backend |
| `/login` | SSG | Password login gate |

### Legal Pages

| Route | Type | Description |
|-------|------|-------------|
| `/legal/privacy-policy` | SSG | Privacy policy |
| `/legal/terms-of-service` | SSG | Terms of service |
| `/legal/dmca` | SSG | DMCA takedown policy |
| `/legal/api-terms` | SSG | API terms of use |
| `/legal/data-sharing` | SSG | Data sharing policy |
| `/legal/shelter-terms` | SSG | Shelter partner terms |

### Partner Pages

| Route | Type | Description |
|-------|------|-------------|
| `/partners` | SSG | Partner program landing |
| `/partners/login` | SSG | Partner login |
| `/partners/register` | SSG | Partner registration |
| `/partners/dashboard` | SSG+Client | Partner overview dashboard |
| `/partners/dashboard/dogs` | SSG+Client | Partner dog management |
| `/partners/dashboard/keys` | SSG+Client | API key management |
| `/partners/dashboard/webhooks` | SSG+Client | Webhook configuration |
| `/partners/dashboard/analytics` | SSG+Client | Partner analytics |
| `/partners/dashboard/communications` | SSG+Client | Partner communications |

### Admin Pages

| Route | Type | Description |
|-------|------|-------------|
| `/admin/dashboard` | SSR | Admin data health dashboard |
| `/admin/review-queue` | SSR | Evidence review queue |

### SEO Routes

| Route | Type | Description |
|-------|------|-------------|
| `/sitemap.xml` | Generated | Dynamic sitemap (static pages, states, top dogs/shelters) |
| `/robots.txt` | Generated | Crawler rules — allows all, blocks /api/ and /_next/ |

---

## API Endpoints

### Public APIs

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/dogs` | GET | Rate-limited | Paginated, filtered, sorted dog listing |
| `/api/dogs/[id]/interest` | POST | Rate-limited (10/min) | Submit adoption interest |
| `/api/shelters` | GET | Rate-limited | Shelter directory with 12+ filters |
| `/api/stats` | GET | None | Public site statistics |
| `/api/login` | POST | None | Site password authentication |
| `/api/foster` | POST | Rate-limited (5/min) | Foster application submission |
| `/api/affiliate/click` | GET/POST | Rate-limited (30/min) | Product recommendations + click tracking |

### Cron/Admin APIs (all require CRON_SECRET)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/cron/sync-dogs` | GET | Sync from RescueGroups + verification batch |
| `/api/cron/update-urgency` | GET | Evening agent: urgency, verification, audits |
| `/api/cron/sync-gov-data` | GET | Government open data sync (Socrata) |
| `/api/cron/audit-dogs` | GET | Run data quality audit |
| `/api/cron/verify-dogs` | GET | Run verification batch |
| `/api/debug` | GET | System health dashboard |
| `/api/audit` | GET/POST | Audit system management |
| `/api/backup` | GET | Database export (JSON/CSV) |
| `/api/reprocess-dates` | GET | Reprocess description date parsing |
| `/api/admin/fix-wait-times` | POST | Batch fix unreasonable wait times |
| `/api/admin/dashboard` | GET | Admin dashboard data |
| `/api/admin/onboarding` | GET/POST | Shelter onboarding management |
| `/api/admin/partner` | GET | Partner dashboard data |
| `/api/admin/scrapers` | GET | Scraper status and stats |

### Social Media APIs (require CRON_SECRET)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/social/generate` | POST | Generate social media content |
| `/api/social/schedule` | GET/POST/DELETE | Manage scheduled posts |
| `/api/social/publish` | POST | Publish content to platforms |
| `/api/outreach/process` | POST | Process shelter outreach campaigns |
| `/api/outreach/webhook` | POST | SendGrid inbound email webhook |

### Partner API v1 (require API key)

| Endpoint | Method | Scope | Description |
|----------|--------|-------|-------------|
| `/api/v1/dogs` | GET | read | List shelter's dogs |
| `/api/v1/dogs` | POST | write | Create a dog listing |
| `/api/v1/dogs/[id]` | GET/PUT/DELETE | read/write | Manage individual dog |
| `/api/v1/dogs/bulk` | POST | bulk_import | Bulk create/update dogs |
| `/api/v1/dogs/import` | POST | bulk_import | CSV import |
| `/api/v1/webhooks` | GET/POST/DELETE | webhook_manage | Webhook management |
| `/api/v1/account` | GET | read | Account info |
| `/api/webhooks/incoming/[shelterId]` | POST | Webhook signature | Inbound data from shelters |

### Partner Portal APIs

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/partners/login` | POST | Rate-limited (10/min) | Partner login |
| `/api/partners/register` | POST | Rate-limited (5/min) | Partner registration |
| `/api/partners/keys` | POST/DELETE | Session cookie | API key management |
| `/api/partners/webhooks` | POST/DELETE | Session cookie | Webhook management |

---

## Database Schema

### Tables (17 migrations, 001-017)

| Table | Rows | Description |
|-------|------|-------------|
| states | 50 | US states with codes, dog/shelter counts |
| shelters | ~45K | Shelters/rescues with location, contact, type, platform, partner status |
| dogs | ~57K | Adoptable dogs with breed, age, photos, status, provenance |
| breeds | 170 | Dog breed reference data |
| daily_stats | varies | Daily aggregated statistics |
| audit_runs | varies | Audit execution history |
| audit_logs | varies | Individual audit findings |
| foster_applications | varies | Foster program applications |
| shelter_communications | varies | Shelter outreach communications |
| shelter_contacts | varies | Shelter contact people |
| shelter_api_keys | varies | Partner API keys (hashed) |
| webhook_endpoints | varies | Partner webhook configurations |
| affiliate_products | varies | Affiliate product catalog |
| affiliate_clicks | varies | Affiliate click tracking |
| outreach_targets | varies | Shelter outreach campaign targets |
| social_posts | varies | Social media posts |

### Key Dog Fields

- `id` (UUID, PK), `name`, `breed_primary`, `breed_secondary`, `breed_mixed`
- `size_general`, `age_category`, `age_text`, `age_months`
- `sex`, `color_primary`
- `photo_url`, `photo_urls` (array)
- `description`, `description_plain`, `status`, `is_available`
- `intake_date` — used for LED wait time counter
- `original_intake_date` — preserved original before any capping
- `euthanasia_date` — used for countdown timer
- `urgency_level` (critical/high/medium/normal)
- `date_confidence` (verified/high/medium/low)
- `date_source` — how intake_date was determined
- `ranking_eligible` — whether this dog ranks in "Waiting the Longest"
- `intake_date_observation_count` — how many syncs confirmed the same date
- `shelter_id` (FK to shelters)
- `external_id`, `external_source`, `external_url`
- `source_links` (JSONB array of source URLs)
- `view_count`, `inquiry_count`

### Key Shelter Fields

- `id` (UUID), `name`, `city`, `state_code`, `zip_code`, `county`
- `phone`, `email`, `website`, `facebook_url`
- `shelter_type` (municipal/private/humane_society/spca/rescue/foster_network)
- `latitude`, `longitude`
- `available_dog_count`, `urgent_dog_count` (denormalized)
- `is_verified`, `is_kill_shelter`, `accepts_rescue_pulls`
- `partner_status`, `partner_tier`
- `website_platform` (shelterluv/shelterbuddy/petango/chameleon/etc.)
- `ein` (nonprofit EIN for verification)
- `last_scraped_at`, `scrape_frequency_hours`
- `external_id`, `external_source`

---

## Data Pipeline

```
                    ┌──────────────────────┐
                    │  RescueGroups v5 API  │
                    │  (~33K dogs, primary) │
                    └──────────┬───────────┘
                               │
                    ┌──────────┴───────────┐
                    │ 50-State Scraper     │     ┌──────────────────┐
                    │ (8 platform adapters)│     │ Government Data  │
                    └──────────┬───────────┘     │ (Socrata API)    │
                               │                 └────────┬─────────┘
                               │                          │
                    ┌──────────┴──────────────────────────┴──────┐
                    │              Mapper / Normalizer             │
                    │  • intake_date computation (7-strategy)     │
                    │  • date confidence scoring                  │
                    │  • euthanasia date extraction               │
                    │  • returned-after-adoption detection        │
                    │  • age sanity checks                        │
                    └──────────────────┬──────────────────────────┘
                                       │
                    ┌──────────────────┴──────────────────────────┐
                    │            Dedup + Upsert Engine             │
                    │  • Cross-platform dedup by name/breed/shelter│
                    │  • Batch insert/update                      │
                    │  • Ground truth adoption detection           │
                    └──────────────────┬──────────────────────────┘
                                       │
                    ┌──────────────────┴──────────────────────────┐
                    │          Supabase PostgreSQL                 │
                    │  ~57K dogs, ~45K shelters, 50 states        │
                    └──────────────────┬──────────────────────────┘
                                       │
                    ┌──────────────────┴──────────────────────────┐
                    │        Verification + Audit Pipeline         │
                    │  • RG API individual lookups                │
                    │  • Description date parsing                 │
                    │  • Urgency signal detection                 │
                    │  • Age sanity cap                           │
                    │  • Credibility scoring                      │
                    └─────────────────────────────────────────────┘
```

---

## Local Agents

| Agent | Port | File | Purpose |
|-------|------|------|---------|
| Data Agent | 3847 | `agent/index.ts` | Verification, sync, error correction, audits |
| Improvement Agent | 3848 | `agent/improve.ts` | Code quality scanning, 10 checkers |
| Scraper Agent | 3849 | `agent/scraper.ts` | 50-state universal shelter website scraper |
| Freshness Agent | 3850 | `agent/freshness.ts` | Expired dog verification, stale listing cleanup |
| Intelligence Agent | 3851 | `agent/intelligence.ts` | Shelter risk scoring, breed/state risk, anomaly detection |
| Backup System | — | `agent/backup.ts` | Zip + git bundle + DB export + Google Drive via rclone |
| Master Launcher | — | `agent/start-all.ts` | Starts all 5 agents with auto-restart |

```bash
npm run agent:all      # Start all agents
npm run agent          # Data agent only
npm run agent:scraper  # Scraper agent only
npm run agent:freshness # Freshness agent only
npm run agent:intel    # Intelligence agent only
npm run agent:improve  # Improvement agent only
```

---

## Environment Variables

See [docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md) for the complete reference.

Key variables:

- `NEXT_PUBLIC_SUPABASE_URL` / `NEXT_PUBLIC_SUPABASE_ANON_KEY` / `SUPABASE_SERVICE_ROLE_KEY`
- `CRON_SECRET` — protects all admin/cron/debug/social/outreach routes
- `SITE_PASSWORD` — site-wide login gate
- `RESCUEGROUPS_API_KEY` — primary data source
- `SENDGRID_API_KEY` — email outreach
- `SUPABASE_DB_PASSWORD` — direct DB connections

---

## Deployment & Crons

### Deployment

- GitHub repo: `ianmerrill10/WaitingTheLongestFinal`
- Auto-deploys on push to `main`
- Vercel project: `waitingthelongest`
- Region: iad1 (US East - N. Virginia)
- Build command: `NODE_OPTIONS=--max-old-space-size=4096 next build`

### Cron Jobs (vercel.json)

| Schedule | Endpoint | Description |
|----------|----------|-------------|
| 6 AM daily | `/api/cron/sync-dogs?pages=50&start=1&verify_batch=100` | Sync 5K dogs from RG + verify 100 |
| 6 PM daily | `/api/cron/update-urgency` | Urgency recalc, verification, audits (5 steps) |

### DNS (IONOS)

- A record: `waitingthelongest.com` → `76.76.21.21`
- CNAME: `www.waitingthelongest.com` → `cname.vercel-dns.com`

---

## Security

- **All cron/admin/debug/social/outreach routes** require `Authorization: Bearer $CRON_SECRET`
- **CRON_SECRET null check** — routes reject auth if env var is unset (prevents "Bearer undefined" bypass)
- **Partner API** uses SHA-256 hashed API keys with scope-based access control
- **Webhook signatures** are required (not optional) for inbound webhooks
- **Rate limiting** on all public endpoints (120/min for reads, 5-30/min for writes)
- **Input sanitization** — SQL LIKE wildcards escaped in all search params
- **Sort param whitelisting** — only allowed column names in `.order()` calls
- **Error message suppression** — Supabase errors logged server-side, generic messages to clients
- **Login fail-closed** — in production, returns 503 if `SITE_PASSWORD` is not configured
- **Password-protected site** — middleware enforces login on all pages except /login and /api/login

### Known Security Limitations

- In-memory rate limiting resets per serverless invocation (Vercel)
- Partner session token (`wtl_session`) not validated server-side beyond cookie existence
- Admin cookie (`site_auth=authenticated`) is predictable — consider JWT upgrade
- No CSRF protection on mutation endpoints

---

## Debugging & Monitoring

### Debug Endpoint

```bash
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.com/api/debug"
```

Returns JSON with: database connectivity, dog/shelter counts, urgency breakdown, date health, environment check, RescueGroups API status, audit stats, verification coverage.

### Common Debug Scenarios

**Wait times look wrong:**
- Check `date_confidence` and `date_source` for the dog
- Use `/api/admin/fix-wait-times` to batch-cap unreasonable wait times
- Check `ranking_eligible` flag — only verified dogs appear in "Waiting the Longest"

**Dogs not showing up:**
- Check `is_available` flag
- Check `shelter_id` is not null
- Check `ranking_eligible` for homepage section

**Expired euthanasia dogs showing:**
- Homepage and urgent page filter `euthanasia_date > now()`
- Evening agent auto-deactivates dogs expired >48h
- Freshness agent verifies 0-48h window via RG API

---

## Key Components

### LED Counter System

- `LEDDigit.tsx` renders a single 7-segment digit using CSS
- `LEDCounter.tsx` shows wait time (green): YY:MO:DY:HR:MN:SC
- `CountdownTimer.tsx` shows euthanasia countdown (red): DY:HR:MN:SC
- `NationalAvgCounter.tsx` shows national average wait time on homepage
- Segment patterns defined in `constants.ts` (SEGMENT_PATTERNS)
- CSS in `globals.css` with beveled segments, glow effects, critical pulse

### Dog Cards

- `DogCard.tsx` shows photo, name, breed, location, urgency badge, share button
- Shows LED counter (wait time) or countdown timer (euthanasia)
- `DogGrid.tsx` renders responsive grid of DogCards
- `DogListings.tsx` adds filtering, sorting, pagination UI

### Shelter Directory

- `ShelterListings.tsx` — client-side with sidebar filters, grid/table toggle
- `ShelterCard.tsx` — card view with key stats
- `ShelterTable.tsx` — table view with sortable columns
- `ShelterExport.tsx` — CSV export functionality

### Bottom Navigation (Mobile)

- `BottomTabs.tsx` — fixed bottom bar on mobile (hidden on md+)
- Tabs: Home, Browse, Urgent, Foster, States
- Active state highlighting, urgent pulse indicator

---

## Quick Reference

> **SECURITY NOTE (2026-03-29):** Secrets were previously exposed in this file and are in git history.
> CRON_SECRET and RESCUEGROUPS_API_KEY have been rotated. Old values are compromised.
> All secrets are now stored ONLY in Vercel environment variables and .env.local.
> Never commit secrets to this file again.

**Supabase Ref:** `hpssqzqwtsczsxvdfktt`
**Supabase URL:** `https://hpssqzqwtsczsxvdfktt.supabase.co`
**Amazon Associates:** `waitingthelon-20`

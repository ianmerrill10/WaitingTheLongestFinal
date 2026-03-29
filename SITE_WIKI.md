# WaitingTheLongest.com — Site Wiki

> Last updated: 2026-03-18
> Live URL: https://waitingthelongest.com
> Vercel alias: https://waitingthelongest.vercel.app

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [All Routes & Pages](#all-routes--pages)
3. [API Endpoints](#api-endpoints)
4. [All Source Files](#all-source-files)
5. [Database Schema](#database-schema)
6. [Data Pipeline](#data-pipeline)
7. [Environment Variables](#environment-variables)
8. [Deployment & Crons](#deployment--crons)
9. [Debugging & Monitoring](#debugging--monitoring)
10. [Key Components](#key-components)
11. [Known Issues & TODOs](#known-issues--todos)

---

## Architecture Overview

```
User Browser
    │
    ▼
Vercel Edge Network (CDN + SSL)
    │
    ▼
Next.js 14 (App Router)
    ├── Static pages (SSG): /about, /foster, /dogs, /search
    ├── Dynamic pages (SSR): /, /urgent, /overlooked, /states/*, /shelters/*, /dogs/*
    └── API routes: /api/dogs, /api/cron/*, /api/debug
         │
         ▼
    Supabase (PostgreSQL)
         ├── 50 states
         ├── ~45K shelters
         ├── ~10K+ dogs (syncing)
         └── 170 breeds
```

**Tech Stack:**
- **Framework:** Next.js 14.2 (App Router) + TypeScript
- **Database:** Supabase (PostgreSQL + pg_trgm)
- **Styling:** Tailwind CSS (mobile-first)
- **Hosting:** Vercel (paid account)
- **Domain:** waitingthelongest.com (DNS via IONOS)
- **Data Source:** RescueGroups.org API v5

---

## All Routes & Pages

### Public Pages

| Route | Type | Description |
|-------|------|-------------|
| `/` | SSR | Homepage — stats, urgent dogs, longest waiting, overlooked, state grid |
| `/dogs` | SSG+Client | Browse all dogs — search, filter, sort, paginate |
| `/dogs/[id]` | SSR | Dog detail — photos, LED counter, description, shelter info, interest form |
| `/urgent` | SSR | Urgent dogs — critical/high/medium sections with countdown timers |
| `/overlooked` | SSR | Overlooked Angels — senior, black, pit bull, bonded, special needs, long-timer dogs |
| `/states` | SSR | States directory — all 50 states with dog/shelter counts |
| `/states/[code]` | SSR | State detail — dogs and shelters in a specific state |
| `/shelters` | SSR | Shelter directory — searchable, filterable, paginated |
| `/shelters/[id]` | SSR | Shelter detail — shelter info, stats, available dogs |
| `/search` | SSG+Client | Search landing — same as /dogs with filters |
| `/about` | SSG | About/mission page — static content |
| `/foster` | SSG | Foster program page — education, FAQ, application (placeholder) |

### SEO Routes

| Route | Type | Description |
|-------|------|-------------|
| `/sitemap.xml` | Generated | Dynamic sitemap with static pages, 50 states, top 100 dogs/shelters |
| `/robots.txt` | Generated | Crawler rules — allows all, blocks /api/ and /_next/ |

### API Endpoints

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/dogs` | GET | None | Paginated, filtered, sorted dog listing |
| `/api/dogs/[id]/interest` | POST | None | Submit adoption interest form |
| `/api/cron/sync-dogs` | GET | CRON_SECRET | Sync dogs from RescueGroups (params: `pages`, `start`) |
| `/api/cron/update-urgency` | GET | CRON_SECRET | Recalculate urgency levels |
| `/api/debug` | GET | CRON_SECRET | System health dashboard (param: `fix=bad_dates`) |

### API Query Parameters

**`/api/dogs`:**
- `page` (int, default 1)
- `limit` (int, default 24, max 100)
- `sort`: `wait_time` | `urgency` | `newest` | `name`
- `breed` (string, ilike search)
- `size`: `small` | `medium` | `large` | `xlarge`
- `age`: `puppy` | `young` | `adult` | `senior`
- `gender`: `male` | `female`
- `state` (2-letter code)
- `urgency`: `critical` | `high` | `medium` | `normal`
- `q` (full-text search)

**`/api/cron/sync-dogs`:**
- `pages` (int, default 10, max 50)
- `start` (int, default 1) — starting page number for pagination

**`/api/debug`:**
- `fix=bad_dates` — repairs dogs with intake_date > 5 years old

---

## All Source Files

### App Pages (src/app/)
```
src/app/layout.tsx              — Root layout (Header, Footer, BottomTabs)
src/app/page.tsx                — Homepage
src/app/globals.css             — Global CSS (LED counters, urgency badges)
src/app/about/page.tsx          — About page
src/app/dogs/page.tsx           — Browse dogs
src/app/dogs/[id]/page.tsx      — Dog detail
src/app/foster/page.tsx         — Foster program
src/app/overlooked/page.tsx     — Overlooked Angels
src/app/search/page.tsx         — Search landing
src/app/shelters/page.tsx       — Shelter directory
src/app/shelters/[id]/page.tsx  — Shelter detail
src/app/states/page.tsx         — States directory
src/app/states/[code]/page.tsx  — State detail
src/app/urgent/page.tsx         — Urgent dogs
src/app/sitemap.ts              — Dynamic sitemap generator
src/app/robots.ts               — Robots.txt generator
```

### API Routes (src/app/api/)
```
src/app/api/dogs/route.ts                — Dog listing API
src/app/api/dogs/[id]/interest/route.ts   — Adoption interest form
src/app/api/cron/sync-dogs/route.ts       — RescueGroups sync cron
src/app/api/cron/update-urgency/route.ts  — Urgency recalculation cron
src/app/api/debug/route.ts                — Debug/monitoring dashboard
```

### Components (src/components/)
```
Layout:
  src/components/layout/Header.tsx      — Site header with nav
  src/components/layout/Footer.tsx      — Site footer
  src/components/layout/BottomTabs.tsx  — Mobile bottom navigation

Counters:
  src/components/counters/LEDDigit.tsx      — Single 7-segment digit
  src/components/counters/LEDCounter.tsx    — Wait time LED counter (green)
  src/components/counters/CountdownTimer.tsx — Euthanasia countdown (red)

Dogs:
  src/components/dogs/DogCard.tsx      — Dog card with photo, info, counters
  src/components/dogs/DogGrid.tsx      — Responsive grid of dog cards
  src/components/dogs/DogListings.tsx  — Filterable/sortable dog list

Shelters:
  src/components/shelters/ShelterSearch.tsx — Shelter search with filters

UI:
  src/components/ui/UrgencyBadge.tsx   — Urgency level badge
  src/components/ui/Pagination.tsx     — Page navigation
  src/components/ui/InterestForm.tsx   — Adoption interest form
  src/components/ui/ShareButtons.tsx   — Social sharing buttons
```

### Libraries (src/lib/)
```
Supabase:
  src/lib/supabase/client.ts   — Browser Supabase client
  src/lib/supabase/server.ts   — Server-side Supabase client (cookies)
  src/lib/supabase/admin.ts    — Admin Supabase client (service role)

RescueGroups:
  src/lib/rescuegroups/client.ts  — v5 API client with rate limiting
  src/lib/rescuegroups/mapper.ts  — Maps API data to dog schema
  src/lib/rescuegroups/sync.ts    — Sync logic with batch operations

Utilities:
  src/lib/constants.ts          — App constants, LED patterns, labels
  src/lib/utils/wait-time.ts    — Wait time/countdown calculations
```

### Types (src/types/)
```
src/types/dog.ts      — Dog interface
src/types/shelter.ts  — Shelter interface
src/types/state.ts    — State interface
```

### Data Scripts (data/)
```
data/seed-all.ts       — Run all seed scripts
data/seed-breeds.ts    — Seed 170 dog breeds
data/seed-shelters.ts  — Seed 45K+ shelters from org data
data/seed-states.ts    — Seed 50 US states
data/setup-database.ts — Database setup script
```

### Database (supabase/)
```
supabase/setup.sql                  — Consolidated DB setup
supabase/migrations/001_states.sql  — States table
supabase/migrations/002_shelters.sql — Shelters table
supabase/migrations/003_dogs.sql    — Dogs table
supabase/migrations/004_breeds.sql  — Breeds table
supabase/migrations/005_views.sql   — Database views
supabase/migrations/006_users_favorites.sql — Users & favorites
```

### Config Files
```
next.config.mjs       — Next.js config (image domains)
tailwind.config.ts     — Tailwind theme (brand colors, LED colors)
postcss.config.mjs     — PostCSS config
tsconfig.json          — TypeScript config
vercel.json            — Vercel config (crons)
package.json           — Dependencies
.env.local             — Environment variables (not in git)
.env.example           — Example env template
CLAUDE.md              — AI assistant instructions
```

---

## Database Schema

### Tables

| Table | Rows | Description |
|-------|------|-------------|
| states | 50 | US states with codes |
| shelters | ~45K | Shelters/rescues with location, contact, type |
| dogs | ~10K+ | Adoptable dogs with breed, age, photos, status |
| breeds | 170 | Dog breed reference data |
| favorites | - | User favorited dogs |
| euthanasia_reports | - | Community euthanasia date reports |

### Key Dog Fields
- `id` (UUID, primary key)
- `name`, `breed_primary`, `breed_secondary`, `breed_mixed`
- `size` (small/medium/large/xlarge), `age_category` (puppy/young/adult/senior)
- `gender` (male/female)
- `primary_photo_url`, `photo_urls` (array), `thumbnail_url`
- `description`, `status`, `is_available`
- `intake_date` — used for LED wait time counter
- `euthanasia_date` — used for countdown timer (if set)
- `urgency_level` (critical/high/medium/normal)
- `shelter_id` (FK to shelters)
- `external_id`, `external_source` (rescuegroups)
- `last_synced_at`

### Key Shelter Fields
- `id` (UUID), `name`, `city`, `state_code`, `zip_code`
- `phone`, `email`, `website`, `facebook_url`
- `shelter_type` (municipal/private/humane_society/spca/rescue/foster_network)
- `latitude`, `longitude`
- `external_id`, `external_source`
- `is_verified`, `is_kill_shelter`

---

## Data Pipeline

```
RescueGroups v5 API
    │
    │  GET /public/animals/search/available/dogs/
    │  ?page=N&limit=100&include=orgs,pictures
    │
    ▼
src/lib/rescuegroups/client.ts (rate-limited: 2/sec, 100/min)
    │
    ▼
src/lib/rescuegroups/mapper.ts
    │  Maps: name, breed, size, age, photos, description
    │  intake_date = updatedDate || createdDate
    │
    ▼
src/lib/rescuegroups/sync.ts
    │  Batch lookups (existing dogs by external_id)
    │  Auto-creates shelters from org data
    │  Batch inserts (50 at a time)
    │  Individual updates for existing dogs
    │
    ▼
Supabase PostgreSQL
```

### Sync Commands
```bash
# Sync first 5K dogs (pages 1-50)
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.vercel.app/api/cron/sync-dogs?pages=50"

# Sync next 5K (pages 51-100)
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.vercel.app/api/cron/sync-dogs?pages=50&start=51"

# Sync pages 101-150
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.vercel.app/api/cron/sync-dogs?pages=50&start=101"
```

---

## Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `NEXT_PUBLIC_SUPABASE_URL` | Yes | Supabase project URL |
| `NEXT_PUBLIC_SUPABASE_ANON_KEY` | Yes | Supabase anonymous key |
| `SUPABASE_SERVICE_ROLE_KEY` | Yes | Supabase admin key (server only) |
| `RESCUEGROUPS_API_KEY` | Yes | RescueGroups v5 API key |
| `CRON_SECRET` | Yes | Bearer token for cron/debug endpoints |
| `NEXT_PUBLIC_SITE_URL` | No | Site URL for OG tags |
| `SENDGRID_API_KEY` | No | SendGrid for email (placeholder) |
| `THEDOGAPI_KEY_1-4` | No | TheDogAPI breed photos (placeholder) |

---

## Deployment & Crons

### Deployment
- GitHub repo: `ianmerrill10/WaitingTheLongestFinal`
- Auto-deploys on push to `main`
- Vercel project: `waitingthelongest`
- Region: iad1 (US East - N. Virginia)

### Cron Jobs (vercel.json)
```json
{
  "crons": [
    { "path": "/api/cron/sync-dogs", "schedule": "0 6 * * *" },
    { "path": "/api/cron/update-urgency", "schedule": "0 18 * * *" }
  ]
}
```
- `sync-dogs`: 6 AM daily — syncs first 10 pages (1,000 dogs)
- `update-urgency`: 6 PM daily — recalculates urgency levels

### DNS (IONOS)
- A record: `waitingthelongest.com` → `76.76.21.21`
- CNAME: `www.waitingthelongest.com` → `cname.vercel-dns.com`

---

## Debugging & Monitoring

### Debug Endpoint
```bash
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.vercel.app/api/debug"
```

Returns JSON with:
- `database`: connectivity, latency
- `dogs`: total/available/unavailable counts
- `dogs_by_urgency`: breakdown by level
- `bad_intake_dates`: dogs with intake > 5 years (bad data)
- `dogs_no_photos`: count of dogs without photos
- `shelters`: total/verified, top 10 states
- `last_sync`: most recent sync timestamp
- `environment`: which env vars are set (no values exposed)
- `rescuegroups_api`: connectivity check

### Data Repair
```bash
# Fix dogs with stale intake dates (>5 years)
curl -H "Authorization: Bearer $CRON_SECRET" \
  "https://waitingthelongest.vercel.app/api/debug?fix=bad_dates"
```

### Common Debug Scenarios

**Wait times look wrong:**
- Check `intake_date` via debug endpoint → `bad_intake_dates`
- Run `?fix=bad_dates` to repair
- Root cause: RescueGroups `createdDate` can be very old

**Dogs not showing up:**
- Check `is_available` flag
- Check `shelter_id` is not null
- Run sync to refresh data

**Pages returning 500:**
- Check Vercel deployment logs: `npx vercel logs`
- Check Supabase is up: debug endpoint → `database.connected`
- Check RescueGroups: debug endpoint → `rescuegroups_api.connected`

---

## Key Components

### LED Counter System
- `LEDDigit.tsx` renders a single 7-segment digit using CSS
- `LEDCounter.tsx` shows wait time (green): YY:MO:DY:HR:MN:SC
- `CountdownTimer.tsx` shows euthanasia countdown (red): DY:HR:MN:SC
- Segment patterns defined in `constants.ts` (SEGMENT_PATTERNS)
- CSS in `globals.css` with beveled segments, glow effects, critical pulse

### Dog Cards
- `DogCard.tsx` shows photo, name, breed, location, urgency badge
- Shows LED counter (wait time) or countdown timer (euthanasia)
- `DogGrid.tsx` renders responsive grid of DogCards

### Bottom Navigation (Mobile)
- `BottomTabs.tsx` — fixed bottom bar on mobile (hidden on md+)
- Tabs: Home, Browse, Urgent, Foster, States
- Active state highlighting, urgent pulse indicator

---

## Known Issues & TODOs

### Bugs to Fix
- [ ] Foster application form is disabled (no backend)
- [ ] Interest form doesn't actually send email (console.log only)
- [ ] Shelter detail map shows placeholder
- [ ] States page fetches all 45K shelters to count by state (should use GROUP BY)

### Phase 1 MVP Remaining
- [ ] Sync remaining dogs (have ~10K of 33K available)
- [ ] TheDogAPI breed photos integration
- [ ] SendGrid email integration for interest forms
- [ ] User authentication (favorites, saved searches)

### Phase 2 Features
- [ ] Admin dashboard (static/admin/ exists but not connected)
- [ ] Community euthanasia date reporting
- [ ] Foster matching system
- [ ] Success stories page
- [ ] Push notifications for urgent dogs
- [ ] Transport coordination

---

## Quick Reference

> **SECURITY NOTE (2026-03-29):** Secrets were previously exposed in this file and are in git history.
> CRON_SECRET and RESCUEGROUPS_API_KEY have been rotated. Old values are compromised.
> All secrets are now stored ONLY in Vercel environment variables and .env.local.
> Never commit secrets to this file again.

**Supabase Ref:** `hpssqzqwtsczsxvdfktt`
**Supabase URL:** `https://hpssqzqwtsczsxvdfktt.supabase.co`
**Amazon Associates:** `waitingthelon-20`

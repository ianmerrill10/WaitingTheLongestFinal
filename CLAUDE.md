# WaitingTheLongest.com - Project Rules

## Mission
Find all adoptable dogs in the United States from shelters, rescues, and non-profit agencies.
Show real-time euthanasia countdown timers so visitors can help save dogs before time runs out.
Every dog listed for free. Every shelter integration for free.

## BANNED: PETFINDER
**PETFINDER IS PERMANENTLY BANNED FROM THIS PROJECT.**
- DO NOT use any Petfinder API
- DO NOT ask for a Petfinder API key
- DO NOT look for a Petfinder API key
- DO NOT reference Petfinder in any code, comments, or documentation
- DO NOT suggest Petfinder as a data source
- The concept of Petfinder is banned entirely
- If you see Petfinder referenced anywhere, DELETE IT

## Tech Stack
- **Framework:** Next.js 14+ (App Router) with TypeScript
- **Database:** Supabase (PostgreSQL + Realtime + Auth)
- **Styling:** Tailwind CSS (mobile-first)
- **Hosting:** Vercel (paid account)
- **Domain:** waitingthelongest.com (DNS via IONOS)
- **Local Agents:** Node.js/tsx processes for 24/7 data management

## Data Sources (Approved)
- RescueGroups.org API v5 (primary — ~33K dogs)
- Government open data via Socrata API (Austin TX, Bloomington IN, Norfolk VA)
- TheDogAPI (breed data, 4 keys)
- 50-state universal scraper (8 platform adapters)
- Direct shelter feeds (JSON/CSV via partner API)
- ShelterLuv / PetPoint / Chameleon / ShelterManager / Petstablished public pages
- Community reports (verified users)
- Our ~45K organization database

## Key Rules
- Dogs only (cats and other animals come in future phases)
- No breeders, no pet stores, no for-profit entities
- All shelter/rescue services are FREE
- Adoption flow redirects to shelter's own website (we don't process adoptions)
- LED wait time format: YY:MM:DD:HH:MM:SS (counting up from intake_date)
- Euthanasia countdown format: DD:HH:MM:SS (counting down to euthanasia_date)
- Urgency levels: CRITICAL (<24h), HIGH (<72h), MEDIUM (<7d), NORMAL (>7d)
- No ads on urgent/euthanasia dog pages
- Amazon Associates ID: waitingthelon-20
- DB column for state is `state_code` (NOT `state`)
- DB columns: `available_dog_count`, `urgent_dog_count` (NOT `available_dogs`, `total_dogs`)
- Tests use `node:test` with `tsx --test` (NOT Jest)
- Build requires `NODE_OPTIONS=--max-old-space-size=4096`

## File Structure
- `src/app/` - Next.js App Router (42 pages, 48 API routes)
- `src/components/` - React components (29 components)
- `src/lib/` - Core business logic (17 modules)
  - `rescuegroups/` - RG API client, mapper, sync
  - `verification/` - Dog listing verification engine
  - `audit/` - Data quality audit system
  - `scrapers/` - 50-state scraper with 8 adapters
  - `connectors/` - Gov data sync (Socrata)
  - `shelter-api/` - Partner API auth, rate limiting
  - `shelter-crm/` - Shelter relationship management
  - `outreach/` - Email outreach automation
  - `social/` - Social media content generation
  - `monetization/` - Affiliate tracking
  - `stats/` - Daily statistics
  - `supabase/` - Database clients
  - `utils/` - Shared utilities
- `src/types/` - TypeScript type definitions
- `src/data/` - Seed data and reference tables
- `supabase/migrations/` - Database migrations (001-017)
- `agent/` - Local autonomous agents (7 files, 5 agents)
- `tests/` - Test files (tsx --test)
- `docs/` - Documentation and archives

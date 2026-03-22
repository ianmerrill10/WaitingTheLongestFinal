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
- **Domain:** waitingthelongest.com

## Data Sources (Approved)
- RescueGroups.org API v5 (primary)
- TheDogAPI (breed data, 4 keys)
- Adopt-a-Pet (scraper)
- Direct shelter feeds (JSON/CSV)
- ShelterLuv / PetPoint / Chameleon public pages
- Community reports (verified users)
- Our 40,085 organization database

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

## File Structure
- `src/app/` - Next.js App Router pages and API routes
- `src/components/` - React components
- `src/lib/` - Utilities, Supabase clients, API clients
- `src/types/` - TypeScript type definitions
- `src/data/` - Seed data scripts
- `supabase/migrations/` - Database migrations (001-011)
- `agent/` - Local autonomous agents (data, improvement, backup)
- `sql/` - Additional SQL modules (aggregator schema)
- `backups/` - Local backup archives (auto-generated)

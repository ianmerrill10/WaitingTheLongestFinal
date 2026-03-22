# Data Sources Strategy

## Current State

**Primary source:** RescueGroups.org API v5
- ~33,000+ dogs synced
- Search endpoint returns basic data (no availableDate/foundDate/killDate)
- Individual animal endpoint returns rich data (availableDate, foundDate, killDate, birthDate)
- killDate field is populated by 0 of 33K dogs (shelters rarely enable this in RG)
- Verification agent captures dates via individual lookups (~7,200 dogs/day)

**Gap:** Euthanasia countdown timers — the site's core feature — have no real data. RescueGroups killDate is almost never populated.

## Research Findings (March 2026)

### Platform API Availability

| Platform | Public API? | Euthanasia Data? | Access Model |
|----------|------------|-----------------|--------------|
| RescueGroups | Yes (v5) | killDate field exists, rarely populated | API key (we have) |
| ShelterLuv | Yes (v1) | No euthanasia fields exposed | Per-shelter API key needed |
| PetPoint | No public API | No | Walled garden |
| Chameleon/PetHarbor | Scrapeable HTML | No | Public pages |
| ShelterManager (ASM) | Yes (open source) | Possibly via flags | Per-shelter auth needed |
| ShelterBuddy | Documented API | Possible "commitment date" | Per-shelter access |

### Shelters That Publicly Post At-Risk Lists

| Shelter | Location | Format | Has Dates? |
|---------|----------|--------|-----------|
| NYC ACC | New York | ShelterBuddy portal | Yes (commitment date) |
| The Animal Foundation | Las Vegas | Crystal Reports | Yes (commitment date) |
| BARCS | Baltimore | Web page | Urgency only |
| El Paso Animal Services | El Paso TX | PetBridge embed | "Two weeks or less" |
| San Antonio ACS | San Antonio TX | PDF | Yes (capacity list) |
| Fulton County Animal Services | Atlanta | Web page | Daily updates |
| DeKalb County Animal Services | Atlanta | Web page | Daily updates |

### Key Discovery: DogsInDanger.com

DogsInDanger.com receives data FROM RescueGroups.org. Shelters must explicitly enable sharing of euthanasia date/reason in RescueGroups export settings. This means the killDate field CAN be populated — shelters just need to turn it on.

## Recommended Strategy (Priority Order)

### Phase 1: Maximize RescueGroups (NOW)

**Effort: Low | Impact: Medium**

1. Continue running verification agent to capture all available dates
2. Monitor killDate captures — if any appear, our pipeline already handles them
3. Parse dog descriptions for euthanasia keywords ("will be euthanized", "on the list", "time is running out") to flag urgency even without a date

### Phase 2: Municipal Shelter Scrapers (NEXT)

**Effort: Medium | Impact: High**

Build targeted scrapers for the 10-15 largest municipal kill shelters that post at-risk lists publicly:

- **NYC ACC** (ShelterBuddy API at newhope.shelterbuddy.com) — highest priority, structured data
- **Las Vegas Animal Foundation** (Crystal Reports at rescue2.animalfoundation.com:8448)
- **Fulton + DeKalb County GA** (LifeLine Animal Project pages)
- **El Paso TX** (PetBridge embed)
- **San Antonio TX** (PDF scraping)

These shelters process the highest volumes and are the most likely to euthanize. Even covering 10 shelters would give us real euthanasia data for thousands of dogs.

Implementation:
- Create `src/lib/scrapers/` directory
- One scraper per shelter/platform
- Map scraped data to our dog schema
- Run scrapers on schedule via the local agent
- Match scraped dogs to existing RG records by name/breed/shelter

### Phase 3: ShelterBuddy API Integration

**Effort: Medium | Impact: Medium**

ShelterBuddy has a documented public API with incremental sync support:
- Endpoint: `/api/animals`
- Supports `UpdatedSinceUtc` for incremental sync
- Used by NYC ACC's New Hope portal
- May expose "commitment date" (euthanasia deadline)

Investigate whether the public API returns urgency/commitment fields.

### Phase 4: Direct Shelter Partnerships

**Effort: High | Impact: High (long-term)**

Our partner registration system already captures `current_software` and `integration_preference`. When shelters register:

1. **ShelterLuv shelters** → request their API key → sync via ShelterLuv API
2. **ASM shelters** → use their service API (JSON endpoints) → sync animals
3. **CSV/manual shelters** → use our bulk import API (`/api/v1/dogs/import`)

The partner portal already supports these flows — we just need shelters to sign up.

### Phase 5: Description-Based Urgency Detection

**Effort: Low | Impact: Medium**

Many shelters include urgency language in dog descriptions even without setting euthanasia dates:
- "on the euth list"
- "will be euthanized by [date]"
- "needs rescue by [date]"
- "CODE RED"
- "at risk of euthanasia"
- "urgent — needs out by [date]"

Build a text parser that:
1. Scans all dog descriptions for urgency keywords
2. Extracts dates when mentioned
3. Sets `urgency_level` and `euthanasia_date` when confident
4. Flags ambiguous cases for review

This leverages data we already have and requires no external API access.

## NOT Recommended

- **PetPoint API** — No public access, walled garden, no path forward
- **PetHarbor.com scraping** — No euthanasia data, duplicates what RG provides
- **Adopt-a-Pet scraping** — Legal risk, may duplicate RG data
- **Any Petfinder integration** — PERMANENTLY BANNED from this project

## Infrastructure Already in Place

- `aggregator_sources` table with `source_type` enum
- `shelter_onboarding.current_software` captures platform
- `shelters.integration_type` tracks data flow method
- CSV bulk import endpoint at `/api/v1/dogs/import`
- Verification engine with callback support for real-time events
- Local agent with extensible task scheduler

## Next Concrete Steps

1. Build description urgency parser (Phase 5 — quick win)
2. Build NYC ACC scraper (Phase 2 — highest-impact single shelter)
3. Add scraper task to local agent schedule
4. Track euthanasia data capture metrics in daily_stats

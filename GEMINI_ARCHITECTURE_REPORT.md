# Architectural Blueprint for an Autonomous, High-Fidelity Pet Adoption Tracking and Media Generation System

*Deep research report from Gemini — saved for reference and implementation guidance*
*Updated version: confirms Petfinder API was decommissioned Dec 2, 2025 — aligns with our permanent ban*

## Overview

The intersection of artificial intelligence, decentralized data aggregation, and social media automation presents unprecedented opportunities for animal welfare advocacy. The initiative to track, verify, and showcase dogs experiencing the longest shelter wait times across the United States requires an architecture that is not only highly autonomous but also exceptionally rigorous in its data validation. Building a perpetually running, locally hosted autonomous agent on a Windows environment to maintain a 99.99% accurate database demands a sophisticated orchestration of large language models (LLMs), computer vision for identity resolution, natural language processing for temporal extraction, and headless media generation pipelines.

The historical landscape of animal welfare data has been notoriously fractured. With thousands of independent shelters, rescues, and municipal animal control facilities utilizing disparate shelter management systems (SMS)—such as Shelterluv, PetPoint, Chameleon, and Shelter Buddy—the consolidation of a single source of truth has been exceptionally difficult. Compounding this fragmentation is the frequent transfer of animals between municipal facilities and private rescues, a process that routinely overwrites historical data and artificially resets an animal's recorded wait time. To engineer a solution named "Waiting the Longest" that strictly excludes for-profit breeders, tracks wait times with verified accuracy, and autonomously generates high-retention vertical video content for platforms like TikTok and Facebook, a multi-layered agentic framework must be established.

## The Autonomous Daemon Framework in a Windows Environment

### The Agentic Loop and Worktree Isolation

Claude Code operates on a deterministic agentic loop comprising three distinct, sequential phases: Understand, Act, and Validate. For a persistent background task, this loop must be completely decoupled from interactive user inputs and embedded into a scheduled, self-monitoring lifecycle.

### Scheduled Execution

| Scheduling Modality | Description | Optimal Use Case |
|---|---|---|
| Windows Service (NSSM) | OS-level persistence wrapper that restarts failed processes | System boot recovery; fatal exception handling |
| Claude Code /loop | Native CLI command for recurring prompt execution | Regular API polling; batch deduplication processing |
| Background Worktrees | Isolated execution environments that commit changes independently | Sandbox execution of untrusted shelter data parsing |

### Deterministic Self-Validation via Stop Hooks

A critical vulnerability in long-running autonomous LLM agents is the accumulation of errors—often termed "drifting." To achieve the 99.99% accuracy threshold:
- Implement stop hooks that automatically execute after tool calls to verify output
- If parsed data violates expected constraints (missing intake date, negative wait time, etc.), halt the commit process
- Feed error traces back into the agent's context for self-correction

### State Persistence and Memory Management

- CLAUDE.md as persistent memory matrix
- Model Context Protocol (MCP) memory server for granular state persistence
- Track which shelters scraped, API rate limit status, data anomaly resolution

## Decentralized Data Ingestion and Entity Validation

### Primary Aggregation Vectors

| Data Source | Protocol/Format | Key Value | Limitations |
|---|---|---|---|
| RescueGroups.org v5 API | RESTful HTTP/JSON | 6,200+ partners; deep metadata | Strict rate limiting (429 errors) |
| Shelter Pet Data Alliance (SPDA) | API Integration | Direct SMS integration (Shelterluv, PetPoint, ShelterBuddy) | Requires partner authorization |
| Municipal Open Data | CSV/JSON via Socrata | Authoritative, hourly updates; accurate intake dates | Fragmented; requires custom parsers per county |

### Entity Validation: Enforcing the Non-Profit Constraint

To exclude for-profit breeders/pet stores at 99.99% accuracy:
1. Extract shelter's legal name, address, and EIN
2. Cross-reference against IRS Tax Exempt Organization Search (TEOS) database
3. Verify active, unrevoked 501(c)(3) status
4. For municipal entities: validate .gov domains, official seals, USDA AWA registration
5. Any entity failing checks is permanently blacklisted

## Identity Resolution and Deduplication Architecture

### Visual Deduplication via Perceptual Hashing (pHash)

Traditional cryptographic hashes are useless for image dedup. pHash solves this by:
1. Convert to grayscale, resize to 32x32
2. Apply low-pass filter
3. Compute DCT to separate frequencies
4. Generate 64-bit hash based on frequency comparison to mean
5. Compare using Hamming distance (threshold: edit distance <= 8)

For scale: use Vantage Point Trees (MVP Trees) or BK-Trees for rapid Hamming space queries.

### Deterministic Matching via Microchip Registries

- Extract microchip IDs from API payloads (9, 10, or 15-digit ISO standard)
- Cross-reference against AAHA, PetLink, 24Petwatch databases
- Deterministically link records across different shelter systems
- Force retention of earliest intake date across all linked records

## Algorithmic Determination of "Wait Time"

### Extracting and Normalizing Temporal Data

1. Extract `createdDate`, `intake_date`, `availableDate` from APIs
2. Normalize to universal UNIX timestamp
3. When identity resolution links records: use earliest verified intake_date

### NLP Date Extraction and Archival Verification

- Named entity recognition (NER) for temporal markers in descriptions
- Parse embedded clues: "Rex has been with us since last Thanksgiving"
- Query Wayback Machine API for earliest URL/image index date
- Establish chronological floor for minimum wait time

## High-Concurrency Database Architecture

### Recommended Schema

| Table | Primary Purpose | Key Fields |
|---|---|---|
| Entities | Unique biological dog | entity_id, microchip_id, earliest_intake_date, calculated_wait_days, status |
| Listings | Individual shelter postings | listing_id, entity_id (FK), source_url, reported_intake_date, shelter_id |
| Media | Visual assets and hash signatures | media_id, entity_id (FK), image_url, phash_signature_64bit, wayback_index_date |
| Organizations | Verified shelter/rescue data | shelter_id, name, ein, 501c3_verified, api_source |

### Key Optimization

Isolate Entities from volatile Listings table for optimized B-tree index scans on `calculated_wait_days`.

## Automated Media Generation Pipeline

### Programmatic Video Synthesis Workflow

1. **Data Query**: Select dog with highest verified wait time not yet featured
2. **Asset Retrieval**: Download, normalize, content-aware crop images
3. **Script Generation**: LLM generates emotionally resonant short-form script
4. **Audio Synthesis**: TTS (Kokoro/Edge TTS) for realistic voiceover
5. **Compositing**: FFmpeg layers visuals + audio + captions + "Days Waiting" overlay

### Platform Distribution

- Use Ayrshare API for scheduled posting
- Headless browser (Playwright) for 1080p TikTok uploads (bypassing 720p API limit)
- Local Windows execution avoids IP blacklisting from cloud IPs

## Implementation Priority for WaitingTheLongest.com

### Already Built (in our codebase):
- [x] RescueGroups v5 API integration
- [x] Autonomous agent daemon with scheduled tasks
- [x] NLP date extraction from descriptions
- [x] Age sanity checks
- [x] Deduplication by external_id
- [x] Self-validation via audit system (10 checks)
- [x] CLAUDE.md persistent memory
- [x] Description urgency parsing
- [x] NYC ACC scraper
- [x] Listing classifier (breeder detection)

### Next to implement:
- [ ] pHash visual deduplication
- [ ] Microchip ID extraction and matching
- [ ] 501(c)(3) / IRS TEOS verification
- [ ] Wayback Machine earliest index verification
- [ ] SPDA API integration
- [ ] Municipal open data scrapers (Socrata feeds)
- [ ] Video generation pipeline (FFmpeg + TTS)
- [ ] Social media auto-posting (TikTok, Facebook)
- [ ] Entities/Listings table separation for true identity resolution

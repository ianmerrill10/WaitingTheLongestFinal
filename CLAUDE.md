# WaitingTheLongest.com - Project Context

## What This Is
A C++17 web application (Drogon framework) to save dogs from shelter euthanasia. Shows real-time LED wait time counters for every shelter dog, matches adopters to dogs via lifestyle quiz, removes barriers (transport, housing, fees), and amplifies reach via social/TikTok automation.

## Tech Stack
- **Language:** C++17 with Drogon web framework
- **Database:** PostgreSQL 16 with libpqxx (RAII connection pooling)
- **Cache:** Redis (sessions, API caching)
- **Real-time:** WebSocket for LED counter updates (YY:MM:DD:HH:MM:SS format)
- **Frontend:** Static HTML/CSS/JS served by Drogon (no JS framework)
- **Build:** CMake 3.16+
- **Deploy:** Docker + docker-compose (app, postgres, redis, nginx, certbot)

## Architecture
- `src/core/` - Controllers, services, models, db, auth, debug, websocket, EventBus
- `src/modules/` - IModule interface, ModuleManager, 8 intervention modules
- `src/content/` - TikTok, blog, social media, video/image generation, templates
- `src/analytics/` - Event tracking, metrics, impact, reporting
- `src/notifications/` - Push (FCM), email (SendGrid), SMS (Twilio) channels
- `src/aggregators/` - RescueGroups, direct shelter data sync (NO PETFINDER - their API does not exist)
- `src/workers/` - Background job scheduler with cron parsing
- `src/clients/` - HTTP client, OAuth2, rate limiter
- `src/admin/` - Admin service, system config, audit log
- `sql/core/` - 15 core migration files (001-013 + seeds)
- `sql/modules/` - 15 module migration files
- `static/` - Frontend HTML/CSS/JS + admin dashboard
- `config/` - JSON config files (dev, production, aggregators, media, tiktok)

## Key Design Decisions
- Urgency levels: NORMAL (>7d), MEDIUM (4-7d), HIGH (1-3d), CRITICAL (<24h)
- LED counter format: YY:MM:DD:HH:MM:SS (wait time), DD:HH:MM:SS (countdown)
- JWT auth with HS256, bcrypt cost factor 12
- EventBus pub-sub for inter-module communication
- Modules can be independently enabled/disabled
- Foster-first approach (14x better adoption outcomes)
- Adoption flow redirects to shelter's own website (we don't handle transactions)
- Agency registration is free (no fees/deposits)

## Current Status
- **Code:** 425+ files written, NEVER COMPILED
- **Phase:** Pre-compilation. Need to compile, fix errors, test, deploy.
- **Tracking:** See LAUNCH_BLUEPRINT.md for execution plan

## Build Commands
```bash
# Docker (recommended)
docker-compose up -d

# Local
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Important Files
- `LAUNCH_BLUEPRINT.md` - Step-by-step plan to go live
- `BUILD_PLAN.md` - Complete feature specification (58KB)
- `FILESLIST.md` - Inventory of all 425+ files
- `CODING_STANDARDS.md` - Code conventions all agents followed
- `INTEGRATION_CONTRACTS.md` - Interface contracts between components
- `WORK_LOG.md` - Chronological build history

## Conventions
- Namespaces: `wtl::core::services`, `wtl::modules`, `wtl::content::tiktok`, etc.
- Singletons: `ClassName::getInstance()` pattern throughout
- Config: `Config::getInstance().getString("key", "default")`
- DB: `ConnectionPool::getInstance().getConnection()` returns RAII handle
- Errors: `ErrorCapture::getInstance().captureError(...)` + circuit breaker via SelfHealing
- Events: `EventBus::getInstance().publish(EventType::DOG_CREATED, data)`
- All services init from config: `service.initializeFromConfig()`

## CRITICAL: NO PETFINDER API

**THERE IS NO PETFINDER API. IT DOES NOT EXIST. PETFINDER SHUT DOWN THEIR PUBLIC API.**

- Do NOT reference, suggest, or mention a "Petfinder API" anywhere
- Do NOT suggest registering for Petfinder API keys
- Do NOT add Petfinder as a data source
- Do NOT write code that calls any Petfinder endpoint
- The PetfinderAggregator.cc/h files have been DELETED from this project
- The ONLY external data source is RescueGroups.org API + direct shelter feeds
- If you mention Petfinder API, you are wrong. Period.

## Don't
- Don't add stubs or placeholder code - everything must be production-complete
- Don't change the LED counter format (YY:MM:DD:HH:MM:SS is final)
- Don't add payment processing (we redirect to shelter websites)
- Don't use any JS framework for frontend (vanilla HTML/CSS/JS only)
- Don't break the module independence (modules must work when others are disabled)
- **Don't EVER mention or suggest using a Petfinder API - IT DOES NOT EXIST**

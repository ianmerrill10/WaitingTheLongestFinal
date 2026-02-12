# WaitingTheLongest.com

Real-time dog rescue platform showing shelter dogs waiting for adoption. Every second matters.

## Mission

Save dogs from shelter euthanasia by showing real-time LED wait time counters, matching adopters to dogs via lifestyle quiz, removing barriers (transport, housing, fees), and amplifying reach via social media automation.

## Tech Stack

- **Backend:** C++17 with [Drogon](https://github.com/drogonframework/drogon) web framework
- **Database:** PostgreSQL 16 with libpqxx
- **Cache:** Redis 7 (sessions, API caching)
- **Real-time:** WebSocket for LED counter updates (YY:MM:DD:HH:MM:SS format)
- **Frontend:** Static HTML/CSS/JS (no JS framework)
- **Proxy:** Nginx with rate limiting and WebSocket support
- **Deploy:** Docker Compose (app, postgres, redis, nginx, certbot)

## Quick Start

```bash
# Clone and configure
cp .env.example .env
# Edit .env with your credentials

# Start all services
docker compose up -d

# Check health
curl http://localhost/health-check
```

## Architecture

```
src/
  core/         - Controllers, services, models, auth, WebSocket, EventBus
  modules/      - 8 intervention modules (foster, transport, housing, etc.)
  content/      - TikTok, blog, social media, video/image generation
  analytics/    - Event tracking, metrics, impact reporting
  notifications/- Push (FCM), email (SendGrid), SMS (Twilio)
  aggregators/  - RescueGroups.org API, direct shelter data sync
  workers/      - Background job scheduler with cron parsing
  clients/      - HTTP client, OAuth2, rate limiter
  admin/        - Admin service, system config, audit log
```

## Key Features

- **LED Wait Time Counters** - Real-time YY:MM:DD:HH:MM:SS display for every shelter dog
- **Lifestyle Matching Quiz** - Match adopters to compatible dogs
- **Urgency System** - NORMAL (>7d), MEDIUM (4-7d), HIGH (1-3d), CRITICAL (<24h)
- **8 Intervention Modules** - Foster, transport, housing, breed advocacy, and more
- **Social Media Automation** - TikTok, Instagram, Twitter, Facebook content generation
- **Admin Dashboard** - Full management interface with analytics

## Data Sources

- **RescueGroups.org API** - Primary external data source
- **Direct Shelter Feeds** - Direct integrations with individual shelters

> **Note:** There is NO Petfinder API available. Their public API has been shut down.

## Environment Variables

See [.env.example](.env.example) for all required configuration.

## Documentation

- [LAUNCH_BLUEPRINT.md](LAUNCH_BLUEPRINT.md) - Step-by-step deployment plan
- [BUILD_PLAN.md](BUILD_PLAN.md) - Complete feature specification
- [CODING_STANDARDS.md](CODING_STANDARDS.md) - Code conventions
- [INTEGRATION_CONTRACTS.md](INTEGRATION_CONTRACTS.md) - Interface contracts
- [PROGRESS_REPORT.md](PROGRESS_REPORT.md) - Current status

## License

All rights reserved. See [LICENSE](LICENSE) for details.

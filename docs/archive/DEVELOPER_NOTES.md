# WaitingTheLongest.com - Developer Notes

**Version:** 1.0
**Created:** 2024-01-28
**Purpose:** Comprehensive technical documentation for current and future developers.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Architecture](#2-architecture)
3. [Getting Started](#3-getting-started)
4. [Core Components](#4-core-components)
5. [Database](#5-database)
6. [API Reference](#6-api-reference)
7. [WebSocket](#7-websocket)
8. [Modules](#8-modules)
9. [Debug System](#9-debug-system)
10. [Configuration](#10-configuration)
11. [Deployment](#11-deployment)
12. [Troubleshooting](#12-troubleshooting)
13. [Decision Log](#13-decision-log)

---

## 1. Project Overview

### 1.1 Mission
WaitingTheLongest.com is an **active intervention system** designed to save dogs from shelter euthanasia. Unlike passive listing services, this system actively pushes dogs to potential adopters/fosters through automated social media, urgency alerts, and intelligent matching.

### 1.2 Key Statistics (Research-Based)
- 334,000-670,000 dogs euthanized annually in the US
- Foster care makes dogs 14x more likely to be adopted
- Video content doubles adoption rates
- TikTok doubled adoption rates at shelters that used it
- Top 5 states (CA, TX, NC, FL, LA) account for 52% of euthanasia

### 1.3 Core Features
1. **LED Wait Time Counter** - Shows how long each dog has waited (YY:MM:DD:HH:MM:SS)
2. **Countdown Timer** - Shows time remaining for dogs on euthanasia lists
3. **Foster-First Pathway** - Prioritizes fostering (14x more effective)
4. **Urgency Engine** - Monitors kill shelters, triggers alerts
5. **Automated TikTok** - Posts urgent dogs automatically
6. **DOGBLOG** - Content about urgent dogs + educational content
7. **Debug System** - Self-healing, error capture, exportable logs

### IMPORTANT: No Petfinder API
There is NO Petfinder API available. Their public API has been shut down. The only external data sources are RescueGroups.org API and direct shelter feeds. Do not reference or attempt to use any Petfinder API.

### 1.4 Technology Stack
- **Language:** C++17
- **Web Framework:** Drogon
- **Database:** PostgreSQL 16
- **Cache:** Redis
- **Build System:** CMake

---

## 2. Architecture

### 2.1 High-Level Architecture
```
┌─────────────────────────────────────────────────────────────────┐
│                         CLIENTS                                  │
│  (Web Browser, Mobile App, API Consumers)                       │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                      DROGON HTTP SERVER                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │ Controllers │  │  WebSocket  │  │ Middleware  │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
└─────────┼────────────────┼────────────────┼─────────────────────┘
          │                │                │
          ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        SERVICES LAYER                            │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐       │
│  │DogService │ │ShelterSvc │ │SearchSvc  │ │AuthService│       │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘       │
└────────┼─────────────┼─────────────┼─────────────┼──────────────┘
         │             │             │             │
         ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────────┐
│                     CONNECTION POOL                              │
│                    (PostgreSQL + Redis)                          │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│                        DATABASE                                  │
│  dogs │ shelters │ states │ users │ foster_homes │ error_logs  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Module Architecture
```
┌─────────────────────────────────────────────────────────────────┐
│                      CORE PLATFORM                               │
│  (Always runs - can operate without any modules)                │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                     EVENT BUS / API
                            │
      ┌─────────────────────┼─────────────────────┐
      │                     │                     │
┌─────┴─────┐         ┌─────┴─────┐         ┌─────┴─────┐
│  Module A │         │  Module B │         │  Module C │
│  (Optional)│         │  (Optional)│         │  (Optional)│
└───────────┘         └───────────┘         └───────────┘
```

Each module:
- Can be enabled/disabled via config.json
- Communicates via EventBus (no direct dependencies)
- Has its own database tables (created only if enabled)
- Tracks its own metrics

---

## 3. Getting Started

### 3.1 Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libpq-dev \
    libpqxx-dev \
    libjsoncpp-dev \
    libssl-dev \
    libuuid1 \
    uuid-dev \
    zlib1g-dev \
    postgresql-16 \
    redis-server

# Install Drogon
git clone https://github.com/drogonframework/drogon.git
cd drogon
git submodule update --init
mkdir build && cd build
cmake ..
make && sudo make install
```

### 3.2 Building the Project
```bash
# Clone repository
git clone https://github.com/yourusername/WaitingTheLongest.git
cd WaitingTheLongest

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run tests
./tests/run_tests

# Run application
./WaitingTheLongest
```

### 3.3 Configuration
Copy and modify the config template:
```bash
cp config/config.example.json config/config.json
# Edit config.json with your settings
```

### 3.4 Database Setup
```bash
# Create database
sudo -u postgres createdb waitingthelongest

# Run migrations
./scripts/migrate.sh

# Seed initial data
./scripts/seed.sh
```

---

## 4. Core Components

### 4.1 ConnectionPool
**Location:** `src/core/db/ConnectionPool.h`

Manages PostgreSQL connections with automatic pooling and recovery.

```cpp
// Usage
auto conn = ConnectionPool::getInstance().acquire();
pqxx::work txn(*conn);
auto result = txn.exec("SELECT * FROM dogs");
txn.commit();
ConnectionPool::getInstance().release(conn);
```

**Configuration:**
- `database.pool_size` - Number of connections (default: 20)
- `database.connection_timeout_ms` - Timeout for acquiring (default: 5000)

### 4.2 Services
All services are singletons accessed via `getInstance()`:

```cpp
auto& dogService = DogService::getInstance();
auto& shelterService = ShelterService::getInstance();
auto& stateService = StateService::getInstance();
auto& userService = UserService::getInstance();
auto& searchService = SearchService::getInstance();
auto& authService = AuthService::getInstance();
```

### 4.3 Models
Models handle data conversion between JSON, database rows, and C++ objects:

```cpp
// From database
Dog dog = Dog::fromDbRow(row);

// To JSON (for API responses)
Json::Value json = dog.toJson();

// From JSON (for API requests)
Dog dog = Dog::fromJson(requestBody);
```

---

## 5. Database

### 5.1 Schema Overview
```
states (50 rows)
  └── shelters (many per state)
        └── dogs (many per shelter)

users
  ├── favorites (dogs they've favorited)
  ├── sessions (login sessions)
  └── foster_homes (if foster)
        └── foster_placements
```

### 5.2 Key Tables

**dogs** - Central table for all adoptable dogs
- `id` (UUID) - Primary key
- `shelter_id` - Foreign key to shelters
- `intake_date` - When dog entered shelter (used for wait time)
- `euthanasia_date` - If scheduled (used for countdown)
- `urgency_level` - Calculated: normal, medium, high, critical

**shelters** - Shelter/rescue organizations
- `is_kill_shelter` - Boolean for urgency prioritization
- `avg_hold_days` - Average days before euthanasia

### 5.3 Migrations
Migrations are in `sql/core/` and numbered sequentially:
```
001_extensions.sql
002_states.sql
003_shelters.sql
...
```

Run migrations with:
```bash
./scripts/migrate.sh
```

---

## 6. API Reference

### 6.1 Response Format
All responses follow this structure:

**Success:**
```json
{
    "success": true,
    "data": { ... },
    "meta": { "total": 100, "page": 1, "per_page": 20 }
}
```

**Error:**
```json
{
    "success": false,
    "error": {
        "code": "ERROR_CODE",
        "message": "Human readable message"
    }
}
```

### 6.2 Endpoints

**Dogs**
- `GET /api/dogs` - List dogs (paginated)
- `GET /api/dogs/:id` - Get single dog
- `GET /api/dogs/search` - Search with filters
- `GET /api/dogs/urgent` - Get most urgent dogs
- `GET /api/dogs/longest-waiting` - Get longest waiting

**Shelters**
- `GET /api/shelters` - List shelters
- `GET /api/shelters/:id` - Get single shelter
- `GET /api/shelters/:id/dogs` - Get dogs at shelter

**States**
- `GET /api/states` - List all states
- `GET /api/states/:code` - Get single state
- `GET /api/states/:code/dogs` - Get dogs in state
- `GET /api/states/:code/shelters` - Get shelters in state

**Auth**
- `POST /api/auth/register` - Register new user
- `POST /api/auth/login` - Login
- `POST /api/auth/logout` - Logout
- `POST /api/auth/refresh` - Refresh token

**Foster**
- `POST /api/foster/register` - Register as foster
- `GET /api/foster/matches` - Get matched dogs
- `POST /api/foster/apply/:dog_id` - Apply to foster

**Debug (Admin only)**
- `GET /api/debug/errors` - Get error logs
- `GET /api/debug/errors/export` - Export as file
- `GET /api/debug/health` - System health
- `POST /api/debug/heal` - Trigger healing action

---

## 7. WebSocket

### 7.1 Connection
Connect to WebSocket at: `ws://host:port/ws/dogs`

### 7.2 Message Types

**Subscribe to dog updates:**
```json
{
    "type": "SUBSCRIBE_DOG",
    "dog_id": "uuid-here"
}
```

**Wait time update (server → client):**
```json
{
    "type": "WAIT_TIME_UPDATE",
    "dog_id": "uuid",
    "payload": {
        "years": 2,
        "months": 3,
        "days": 15,
        "hours": 8,
        "minutes": 45,
        "seconds": 32,
        "formatted": "02:03:15:08:45:32"
    }
}
```

**Countdown update (server → client):**
```json
{
    "type": "COUNTDOWN_UPDATE",
    "dog_id": "uuid",
    "payload": {
        "days": 2,
        "hours": 14,
        "minutes": 30,
        "seconds": 0,
        "formatted": "02:14:30:00",
        "is_critical": false
    }
}
```

---

## 8. Modules

### 8.1 Enabling/Disabling Modules
In `config.json`:
```json
{
    "modules": {
        "urgency_engine": { "enabled": true },
        "foster_first": { "enabled": true },
        "tiktok_automation": { "enabled": false }
    }
}
```

### 8.2 Creating a New Module
1. Create directory: `src/modules/your_module/`
2. Implement `IModule` interface
3. Register in `ModuleManager`
4. Create SQL migrations in `sql/modules/`

```cpp
class YourModule : public IModule {
public:
    std::string getName() const override { return "your_module"; }
    bool initialize() override { /* setup */ return true; }
    void shutdown() override { /* cleanup */ }
    // ... other methods
};
```

---

## 9. Debug System

### 9.1 Error Capture
All errors should use the capture macros:

```cpp
WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, "Error message", metadata);
WTL_CAPTURE_CRITICAL(ErrorCategory::EXTERNAL_API, "API failed", metadata);
```

### 9.2 Self-Healing
Wrap risky operations:

```cpp
auto result = SelfHealing::getInstance().executeWithHealing<Dog>(
    "operation_name",
    [&]() { return riskyOperation(); },
    [&]() { return fallbackValue(); }
);
```

### 9.3 Exporting Logs
Via API:
```
GET /api/debug/errors/export?format=txt
GET /api/debug/errors/export?format=json
GET /api/debug/errors/export?format=ai
```

The `ai` format is optimized for feeding to Claude for analysis.

---

## 10. Configuration

### 10.1 config.json Structure
```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "threads": 4
    },
    "database": {
        "host": "localhost",
        "port": 5432,
        "name": "waitingthelongest",
        "user": "postgres",
        "password": "secret",
        "pool_size": 20
    },
    "redis": {
        "host": "localhost",
        "port": 6379
    },
    "auth": {
        "jwt_secret": "your-secret-key",
        "token_expiry_hours": 24
    },
    "modules": {
        "urgency_engine": { "enabled": true },
        "foster_first": { "enabled": true }
    },
    "debug": {
        "enabled": true,
        "log_level": "info",
        "retain_days": 30
    }
}
```

---

## 11. Deployment

### 11.1 Docker
```bash
docker build -t waitingthelongest .
docker-compose up -d
```

### 11.2 Manual Deployment
1. Build release: `cmake -DCMAKE_BUILD_TYPE=Release .. && make`
2. Copy binary and config to server
3. Set up systemd service
4. Configure nginx reverse proxy
5. Set up SSL with Let's Encrypt

---

## 12. Troubleshooting

### 12.1 Common Issues

**Connection pool exhausted:**
- Check `database.pool_size` in config
- Look for connection leaks (unreleased connections)
- Check long-running queries

**WebSocket disconnections:**
- Check nginx timeout settings
- Verify client heartbeat implementation

**Slow search queries:**
- Verify indexes exist
- Check EXPLAIN ANALYZE output
- Consider adding pg_trgm indexes for text search

### 12.2 Debug Mode
Enable verbose logging:
```json
{
    "debug": {
        "log_level": "debug"
    }
}
```

---

## 13. Decision Log

### D001: Why C++ with Drogon?
**Date:** 2024-01-28
**Decision:** Use C++17 with Drogon framework
**Reasoning:**
- Performance critical for real-time updates
- WebSocket support built-in
- Mature PostgreSQL support via pqxx
- Single binary deployment

### D002: Foster-First Approach
**Date:** 2024-01-28
**Decision:** Make fostering the primary CTA, not adoption
**Reasoning:**
- Research shows foster makes dogs 14x more likely to be adopted
- Empties shelter kennels faster
- Lower commitment barrier increases conversion

### D003: Two Counter Systems
**Date:** 2024-01-28
**Decision:** Implement both wait time (up) and countdown (down) timers
**Reasoning:**
- Wait time shows how long overlooked
- Countdown creates urgency for dogs facing euthanasia
- Different psychological triggers for different situations

### D004: Modular Architecture
**Date:** 2024-01-28
**Decision:** All features beyond core as optional modules
**Reasoning:**
- Can disable features without breaking system
- Easier testing and maintenance
- Allows gradual rollout
- Each module tracks own metrics

---

**END OF DEVELOPER NOTES**

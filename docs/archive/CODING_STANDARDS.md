# WaitingTheLongest.com - Coding Standards

**Version:** 1.0
**Created:** 2024-01-28
**Purpose:** Ensure all code produced by any agent follows identical patterns for seamless integration.

---

## 1. File Organization

### 1.1 Directory Structure
```
WaitingTheLongest/
├── src/
│   ├── main.cc                    # Entry point ONLY
│   ├── core/                      # Core platform (always required)
│   │   ├── controllers/           # HTTP API controllers
│   │   ├── services/              # Business logic
│   │   ├── models/                # Data models/DTOs
│   │   ├── db/                    # Database utilities
│   │   ├── websocket/             # WebSocket handlers
│   │   ├── auth/                  # Authentication
│   │   ├── debug/                 # Debug/error/healing system
│   │   └── utils/                 # Shared utilities
│   │
│   └── modules/                   # Optional modules
│       ├── module_manager/        # Module loading system
│       └── [module_name]/         # Each module isolated
│
├── include/                       # Public headers (if needed)
├── sql/                           # Database migrations
│   ├── core/                      # Core schema
│   └── modules/                   # Module schemas
├── static/                        # Static web assets
├── config/                        # Configuration files
├── tests/                         # Unit and integration tests
└── docs/                          # Documentation
```

### 1.2 File Naming
- **C++ Source:** `PascalCase.cc` (e.g., `DogService.cc`)
- **C++ Headers:** `PascalCase.h` (e.g., `DogService.h`)
- **SQL Migrations:** `NNN_description.sql` (e.g., `001_core_schema.sql`)
- **Config Files:** `lowercase.json` or `lowercase.yaml`
- **Documentation:** `UPPERCASE.md` for root docs, `lowercase.md` for subdocs

---

## 2. C++ Coding Standards

### 2.1 Language Version
- **C++17** minimum (for std::optional, std::variant, structured bindings)
- Compile with `-std=c++17` or higher

### 2.2 Namespaces
All code lives under the `wtl` (WaitingTheLongest) namespace:

```cpp
namespace wtl {
namespace core {
namespace services {
    // DogService lives here
}
}
}

// Or using C++17 nested namespaces:
namespace wtl::core::services {
    // DogService lives here
}
```

**Namespace hierarchy:**
- `wtl::core` - Core platform code
- `wtl::core::controllers` - API controllers
- `wtl::core::services` - Business logic services
- `wtl::core::models` - Data models
- `wtl::core::db` - Database utilities
- `wtl::core::auth` - Authentication
- `wtl::core::debug` - Debug system
- `wtl::core::utils` - Utilities
- `wtl::modules::[name]` - Module-specific code

### 2.3 Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `DogService` |
| Methods | camelCase | `findById()` |
| Variables | snake_case | `dog_count` |
| Constants | UPPER_SNAKE | `MAX_CONNECTIONS` |
| Private members | trailing underscore | `connection_pool_` |
| Namespaces | lowercase | `wtl::core` |
| Enums | PascalCase | `ErrorCategory` |
| Enum values | UPPER_SNAKE | `DATABASE_ERROR` |
| Template params | PascalCase | `typename ResultType` |

### 2.4 Header File Structure

Every header file follows this structure:

```cpp
/**
 * @file DogService.h
 * @brief Service for dog-related business logic
 *
 * PURPOSE:
 * Handles all dog CRUD operations, search, and urgency calculations.
 * This is the primary interface for dog data access.
 *
 * USAGE:
 * auto& service = DogService::getInstance();
 * auto dog = service.findById(id);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry logic)
 *
 * MODIFICATION GUIDE:
 * - To add new query methods, follow the pattern in findById()
 * - All database access must use the connection pool
 * - All errors must be captured with WTL_CAPTURE_ERROR macro
 *
 * @author Agent [N] - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes (alphabetical)
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Third-party includes (alphabetical)
#include <drogon/drogon.h>
#include <json/json.h>

// Project includes (alphabetical)
#include "core/db/ConnectionPool.h"
#include "core/models/Dog.h"

namespace wtl::core::services {

/**
 * @class DogService
 * @brief Singleton service for dog operations
 *
 * Thread-safe singleton that manages all dog-related business logic.
 * Uses connection pooling for database access.
 */
class DogService {
public:
    // ... declarations
};

} // namespace wtl::core::services
```

### 2.5 Source File Structure

```cpp
/**
 * @file DogService.cc
 * @brief Implementation of DogService
 * @see DogService.h for documentation
 */

#include "core/services/DogService.h"

// Additional includes needed only for implementation
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

DogService& DogService::getInstance() {
    static DogService instance;
    return instance;
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

std::optional<Dog> DogService::findById(const std::string& id) {
    // Implementation with full error handling
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void DogService::validateDog(const Dog& dog) {
    // Implementation
}

} // namespace wtl::core::services
```

### 2.6 Error Handling

**ALWAYS use the debug system for errors:**

```cpp
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

// For simple error capture:
try {
    // risky operation
} catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
        ErrorCategory::DATABASE,
        "Failed to fetch dog: " + std::string(e.what()),
        {{"dog_id", id}, {"operation", "findById"}}
    );
    throw; // or handle gracefully
}

// For operations that should auto-heal:
auto result = SelfHealing::getInstance().executeWithHealing<Dog>(
    "dog_fetch_" + id,
    [&]() { return fetchDogFromDb(id); },
    [&]() { return fetchDogFromCache(id); }  // Fallback
);
```

### 2.7 Database Access

**ALWAYS use ConnectionPool, never raw connections:**

```cpp
#include "core/db/ConnectionPool.h"

void DogService::saveDog(const Dog& dog) {
    auto conn = ConnectionPool::getInstance().acquire();

    try {
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO dogs (id, name, breed) VALUES ($1, $2, $3)",
            dog.id, dog.name, dog.breed
        );
        txn.commit();
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(), {{"dog_id", dog.id}});
        throw;
    }

    ConnectionPool::getInstance().release(conn);
}
```

### 2.8 JSON Handling

Use `Json::Value` from jsoncpp (included with Drogon):

```cpp
Json::Value DogService::dogToJson(const Dog& dog) {
    Json::Value json;
    json["id"] = dog.id;
    json["name"] = dog.name;
    json["breed"] = dog.breed;
    json["age_months"] = dog.age_months;
    json["wait_time"] = calculateWaitTime(dog);
    return json;
}

Dog DogService::jsonToDog(const Json::Value& json) {
    Dog dog;
    dog.id = json["id"].asString();
    dog.name = json["name"].asString();
    dog.breed = json.get("breed", "Unknown").asString();
    dog.age_months = json.get("age_months", 0).asInt();
    return dog;
}
```

---

## 3. API Controller Standards

### 3.1 Controller Structure

```cpp
/**
 * @file DogsController.h
 * @brief REST API endpoints for dog operations
 */

#pragma once

#include <drogon/HttpController.h>

namespace wtl::core::controllers {

class DogsController : public drogon::HttpController<DogsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DogsController::getAllDogs, "/api/dogs", drogon::Get);
    ADD_METHOD_TO(DogsController::getDogById, "/api/dogs/{id}", drogon::Get);
    ADD_METHOD_TO(DogsController::searchDogs, "/api/dogs/search", drogon::Get);
    ADD_METHOD_TO(DogsController::createDog, "/api/dogs", drogon::Post);
    ADD_METHOD_TO(DogsController::updateDog, "/api/dogs/{id}", drogon::Put);
    ADD_METHOD_TO(DogsController::deleteDog, "/api/dogs/{id}", drogon::Delete);
    METHOD_LIST_END

    void getAllDogs(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getDogById(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    // ... other methods
};

} // namespace wtl::core::controllers
```

### 3.2 Response Format

All API responses follow this structure:

**Success:**
```json
{
    "success": true,
    "data": { ... },
    "meta": {
        "total": 100,
        "page": 1,
        "per_page": 20
    }
}
```

**Error:**
```json
{
    "success": false,
    "error": {
        "code": "DOG_NOT_FOUND",
        "message": "Dog with ID xyz not found",
        "details": { ... }
    }
}
```

### 3.3 Response Helper

```cpp
namespace wtl::core::utils {

class ApiResponse {
public:
    static drogon::HttpResponsePtr success(const Json::Value& data);
    static drogon::HttpResponsePtr success(const Json::Value& data, const Json::Value& meta);
    static drogon::HttpResponsePtr error(const std::string& code,
                                          const std::string& message,
                                          int httpStatus = 400);
    static drogon::HttpResponsePtr notFound(const std::string& resource);
    static drogon::HttpResponsePtr serverError(const std::string& message);
};

} // namespace wtl::core::utils
```

---

## 4. SQL Standards

### 4.1 Migration File Format

```sql
-- ============================================================================
-- Migration: 001_core_schema.sql
-- Purpose: Create core database tables for dogs, shelters, states, users
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES: None (initial migration)
--
-- ROLLBACK: See 001_core_schema_rollback.sql
-- ============================================================================

-- Enable required extensions
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";  -- For text search

-- ============================================================================
-- STATES TABLE
-- Purpose: US states for geographic organization
-- ============================================================================
CREATE TABLE states (
    code VARCHAR(2) PRIMARY KEY,           -- State abbreviation (TX, CA, etc.)
    name VARCHAR(100) NOT NULL,            -- Full state name
    region VARCHAR(50),                    -- Geographic region
    is_active BOOLEAN DEFAULT TRUE,        -- Whether we're active in this state
    dog_count INTEGER DEFAULT 0,           -- Cached count for performance
    shelter_count INTEGER DEFAULT 0,       -- Cached count for performance
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Add comment for documentation
COMMENT ON TABLE states IS 'US states - geographic organization for shelters and dogs';
COMMENT ON COLUMN states.code IS 'Two-letter state abbreviation (primary key)';
```

### 4.2 Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Tables | snake_case, plural | `dogs`, `shelter_contacts` |
| Columns | snake_case | `created_at`, `dog_name` |
| Primary keys | `id` (UUID) | `id UUID PRIMARY KEY` |
| Foreign keys | `{table}_id` | `shelter_id`, `user_id` |
| Indexes | `idx_{table}_{columns}` | `idx_dogs_shelter_id` |
| Constraints | `{table}_{type}_{columns}` | `dogs_fk_shelter_id` |

### 4.3 Required Columns

Every table MUST have:
```sql
id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
created_at TIMESTAMP DEFAULT NOW(),
updated_at TIMESTAMP DEFAULT NOW()
```

---

## 5. Comment Standards

### 5.1 File Header (Required)
Every file starts with a documentation block explaining purpose, usage, and modification guide.

### 5.2 Section Dividers
Use clear section dividers in implementation files:

```cpp
// ============================================================================
// SECTION NAME
// ============================================================================
```

### 5.3 Method Comments
Public methods need documentation:

```cpp
/**
 * @brief Find a dog by its unique ID
 *
 * @param id The UUID of the dog to find
 * @return std::optional<Dog> The dog if found, std::nullopt otherwise
 * @throws DatabaseException if database connection fails
 *
 * @example
 * auto dog = service.findById("550e8400-e29b-41d4-a716-446655440000");
 * if (dog) {
 *     std::cout << dog->name << std::endl;
 * }
 */
std::optional<Dog> findById(const std::string& id);
```

### 5.4 Inline Comments
Use inline comments to explain WHY, not WHAT:

```cpp
// BAD: Increment counter
counter++;

// GOOD: Increment to account for the current dog being processed
counter++;

// GOOD: Using 14 days because research shows this is the average
// time before a dog becomes "at risk" in high-intake shelters
const int RISK_THRESHOLD_DAYS = 14;
```

---

## 6. Testing Standards

### 6.1 Test File Location
Tests mirror source structure:
- `src/core/services/DogService.cc` → `tests/core/services/DogServiceTest.cc`

### 6.2 Test Naming
```cpp
TEST(DogService, FindById_ValidId_ReturnsDog)
TEST(DogService, FindById_InvalidId_ReturnsNullopt)
TEST(DogService, FindById_DatabaseError_ThrowsException)
```

---

## 7. Git Commit Standards

```
[AGENT-N] Component: Brief description

- Detail 1
- Detail 2

Files: file1.cc, file2.h
```

Example:
```
[AGENT-3] DogService: Implement CRUD operations

- Added findById, findAll, save, update, delete methods
- Integrated with ConnectionPool for database access
- Added error capture and self-healing wrappers

Files: DogService.h, DogService.cc
```

---

## 8. FILESLIST.md Update Protocol

**EVERY file operation requires FILESLIST.md update:**

1. **Before finishing any task**, update FILESLIST.md
2. Include: File Path, Purpose, Created, Last Updated, Status, Known Issues, Change History, Notes
3. Change History must show: `Created`, `Modified: [what]`, `Renamed from: [old]`, `Deleted: [why]`

---

## 9. Integration Points

### 9.1 Service Registration
All services must be singletons accessible via `getInstance()`:

```cpp
auto& dogService = DogService::getInstance();
auto& shelterService = ShelterService::getInstance();
```

### 9.2 Controller Registration
Controllers auto-register with Drogon via inheritance.

### 9.3 Module Registration
Modules register with ModuleManager:

```cpp
ModuleManager::getInstance().registerModule("urgency_engine",
    std::make_unique<UrgencyEngineModule>());
```

---

## 10. Data Source Policy

**THERE IS NO PETFINDER API.** Do not write code referencing a Petfinder API. The only external data sources are:
- RescueGroups.org API
- Direct shelter feeds
- TheDogAPI (breed data only)

The PetfinderAggregator files have been deleted from this project.

---

**END OF CODING STANDARDS**

# WaitingTheLongest.com - Integration Contracts

**Version:** 1.0
**Created:** 2024-01-28
**Purpose:** Define interfaces that agents must implement for seamless integration.

---

## 1. Database Connection Contract

**Responsible Agent:** Agent 1 (Project Setup)
**Consumers:** All agents

### Interface: ConnectionPool

```cpp
namespace wtl::core::db {

class ConnectionPool {
public:
    /**
     * Get singleton instance
     * @return Reference to the connection pool
     */
    static ConnectionPool& getInstance();

    /**
     * Acquire a database connection from the pool
     * @return Shared pointer to pqxx connection
     * @throws DatabaseException if pool exhausted after retries
     */
    std::shared_ptr<pqxx::connection> acquire();

    /**
     * Release a connection back to the pool
     * @param conn The connection to release
     */
    void release(std::shared_ptr<pqxx::connection> conn);

    /**
     * Get pool statistics
     * @return JSON with pool_size, active, available, waiting
     */
    Json::Value getStats();

    /**
     * Reset all connections (for self-healing)
     * @return Number of connections reset
     */
    int resetAll();
};

} // namespace wtl::core::db
```

---

## 2. Model Contracts

**Responsible Agent:** Agent 3 (Models & Services)
**Consumers:** Agents 4, 5, 7, 8, 9

### 2.1 Dog Model

```cpp
namespace wtl::core::models {

struct Dog {
    std::string id;                          // UUID
    std::string name;
    std::string shelter_id;                  // FK to shelters

    // Basic info
    std::string breed_primary;
    std::optional<std::string> breed_secondary;
    std::string size;                        // small, medium, large, xlarge
    std::string age_category;                // puppy, young, adult, senior
    int age_months;
    std::string gender;                      // male, female
    std::string color_primary;
    std::optional<std::string> color_secondary;
    std::optional<double> weight_lbs;

    // Status
    std::string status;                      // adoptable, pending, adopted, etc.
    bool is_available;

    // Dates
    std::chrono::system_clock::time_point intake_date;
    std::optional<std::chrono::system_clock::time_point> available_date;
    std::optional<std::chrono::system_clock::time_point> euthanasia_date;

    // Urgency (calculated)
    std::string urgency_level;               // normal, medium, high, critical
    bool is_on_euthanasia_list;

    // Media
    std::vector<std::string> photo_urls;
    std::optional<std::string> video_url;

    // Description
    std::string description;
    std::vector<std::string> tags;           // good_with_kids, house_trained, etc.

    // External
    std::optional<std::string> external_id;  // ID from source system
    std::string source;                      // rescuegroups, shelter_direct, etc.

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // Conversion methods
    static Dog fromJson(const Json::Value& json);
    static Dog fromDbRow(const pqxx::row& row);
    Json::Value toJson() const;
};

} // namespace wtl::core::models
```

### 2.2 Shelter Model

```cpp
namespace wtl::core::models {

struct Shelter {
    std::string id;                          // UUID
    std::string name;
    std::string state_code;                  // FK to states

    // Location
    std::string city;
    std::string address;
    std::string zip_code;
    std::optional<double> latitude;
    std::optional<double> longitude;

    // Contact
    std::optional<std::string> phone;
    std::optional<std::string> email;
    std::optional<std::string> website;

    // Classification
    std::string shelter_type;                // municipal, rescue, private
    bool is_kill_shelter;
    int avg_hold_days;
    std::optional<std::string> euthanasia_schedule;
    bool accepts_rescue_pulls;

    // Stats (cached)
    int dog_count;
    int available_count;

    // Status
    bool is_verified;
    bool is_active;

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // Conversion methods
    static Shelter fromJson(const Json::Value& json);
    static Shelter fromDbRow(const pqxx::row& row);
    Json::Value toJson() const;
};

} // namespace wtl::core::models
```

### 2.3 State Model

```cpp
namespace wtl::core::models {

struct State {
    std::string code;                        // Two-letter code (PK)
    std::string name;
    std::string region;
    bool is_active;
    int dog_count;
    int shelter_count;

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    static State fromJson(const Json::Value& json);
    static State fromDbRow(const pqxx::row& row);
    Json::Value toJson() const;
};

} // namespace wtl::core::models
```

### 2.4 User Model

```cpp
namespace wtl::core::models {

struct User {
    std::string id;                          // UUID
    std::string email;
    std::string password_hash;
    std::string display_name;

    // Profile
    std::optional<std::string> phone;
    std::optional<std::string> zip_code;

    // Roles
    std::string role;                        // user, foster, shelter_admin, admin
    std::optional<std::string> shelter_id;   // If shelter_admin

    // Preferences
    Json::Value notification_preferences;
    Json::Value search_preferences;

    // Status
    bool is_active;
    bool email_verified;
    std::optional<std::chrono::system_clock::time_point> last_login;

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    static User fromJson(const Json::Value& json);
    static User fromDbRow(const pqxx::row& row);
    Json::Value toJson() const;  // Excludes password_hash
    Json::Value toJsonPrivate() const;  // For admin use
};

} // namespace wtl::core::models
```

---

## 3. Service Contracts

**Responsible Agent:** Agent 3 (Models & Services)
**Consumers:** Agents 4, 5, 7, 8, 9

### 3.1 DogService

```cpp
namespace wtl::core::services {

class DogService {
public:
    static DogService& getInstance();

    // CRUD
    std::optional<Dog> findById(const std::string& id);
    std::vector<Dog> findAll(int limit = 100, int offset = 0);
    std::vector<Dog> findByShelterId(const std::string& shelter_id);
    std::vector<Dog> findByStateCode(const std::string& state_code);
    Dog save(const Dog& dog);
    Dog update(const Dog& dog);
    bool deleteById(const std::string& id);

    // Queries
    int countAll();
    int countByShelterId(const std::string& shelter_id);
    int countByStateCode(const std::string& state_code);

    // Wait time
    WaitTimeComponents calculateWaitTime(const Dog& dog);
    std::optional<CountdownComponents> calculateCountdown(const Dog& dog);
};

// Wait time structure
struct WaitTimeComponents {
    int years;
    int months;
    int days;
    int hours;
    int minutes;
    int seconds;
    std::string formatted;  // "02:03:15:08:45:32"
};

struct CountdownComponents {
    int days;
    int hours;
    int minutes;
    int seconds;
    std::string formatted;  // "02:14:30:00"
    bool is_critical;       // < 24 hours
};

} // namespace wtl::core::services
```

### 3.2 ShelterService

```cpp
namespace wtl::core::services {

class ShelterService {
public:
    static ShelterService& getInstance();

    std::optional<Shelter> findById(const std::string& id);
    std::vector<Shelter> findAll(int limit = 100, int offset = 0);
    std::vector<Shelter> findByStateCode(const std::string& state_code);
    std::vector<Shelter> findKillShelters();
    Shelter save(const Shelter& shelter);
    Shelter update(const Shelter& shelter);
    bool deleteById(const std::string& id);

    int countAll();
    int countByStateCode(const std::string& state_code);
    void updateDogCount(const std::string& shelter_id);
};

} // namespace wtl::core::services
```

### 3.3 StateService

```cpp
namespace wtl::core::services {

class StateService {
public:
    static StateService& getInstance();

    std::optional<State> findByCode(const std::string& code);
    std::vector<State> findAll();
    std::vector<State> findActive();
    State save(const State& state);
    State update(const State& state);

    void updateCounts(const std::string& code);
    void updateAllCounts();
};

} // namespace wtl::core::services
```

### 3.4 UserService

```cpp
namespace wtl::core::services {

class UserService {
public:
    static UserService& getInstance();

    std::optional<User> findById(const std::string& id);
    std::optional<User> findByEmail(const std::string& email);
    User save(const User& user);
    User update(const User& user);
    bool deleteById(const std::string& id);

    bool verifyPassword(const std::string& email, const std::string& password);
    std::string hashPassword(const std::string& password);
    void updateLastLogin(const std::string& user_id);
};

} // namespace wtl::core::services
```

---

## 4. Controller Response Contract

**Responsible Agent:** Agent 4 (API Controllers)
**Consumers:** Frontend, External APIs

### 4.1 Response Helper

```cpp
namespace wtl::core::utils {

class ApiResponse {
public:
    // Success responses
    static drogon::HttpResponsePtr success(const Json::Value& data);
    static drogon::HttpResponsePtr success(const Json::Value& data,
                                            int total, int page, int per_page);
    static drogon::HttpResponsePtr created(const Json::Value& data);
    static drogon::HttpResponsePtr noContent();

    // Error responses
    static drogon::HttpResponsePtr badRequest(const std::string& message);
    static drogon::HttpResponsePtr unauthorized(const std::string& message = "Unauthorized");
    static drogon::HttpResponsePtr forbidden(const std::string& message = "Forbidden");
    static drogon::HttpResponsePtr notFound(const std::string& resource);
    static drogon::HttpResponsePtr conflict(const std::string& message);
    static drogon::HttpResponsePtr serverError(const std::string& message);

    // Validation error
    static drogon::HttpResponsePtr validationError(
        const std::vector<std::pair<std::string, std::string>>& errors);
};

} // namespace wtl::core::utils
```

---

## 5. WebSocket Contract

**Responsible Agent:** Agent 5 (WebSocket Server)
**Consumers:** Frontend

### 5.1 Message Types

```cpp
namespace wtl::core::websocket {

enum class MessageType {
    // Client -> Server
    SUBSCRIBE_DOG,           // Subscribe to dog wait time updates
    UNSUBSCRIBE_DOG,
    SUBSCRIBE_SHELTER,       // Subscribe to shelter updates
    UNSUBSCRIBE_SHELTER,
    SUBSCRIBE_URGENT,        // Subscribe to all urgent dogs

    // Server -> Client
    WAIT_TIME_UPDATE,        // Wait time update for subscribed dog
    COUNTDOWN_UPDATE,        // Countdown update for urgent dog
    DOG_STATUS_CHANGE,       // Dog adopted, status changed, etc.
    URGENT_ALERT,            // New critical dog alert
    CONNECTION_ACK,          // Connection acknowledged
    ERROR                    // Error message
};

struct WebSocketMessage {
    MessageType type;
    std::string dog_id;      // Optional, depends on type
    std::string shelter_id;  // Optional, depends on type
    Json::Value payload;

    std::string toJson() const;
    static WebSocketMessage fromJson(const std::string& json);
};

} // namespace wtl::core::websocket
```

### 5.2 Payload Formats

**WAIT_TIME_UPDATE:**
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

**COUNTDOWN_UPDATE:**
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
        "is_critical": false,
        "urgency_level": "high"
    }
}
```

---

## 6. Authentication Contract

**Responsible Agent:** Agent 6 (Authentication)
**Consumers:** All controllers

### 6.1 AuthService

```cpp
namespace wtl::core::auth {

struct AuthToken {
    std::string token;
    std::string user_id;
    std::string role;
    std::chrono::system_clock::time_point expires_at;
};

struct AuthResult {
    bool success;
    std::optional<AuthToken> token;
    std::optional<std::string> error;
};

class AuthService {
public:
    static AuthService& getInstance();

    // Authentication
    AuthResult login(const std::string& email, const std::string& password);
    AuthResult refreshToken(const std::string& refresh_token);
    void logout(const std::string& token);

    // Token validation
    std::optional<AuthToken> validateToken(const std::string& token);
    bool hasRole(const std::string& token, const std::string& required_role);

    // Registration
    AuthResult registerUser(const std::string& email,
                            const std::string& password,
                            const std::string& display_name);
    bool verifyEmail(const std::string& verification_token);

    // Password
    bool requestPasswordReset(const std::string& email);
    bool resetPassword(const std::string& reset_token, const std::string& new_password);
};

} // namespace wtl::core::auth
```

### 6.2 Auth Middleware

```cpp
namespace wtl::core::auth {

class AuthMiddleware {
public:
    // Extract and validate token from request
    static std::optional<AuthToken> authenticate(const drogon::HttpRequestPtr& req);

    // Check role
    static bool requireRole(const drogon::HttpRequestPtr& req,
                           const std::string& role,
                           std::function<void(const drogon::HttpResponsePtr&)>& callback);
};

// Convenience macros for controllers
#define REQUIRE_AUTH(req, callback) \
    auto auth_token = AuthMiddleware::authenticate(req); \
    if (!auth_token) { \
        callback(ApiResponse::unauthorized()); \
        return; \
    }

#define REQUIRE_ROLE(req, callback, role) \
    REQUIRE_AUTH(req, callback) \
    if (!AuthMiddleware::requireRole(req, role, callback)) { \
        return; \
    }

} // namespace wtl::core::auth
```

---

## 7. Search Contract

**Responsible Agent:** Agent 7 (Search Service)
**Consumers:** Agent 4 (Controllers)

### 7.1 SearchService

```cpp
namespace wtl::core::services {

struct SearchFilters {
    std::optional<std::string> state_code;
    std::optional<std::string> shelter_id;
    std::optional<std::string> breed;
    std::optional<std::string> size;           // small, medium, large, xlarge
    std::optional<std::string> age_category;   // puppy, young, adult, senior
    std::optional<std::string> gender;
    std::optional<bool> good_with_kids;
    std::optional<bool> good_with_dogs;
    std::optional<bool> good_with_cats;
    std::optional<bool> house_trained;
    std::optional<std::string> urgency_level;
    std::optional<std::string> query;          // Full-text search
};

struct SearchOptions {
    int page = 1;
    int per_page = 20;
    std::string sort_by = "wait_time";         // wait_time, urgency, name, intake_date
    std::string sort_order = "desc";           // asc, desc
};

struct SearchResult {
    std::vector<Dog> dogs;
    int total;
    int page;
    int per_page;
    int total_pages;
};

class SearchService {
public:
    static SearchService& getInstance();

    SearchResult searchDogs(const SearchFilters& filters,
                            const SearchOptions& options);

    // Quick searches
    std::vector<Dog> getLongestWaiting(int limit = 10);
    std::vector<Dog> getMostUrgent(int limit = 10);
    std::vector<Dog> getRecentlyAdded(int limit = 10);
    std::vector<Dog> getByBreed(const std::string& breed, int limit = 20);

    // Autocomplete
    std::vector<std::string> suggestBreeds(const std::string& query);
    std::vector<std::string> suggestLocations(const std::string& query);
};

} // namespace wtl::core::services
```

---

## 8. Event Bus Contract

**Responsible Agent:** Agent 10 (Module Manager)
**Consumers:** All modules

### 8.1 EventBus

```cpp
namespace wtl::core {

enum class EventType {
    // Dog events
    DOG_CREATED,
    DOG_UPDATED,
    DOG_DELETED,
    DOG_STATUS_CHANGED,
    DOG_URGENCY_CHANGED,

    // Shelter events
    SHELTER_CREATED,
    SHELTER_UPDATED,

    // User events
    USER_REGISTERED,
    USER_LOGIN,

    // Foster events
    FOSTER_APPLICATION_SUBMITTED,
    FOSTER_APPROVED,
    FOSTER_PLACEMENT_STARTED,
    FOSTER_PLACEMENT_ENDED,

    // Adoption events
    ADOPTION_APPLICATION_SUBMITTED,
    ADOPTION_COMPLETED,

    // Urgency events
    DOG_BECAME_CRITICAL,
    EUTHANASIA_LIST_UPDATED,

    // System events
    SYNC_COMPLETED,
    ERROR_OCCURRED
};

struct Event {
    EventType type;
    std::string entity_id;        // ID of affected entity
    Json::Value data;             // Event-specific data
    std::chrono::system_clock::time_point timestamp;
    std::string source;           // Which component fired the event
};

using EventHandler = std::function<void(const Event&)>;

class EventBus {
public:
    static EventBus& getInstance();

    // Subscribe to events
    std::string subscribe(EventType type, EventHandler handler);
    void unsubscribe(const std::string& subscription_id);

    // Publish events
    void publish(const Event& event);
    void publishAsync(const Event& event);  // Non-blocking

    // Batch operations
    void publishBatch(const std::vector<Event>& events);
};

// Convenience function
inline void emitEvent(EventType type,
                      const std::string& entity_id,
                      const Json::Value& data = {},
                      const std::string& source = "core") {
    EventBus::getInstance().publish({
        type, entity_id, data,
        std::chrono::system_clock::now(),
        source
    });
}

} // namespace wtl::core
```

---

## 9. Module Contract

**Responsible Agent:** Agent 10 (Module Manager)
**Consumers:** All module implementations

### 9.1 Module Interface

```cpp
namespace wtl::modules {

class IModule {
public:
    virtual ~IModule() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // Identity
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::vector<std::string> getDependencies() const = 0;

    // Status
    virtual bool isEnabled() const = 0;
    virtual Json::Value getStatus() const = 0;

    // Configuration
    virtual void configure(const Json::Value& config) = 0;
};

class ModuleManager {
public:
    static ModuleManager& getInstance();

    // Registration
    void registerModule(const std::string& name, std::unique_ptr<IModule> module);
    void unregisterModule(const std::string& name);

    // Lifecycle
    void initializeAll();
    void shutdownAll();

    // Access
    IModule* getModule(const std::string& name);
    std::vector<std::string> getEnabledModules();

    // Configuration
    void loadConfig(const std::string& config_path);
    bool isModuleEnabled(const std::string& name);
};

} // namespace wtl::modules
```

---

## 10. Debug System Contract

**Responsible Agent:** Agent 1 (Project Setup)
**Consumers:** All agents

### 10.1 Error Capture

All agents MUST use these for error handling:

```cpp
// Capture an error
WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, "Error message", {{"key", "value"}});

// Capture critical error
WTL_CAPTURE_CRITICAL(ErrorCategory::EXTERNAL_API, "API down", {{"api", "rescuegroups"}});
```

### 10.2 Self-Healing

All agents MUST wrap risky operations:

```cpp
auto result = SelfHealing::getInstance().executeWithHealing<ReturnType>(
    "operation_name",
    [&]() { return riskyOperation(); },
    [&]() { return fallbackOperation(); }
);
```

---

## 11. Data Source Policy

**CRITICAL: There is NO Petfinder API.** Petfinder shut down their public API. Do not write aggregator code, configuration, or contracts referencing a Petfinder API. The PetfinderAggregator files in this project are dead code.

The only external data sources are:
- **RescueGroups.org API** (primary)
- **Direct shelter feeds** (secondary)
- **TheDogAPI** (breed data only)

---

**END OF INTEGRATION CONTRACTS**

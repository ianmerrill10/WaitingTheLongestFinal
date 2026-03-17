# WaitingTheLongest.com - Agent Communication Protocol

**Version:** 1.0
**Created:** 2024-01-28
**Purpose:** Define how agents communicate, document work, and coordinate.

---

## 1. Agent Identification

Each agent has a unique identifier:
- **Agent 1:** Project Setup (CMake, config, directory structure)
- **Agent 2:** Database Schema (PostgreSQL migrations)
- **Agent 3:** Models & Services (Dog, Shelter, State, User)
- **Agent 4:** API Controllers (REST endpoints)
- **Agent 5:** WebSocket Server (LED counters, real-time updates)
- **Agent 6:** Authentication (login, registration, sessions)
- **Agent 7:** Search Service (filtering, pagination, full-text)
- **Agent 8:** Urgency Engine (kill shelter monitoring, countdowns)
- **Agent 9:** Foster System (matching, registration)
- **Agent 10:** Module Manager (event bus, module loading)

---

## 2. File Creation Protocol

### 2.1 Before Creating Any File
1. Check if file already exists
2. If exists, coordinate with responsible agent or extend carefully
3. Follow CODING_STANDARDS.md exactly

### 2.2 After Creating Any File
1. **MANDATORY:** Update FILESLIST.md with new entry
2. **MANDATORY:** Update WORK_LOG.md with completion note
3. **MANDATORY:** Mark task complete in WORK_SCHEDULE.md

### 2.3 FILESLIST.md Entry Format
```markdown
| src/core/services/DogService.cc | Dog CRUD operations and business logic | 2024-01-28 | 2024-01-28 | Complete | None | Created | Singleton pattern, uses ConnectionPool |
```

---

## 3. Work Logging Protocol

### 3.1 WORK_LOG.md Entry Format
```markdown
## [TIMESTAMP] Agent [N] - [Action]

**Task:** Brief description
**Files Created/Modified:**
- `path/to/file.cc` - Created - Purpose
- `path/to/other.h` - Modified - What changed

**Status:** Complete | In Progress | Blocked
**Notes:** Any relevant information
**Blockers:** None | Description of blocker
**Next Steps:** What should happen next

---
```

### 3.2 When to Log
- When starting a new task
- When completing a task
- When encountering a blocker
- When modifying files created by another agent
- When making architectural decisions

---

## 4. Dependency Declaration

### 4.1 If Your Code Depends on Another Agent's Work

Declare at the top of your file:
```cpp
/**
 * DEPENDENCIES (from other agents):
 * - ConnectionPool (Agent 1) - Database connections
 * - Dog model (Agent 3) - Dog data structure
 * - ErrorCapture (Agent 1) - Error logging
 */
```

### 4.2 If Your Code is Depended Upon

Create a clear public interface and document it:
```cpp
/**
 * PUBLIC INTERFACE - Used by other agents
 *
 * Other agents depend on these methods. Do not change signatures
 * without coordinating via INTEGRATION_CONTRACTS.md
 */
```

---

## 5. Conflict Resolution

### 5.1 File Conflicts
If two agents need to modify the same file:
1. Agent with primary responsibility takes precedence
2. Secondary agent documents needed changes in WORK_LOG.md
3. Orchestrator resolves during integration

### 5.2 Interface Conflicts
If agents disagree on an interface:
1. Document both approaches in INTEGRATION_CONTRACTS.md
2. Orchestrator makes final decision
3. All agents update their code accordingly

---

## 6. Naming Consistency

### 6.1 Cross-Agent References
Always use fully qualified names when referencing other agents' code:

```cpp
// GOOD - Clear reference
wtl::core::db::ConnectionPool::getInstance()
wtl::core::services::DogService::getInstance()

// BAD - Ambiguous
ConnectionPool::getInstance()
```

### 6.2 Include Paths
Use consistent include paths from project root:

```cpp
// GOOD
#include "core/db/ConnectionPool.h"
#include "core/services/DogService.h"
#include "core/models/Dog.h"

// BAD
#include "ConnectionPool.h"
#include "../db/ConnectionPool.h"
```

---

## 7. Status Reporting

### 7.1 Task Status Values
- **Not Started:** Task not yet begun
- **In Progress:** Currently working on task
- **Complete:** Task finished and tested
- **Blocked:** Cannot proceed (document reason)
- **Needs Review:** Complete but needs orchestrator review

### 7.2 File Status Values
- **Complete:** File is production-ready
- **In Progress:** File is being developed
- **Needs Review:** File needs orchestrator review
- **Has Issues:** File has known bugs (document in Known Issues)
- **Deprecated:** File should not be used
- **Deleted:** File was removed (keep history)

---

## 8. Documentation Requirements

### 8.1 Every File Must Have
1. File header with purpose, usage, modification guide
2. All public methods documented
3. Complex logic explained with comments
4. Dependencies declared

### 8.2 Every Feature Must Have
1. Entry in DEVELOPER_NOTES.md explaining the feature
2. Usage examples
3. Configuration options documented

---

## 9. Error Handling Coordination

### 9.1 All Errors Must
1. Use ErrorCategory enum from debug system
2. Be captured with WTL_CAPTURE_ERROR macro
3. Include relevant context in metadata

### 9.2 Error Categories by Agent
- Agent 1: CONFIGURATION, FILE_IO
- Agent 2: DATABASE (schema-related)
- Agent 3: DATABASE, VALIDATION, BUSINESS_LOGIC
- Agent 4: HTTP_REQUEST, VALIDATION
- Agent 5: WEBSOCKET
- Agent 6: AUTHENTICATION
- Agent 7: DATABASE, VALIDATION
- Agent 8: EXTERNAL_API, BUSINESS_LOGIC
- Agent 9: DATABASE, BUSINESS_LOGIC
- Agent 10: MODULE, CONFIGURATION

---

## 10. Integration Checkpoints

### 10.1 Before Marking Complete
1. Code compiles without errors
2. Follows all CODING_STANDARDS.md rules
3. FILESLIST.md updated for all files
4. WORK_LOG.md entry added
5. WORK_SCHEDULE.md task marked complete
6. All dependencies declared
7. All public interfaces documented

### 10.2 Integration Contract Compliance
1. Check INTEGRATION_CONTRACTS.md for required interfaces
2. Ensure your implementation matches the contract
3. Document any deviations

---

## 11. Data Source Policy

**CRITICAL: There is NO Petfinder API.** Their public API has been shut down. Do not reference, suggest, or write code for a Petfinder API integration. The only external data sources are RescueGroups.org API and direct shelter feeds.

---

**END OF COMMUNICATION PROTOCOL**

# ==============================================================================
# CMakeLists.txt
# Root CMake configuration for WaitingTheLongest.com
#
# PURPOSE:
# Configure build system for the WaitingTheLongest dog rescue platform.
# Sets up all dependencies, compiler flags, and build targets.
#
# USAGE:
# mkdir build && cd build
# cmake .. -DCMAKE_BUILD_TYPE=Release
# cmake --build .
#
# DEPENDENCIES:
# - Drogon framework (HTTP server, WebSocket)
# - pqxx (PostgreSQL client)
# - jsoncpp (JSON parsing - included with Drogon)
# - OpenSSL (encryption, hashing)
# - bcrypt (password hashing)
#
# MODIFICATION GUIDE:
# - Add new source files to the appropriate src/ subdirectory
# - Add new dependencies using find_package()
# - Add new modules in src/modules/ directory
#
# @author Final Integration Agent - WaitingTheLongest Build System
# @date 2024-01-28
# @updated 2024-01-28 - Complete source file integration
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

project(WaitingTheLongest
    VERSION 1.0.0
    DESCRIPTION "Dog rescue platform - Find dogs waiting the longest for homes"
    LANGUAGES CXX
)

# ==============================================================================
# COMPILER SETTINGS
# ==============================================================================

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings for different platforms
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -DDEBUG)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
elseif(MSVC)
    add_compile_options(
        /W4
        /wd4100  # unreferenced formal parameter
        /wd4458  # declaration hides class member
        /utf-8
    )

    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /Zi)
        add_compile_definitions(DEBUG)
    else()
        add_compile_options(/O2)
        add_compile_definitions(NDEBUG)
    endif()
endif()

# ==============================================================================
# FIND DEPENDENCIES
# ==============================================================================

# Drogon - Main HTTP framework (includes jsoncpp)
find_package(Drogon REQUIRED)
message(STATUS "Found Drogon: ${Drogon_DIR}")

# PostgreSQL via pqxx
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PQXX QUIET libpqxx)
endif()

if(NOT PQXX_FOUND)
    find_path(PQXX_INCLUDE_DIR pqxx/pqxx)
    find_library(PQXX_LIBRARY NAMES pqxx libpqxx)
    find_library(PQ_LIBRARY NAMES pq libpq)

    if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY AND PQ_LIBRARY)
        set(PQXX_FOUND TRUE)
        set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIR})
        set(PQXX_LIBRARIES ${PQXX_LIBRARY} ${PQ_LIBRARY})
    else()
        message(FATAL_ERROR "Could not find pqxx library")
    endif()
endif()
message(STATUS "Found pqxx: ${PQXX_LIBRARIES}")

# OpenSSL
find_package(OpenSSL REQUIRED)
message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")

# Threads
find_package(Threads REQUIRED)

# CURL - for HTTP requests in notification channels
find_package(CURL REQUIRED)
message(STATUS "Found CURL: ${CURL_VERSION_STRING}")

# ==============================================================================
# BCRYPT CONFIGURATION
# ==============================================================================

# bcrypt for password hashing - may need to be built from source or found
find_path(BCRYPT_INCLUDE_DIR bcrypt/BCrypt.hpp)
find_library(BCRYPT_LIBRARY NAMES bcrypt libbcrypt)

if(BCRYPT_INCLUDE_DIR AND BCRYPT_LIBRARY)
    set(BCRYPT_FOUND TRUE)
    message(STATUS "Found bcrypt: ${BCRYPT_LIBRARY}")
else()
    # Fallback: Use OpenSSL's crypto for password hashing
    message(WARNING "bcrypt not found, will use OpenSSL for password hashing")
    set(BCRYPT_FOUND FALSE)
    set(BCRYPT_INCLUDE_DIR "")
    set(BCRYPT_LIBRARY "")
    add_compile_definitions(USE_OPENSSL_PASSWORD_HASH)
endif()

# ==============================================================================
# INCLUDE DIRECTORIES
# ==============================================================================

# Project include paths
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${PQXX_INCLUDE_DIRS}
)

if(BCRYPT_FOUND)
    include_directories(${BCRYPT_INCLUDE_DIR})
endif()

# ==============================================================================
# SOURCE FILES - CORE
# ==============================================================================

# Core utility sources
set(CORE_UTILS_SOURCES
    src/core/utils/Config.cc
    src/core/utils/ApiResponse.cc
    src/core/utils/WaitTimeCalculator.cc
)

# Database sources
set(CORE_DB_SOURCES
    src/core/db/ConnectionPool.cc
)

# Debug system sources
set(CORE_DEBUG_SOURCES
    src/core/debug/ErrorCapture.cc
    src/core/debug/SelfHealing.cc
    src/core/debug/LogGenerator.cc
    src/core/debug/HealthCheck.cc
)

# Core EventBus
set(CORE_EVENTBUS_SOURCES
    src/core/EventBus.cc
)

# ==============================================================================
# SOURCE FILES - MODELS
# ==============================================================================

set(MODEL_SOURCES
    src/core/models/Dog.cc
    src/core/models/Shelter.cc
    src/core/models/State.cc
    src/core/models/User.cc
    src/core/models/FosterHome.cc
    src/core/models/FosterPlacement.cc
)

# ==============================================================================
# SOURCE FILES - SERVICES
# ==============================================================================

set(SERVICE_SOURCES
    src/core/services/DogService.cc
    src/core/services/ShelterService.cc
    src/core/services/StateService.cc
    src/core/services/UserService.cc
    src/core/services/SearchService.cc
    src/core/services/SearchQueryBuilder.cc
    src/core/services/FullTextSearch.cc
    src/core/services/FosterService.cc
    src/core/services/FosterMatcher.cc
    src/core/services/FosterApplicationHandler.cc
    src/core/services/UrgencyCalculator.cc
    src/core/services/KillShelterMonitor.cc
    src/core/services/EuthanasiaTracker.cc
    src/core/services/AlertService.cc
    src/core/services/UrgencyWorker.cc
    src/core/services/MatchingService.cc
    src/core/services/SponsorshipService.cc
)

# ==============================================================================
# SOURCE FILES - CONTROLLERS (CORE)
# ==============================================================================

set(CORE_CONTROLLER_SOURCES
    src/core/controllers/DogsController.cc
    src/core/controllers/SheltersController.cc
    src/core/controllers/StatesController.cc
    src/core/controllers/UsersController.cc
    src/core/controllers/SearchController.cc
    src/core/controllers/HealthController.cc
    src/core/controllers/AuthController.cc
    src/core/controllers/FosterController.cc
    src/core/controllers/UrgencyController.cc
    src/core/controllers/DebugController.cc
    src/core/controllers/WorkerController.cc
    src/core/controllers/MatchingController.cc
    src/core/controllers/SponsorController.cc
    src/core/controllers/HousingController.cc
    src/core/controllers/TransportController.cc
    src/core/controllers/ResourcesController.cc
)

# ==============================================================================
# SOURCE FILES - WEBSOCKET
# ==============================================================================

set(WEBSOCKET_SOURCES
    src/core/websocket/WebSocketMessage.cc
    src/core/websocket/ConnectionManager.cc
    src/core/websocket/WaitTimeWebSocket.cc
    src/core/websocket/UrgencyWebSocket.cc
    src/core/websocket/BroadcastService.cc
    src/core/websocket/WaitTimeWorker.cc
)

# ==============================================================================
# SOURCE FILES - AUTH
# ==============================================================================

set(AUTH_SOURCES
    src/core/auth/PasswordUtils.cc
    src/core/auth/JwtUtils.cc
    src/core/auth/SessionManager.cc
    src/core/auth/AuthService.cc
    src/core/auth/AuthMiddleware.cc
)

# ==============================================================================
# SOURCE FILES - MODULES
# ==============================================================================

set(MODULE_SOURCES
    src/modules/ModuleManager.cc
    src/modules/ModuleConfig.cc
    src/modules/ModuleLoader.cc
    src/modules/example/ExampleModule.cc
)

# ==============================================================================
# SOURCE FILES - INTERVENTION MODULES
# ==============================================================================

set(INTERVENTION_MODULE_SOURCES
    src/modules/intervention/SurrenderPreventionModule.cc
    src/modules/intervention/ReturnPreventionModule.cc
    src/modules/intervention/TransportNetworkModule.cc
    src/modules/intervention/AIMediaModule.cc
    src/modules/intervention/BreedInterventionModule.cc
    src/modules/intervention/RescueCoordinationModule.cc
    src/modules/intervention/PredictiveMLModule.cc
    src/modules/intervention/EmployerPartnershipsModule.cc
)

# ==============================================================================
# SOURCE FILES - CONTENT (TIKTOK)
# ==============================================================================

set(CONTENT_TIKTOK_SOURCES
    src/content/tiktok/TikTokPost.cc
    src/content/tiktok/TikTokTemplate.cc
    src/content/tiktok/TikTokClient.cc
    src/content/tiktok/TikTokEngine.cc
    src/content/tiktok/TikTokWorker.cc
)

# ==============================================================================
# SOURCE FILES - CONTENT (BLOG)
# ==============================================================================

set(CONTENT_BLOG_SOURCES
    src/content/blog/BlogPost.cc
    src/content/blog/EducationalContent.cc
    src/content/blog/ContentGenerator.cc
    src/content/blog/BlogEngine.cc
    src/content/blog/BlogService.cc
)

# ==============================================================================
# SOURCE FILES - CONTENT (SOCIAL)
# ==============================================================================

set(CONTENT_SOCIAL_SOURCES
    src/content/social/Platform.cc
    src/content/social/SocialPost.cc
    src/content/social/SocialAnalytics.cc
    src/content/social/SocialMediaEngine.cc
    src/content/social/SocialWorker.cc
    src/content/social/ShareCardGenerator.cc
    src/content/social/SocialController.cc
    src/content/social/clients/FacebookClient.cc
    src/content/social/clients/InstagramClient.cc
    src/content/social/clients/TwitterClient.cc
)

# ==============================================================================
# SOURCE FILES - CONTENT (MEDIA)
# ==============================================================================

set(CONTENT_MEDIA_SOURCES
    src/content/media/OverlayConfig.cc
    src/content/media/VideoTemplate.cc
    src/content/media/FFmpegWrapper.cc
    src/content/media/ImageProcessor.cc
    src/content/media/VideoGenerator.cc
    src/content/media/MediaStorage.cc
    src/content/media/MediaQueue.cc
    src/content/media/MediaWorker.cc
)

# ==============================================================================
# SOURCE FILES - CONTENT (TEMPLATES)
# ==============================================================================

set(CONTENT_TEMPLATES_SOURCES
    src/content/templates/TemplateEngine.cc
    src/content/templates/TemplateContext.cc
    src/content/templates/TemplateHelpers.cc
    src/content/templates/TemplateRepository.cc
)

# ==============================================================================
# SOURCE FILES - NOTIFICATIONS
# ==============================================================================

set(NOTIFICATION_SOURCES
    src/notifications/Notification.cc
    src/notifications/NotificationPreferences.cc
    src/notifications/NotificationService.cc
    src/notifications/NotificationWorker.cc
    src/notifications/UrgentAlertService.cc
    src/notifications/channels/PushChannel.cc
    src/notifications/channels/EmailChannel.cc
    src/notifications/channels/SmsChannel.cc
)

# ==============================================================================
# SOURCE FILES - ANALYTICS
# ==============================================================================

set(ANALYTICS_SOURCES
    src/analytics/AnalyticsEvent.cc
    src/analytics/DashboardStats.cc
    src/analytics/MetricsAggregator.cc
    src/analytics/ImpactTracker.cc
    src/analytics/SocialAnalytics.cc
    src/analytics/AnalyticsService.cc
    src/analytics/AnalyticsWorker.cc
    src/analytics/ReportGenerator.cc
)

# ==============================================================================
# SOURCE FILES - AGGREGATORS
# ==============================================================================

set(AGGREGATOR_SOURCES
    src/aggregators/BaseAggregator.cc
    src/aggregators/DataMapper.cc
    src/aggregators/DuplicateDetector.cc
    src/aggregators/RescueGroupsAggregator.cc
    src/aggregators/ShelterDirectAggregator.cc
    src/aggregators/AggregatorManager.cc
)

# ==============================================================================
# SOURCE FILES - WORKERS
# ==============================================================================

set(WORKER_SOURCES
    src/workers/BaseWorker.cc
    src/workers/WorkerManager.cc
    src/workers/ApiSyncWorker.cc
    src/workers/UrgencyUpdateWorker.cc
    src/workers/ContentGenerationWorker.cc
    src/workers/CleanupWorker.cc
    src/workers/HealthCheckWorker.cc
    src/workers/scheduler/ScheduledJob.cc
    src/workers/scheduler/CronParser.cc
    src/workers/scheduler/Scheduler.cc
    src/workers/jobs/JobQueue.cc
    src/workers/jobs/JobProcessor.cc
)

# ==============================================================================
# SOURCE FILES - CLIENTS (EXTERNAL API)
# ==============================================================================

set(CLIENT_SOURCES
    src/clients/RateLimiter.cc
    src/clients/HttpClient.cc
    src/clients/OAuth2Client.cc
)

# ==============================================================================
# SOURCE FILES - ADMIN
# ==============================================================================

set(ADMIN_SOURCES
    src/admin/AdminService.cc
    src/admin/SystemConfig.cc
    src/admin/AuditLog.cc
)

# ==============================================================================
# SOURCE FILES - CONTROLLERS (TOP-LEVEL)
# ==============================================================================

set(CONTROLLER_SOURCES
    src/controllers/AdminController.cc
    src/controllers/BlogController.cc
    src/controllers/TikTokController.cc
    src/controllers/TemplateController.cc
    src/controllers/AnalyticsController.cc
    src/controllers/NotificationController.cc
    src/controllers/AggregatorController.cc
    src/controllers/MediaController.cc
)

# ==============================================================================
# ALL SOURCES COMBINED
# ==============================================================================

set(ALL_SOURCES
    # Main entry point
    src/main.cc

    # Core sources
    ${CORE_UTILS_SOURCES}
    ${CORE_DB_SOURCES}
    ${CORE_DEBUG_SOURCES}
    ${CORE_EVENTBUS_SOURCES}

    # Models
    ${MODEL_SOURCES}

    # Services
    ${SERVICE_SOURCES}

    # Core Controllers
    ${CORE_CONTROLLER_SOURCES}

    # WebSocket
    ${WEBSOCKET_SOURCES}

    # Auth
    ${AUTH_SOURCES}

    # Modules
    ${MODULE_SOURCES}

    # Intervention Modules
    ${INTERVENTION_MODULE_SOURCES}

    # Content - TikTok
    ${CONTENT_TIKTOK_SOURCES}

    # Content - Blog
    ${CONTENT_BLOG_SOURCES}

    # Content - Social
    ${CONTENT_SOCIAL_SOURCES}

    # Content - Media
    ${CONTENT_MEDIA_SOURCES}

    # Content - Templates
    ${CONTENT_TEMPLATES_SOURCES}

    # Notifications
    ${NOTIFICATION_SOURCES}

    # Analytics
    ${ANALYTICS_SOURCES}

    # Aggregators
    ${AGGREGATOR_SOURCES}

    # Workers
    ${WORKER_SOURCES}

    # External API Clients
    ${CLIENT_SOURCES}

    # Admin
    ${ADMIN_SOURCES}

    # Top-level Controllers
    ${CONTROLLER_SOURCES}
)

# ==============================================================================
# MAIN EXECUTABLE
# ==============================================================================

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Drogon::Drogon
        ${PQXX_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        CURL::libcurl
)

# Link libcrypt for bcrypt password hashing via crypt_r()
target_link_libraries(${PROJECT_NAME} PRIVATE crypt)

# ==============================================================================
# INSTALLATION
# ==============================================================================

# Install executable
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install configuration templates
install(DIRECTORY config/
    DESTINATION etc/waitingthelongest
    FILES_MATCHING PATTERN "*.example.*"
)

# Install static files
install(DIRECTORY static/
    DESTINATION share/waitingthelongest/static
)

# Install SQL migrations
install(DIRECTORY sql/
    DESTINATION share/waitingthelongest/sql
    FILES_MATCHING PATTERN "*.sql"
)

# Install content templates
install(DIRECTORY content_templates/
    DESTINATION share/waitingthelongest/content_templates
)

# ==============================================================================
# TESTING (OPTIONAL)
# ==============================================================================

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()

    # Find GTest
    find_package(GTest REQUIRED)

    # Test executable
    add_executable(${PROJECT_NAME}_tests
        tests/test_main.cc
        # Add test files here
    )

    target_link_libraries(${PROJECT_NAME}_tests
        PRIVATE
            GTest::GTest
            GTest::Main
            Drogon::Drogon
            ${PQXX_LIBRARIES}
            OpenSSL::SSL
            OpenSSL::Crypto
            Threads::Threads
    )

    # Register tests
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests)
endif()

# ==============================================================================
# BUILD INFORMATION
# ==============================================================================

# Count source files for summary
list(LENGTH ALL_SOURCES SOURCE_COUNT)

message(STATUS "")
message(STATUS "=== WaitingTheLongest Build Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
message(STATUS "bcrypt found:   ${BCRYPT_FOUND}")
message(STATUS "Source files:   ${SOURCE_COUNT}")
message(STATUS "==============================================")
message(STATUS "")
message(STATUS "Source file breakdown:")
message(STATUS "  - Core Utils:       3 files")
message(STATUS "  - Database:         1 file")
message(STATUS "  - Debug:            4 files")
message(STATUS "  - EventBus:         1 file")
message(STATUS "  - Models:           6 files")
message(STATUS "  - Services:        17 files")
message(STATUS "  - Core Controllers:16 files")
message(STATUS "  - WebSocket:        6 files")
message(STATUS "  - Auth:             5 files")
message(STATUS "  - Modules:          4 files")
message(STATUS "  - TikTok Content:   5 files")
message(STATUS "  - Blog Content:     5 files")
message(STATUS "  - Social Content:  10 files")
message(STATUS "  - Media Content:    8 files")
message(STATUS "  - Templates:        4 files")
message(STATUS "  - Notifications:    8 files")
message(STATUS "  - Analytics:        8 files")
message(STATUS "  - Aggregators:      7 files")
message(STATUS "  - Workers:         12 files")
message(STATUS "  - Clients:          3 files")
message(STATUS "  - Admin:            3 files")
message(STATUS "  - Controllers:      8 files")
message(STATUS "==============================================")
message(STATUS "")

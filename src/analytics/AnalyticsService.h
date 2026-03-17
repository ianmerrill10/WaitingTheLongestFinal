/**
 * @file AnalyticsService.h
 * @brief Main analytics service for WaitingTheLongest.com
 *
 * PURPOSE:
 * Central service for all analytics tracking operations. Provides methods
 * to track events, page views, dog interactions, searches, adoptions, and
 * social media engagement. Also provides methods to query dashboard stats
 * and generate reports.
 *
 * USAGE:
 * auto& service = AnalyticsService::getInstance();
 * service.trackEvent(AnalyticsEventType::DOG_VIEW, "dog", dog_id, data);
 * auto stats = service.getDashboardStats(DateRange::lastWeek());
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - MetricsAggregator (metrics aggregation)
 * - ImpactTracker (impact tracking)
 * - SocialAnalytics (social media tracking)
 *
 * MODIFICATION GUIDE:
 * - Add new tracking methods for new event types
 * - Update getDashboardStats for new metrics
 * - Keep tracking methods lightweight for performance
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <json/json.h>

#include "analytics/AnalyticsEvent.h"
#include "analytics/DashboardStats.h"
#include "analytics/EventType.h"
#include "analytics/ImpactTracker.h"
#include "analytics/MetricsAggregator.h"
#include "analytics/SocialAnalytics.h"

namespace wtl::analytics {

/**
 * @class AnalyticsService
 * @brief Singleton service for analytics tracking and reporting
 *
 * The main interface for all analytics operations on the platform.
 * Provides methods for tracking events, querying statistics, and
 * generating reports.
 */
class AnalyticsService {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the AnalyticsService instance
     */
    static AnalyticsService& getInstance();

    // Prevent copying
    AnalyticsService(const AnalyticsService&) = delete;
    AnalyticsService& operator=(const AnalyticsService&) = delete;

    // =========================================================================
    // EVENT TRACKING
    // =========================================================================

    /**
     * @brief Track a generic event
     * @param event_type The type of event
     * @param entity_type The type of entity (dog, shelter, user, etc.)
     * @param entity_id The ID of the entity
     * @param data Additional event data as JSON
     * @param user_id Optional user ID if authenticated
     * @param session_id Session ID for the user
     * @return The created event ID
     */
    std::string trackEvent(
        AnalyticsEventType event_type,
        const std::string& entity_type,
        const std::string& entity_id,
        const Json::Value& data = Json::Value(Json::objectValue),
        const std::optional<std::string>& user_id = std::nullopt,
        const std::string& session_id = "");

    /**
     * @brief Track a full event object
     * @param event The event to track
     * @return The event ID
     */
    std::string trackEvent(const AnalyticsEvent& event);

    /**
     * @brief Track a page view
     * @param page The page path/name
     * @param user_id Optional user ID
     * @param session_id Session ID
     * @param referrer Referrer URL
     * @param data Additional data
     * @return The event ID
     */
    std::string trackPageView(
        const std::string& page,
        const std::optional<std::string>& user_id = std::nullopt,
        const std::string& session_id = "",
        const std::optional<std::string>& referrer = std::nullopt,
        const Json::Value& data = Json::Value(Json::objectValue));

    /**
     * @brief Track a dog profile view
     * @param dog_id The dog's ID
     * @param user_id Optional user ID
     * @param source Where the view came from (search, home, social, etc.)
     * @param session_id Session ID
     * @return The event ID
     */
    std::string trackDogView(
        const std::string& dog_id,
        const std::optional<std::string>& user_id = std::nullopt,
        const std::string& source = "direct",
        const std::string& session_id = "");

    /**
     * @brief Track a search action
     * @param query The search query
     * @param filters Applied filters as JSON
     * @param result_count Number of results returned
     * @param user_id Optional user ID
     * @param session_id Session ID
     * @return The event ID
     */
    std::string trackSearch(
        const std::string& query,
        const Json::Value& filters,
        int result_count,
        const std::optional<std::string>& user_id = std::nullopt,
        const std::string& session_id = "");

    /**
     * @brief Track an adoption event
     * @param dog_id The adopted dog's ID
     * @param user_id The adopter's user ID
     * @param source Attribution source
     * @return The event ID
     */
    std::string trackAdoption(
        const std::string& dog_id,
        const std::string& user_id,
        const std::string& source = "direct");

    /**
     * @brief Track a foster placement
     * @param dog_id The fostered dog's ID
     * @param foster_id The foster home ID
     * @param user_id The foster parent's user ID
     * @param source Attribution source
     * @return The event ID
     */
    std::string trackFosterPlacement(
        const std::string& dog_id,
        const std::string& foster_id,
        const std::string& user_id,
        const std::string& source = "direct");

    /**
     * @brief Track a dog being shared
     * @param dog_id The dog's ID
     * @param platform The platform shared to (facebook, twitter, etc.)
     * @param user_id Optional user ID
     * @param session_id Session ID
     * @return The event ID
     */
    std::string trackShare(
        const std::string& dog_id,
        const std::string& platform,
        const std::optional<std::string>& user_id = std::nullopt,
        const std::string& session_id = "");

    /**
     * @brief Track a dog being favorited
     * @param dog_id The dog's ID
     * @param user_id The user's ID
     * @param added True if added, false if removed
     * @return The event ID
     */
    std::string trackFavorite(
        const std::string& dog_id,
        const std::string& user_id,
        bool added = true);

    /**
     * @brief Track a social media referral visit
     * @param platform The social platform
     * @param content_id The content ID that referred them
     * @param session_id Session ID
     * @return The event ID
     */
    std::string trackSocialReferral(
        const std::string& platform,
        const std::string& content_id,
        const std::string& session_id);

    // =========================================================================
    // DASHBOARD STATISTICS
    // =========================================================================

    /**
     * @brief Get dashboard statistics for a date range
     * @param range The date range to query
     * @return Complete dashboard statistics
     */
    DashboardStats getDashboardStats(const DateRange& range);

    /**
     * @brief Get real-time statistics (current moment)
     * @return Real-time stats
     */
    RealTimeStats getRealTimeStats();

    /**
     * @brief Get engagement statistics
     * @param range Date range
     * @return Engagement stats
     */
    EngagementStats getEngagementStats(const DateRange& range);

    /**
     * @brief Get conversion statistics
     * @param range Date range
     * @return Conversion stats
     */
    ConversionStats getConversionStats(const DateRange& range);

    /**
     * @brief Get trend data for the dashboard
     * @param range Date range
     * @return Trend statistics
     */
    TrendStats getTrendStats(const DateRange& range);

    // =========================================================================
    // ENTITY-SPECIFIC STATISTICS
    // =========================================================================

    /**
     * @brief Get statistics for a specific dog
     * @param dog_id The dog's ID
     * @param range Date range (optional, defaults to all time)
     * @return Dog statistics
     */
    DogStats getDogStats(const std::string& dog_id,
                         const DateRange& range = DateRange::lastMonth());

    /**
     * @brief Get statistics for a specific shelter
     * @param shelter_id The shelter's ID
     * @param range Date range
     * @return Shelter statistics
     */
    ShelterStats getShelterStats(const std::string& shelter_id,
                                  const DateRange& range = DateRange::lastMonth());

    /**
     * @brief Get social media statistics
     * @param platform Platform name (or "all" for all platforms)
     * @param range Date range
     * @return Platform statistics or overview
     */
    Json::Value getSocialStats(const std::string& platform, const DateRange& range);

    /**
     * @brief Get impact statistics (lives saved)
     * @param range Date range
     * @return Impact summary
     */
    ImpactSummary getImpactStats(const DateRange& range);

    // =========================================================================
    // TOP PERFORMERS
    // =========================================================================

    /**
     * @brief Get most viewed dogs
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top performers
     */
    std::vector<TopPerformer> getMostViewedDogs(const DateRange& range, int limit = 10);

    /**
     * @brief Get most shared dogs
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top performers
     */
    std::vector<TopPerformer> getMostSharedDogs(const DateRange& range, int limit = 10);

    /**
     * @brief Get top shelters by adoption rate
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top performers
     */
    std::vector<TopPerformer> getTopSheltersByAdoptions(const DateRange& range, int limit = 10);

    // =========================================================================
    // EVENT QUERYING
    // =========================================================================

    /**
     * @brief Get recent events of a specific type
     * @param event_type The event type to query
     * @param limit Maximum results
     * @return Vector of events
     */
    std::vector<AnalyticsEvent> getRecentEvents(AnalyticsEventType event_type, int limit = 50);

    /**
     * @brief Get events for a specific entity
     * @param entity_type The entity type
     * @param entity_id The entity ID
     * @param range Date range
     * @return Vector of events
     */
    std::vector<AnalyticsEvent> getEventsByEntity(
        const std::string& entity_type,
        const std::string& entity_id,
        const DateRange& range);

    /**
     * @brief Get events for a specific user
     * @param user_id The user ID
     * @param range Date range
     * @return Vector of events
     */
    std::vector<AnalyticsEvent> getEventsByUser(
        const std::string& user_id,
        const DateRange& range);

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Enable or disable async event tracking
     * @param enabled True to enable async tracking
     */
    void setAsyncTracking(bool enabled);

    /**
     * @brief Set maximum queue size for async tracking
     * @param size Maximum queue size
     */
    void setMaxQueueSize(size_t size);

    /**
     * @brief Flush pending events to database
     * @return Number of events flushed
     */
    int flushEvents();

    /**
     * @brief Get current queue size
     * @return Number of events in queue
     */
    size_t getQueueSize() const;

    /**
     * @brief Shutdown the service gracefully
     */
    void shutdown();

private:
    /**
     * @brief Private constructor for singleton
     */
    AnalyticsService();

    /**
     * @brief Destructor
     */
    ~AnalyticsService();

    /**
     * @brief Save event to database
     * @param event The event to save
     * @return True if saved successfully
     */
    bool saveEvent(const AnalyticsEvent& event);

    /**
     * @brief Process events from the queue
     */
    void processQueue();

    /**
     * @brief Start the background processing thread
     */
    void startBackgroundThread();

    /**
     * @brief Stop the background processing thread
     */
    void stopBackgroundThread();

    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /** Queue for async event tracking */
    std::queue<AnalyticsEvent> event_queue_;

    /** Maximum queue size */
    size_t max_queue_size_ = 10000;

    /** Async tracking enabled */
    bool async_tracking_ = true;

    /** Background thread running */
    bool running_ = false;

    /** Background processing thread */
    std::unique_ptr<std::thread> background_thread_;

    /** Condition variable for queue processing */
    std::condition_variable queue_cv_;
};

} // namespace wtl::analytics

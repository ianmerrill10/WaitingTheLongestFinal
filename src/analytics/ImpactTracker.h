/**
 * @file ImpactTracker.h
 * @brief Life-saving impact tracking for WaitingTheLongest.com
 *
 * PURPOSE:
 * Tracks and calculates the life-saving impact of the platform including:
 * - Dogs saved from euthanasia
 * - Foster placements that led to adoption
 * - Transport rescues
 * - Time-to-adoption improvements
 * - Attribution: which content/notification led to adoption
 *
 * This is the heart of measuring our mission success - every dog saved matters.
 *
 * USAGE:
 * auto& tracker = ImpactTracker::getInstance();
 * tracker.recordRescue(dog_id, source, attribution);
 * auto impact = tracker.getImpactSummary(DateRange::lastMonth());
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - DashboardStats (data structures)
 *
 * MODIFICATION GUIDE:
 * - Add new impact types to ImpactType enum
 * - Update recording and query methods for new impact types
 * - Keep attribution tracking accurate for measuring effectiveness
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>
#include <pqxx/pqxx>

#include "analytics/DashboardStats.h"

namespace wtl::analytics {

/**
 * @enum ImpactType
 * @brief Types of life-saving impact events
 */
enum class ImpactType {
    ADOPTION,               // Dog was adopted
    FOSTER_PLACEMENT,       // Dog went to foster
    FOSTER_TO_ADOPTION,     // Foster led to adoption
    RESCUE_PULL,            // Dog was rescued/pulled from shelter
    TRANSPORT,              // Dog was transported to safety
    SAVED_FROM_EUTHANASIA,  // Dog was saved from euthanasia list
    RETURNED_TO_OWNER,      // Lost dog reunited with owner
    TRANSFER_TO_RESCUE      // Transferred to another rescue
};

/**
 * @brief Convert ImpactType to string
 */
inline std::string impactTypeToString(ImpactType type) {
    switch (type) {
        case ImpactType::ADOPTION: return "ADOPTION";
        case ImpactType::FOSTER_PLACEMENT: return "FOSTER_PLACEMENT";
        case ImpactType::FOSTER_TO_ADOPTION: return "FOSTER_TO_ADOPTION";
        case ImpactType::RESCUE_PULL: return "RESCUE_PULL";
        case ImpactType::TRANSPORT: return "TRANSPORT";
        case ImpactType::SAVED_FROM_EUTHANASIA: return "SAVED_FROM_EUTHANASIA";
        case ImpactType::RETURNED_TO_OWNER: return "RETURNED_TO_OWNER";
        case ImpactType::TRANSFER_TO_RESCUE: return "TRANSFER_TO_RESCUE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse ImpactType from string
 */
inline std::optional<ImpactType> impactTypeFromString(const std::string& str) {
    if (str == "ADOPTION") return ImpactType::ADOPTION;
    if (str == "FOSTER_PLACEMENT") return ImpactType::FOSTER_PLACEMENT;
    if (str == "FOSTER_TO_ADOPTION") return ImpactType::FOSTER_TO_ADOPTION;
    if (str == "RESCUE_PULL") return ImpactType::RESCUE_PULL;
    if (str == "TRANSPORT") return ImpactType::TRANSPORT;
    if (str == "SAVED_FROM_EUTHANASIA") return ImpactType::SAVED_FROM_EUTHANASIA;
    if (str == "RETURNED_TO_OWNER") return ImpactType::RETURNED_TO_OWNER;
    if (str == "TRANSFER_TO_RESCUE") return ImpactType::TRANSFER_TO_RESCUE;
    return std::nullopt;
}

/**
 * @struct Attribution
 * @brief Attribution data for tracking what led to a life-saving event
 */
struct Attribution {
    std::string source;               // Where did they find the dog?
    std::optional<std::string> campaign_id;    // Which campaign?
    std::optional<std::string> content_id;     // Which content (TikTok, etc)?
    std::optional<std::string> notification_id; // Which notification?
    std::optional<std::string> referrer;       // Referrer URL
    std::optional<std::string> utm_source;
    std::optional<std::string> utm_medium;
    std::optional<std::string> utm_campaign;
    std::optional<std::string> user_id;        // User who took action

    Json::Value toJson() const;
    static Attribution fromJson(const Json::Value& json);
};

/**
 * @struct ImpactEvent
 * @brief A single life-saving impact event
 */
struct ImpactEvent {
    std::string id;
    ImpactType impact_type;
    std::string dog_id;
    std::string dog_name;
    std::optional<std::string> shelter_id;
    std::optional<std::string> shelter_name;
    std::optional<std::string> state_code;
    Attribution attribution;

    // Timing info
    std::chrono::system_clock::time_point event_time;
    std::optional<int> days_in_shelter;       // How long was dog in shelter
    std::optional<int> days_on_platform;      // How long was dog on our platform
    std::optional<int> hours_to_euthanasia;   // How close to euthanasia (0 = was on list)

    // Additional context
    bool was_urgent = false;
    bool was_critical = false;
    bool was_on_euthanasia_list = false;
    Json::Value additional_data;

    Json::Value toJson() const;
    static ImpactEvent fromDbRow(const pqxx::row& row);
};

/**
 * @struct ImpactMetrics
 * @brief Aggregated impact metrics for a time period
 */
struct ImpactMetrics {
    // Core counts
    int total_lives_saved = 0;
    int adoptions = 0;
    int foster_placements = 0;
    int foster_to_adoptions = 0;
    int rescue_pulls = 0;
    int transports = 0;
    int saved_from_euthanasia = 0;
    int returned_to_owners = 0;
    int transfers_to_rescue = 0;

    // Time metrics
    double avg_days_in_shelter = 0;
    double avg_days_on_platform = 0;
    double time_improvement_days = 0;  // Days faster than average

    // Attribution breakdown
    std::map<std::string, int> by_source;    // Source -> count
    std::map<std::string, int> by_platform;  // Social platform -> count
    std::map<std::string, int> by_state;     // State code -> count
    std::map<std::string, int> by_shelter;   // Shelter ID -> count

    // Urgency breakdown
    int urgent_dogs_saved = 0;
    int critical_dogs_saved = 0;
    int euthanasia_list_saves = 0;

    Json::Value toJson() const;
};

/**
 * @struct AttributionReport
 * @brief Report on what sources are leading to adoptions
 */
struct AttributionReport {
    struct SourcePerformance {
        std::string source;
        int total_saves = 0;
        int adoptions = 0;
        int fosters = 0;
        double conversion_rate = 0;  // Views to saves
        double avg_time_to_save = 0; // Days from first view to save
    };

    std::vector<SourcePerformance> sources;
    std::string top_source;
    std::string top_campaign;
    std::string top_content;
    int total_attributed = 0;
    int total_unattributed = 0;

    Json::Value toJson() const;
};

/**
 * @class ImpactTracker
 * @brief Singleton service for tracking life-saving impact
 *
 * This service is crucial for measuring the platform's mission success.
 * It tracks every life saved and attributes it to the source that made
 * it possible, enabling us to optimize for maximum impact.
 */
class ImpactTracker {
public:
    /**
     * @brief Get singleton instance
     */
    static ImpactTracker& getInstance();

    // Prevent copying
    ImpactTracker(const ImpactTracker&) = delete;
    ImpactTracker& operator=(const ImpactTracker&) = delete;

    // =========================================================================
    // RECORDING IMPACT
    // =========================================================================

    /**
     * @brief Record an adoption
     * @param dog_id The adopted dog's ID
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordAdoption(const std::string& dog_id, const Attribution& attribution);

    /**
     * @brief Record a foster placement
     * @param dog_id The fostered dog's ID
     * @param foster_id The foster home ID
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordFosterPlacement(
        const std::string& dog_id,
        const std::string& foster_id,
        const Attribution& attribution);

    /**
     * @brief Record a foster-to-adoption (foster family adopted)
     * @param dog_id The adopted dog's ID
     * @param foster_id The foster home ID
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordFosterToAdoption(
        const std::string& dog_id,
        const std::string& foster_id,
        const Attribution& attribution);

    /**
     * @brief Record a rescue pull
     * @param dog_id The rescued dog's ID
     * @param rescue_id The rescue organization ID
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordRescuePull(
        const std::string& dog_id,
        const std::string& rescue_id,
        const Attribution& attribution);

    /**
     * @brief Record a transport
     * @param dog_id The transported dog's ID
     * @param from_shelter_id Origin shelter
     * @param to_shelter_id Destination shelter/rescue
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordTransport(
        const std::string& dog_id,
        const std::string& from_shelter_id,
        const std::string& to_shelter_id,
        const Attribution& attribution);

    /**
     * @brief Record a dog saved from euthanasia list
     * @param dog_id The dog's ID
     * @param outcome How they were saved (adopted, fostered, rescued)
     * @param attribution Attribution data
     * @return The created impact event ID
     */
    std::string recordSavedFromEuthanasia(
        const std::string& dog_id,
        ImpactType outcome,
        const Attribution& attribution);

    /**
     * @brief Record a generic impact event
     * @param event The impact event to record
     * @return The created event ID
     */
    std::string recordImpact(const ImpactEvent& event);

    // =========================================================================
    // QUERYING IMPACT
    // =========================================================================

    /**
     * @brief Get impact summary for a date range
     * @param range The date range to query
     * @return Impact summary struct
     */
    ImpactSummary getImpactSummary(const DateRange& range);

    /**
     * @brief Get detailed impact metrics
     * @param range The date range to query
     * @return Detailed impact metrics
     */
    ImpactMetrics getImpactMetrics(const DateRange& range);

    /**
     * @brief Get recent impact events
     * @param limit Maximum number of events
     * @return Vector of recent impact events
     */
    std::vector<ImpactEvent> getRecentImpactEvents(int limit = 50);

    /**
     * @brief Get impact events for a specific dog
     * @param dog_id The dog ID
     * @return Vector of impact events
     */
    std::vector<ImpactEvent> getImpactByDog(const std::string& dog_id);

    /**
     * @brief Get impact events for a specific shelter
     * @param shelter_id The shelter ID
     * @param range Date range to query
     * @return Vector of impact events
     */
    std::vector<ImpactEvent> getImpactByShelter(const std::string& shelter_id, const DateRange& range);

    /**
     * @brief Get impact events by state
     * @param state_code The state code
     * @param range Date range to query
     * @return Vector of impact events
     */
    std::vector<ImpactEvent> getImpactByState(const std::string& state_code, const DateRange& range);

    /**
     * @brief Get total lives saved count
     * @return Total number of lives saved (all time)
     */
    int getTotalLivesSaved();

    /**
     * @brief Get lives saved in a time period
     * @param range Date range to query
     * @return Number of lives saved
     */
    int getLivesSaved(const DateRange& range);

    // =========================================================================
    // ATTRIBUTION
    // =========================================================================

    /**
     * @brief Get attribution report for a date range
     * @param range The date range to query
     * @return Attribution report with source performance
     */
    AttributionReport getAttributionReport(const DateRange& range);

    /**
     * @brief Get top performing content by adoptions
     * @param range Date range to query
     * @param limit Maximum results
     * @return Vector of content IDs with adoption counts
     */
    std::vector<std::pair<std::string, int>> getTopContentByAdoptions(const DateRange& range, int limit = 10);

    /**
     * @brief Get top performing campaigns by adoptions
     * @param range Date range to query
     * @param limit Maximum results
     * @return Vector of campaign IDs with adoption counts
     */
    std::vector<std::pair<std::string, int>> getTopCampaignsByAdoptions(const DateRange& range, int limit = 10);

    /**
     * @brief Calculate conversion rate from views to adoptions
     * @param source The traffic source
     * @param range Date range to query
     * @return Conversion rate percentage
     */
    double getConversionRate(const std::string& source, const DateRange& range);

    // =========================================================================
    // TIME ANALYSIS
    // =========================================================================

    /**
     * @brief Get average time to adoption
     * @param range Date range to query
     * @return Average days from intake to adoption
     */
    double getAverageTimeToAdoption(const DateRange& range);

    /**
     * @brief Get average time improvement (vs historical average)
     * @param range Date range to query
     * @return Days improvement (positive = faster)
     */
    double getTimeImprovement(const DateRange& range);

    /**
     * @brief Get adoption time distribution
     * @param range Date range to query
     * @return Map of day buckets (0-7, 8-14, 15-30, etc.) to counts
     */
    std::map<std::string, int> getAdoptionTimeDistribution(const DateRange& range);

    // =========================================================================
    // URGENCY IMPACT
    // =========================================================================

    /**
     * @brief Get count of urgent dogs saved
     * @param range Date range to query
     * @return Number of urgent dogs that were saved
     */
    int getUrgentDogsSaved(const DateRange& range);

    /**
     * @brief Get count of critical dogs saved (< 24 hours)
     * @param range Date range to query
     * @return Number of critical dogs that were saved
     */
    int getCriticalDogsSaved(const DateRange& range);

    /**
     * @brief Get dogs saved from euthanasia list
     * @param range Date range to query
     * @return Number of dogs saved from euthanasia lists
     */
    int getEuthanasiaListSaves(const DateRange& range);

private:
    /**
     * @brief Private constructor for singleton
     */
    ImpactTracker();

    /**
     * @brief Save impact event to database
     */
    bool saveImpactEvent(const ImpactEvent& event);

    /**
     * @brief Get dog context for impact event
     */
    void populateDogContext(ImpactEvent& event);

    /**
     * @brief Calculate time metrics for an event
     */
    void calculateTimeMetrics(ImpactEvent& event);

    /** Mutex for thread safety */
    std::mutex mutex_;

    /** Cached total lives saved (updated periodically) */
    int cached_total_lives_ = 0;
    std::chrono::system_clock::time_point cache_time_;
};

} // namespace wtl::analytics

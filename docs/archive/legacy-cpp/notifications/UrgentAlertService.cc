/**
 * @file UrgentAlertService.cc
 * @brief Implementation of UrgentAlertService emergency broadcast system
 * @see UrgentAlertService.h for documentation
 */

#include "notifications/UrgentAlertService.h"

// Standard library includes
#include <cmath>
#include <random>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/EventBus.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"
#include "notifications/NotificationService.h"

namespace wtl::notifications {

// ============================================================================
// CONSTANTS
// ============================================================================

// Earth's radius in miles for distance calculation
constexpr double EARTH_RADIUS_MILES = 3959.0;

// ============================================================================
// SINGLETON
// ============================================================================

UrgentAlertService& UrgentAlertService::getInstance() {
    static UrgentAlertService instance;
    return instance;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UrgentAlertService::setConfig(const UrgentAlertConfig& config) {
    config_ = config;
}

UrgentAlertConfig UrgentAlertService::getConfig() const {
    return config_;
}

// ============================================================================
// URGENT BROADCASTS
// ============================================================================

BlastResult UrgentAlertService::blastNotification(const core::models::Dog& dog, double radius_miles) {
    auto start_time = std::chrono::steady_clock::now();

    BlastResult result;
    result.users_notified = 0;
    result.users_attempted = 0;
    result.users_within_radius = 0;
    result.rescues_notified = 0;
    result.fosters_notified = 0;

    // Check cooldown
    if (isOnCooldown(dog.id)) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Urgent blast on cooldown for dog",
            {{"dog_id", dog.id}}
        );
        return result;
    }

    // Get shelter info
    auto shelter_opt = getShelterForDog(dog.shelter_id);
    if (!shelter_opt.has_value()) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Could not find shelter for urgent blast",
            {{"dog_id", dog.id}, {"shelter_id", dog.shelter_id}}
        );
        return result;
    }
    auto& shelter = shelter_opt.value();

    // Calculate radius if not specified
    if (radius_miles <= 0) {
        radius_miles = calculateRadius(dog);
    }

    // Cap at maximum
    radius_miles = std::min(radius_miles, config_.max_radius_miles);

    // Find eligible users
    auto eligible_users = findEligibleUsers(dog, radius_miles,
                                            config_.notify_fosters,
                                            config_.notify_rescues);

    result.users_within_radius = static_cast<int>(eligible_users.size());

    // Cap to prevent runaway blasts
    if (eligible_users.size() > static_cast<size_t>(config_.max_notifications_per_alert)) {
        eligible_users.resize(config_.max_notifications_per_alert);
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Urgent blast capped at max notifications",
            {{"dog_id", dog.id}, {"max", std::to_string(config_.max_notifications_per_alert)}}
        );
    }

    // Determine notification type based on urgency
    NotificationType type = NotificationType::HIGH_URGENCY;
    if (dog.urgency_level == "critical") {
        type = NotificationType::CRITICAL_ALERT;
    }

    // Build notification
    Notification notification = buildUrgentNotification(dog, shelter, type);

    // Generate alert ID
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    std::stringstream ss;
    ss << std::hex << dis(gen);
    result.alert_id = "alert_" + ss.str().substr(0, 16);

    // Send to all eligible users
    result.users_attempted = static_cast<int>(eligible_users.size());

    for (const auto& user_id : eligible_users) {
        try {
            auto send_result = NotificationService::getInstance().sendNotification(
                notification, user_id
            );
            if (send_result.success) {
                result.users_notified++;
            }
        } catch (const std::exception& e) {
            // Continue with other users
        }
    }

    // Track channels used
    result.channels_used.push_back("push");
    result.channels_used.push_back("email");
    if (type == NotificationType::CRITICAL_ALERT) {
        result.channels_used.push_back("sms");
    }

    // Set cooldown
    setCooldown(dog.id);

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Log and emit event
    logBlast(result, dog.id);
    emitBlastEvent(result, dog.id);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_blasts_++;
        total_users_notified_ += result.users_notified;
        if (type == NotificationType::CRITICAL_ALERT) {
            total_critical_alerts_++;
        }
        recent_blasts_.push_back(result);
        if (recent_blasts_.size() > 100) {
            recent_blasts_.erase(recent_blasts_.begin());
        }
    }

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
        "Urgent blast completed",
        {{"dog_id", dog.id},
         {"users_notified", std::to_string(result.users_notified)},
         {"radius_miles", std::to_string(radius_miles)}}
    );

    return result;
}

BlastResult UrgentAlertService::triggerCriticalAlert(const core::models::Dog& dog) {
    // Critical alerts use expanded radius
    return blastNotification(dog, config_.critical_radius_miles);
}

BlastResult UrgentAlertService::triggerHighUrgencyAlert(const core::models::Dog& dog) {
    return blastNotification(dog, config_.default_radius_miles);
}

BlastResult UrgentAlertService::sendFosterNeededAlert(const core::models::Dog& dog, double radius_miles) {
    auto start_time = std::chrono::steady_clock::now();

    BlastResult result;
    result.users_notified = 0;
    result.users_attempted = 0;

    if (radius_miles <= 0) {
        radius_miles = config_.default_radius_miles;
    }

    // Find matching foster homes
    auto fosters = findMatchingFosters(dog, radius_miles);
    result.users_within_radius = static_cast<int>(fosters.size());
    result.users_attempted = static_cast<int>(fosters.size());

    // Get shelter info
    auto shelter_opt = getShelterForDog(dog.shelter_id);
    if (!shelter_opt.has_value()) {
        return result;
    }
    auto& shelter = shelter_opt.value();

    // Build notification
    Notification notification;
    notification.type = NotificationType::FOSTER_NEEDED_URGENT;
    notification.title = "Foster Needed: " + dog.name;
    notification.body = dog.name + " at " + shelter.name + " urgently needs a foster home. "
                       "Your profile matches - can you help?";
    notification.dog_id = dog.id;
    notification.shelter_id = shelter.id;
    notification.action_url = "https://waitingthelongest.com/dogs/" + dog.id + "?foster=true";

    if (!dog.photo_urls.empty()) {
        notification.image_url = dog.photo_urls[0];
    }

    // Send to matching fosters
    for (const auto& user_id : fosters) {
        try {
            auto send_result = NotificationService::getInstance().sendNotification(
                notification, user_id
            );
            if (send_result.success) {
                result.users_notified++;
                result.fosters_notified++;
            }
        } catch (const std::exception& e) {
            // Continue
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

int UrgentAlertService::notifyRescueOrganizations(const core::models::Dog& dog, double radius_miles) {
    if (radius_miles <= 0) {
        radius_miles = config_.default_radius_miles;
    }

    // Get shelter info for location
    auto shelter_opt = getShelterForDog(dog.shelter_id);
    if (!shelter_opt.has_value()) {
        return 0;
    }
    auto& shelter = shelter_opt.value();

    // Find nearby rescues
    auto rescues = findNearbyRescues(
        shelter.state_code,
        radius_miles,
        shelter.latitude.value_or(0.0),
        shelter.longitude.value_or(0.0)
    );

    // Build notification
    Notification notification;
    notification.type = NotificationType::CRITICAL_ALERT;
    notification.title = "Rescue Pull Needed: " + dog.name;
    notification.body = dog.name + " at " + shelter.name + " needs a rescue pull. "
                       + std::string(dog.urgency_level == "critical" ? "CRITICAL - less than 24 hours!" : "");
    notification.dog_id = dog.id;
    notification.shelter_id = shelter.id;
    notification.action_url = "https://waitingthelongest.com/dogs/" + dog.id;

    if (!dog.photo_urls.empty()) {
        notification.image_url = dog.photo_urls[0];
    }

    int notified = 0;
    for (const auto& rescue_id : rescues) {
        try {
            auto send_result = NotificationService::getInstance().sendNotification(
                notification, rescue_id
            );
            if (send_result.success) {
                notified++;
            }
        } catch (const std::exception& e) {
            // Continue
        }
    }

    return notified;
}

// ============================================================================
// TARGETING
// ============================================================================

std::vector<std::string> UrgentAlertService::findEligibleUsers(const core::models::Dog& dog,
                                                                 double radius_miles,
                                                                 bool include_fosters,
                                                                 bool include_rescues) {
    std::vector<std::string> users;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get shelter location
        auto shelter_result = txn.exec_params(
            "SELECT latitude, longitude, state_code FROM shelters WHERE id = $1",
            dog.shelter_id
        );

        if (shelter_result.empty()) {
            wtl::core::db::ConnectionPool::getInstance().release(conn);
            return users;
        }

        double shelter_lat = shelter_result[0]["latitude"].is_null() ? 0.0 :
                             shelter_result[0]["latitude"].as<double>();
        double shelter_lon = shelter_result[0]["longitude"].is_null() ? 0.0 :
                             shelter_result[0]["longitude"].as<double>();
        std::string state_code = shelter_result[0]["state_code"].as<std::string>();

        // Build query to find eligible users
        std::string query = R"(
            SELECT DISTINCT u.id FROM users u
            JOIN user_notification_preferences p ON u.id = p.user_id
            WHERE (
                -- Has critical alerts enabled
                (p.receive_critical_alerts = true AND (
                    -- No location set means receive all
                    p.home_latitude IS NULL OR p.home_longitude IS NULL
                    -- Or radius is 0 (everywhere)
                    OR p.critical_alert_radius_miles = 0
                    -- Or within radius
                    OR (
                        3959 * acos(
                            cos(radians($1)) * cos(radians(p.home_latitude)) *
                            cos(radians(p.home_longitude) - radians($2)) +
                            sin(radians($1)) * sin(radians(p.home_latitude))
                        ) <= $3
                    )
                ))
            )

            UNION

            -- Users who favorited this dog
            SELECT user_id FROM favorites WHERE dog_id = $4

            UNION

            -- Users who favorited any dog at this shelter
            SELECT DISTINCT f.user_id FROM favorites f
            JOIN dogs d ON f.dog_id = d.id
            WHERE d.shelter_id = $5
        )";

        auto result = txn.exec_params(query,
            shelter_lat, shelter_lon, radius_miles, dog.id, dog.shelter_id
        );

        for (const auto& row : result) {
            users.push_back(row["id"].as<std::string>());
        }

        // Add foster homes if requested
        if (include_fosters) {
            auto fosters = findMatchingFosters(dog, radius_miles);
            for (const auto& foster : fosters) {
                if (std::find(users.begin(), users.end(), foster) == users.end()) {
                    users.push_back(foster);
                }
            }
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find eligible users for urgent alert: " + std::string(e.what()),
            {{"dog_id", dog.id}}
        );
    }

    return users;
}

std::vector<std::string> UrgentAlertService::findMatchingFosters(const core::models::Dog& dog,
                                                                   double radius_miles) {
    std::vector<std::string> fosters;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get shelter location
        auto shelter_result = txn.exec_params(
            "SELECT latitude, longitude FROM shelters WHERE id = $1",
            dog.shelter_id
        );

        if (shelter_result.empty()) {
            wtl::core::db::ConnectionPool::getInstance().release(conn);
            return fosters;
        }

        double shelter_lat = shelter_result[0]["latitude"].is_null() ? 0.0 :
                             shelter_result[0]["latitude"].as<double>();
        double shelter_lon = shelter_result[0]["longitude"].is_null() ? 0.0 :
                             shelter_result[0]["longitude"].as<double>();

        // Find foster homes that match dog criteria and are within radius
        auto result = txn.exec_params(
            R"(
            SELECT fh.user_id FROM foster_homes fh
            JOIN user_notification_preferences p ON fh.user_id = p.user_id
            WHERE fh.is_active = true
            AND fh.is_verified = true
            AND fh.current_dogs < fh.max_dogs
            AND p.receive_foster_urgent_alerts = true
            AND (
                -- Size preference matches
                fh.preferred_sizes IS NULL
                OR $1 = ANY(fh.preferred_sizes)
            )
            AND (
                -- Within transport radius
                fh.transport_radius_miles >= (
                    3959 * acos(
                        cos(radians($2)) * cos(radians(fh.latitude)) *
                        cos(radians(fh.longitude) - radians($3)) +
                        sin(radians($2)) * sin(radians(fh.latitude))
                    )
                )
                OR (
                    3959 * acos(
                        cos(radians($2)) * cos(radians(fh.latitude)) *
                        cos(radians(fh.longitude) - radians($3)) +
                        sin(radians($2)) * sin(radians(fh.latitude))
                    ) <= $4
                )
            )
            LIMIT 500
            )",
            dog.size, shelter_lat, shelter_lon, radius_miles
        );

        for (const auto& row : result) {
            fosters.push_back(row["user_id"].as<std::string>());
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find matching fosters: " + std::string(e.what()),
            {{"dog_id", dog.id}}
        );
    }

    return fosters;
}

std::vector<std::string> UrgentAlertService::findNearbyRescues(const std::string& state_code,
                                                                 double radius_miles,
                                                                 double shelter_lat,
                                                                 double shelter_lon) {
    std::vector<std::string> rescues;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find rescue organizations (shelters marked as rescue)
        // that are in the same state or nearby
        auto result = txn.exec_params(
            R"(
            SELECT DISTINCT s.contact_user_id FROM shelters s
            WHERE s.is_rescue = true
            AND s.is_active = true
            AND s.contact_user_id IS NOT NULL
            AND (
                s.state_code = $1
                OR (
                    s.latitude IS NOT NULL AND s.longitude IS NOT NULL
                    AND 3959 * acos(
                        cos(radians($2)) * cos(radians(s.latitude)) *
                        cos(radians(s.longitude) - radians($3)) +
                        sin(radians($2)) * sin(radians(s.latitude))
                    ) <= $4
                )
            )
            LIMIT 100
            )",
            state_code, shelter_lat, shelter_lon, radius_miles
        );

        for (const auto& row : result) {
            rescues.push_back(row["contact_user_id"].as<std::string>());
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find nearby rescues: " + std::string(e.what()),
            {{"state_code", state_code}}
        );
    }

    return rescues;
}

// ============================================================================
// COOLDOWN MANAGEMENT
// ============================================================================

bool UrgentAlertService::isOnCooldown(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(cooldown_mutex_);

    auto it = cooldowns_.find(dog_id);
    if (it == cooldowns_.end()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto cooldown_end = it->second + std::chrono::minutes(config_.cooldown_minutes);

    if (now >= cooldown_end) {
        cooldowns_.erase(it);
        return false;
    }

    return true;
}

void UrgentAlertService::setCooldown(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(cooldown_mutex_);
    cooldowns_[dog_id] = std::chrono::system_clock::now();
}

void UrgentAlertService::clearCooldown(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(cooldown_mutex_);
    cooldowns_.erase(dog_id);
}

// ============================================================================
// INTEGRATION
// ============================================================================

void UrgentAlertService::onUrgencyChanged(const std::string& dog_id,
                                           const std::string& new_urgency_level,
                                           const std::string& old_urgency_level) {
    // Only trigger if urgency increased to critical or high
    if (new_urgency_level == "critical" && old_urgency_level != "critical") {
        // Get dog info and trigger critical alert
        try {
            auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
            pqxx::work txn(*conn);

            auto result = txn.exec_params(
                "SELECT * FROM dogs WHERE id = $1",
                dog_id
            );

            txn.commit();
            wtl::core::db::ConnectionPool::getInstance().release(conn);

            if (!result.empty()) {
                core::models::Dog dog = core::models::Dog::fromDbRow(result[0]);
                triggerCriticalAlert(dog);
            }
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::DATABASE,
                "Failed to trigger alert on urgency change: " + std::string(e.what()),
                {{"dog_id", dog_id}}
            );
        }
    } else if (new_urgency_level == "high" && old_urgency_level == "normal") {
        // Trigger high urgency alert only if moving from normal to high
        try {
            auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
            pqxx::work txn(*conn);

            auto result = txn.exec_params(
                "SELECT * FROM dogs WHERE id = $1",
                dog_id
            );

            txn.commit();
            wtl::core::db::ConnectionPool::getInstance().release(conn);

            if (!result.empty()) {
                core::models::Dog dog = core::models::Dog::fromDbRow(result[0]);
                triggerHighUrgencyAlert(dog);
            }
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::DATABASE,
                "Failed to trigger alert on urgency change: " + std::string(e.what()),
                {{"dog_id", dog_id}}
            );
        }
    }
}

void UrgentAlertService::subscribeToEvents() {
    wtl::core::EventBus::getInstance().subscribe(
        wtl::core::EventType::DOG_URGENCY_CHANGED,
        [this](const wtl::core::Event& event) {
            std::string dog_id = event.data.get("dog_id", "").asString();
            std::string new_level = event.data.get("new_urgency_level", "").asString();
            std::string old_level = event.data.get("old_urgency_level", "").asString();
            onUrgencyChanged(dog_id, new_level, old_level);
        }
    );
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value UrgentAlertService::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_blasts"] = static_cast<Json::UInt64>(total_blasts_);
    stats["total_users_notified"] = static_cast<Json::UInt64>(total_users_notified_);
    stats["total_critical_alerts"] = static_cast<Json::UInt64>(total_critical_alerts_);
    stats["recent_blasts_count"] = static_cast<int>(recent_blasts_.size());

    return stats;
}

std::vector<BlastResult> UrgentAlertService::getRecentBlasts(int limit) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    int count = std::min(limit, static_cast<int>(recent_blasts_.size()));
    std::vector<BlastResult> results(recent_blasts_.end() - count, recent_blasts_.end());
    return results;
}

void UrgentAlertService::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_blasts_ = 0;
    total_users_notified_ = 0;
    total_critical_alerts_ = 0;
    recent_blasts_.clear();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::optional<core::models::Shelter> UrgentAlertService::getShelterForDog(const std::string& shelter_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM shelters WHERE id = $1",
            shelter_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return core::models::Shelter::fromDbRow(result[0]);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get shelter: " + std::string(e.what()),
            {{"shelter_id", shelter_id}}
        );
    }

    return std::nullopt;
}

double UrgentAlertService::calculateRadius(const core::models::Dog& dog) const {
    if (dog.urgency_level == "critical") {
        return config_.critical_radius_miles;
    }
    return config_.default_radius_miles;
}

Notification UrgentAlertService::buildUrgentNotification(const core::models::Dog& dog,
                                                          const core::models::Shelter& shelter,
                                                          NotificationType type) {
    Notification notification;
    notification.type = type;
    notification.dog_id = dog.id;
    notification.shelter_id = shelter.id;

    if (type == NotificationType::CRITICAL_ALERT) {
        notification.title = "URGENT: " + dog.name + " needs help NOW!";
        notification.body = dog.name + " at " + shelter.name + " has less than 24 hours. "
                           "Please help save this life!";
    } else {
        notification.title = "Time Sensitive: " + dog.name + " at Risk";
        notification.body = dog.name + " at " + shelter.name + " is running out of time. "
                           "Can you help?";
    }

    notification.action_url = "https://waitingthelongest.com/dogs/" + dog.id;

    if (!dog.photo_urls.empty()) {
        notification.image_url = dog.photo_urls[0];
    }

    notification.data["urgency_level"] = dog.urgency_level;
    notification.data["shelter_name"] = shelter.name;
    notification.data["state_code"] = shelter.state_code;
    notification.data["breed"] = dog.breed_primary;

    return notification;
}

void UrgentAlertService::logBlast(const BlastResult& result, const std::string& dog_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            INSERT INTO urgent_blast_log (
                alert_id, dog_id, users_notified, users_attempted,
                duration_ms, created_at
            ) VALUES ($1, $2, $3, $4, $5, NOW())
            )",
            result.alert_id,
            dog_id,
            result.users_notified,
            result.users_attempted,
            result.duration.count()
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        // Non-critical, just log
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to log urgent blast: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }
}

void UrgentAlertService::emitBlastEvent(const BlastResult& result, const std::string& dog_id) {
    try {
        wtl::core::Event event;
        event.type = wtl::core::EventType::NOTIFICATION_SENT;
        event.data["alert_id"] = result.alert_id;
        event.data["dog_id"] = dog_id;
        event.data["users_notified"] = result.users_notified;

        wtl::core::EventBus::getInstance().publishAsync(event);

    } catch (const std::exception& e) {
        // Non-critical
    }
}

} // namespace wtl::notifications

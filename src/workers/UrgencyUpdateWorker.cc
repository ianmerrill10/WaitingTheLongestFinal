/**
 * @file UrgencyUpdateWorker.cc
 * @brief Implementation of UrgencyUpdateWorker
 * @see UrgencyUpdateWorker.h for documentation
 */

#include "UrgencyUpdateWorker.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/EventBus.h"

namespace wtl::workers {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

UrgencyUpdateWorker::UrgencyUpdateWorker()
    : BaseWorker("urgency_update_worker",
                 "Recalculates urgency levels and triggers alerts for critical dogs",
                 std::chrono::seconds(60),  // Every minute
                 WorkerPriority::HIGH)  // High priority - lives depend on this
{
}

UrgencyUpdateResult UrgencyUpdateWorker::getLastResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_;
}

UrgencyUpdateResult UrgencyUpdateWorker::updateNow() {
    UrgencyUpdateResult result;

    try {
        updateUrgencyLevels(result);

        if (!result.newly_critical_ids.empty()) {
            triggerAlerts(result.newly_critical_ids, result);
            emitCriticalEvents(result.newly_critical_ids);
        }
    } catch (const std::exception& e) {
        logError("Urgency update failed: " + std::string(e.what()), {});
    }

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        last_result_ = result;
    }

    return result;
}

WorkerResult UrgencyUpdateWorker::doExecute() {
    UrgencyUpdateResult result = updateNow();

    WorkerResult wr = WorkerResult::Success(
        "Updated " + std::to_string(result.dogs_updated) + " dogs, " +
        std::to_string(result.became_critical) + " became critical",
        result.dogs_checked);

    wr.metadata = result.toJson();
    return wr;
}

void UrgencyUpdateWorker::beforeExecute() {
    logInfo("Starting urgency level recalculation...");
}

void UrgencyUpdateWorker::afterExecute(const WorkerResult& result) {
    if (result.success) {
        logInfo("Urgency update complete: " + result.message);
    }
}

void UrgencyUpdateWorker::updateUrgencyLevels(UrgencyUpdateResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get all dogs with euthanasia dates
        auto dogs = txn.exec(R"(
            SELECT id, urgency_level, euthanasia_date,
                   EXTRACT(EPOCH FROM (euthanasia_date - NOW())) / 3600 AS hours_remaining
            FROM dogs
            WHERE is_available = true
            AND euthanasia_date IS NOT NULL
            AND euthanasia_date > NOW()
            ORDER BY euthanasia_date ASC
        )");

        result.dogs_checked = dogs.size();

        for (const auto& row : dogs) {
            std::string dog_id = row["id"].as<std::string>();
            std::string current_level = row["urgency_level"].as<std::string>("normal");
            double hours_remaining = row["hours_remaining"].as<double>();

            // Calculate new urgency level
            std::string new_level;
            if (hours_remaining <= 24) {
                new_level = "critical";
            } else if (hours_remaining <= 72) {
                new_level = "high";
            } else if (hours_remaining <= 168) {  // 7 days
                new_level = "medium";
            } else {
                new_level = "normal";
            }

            // Update if changed
            if (new_level != current_level) {
                txn.exec_params(
                    "UPDATE dogs SET urgency_level = $1, updated_at = NOW() WHERE id = $2",
                    new_level, dog_id);

                result.dogs_updated++;

                if (new_level == "critical" && current_level != "critical") {
                    result.became_critical++;
                    result.newly_critical_ids.push_back(dog_id);
                } else if (new_level == "high" && current_level != "high" && current_level != "critical") {
                    result.became_high++;
                } else if (new_level == "medium") {
                    result.became_medium++;
                }
            }
        }

        // Also update dogs at kill shelters without euthanasia dates
        // based on wait time (longer wait = higher urgency)
        txn.exec(R"(
            UPDATE dogs SET
                urgency_level = CASE
                    WHEN EXTRACT(EPOCH FROM (NOW() - intake_date)) / 86400 > 21 THEN 'high'
                    WHEN EXTRACT(EPOCH FROM (NOW() - intake_date)) / 86400 > 14 THEN 'medium'
                    ELSE 'normal'
                END,
                updated_at = NOW()
            WHERE is_available = true
            AND euthanasia_date IS NULL
            AND shelter_id IN (SELECT id FROM shelters WHERE is_kill_shelter = true)
        )");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logError("Failed to update urgency levels: " + std::string(e.what()), {});
        throw;
    }
}

void UrgencyUpdateWorker::triggerAlerts(const std::vector<std::string>& critical_ids,
                                         UrgencyUpdateResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        for (const auto& dog_id : critical_ids) {
            // Check if alert already exists for this dog
            auto existing = txn.exec_params(
                "SELECT id FROM urgency_alerts WHERE dog_id = $1 AND status = 'active'",
                dog_id);

            if (existing.empty()) {
                // Create new alert
                txn.exec_params(
                    R"(INSERT INTO urgency_alerts (dog_id, alert_type, status, created_at)
                       VALUES ($1, 'critical', 'active', NOW()))",
                    dog_id);

                result.alerts_triggered++;
            }
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logError("Failed to trigger alerts: " + std::string(e.what()), {});
    }
}

void UrgencyUpdateWorker::emitCriticalEvents(const std::vector<std::string>& ids) {
    for (const auto& dog_id : ids) {
        try {
            Json::Value data;
            data["urgency_level"] = "critical";
            data["reason"] = "euthanasia_imminent";

            wtl::core::emitEvent(
                wtl::core::EventType::DOG_BECAME_CRITICAL,
                dog_id,
                "dog",
                data,
                "UrgencyUpdateWorker"
            );
        } catch (const std::exception& e) {
            logWarning("Failed to emit critical event for dog " + dog_id);
        }
    }
}

} // namespace wtl::workers

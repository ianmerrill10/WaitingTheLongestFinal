/**
 * @file UrgencyUpdateWorker.h
 * @brief Worker for updating dog urgency levels
 *
 * PURPOSE:
 * Recalculates urgency levels for all dogs based on euthanasia dates,
 * updates countdown timers, and triggers alerts for newly critical dogs.
 * This is a LIFE-SAVING worker that runs every minute.
 *
 * USAGE:
 * auto worker = std::make_unique<UrgencyUpdateWorker>();
 * WorkerManager::getInstance().registerWorker("urgency_update", std::move(worker));
 *
 * DEPENDENCIES:
 * - BaseWorker for worker infrastructure
 * - UrgencyCalculator for level calculation
 * - AlertService for triggering alerts
 * - EventBus for DOG_BECAME_CRITICAL events
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <json/json.h>
#include "BaseWorker.h"

namespace wtl::workers {

/**
 * @struct UrgencyUpdateResult
 * @brief Result of urgency update operation
 */
struct UrgencyUpdateResult {
    int dogs_checked{0};
    int dogs_updated{0};
    int became_critical{0};
    int became_high{0};
    int became_medium{0};
    int alerts_triggered{0};
    std::vector<std::string> newly_critical_ids;

    Json::Value toJson() const {
        Json::Value json;
        json["dogs_checked"] = dogs_checked;
        json["dogs_updated"] = dogs_updated;
        json["became_critical"] = became_critical;
        json["became_high"] = became_high;
        json["became_medium"] = became_medium;
        json["alerts_triggered"] = alerts_triggered;

        Json::Value ids(Json::arrayValue);
        for (const auto& id : newly_critical_ids) {
            ids.append(id);
        }
        json["newly_critical_ids"] = ids;
        return json;
    }
};

/**
 * @class UrgencyUpdateWorker
 * @brief Worker for updating urgency levels every minute
 */
class UrgencyUpdateWorker : public BaseWorker {
public:
    /**
     * @brief Constructor - runs every minute by default
     */
    UrgencyUpdateWorker();

    ~UrgencyUpdateWorker() override = default;

    /**
     * @brief Get last update result
     */
    UrgencyUpdateResult getLastResult() const;

    /**
     * @brief Force immediate update
     */
    UrgencyUpdateResult updateNow();

protected:
    WorkerResult doExecute() override;
    void beforeExecute() override;
    void afterExecute(const WorkerResult& result) override;

private:
    void updateUrgencyLevels(UrgencyUpdateResult& result);
    void triggerAlerts(const std::vector<std::string>& critical_ids, UrgencyUpdateResult& result);
    void emitCriticalEvents(const std::vector<std::string>& ids);

    mutable std::mutex result_mutex_;
    UrgencyUpdateResult last_result_;
};

} // namespace wtl::workers

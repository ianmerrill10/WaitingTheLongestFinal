/**
 * @file PredictiveMLModule.cc
 * @brief Implementation of the PredictiveMLModule
 * @see PredictiveMLModule.h for documentation
 */

#include "PredictiveMLModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>

namespace wtl::modules::intervention {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string riskTypeToString(RiskType type) {
    switch (type) {
        case RiskType::EUTHANASIA: return "euthanasia";
        case RiskType::LONG_STAY: return "long_stay";
        case RiskType::OVERLOOKED: return "overlooked";
        case RiskType::RETURN: return "return";
        case RiskType::ADOPTION_FAILURE: return "adoption_failure";
        default: return "unknown";
    }
}

RiskType stringToRiskType(const std::string& str) {
    if (str == "euthanasia") return RiskType::EUTHANASIA;
    if (str == "long_stay") return RiskType::LONG_STAY;
    if (str == "overlooked") return RiskType::OVERLOOKED;
    if (str == "return") return RiskType::RETURN;
    if (str == "adoption_failure") return RiskType::ADOPTION_FAILURE;
    return RiskType::EUTHANASIA;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool PredictiveMLModule::initialize() {
    std::cout << "[PredictiveMLModule] Initializing..." << std::endl;

    // Initialize feature weights
    initializeFeatureWeights();

    // Initialize model metrics
    for (auto type : {RiskType::EUTHANASIA, RiskType::LONG_STAY, RiskType::OVERLOOKED,
                       RiskType::RETURN, RiskType::ADOPTION_FAILURE}) {
        ModelMetrics metrics;
        metrics.accuracy = 0.85;
        metrics.precision = 0.82;
        metrics.recall = 0.88;
        metrics.f1_score = 0.85;
        metrics.total_predictions = 0;
        metrics.true_positives = 0;
        metrics.false_positives = 0;
        metrics.true_negatives = 0;
        metrics.false_negatives = 0;
        metrics.model_version = "1.0.0";
        model_metrics_[type] = metrics;
    }

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto created_id = event_bus.subscribe(
        core::EventType::DOG_CREATED,
        [this](const core::Event& event) {
            handleDogCreated(event);
        },
        "predictive_ml_created_handler"
    );
    subscription_ids_.push_back(created_id);

    auto updated_id = event_bus.subscribe(
        core::EventType::DOG_UPDATED,
        [this](const core::Event& event) {
            handleDogUpdated(event);
        },
        "predictive_ml_updated_handler"
    );
    subscription_ids_.push_back(updated_id);

    auto request_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "prediction_request") {
                handlePredictionRequest(event);
            }
        },
        "predictive_ml_request_handler"
    );
    subscription_ids_.push_back(request_id);

    enabled_ = true;
    std::cout << "[PredictiveMLModule] Initialization complete." << std::endl;
    std::cout << "[PredictiveMLModule] Risk threshold: " << risk_threshold_ << std::endl;
    std::cout << "[PredictiveMLModule] Auto-alert high risk: "
              << (auto_alert_high_risk_ ? "enabled" : "disabled") << std::endl;

    return true;
}

void PredictiveMLModule::shutdown() {
    std::cout << "[PredictiveMLModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[PredictiveMLModule] Shutdown complete." << std::endl;
}

void PredictiveMLModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool PredictiveMLModule::isHealthy() const {
    return enabled_;
}

Json::Value PredictiveMLModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["risk_threshold"] = risk_threshold_;
    status["auto_alert_high_risk"] = auto_alert_high_risk_;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["total_predictions"] = static_cast<int>(predictions_.size());
    status["dogs_tracked"] = static_cast<int>(dog_predictions_.size());

    return status;
}

Json::Value PredictiveMLModule::getMetrics() const {
    Json::Value metrics;
    metrics["total_predictions"] = static_cast<Json::Int64>(total_predictions_.load());
    metrics["high_risk_identified"] = static_cast<Json::Int64>(high_risk_identified_.load());
    metrics["alerts_sent"] = static_cast<Json::Int64>(alerts_sent_.load());
    metrics["interventions_triggered"] = static_cast<Json::Int64>(interventions_triggered_.load());
    metrics["outcomes_recorded"] = static_cast<Json::Int64>(outcomes_recorded_.load());

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void PredictiveMLModule::configure(const Json::Value& config) {
    if (config.isMember("risk_threshold")) {
        risk_threshold_ = config["risk_threshold"].asDouble();
    }
    if (config.isMember("auto_alert_high_risk")) {
        auto_alert_high_risk_ = config["auto_alert_high_risk"].asBool();
    }
    if (config.isMember("prediction_refresh_hours")) {
        prediction_refresh_hours_ = config["prediction_refresh_hours"].asInt();
    }
}

Json::Value PredictiveMLModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["risk_threshold"] = 0.7;
    config["auto_alert_high_risk"] = true;
    config["prediction_refresh_hours"] = 24;
    return config;
}

// ============================================================================
// RISK PREDICTION API
// ============================================================================

std::vector<RiskPrediction> PredictiveMLModule::predictAllRisks(const DogFeatures& features) {
    std::vector<RiskPrediction> predictions;

    for (auto type : {RiskType::EUTHANASIA, RiskType::LONG_STAY, RiskType::OVERLOOKED,
                       RiskType::RETURN, RiskType::ADOPTION_FAILURE}) {
        predictions.push_back(predictRisk(features, type));
    }

    return predictions;
}

RiskPrediction PredictiveMLModule::predictRisk(const DogFeatures& features, RiskType risk_type) {
    RiskPrediction prediction;
    prediction.id = generatePredictionId();
    prediction.dog_id = features.dog_id;
    prediction.risk_type = risk_type;
    prediction.alert_sent = false;

    // Calculate probability based on risk type
    switch (risk_type) {
        case RiskType::EUTHANASIA:
            prediction.probability = calculateEuthanasiaRisk(features);
            break;
        case RiskType::LONG_STAY:
            prediction.probability = calculateLongStayRisk(features);
            break;
        case RiskType::OVERLOOKED:
            prediction.probability = calculateOverlookedRisk(features);
            break;
        case RiskType::RETURN:
            prediction.probability = calculateReturnRisk(features);
            break;
        case RiskType::ADOPTION_FAILURE:
            prediction.probability = calculateAdoptionFailureRisk(features);
            break;
    }

    prediction.risk_level = probabilityToRiskLevel(prediction.probability);
    prediction.contributing_factors = identifyContributingFactors(features, risk_type,
                                                                    prediction.probability);
    prediction.recommended_interventions = generateInterventions(features, risk_type,
                                                                   prediction.probability);

    // Estimate days until risk
    if (risk_type == RiskType::EUTHANASIA && features.is_kill_shelter) {
        prediction.days_until_risk = std::max(1, 30 - features.current_wait_days);
    } else if (risk_type == RiskType::LONG_STAY) {
        prediction.days_until_risk = std::max(1, 90 - features.current_wait_days);
    } else {
        prediction.days_until_risk = 30; // Default estimate
    }

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    prediction.predicted_at = ss.str();

    // Store prediction
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        predictions_[prediction.id] = prediction;
        dog_predictions_[features.dog_id].push_back(prediction.id);
    }

    total_predictions_++;

    // Check for high risk
    if (prediction.probability >= risk_threshold_) {
        high_risk_identified_++;
        if (auto_alert_high_risk_) {
            alertHighRiskDog(prediction, features);
        }
    }

    return prediction;
}

std::vector<RiskPrediction> PredictiveMLModule::getHighRiskPredictions(double min_probability) {
    std::vector<RiskPrediction> high_risk;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, prediction] : predictions_) {
        if (prediction.probability >= min_probability) {
            high_risk.push_back(prediction);
        }
    }

    // Sort by probability descending
    std::sort(high_risk.begin(), high_risk.end(),
        [](const RiskPrediction& a, const RiskPrediction& b) {
            return a.probability > b.probability;
        });

    return high_risk;
}

std::vector<RiskPrediction> PredictiveMLModule::getPredictionsForDog(const std::string& dog_id) {
    std::vector<RiskPrediction> result;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = dog_predictions_.find(dog_id);
    if (it != dog_predictions_.end()) {
        for (const auto& pred_id : it->second) {
            auto pred_it = predictions_.find(pred_id);
            if (pred_it != predictions_.end()) {
                result.push_back(pred_it->second);
            }
        }
    }

    return result;
}

std::vector<std::string> PredictiveMLModule::getRecommendedInterventions(const std::string& dog_id) {
    std::vector<std::string> all_interventions;

    auto predictions = getPredictionsForDog(dog_id);
    for (const auto& pred : predictions) {
        if (pred.probability >= 0.5) { // Include medium and high risk
            for (const auto& intervention : pred.recommended_interventions) {
                if (std::find(all_interventions.begin(), all_interventions.end(),
                              intervention) == all_interventions.end()) {
                    all_interventions.push_back(intervention);
                }
            }
        }
    }

    return all_interventions;
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

int PredictiveMLModule::runShelterPredictions(const std::string& shelter_id) {
    // This would typically query the database for all dogs in the shelter
    // For now, return 0 as a placeholder
    std::cout << "[PredictiveMLModule] Running predictions for shelter: "
              << shelter_id << std::endl;
    return 0;
}

int PredictiveMLModule::runCriticalPredictions() {
    // This would run predictions for all critical dogs
    std::cout << "[PredictiveMLModule] Running critical predictions" << std::endl;
    return 0;
}

Json::Value PredictiveMLModule::getShelterRiskSummary(const std::string& shelter_id) {
    Json::Value summary;
    summary["shelter_id"] = shelter_id;
    summary["total_dogs"] = 0;
    summary["high_risk_count"] = 0;
    summary["critical_count"] = 0;

    // Would aggregate predictions by shelter
    return summary;
}

// ============================================================================
// MODEL MANAGEMENT
// ============================================================================

ModelMetrics PredictiveMLModule::getModelMetrics(RiskType risk_type) {
    auto it = model_metrics_.find(risk_type);
    if (it != model_metrics_.end()) {
        return it->second;
    }

    ModelMetrics empty;
    return empty;
}

void PredictiveMLModule::recordOutcome(const std::string& prediction_id, bool actual_outcome) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = predictions_.find(prediction_id);
    if (it == predictions_.end()) return;

    const auto& pred = it->second;
    bool predicted_positive = pred.probability >= risk_threshold_;

    // Update model metrics
    auto metrics_it = model_metrics_.find(pred.risk_type);
    if (metrics_it != model_metrics_.end()) {
        auto& metrics = metrics_it->second;
        metrics.total_predictions++;

        if (predicted_positive && actual_outcome) {
            metrics.true_positives++;
        } else if (predicted_positive && !actual_outcome) {
            metrics.false_positives++;
        } else if (!predicted_positive && actual_outcome) {
            metrics.false_negatives++;
        } else {
            metrics.true_negatives++;
        }

        // Recalculate metrics
        int total = metrics.true_positives + metrics.false_positives +
                    metrics.true_negatives + metrics.false_negatives;
        if (total > 0) {
            metrics.accuracy = static_cast<double>(metrics.true_positives + metrics.true_negatives) / total;
        }
        if (metrics.true_positives + metrics.false_positives > 0) {
            metrics.precision = static_cast<double>(metrics.true_positives) /
                               (metrics.true_positives + metrics.false_positives);
        }
        if (metrics.true_positives + metrics.false_negatives > 0) {
            metrics.recall = static_cast<double>(metrics.true_positives) /
                            (metrics.true_positives + metrics.false_negatives);
        }
        if (metrics.precision + metrics.recall > 0) {
            metrics.f1_score = 2 * (metrics.precision * metrics.recall) /
                              (metrics.precision + metrics.recall);
        }
    }

    outcomes_recorded_++;
}

Json::Value PredictiveMLModule::getFeatureImportance(RiskType risk_type) {
    Json::Value importance(Json::arrayValue);

    auto it = feature_weights_.find(risk_type);
    if (it == feature_weights_.end()) {
        return importance;
    }

    // Convert to sorted array
    std::vector<std::pair<std::string, double>> sorted_features(
        it->second.begin(), it->second.end());

    std::sort(sorted_features.begin(), sorted_features.end(),
        [](const auto& a, const auto& b) {
            return std::abs(a.second) > std::abs(b.second);
        });

    for (const auto& [feature, weight] : sorted_features) {
        Json::Value item;
        item["feature"] = feature;
        item["importance"] = std::abs(weight);
        item["direction"] = weight > 0 ? "increases_risk" : "decreases_risk";
        importance.append(item);
    }

    return importance;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void PredictiveMLModule::handleDogCreated(const core::Event& event) {
    DogFeatures features;
    features.dog_id = event.data.get("dog_id", "").asString();
    features.breed = event.data.get("breed", "Mixed").asString();
    features.age_months = event.data.get("age_months", 24).asInt();
    features.gender = event.data.get("gender", "unknown").asString();
    features.color = event.data.get("color", "").asString();
    features.size = event.data.get("size", "medium").asString();
    features.is_kill_shelter = event.data.get("is_kill_shelter", false).asBool();
    features.current_wait_days = 0;

    // Derive features
    features.is_senior = features.age_months > 84; // > 7 years
    features.is_black = features.color.find("black") != std::string::npos;
    features.is_bully_breed = features.breed.find("pit") != std::string::npos ||
                               features.breed.find("bull") != std::string::npos;

    if (!features.dog_id.empty()) {
        predictAllRisks(features);
    }
}

void PredictiveMLModule::handleDogUpdated(const core::Event& event) {
    // Re-run predictions when dog data is updated
    std::string dog_id = event.data.get("dog_id", "").asString();
    if (!dog_id.empty()) {
        // Clear old predictions
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = dog_predictions_.find(dog_id);
        if (it != dog_predictions_.end()) {
            for (const auto& pred_id : it->second) {
                predictions_.erase(pred_id);
            }
            it->second.clear();
        }
    }
}

void PredictiveMLModule::handlePredictionRequest(const core::Event& event) {
    DogFeatures features;
    features.dog_id = event.data.get("dog_id", "").asString();
    features.breed = event.data.get("breed", "").asString();
    features.age_months = event.data.get("age_months", 0).asInt();
    features.current_wait_days = event.data.get("wait_days", 0).asInt();
    features.is_kill_shelter = event.data.get("is_kill_shelter", false).asBool();

    if (!features.dog_id.empty()) {
        predictAllRisks(features);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

std::string PredictiveMLModule::generatePredictionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "PRD-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

double PredictiveMLModule::calculateEuthanasiaRisk(const DogFeatures& features) {
    double risk = 0.1; // Base risk

    // Kill shelter is the biggest factor
    if (features.is_kill_shelter) risk += 0.3;

    // Shelter capacity
    if (features.shelter_capacity_percent > 90) risk += 0.2;
    else if (features.shelter_capacity_percent > 80) risk += 0.1;

    // Wait time
    if (features.current_wait_days > 60) risk += 0.15;
    else if (features.current_wait_days > 30) risk += 0.1;

    // Breed bias
    if (features.is_bully_breed) risk += 0.1;

    // Age
    if (features.is_senior) risk += 0.1;

    // Special needs
    if (features.special_needs) risk += 0.1;

    // Black dog syndrome
    if (features.is_black) risk += 0.05;

    return std::min(1.0, std::max(0.0, risk));
}

double PredictiveMLModule::calculateLongStayRisk(const DogFeatures& features) {
    double risk = 0.15;

    // Current wait time
    if (features.current_wait_days > 60) risk += 0.25;
    else if (features.current_wait_days > 30) risk += 0.15;

    // Age
    if (features.is_senior) risk += 0.15;
    else if (features.age_months < 12) risk -= 0.1; // Puppies adopted faster

    // Breed factors
    if (features.is_bully_breed) risk += 0.1;
    if (features.is_mixed_breed) risk += 0.05;

    // Black dog syndrome
    if (features.is_black) risk += 0.1;

    // Size
    if (features.size == "xlarge") risk += 0.1;
    else if (features.size == "small") risk -= 0.05;

    // Photo quality
    if (features.photo_count == 0) risk += 0.15;
    else if (features.photo_count < 3) risk += 0.05;

    // Description quality
    if (!features.has_description) risk += 0.1;
    else if (features.description_length < 100) risk += 0.05;

    return std::min(1.0, std::max(0.0, risk));
}

double PredictiveMLModule::calculateOverlookedRisk(const DogFeatures& features) {
    double risk = 0.2;

    // Black dog syndrome
    if (features.is_black) risk += 0.2;

    // Senior dogs
    if (features.is_senior) risk += 0.2;

    // Bully breeds
    if (features.is_bully_breed) risk += 0.15;

    // Mixed breeds vs purebreds
    if (features.is_mixed_breed) risk += 0.05;

    // Larger dogs
    if (features.size == "xlarge" || features.size == "large") risk += 0.1;

    // Poor marketing
    if (features.photo_count == 0) risk += 0.15;
    if (!features.has_description) risk += 0.1;
    if (features.video_count == 0) risk += 0.05;

    return std::min(1.0, std::max(0.0, risk));
}

double PredictiveMLModule::calculateReturnRisk(const DogFeatures& features) {
    double risk = 0.08; // Average return rate

    // Prior returns
    if (features.prior_returns > 0) risk += features.prior_returns * 0.15;

    // Behavioral issues
    if (!features.behavioral_notes.empty()) risk += 0.1;

    // Medical issues
    if (features.special_needs) risk += 0.1;

    // Not good with other pets/kids
    if (!features.good_with_dogs) risk += 0.05;
    if (!features.good_with_cats) risk += 0.03;
    if (!features.good_with_kids) risk += 0.05;

    // Not house trained
    if (!features.house_trained) risk += 0.08;

    // Age factors
    if (features.age_months < 12) risk += 0.05; // Puppy challenges
    if (features.is_senior) risk += 0.03;

    return std::min(1.0, std::max(0.0, risk));
}

double PredictiveMLModule::calculateAdoptionFailureRisk(const DogFeatures& features) {
    // Combination of long stay and overlooked risks
    double long_stay = calculateLongStayRisk(features);
    double overlooked = calculateOverlookedRisk(features);

    double risk = (long_stay + overlooked) / 2;

    // Additional factors
    if (features.is_kill_shelter) risk += 0.1;
    if (features.shelter_adoption_rate < 50) risk += 0.1;

    return std::min(1.0, std::max(0.0, risk));
}

std::string PredictiveMLModule::probabilityToRiskLevel(double probability) {
    if (probability >= 0.8) return "critical";
    if (probability >= 0.6) return "high";
    if (probability >= 0.4) return "medium";
    return "low";
}

std::vector<std::string> PredictiveMLModule::identifyContributingFactors(
    const DogFeatures& features,
    RiskType risk_type,
    double probability) {

    std::vector<std::string> factors;

    if (features.is_kill_shelter) {
        factors.push_back("At kill shelter");
    }
    if (features.is_senior) {
        factors.push_back("Senior dog (7+ years)");
    }
    if (features.is_black) {
        factors.push_back("Black dog syndrome");
    }
    if (features.is_bully_breed) {
        factors.push_back("Bully breed bias");
    }
    if (features.current_wait_days > 30) {
        factors.push_back("Already waiting " + std::to_string(features.current_wait_days) + " days");
    }
    if (features.photo_count < 3) {
        factors.push_back("Insufficient photos");
    }
    if (!features.has_description || features.description_length < 100) {
        factors.push_back("Incomplete profile");
    }
    if (features.shelter_capacity_percent > 80) {
        factors.push_back("Shelter at high capacity");
    }
    if (features.prior_returns > 0) {
        factors.push_back("Previous adoption returns");
    }
    if (features.special_needs) {
        factors.push_back("Special needs dog");
    }

    return factors;
}

std::vector<std::string> PredictiveMLModule::generateInterventions(
    const DogFeatures& features,
    RiskType risk_type,
    double probability) {

    std::vector<std::string> interventions;

    // Photo/profile improvements
    if (features.photo_count < 3) {
        interventions.push_back("Take more high-quality photos");
    }
    if (!features.has_description || features.description_length < 100) {
        interventions.push_back("Write compelling bio with AI assistance");
    }
    if (features.video_count == 0) {
        interventions.push_back("Create video content for social media");
    }

    // Marketing for overlooked dogs
    if (features.is_black || features.is_senior || features.is_bully_breed) {
        interventions.push_back("Feature in 'Overlooked Angels' campaign");
        interventions.push_back("Create myth-busting educational content");
    }

    // Urgent actions
    if (features.is_kill_shelter && probability > 0.5) {
        interventions.push_back("Contact rescue network immediately");
        interventions.push_back("Cross-post to all social media platforms");
        interventions.push_back("Request emergency foster");
    }

    // Transport
    if (probability > 0.6) {
        interventions.push_back("Consider transfer to lower-kill area");
        interventions.push_back("Add to transport network waitlist");
    }

    // General
    interventions.push_back("Increase social media exposure");
    interventions.push_back("Send to email subscribers interested in this type");

    return interventions;
}

void PredictiveMLModule::initializeFeatureWeights() {
    // Euthanasia risk weights
    feature_weights_[RiskType::EUTHANASIA] = {
        {"is_kill_shelter", 0.35},
        {"shelter_capacity", 0.20},
        {"wait_days", 0.15},
        {"is_bully_breed", 0.10},
        {"is_senior", 0.10},
        {"special_needs", 0.05},
        {"is_black", 0.05}
    };

    // Long stay risk weights
    feature_weights_[RiskType::LONG_STAY] = {
        {"is_black", 0.20},
        {"is_senior", 0.15},
        {"photo_count", -0.15},
        {"description_quality", -0.10},
        {"is_bully_breed", 0.10},
        {"size_large", 0.10},
        {"is_puppy", -0.10}
    };

    // Similar for other risk types...
}

void PredictiveMLModule::alertHighRiskDog(const RiskPrediction& prediction,
                                            const DogFeatures& features) {
    core::Event alert;
    alert.type = core::EventType::CUSTOM;
    alert.data["type"] = "high_risk_dog_alert";
    alert.data["dog_id"] = features.dog_id;
    alert.data["risk_type"] = riskTypeToString(prediction.risk_type);
    alert.data["probability"] = prediction.probability;
    alert.data["risk_level"] = prediction.risk_level;
    alert.data["days_until_risk"] = prediction.days_until_risk;

    Json::Value factors(Json::arrayValue);
    for (const auto& factor : prediction.contributing_factors) {
        factors.append(factor);
    }
    alert.data["contributing_factors"] = factors;

    Json::Value interventions(Json::arrayValue);
    for (const auto& intervention : prediction.recommended_interventions) {
        interventions.append(intervention);
    }
    alert.data["recommended_interventions"] = interventions;

    core::EventBus::getInstance().publishAsync(alert);
    alerts_sent_++;

    // Update prediction
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = predictions_.find(prediction.id);
        if (it != predictions_.end()) {
            it->second.alert_sent = true;
        }
    }

    std::cout << "[PredictiveMLModule] HIGH RISK ALERT: Dog " << features.dog_id
              << " - " << riskTypeToString(prediction.risk_type)
              << " risk at " << (prediction.probability * 100) << "%" << std::endl;
}

} // namespace wtl::modules::intervention

/**
 * @file PredictiveMLModule.h
 * @brief Module for predictive machine learning to identify at-risk dogs
 *
 * PURPOSE:
 * Uses machine learning algorithms to predict which dogs are at highest
 * risk of long shelter stays, euthanasia, or adoption failure. Enables
 * proactive intervention before situations become critical.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "predictive_ml": {
 *             "enabled": true,
 *             "risk_threshold": 0.7,
 *             "auto_alert_high_risk": true
 *         }
 *     }
 * }
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - EventBus.h (event subscription)
 *
 * @author WaitingTheLongest Integration Agent
 * @date 2024-01-28
 */

#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @enum RiskType
 * @brief Types of risks that can be predicted
 */
enum class RiskType {
    EUTHANASIA,         // Risk of being euthanized
    LONG_STAY,          // Risk of staying > 90 days
    OVERLOOKED,         // Risk of being overlooked (low views)
    RETURN,             // Risk of being returned after adoption
    ADOPTION_FAILURE    // Risk of failing to be adopted
};

/**
 * @struct DogFeatures
 * @brief Features used for ML prediction
 */
struct DogFeatures {
    std::string dog_id;
    std::string breed;
    std::string size;
    std::string color;
    int age_months;
    std::string gender;
    bool is_mixed_breed;
    bool is_senior; // > 7 years
    bool is_black;
    bool is_bully_breed;
    int photo_count;
    int video_count;
    bool has_description;
    int description_length;
    bool good_with_dogs;
    bool good_with_cats;
    bool good_with_kids;
    bool house_trained;
    bool special_needs;
    std::string medical_notes;
    std::string behavioral_notes;
    int current_wait_days;
    int shelter_capacity_percent;
    bool is_kill_shelter;
    int prior_returns;
    double shelter_adoption_rate;
};

/**
 * @struct RiskPrediction
 * @brief A risk prediction for a dog
 */
struct RiskPrediction {
    std::string id;
    std::string dog_id;
    RiskType risk_type;
    double probability; // 0.0 - 1.0
    std::string risk_level; // low, medium, high, critical
    std::vector<std::string> contributing_factors;
    std::vector<std::string> recommended_interventions;
    std::string predicted_at;
    int days_until_risk; // Estimated days until risk materializes
    bool alert_sent;
};

/**
 * @struct ModelMetrics
 * @brief Performance metrics for the ML model
 */
struct ModelMetrics {
    double accuracy = 0.0;
    double precision = 0.0;
    double recall = 0.0;
    double f1_score = 0.0;
    int total_predictions = 0;
    int true_positives = 0;
    int false_positives = 0;
    int true_negatives = 0;
    int false_negatives = 0;
    std::string last_trained;
    std::string model_version;
};

/**
 * @class PredictiveMLModule
 * @brief Module for predictive ML-based risk assessment
 *
 * Features:
 * - Multi-factor risk scoring
 * - Euthanasia risk prediction
 * - Long-stay prediction
 * - Overlooked dog identification
 * - Return risk assessment
 * - Automated intervention recommendations
 * - Model performance tracking
 */
class PredictiveMLModule : public IModule {
public:
    PredictiveMLModule() = default;
    ~PredictiveMLModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "predictive_ml"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Predictive ML for identifying at-risk dogs and recommending interventions";
    }
    std::vector<std::string> getDependencies() const override {
        return {}; // No dependencies
    }

    // ========================================================================
    // IModule INTERFACE - STATUS
    // ========================================================================

    bool isEnabled() const override { return enabled_; }
    bool isHealthy() const override;
    Json::Value getStatus() const override;
    Json::Value getMetrics() const override;

    // ========================================================================
    // IModule INTERFACE - CONFIGURATION
    // ========================================================================

    void configure(const Json::Value& config) override;
    Json::Value getDefaultConfig() const override;

    // ========================================================================
    // RISK PREDICTION API
    // ========================================================================

    /**
     * @brief Predict all risk types for a dog
     * @param features The dog's features
     * @return List of predictions for each risk type
     */
    std::vector<RiskPrediction> predictAllRisks(const DogFeatures& features);

    /**
     * @brief Predict specific risk for a dog
     * @param features The dog's features
     * @param risk_type The type of risk to predict
     * @return The risk prediction
     */
    RiskPrediction predictRisk(const DogFeatures& features, RiskType risk_type);

    /**
     * @brief Get all high-risk predictions
     * @param min_probability Minimum probability threshold
     * @return List of high-risk predictions
     */
    std::vector<RiskPrediction> getHighRiskPredictions(double min_probability = 0.7);

    /**
     * @brief Get predictions for a specific dog
     * @param dog_id The dog ID
     * @return List of predictions
     */
    std::vector<RiskPrediction> getPredictionsForDog(const std::string& dog_id);

    /**
     * @brief Get recommended interventions for a dog
     * @param dog_id The dog ID
     * @return List of recommended interventions
     */
    std::vector<std::string> getRecommendedInterventions(const std::string& dog_id);

    // ========================================================================
    // BATCH OPERATIONS
    // ========================================================================

    /**
     * @brief Run predictions for all dogs in a shelter
     * @param shelter_id The shelter ID
     * @return Number of dogs analyzed
     */
    int runShelterPredictions(const std::string& shelter_id);

    /**
     * @brief Run predictions for all critical dogs
     * @return Number of dogs analyzed
     */
    int runCriticalPredictions();

    /**
     * @brief Get shelter risk summary
     * @param shelter_id The shelter ID
     * @return Risk summary statistics
     */
    Json::Value getShelterRiskSummary(const std::string& shelter_id);

    // ========================================================================
    // MODEL MANAGEMENT
    // ========================================================================

    /**
     * @brief Get model performance metrics
     * @param risk_type The risk type
     * @return Model metrics
     */
    ModelMetrics getModelMetrics(RiskType risk_type);

    /**
     * @brief Record a prediction outcome (for model improvement)
     * @param prediction_id The prediction ID
     * @param actual_outcome What actually happened
     */
    void recordOutcome(const std::string& prediction_id, bool actual_outcome);

    /**
     * @brief Get feature importance rankings
     * @param risk_type The risk type
     * @return Feature importance as JSON
     */
    Json::Value getFeatureImportance(RiskType risk_type);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogCreated(const core::Event& event);
    void handleDogUpdated(const core::Event& event);
    void handlePredictionRequest(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    std::string generatePredictionId();
    double calculateEuthanasiaRisk(const DogFeatures& features);
    double calculateLongStayRisk(const DogFeatures& features);
    double calculateOverlookedRisk(const DogFeatures& features);
    double calculateReturnRisk(const DogFeatures& features);
    double calculateAdoptionFailureRisk(const DogFeatures& features);
    std::string probabilityToRiskLevel(double probability);
    std::vector<std::string> identifyContributingFactors(const DogFeatures& features,
                                                          RiskType risk_type,
                                                          double probability);
    std::vector<std::string> generateInterventions(const DogFeatures& features,
                                                     RiskType risk_type,
                                                     double probability);
    void initializeFeatureWeights();
    void alertHighRiskDog(const RiskPrediction& prediction, const DogFeatures& features);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    double risk_threshold_ = 0.7;
    bool auto_alert_high_risk_ = true;
    int prediction_refresh_hours_ = 24;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Feature weights for each risk type
    std::map<RiskType, std::map<std::string, double>> feature_weights_;

    // Prediction storage
    std::map<std::string, RiskPrediction> predictions_;
    std::map<std::string, std::vector<std::string>> dog_predictions_; // dog_id -> prediction_ids
    mutable std::mutex data_mutex_;

    // Model metrics tracking
    std::map<RiskType, ModelMetrics> model_metrics_;

    // Metrics
    std::atomic<int64_t> total_predictions_{0};
    std::atomic<int64_t> high_risk_identified_{0};
    std::atomic<int64_t> alerts_sent_{0};
    std::atomic<int64_t> interventions_triggered_{0};
    std::atomic<int64_t> outcomes_recorded_{0};
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string riskTypeToString(RiskType type);
RiskType stringToRiskType(const std::string& str);

} // namespace wtl::modules::intervention

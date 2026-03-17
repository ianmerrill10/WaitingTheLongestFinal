/**
 * @file FosterApplicationHandler.h
 * @brief Handler for processing foster applications workflow
 *
 * PURPOSE:
 * Provides workflow management for foster applications, including validation,
 * submission, approval/rejection processing, and notifications. Ensures
 * applications are properly validated and state transitions are handled correctly.
 *
 * USAGE:
 * auto& handler = FosterApplicationHandler::getInstance();
 * auto result = handler.submit(user_id, dog_id, message);
 * if (result.success) {
 *     // Application submitted
 * }
 *
 * DEPENDENCIES:
 * - FosterService (application persistence)
 * - UserService (user validation)
 * - DogService (dog availability)
 * - FosterHome, FosterApplication models
 *
 * MODIFICATION GUIDE:
 * - Add new validation rules in the validation methods
 * - Add new notification types in the notification methods
 * - Update workflow in submit() and process() as needed
 *
 * @author Agent 9 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>
#include <string>

// Project includes
#include "core/models/FosterPlacement.h"

namespace wtl::core::services {

/**
 * @class FosterApplicationHandler
 * @brief Singleton handler for foster application workflow
 *
 * Manages the complete lifecycle of foster applications from submission
 * through approval or rejection. Includes validation, state management,
 * and notification triggers.
 */
class FosterApplicationHandler {
public:
    // ========================================================================
    // RESULT STRUCTURE
    // ========================================================================

    /**
     * @struct ApplicationResult
     * @brief Result of an application operation
     */
    struct ApplicationResult {
        bool success;                                         ///< Whether operation succeeded
        std::optional<models::FosterApplication> application; ///< The application if successful
        std::optional<std::string> error;                     ///< Error message if failed

        /**
         * @brief Create a successful result
         * @param app The application
         * @return ApplicationResult with success=true
         */
        static ApplicationResult Success(const models::FosterApplication& app) {
            return {true, app, std::nullopt};
        }

        /**
         * @brief Create a failed result
         * @param message Error message
         * @return ApplicationResult with success=false
         */
        static ApplicationResult Failure(const std::string& message) {
            return {false, std::nullopt, message};
        }
    };

    // ========================================================================
    // SINGLETON
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the FosterApplicationHandler singleton
     */
    static FosterApplicationHandler& getInstance();

    // Delete copy/move constructors and assignment operators
    FosterApplicationHandler(const FosterApplicationHandler&) = delete;
    FosterApplicationHandler& operator=(const FosterApplicationHandler&) = delete;
    FosterApplicationHandler(FosterApplicationHandler&&) = delete;
    FosterApplicationHandler& operator=(FosterApplicationHandler&&) = delete;

    // ========================================================================
    // APPLICATION WORKFLOW
    // ========================================================================

    /**
     * @brief Submit a new foster application
     * @param user_id The applying user's UUID
     * @param dog_id The dog's UUID
     * @param message Message from applicant
     * @return ApplicationResult with success status and application or error
     *
     * Validates:
     * - User exists and can foster
     * - Dog exists and is available for foster
     * - Foster home has capacity
     * - No duplicate pending application exists
     */
    ApplicationResult submit(const std::string& user_id,
                             const std::string& dog_id,
                             const std::string& message);

    /**
     * @brief Process an application (approve or reject)
     * @param app_id The application UUID
     * @param approve Whether to approve (true) or reject (false)
     * @param response Response message to applicant
     * @return ApplicationResult with updated application or error
     *
     * Validates:
     * - Application exists and is pending
     * - Dog is still available (for approval)
     * - Foster home still has capacity (for approval)
     */
    ApplicationResult process(const std::string& app_id,
                              bool approve,
                              const std::string& response);

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * @brief Check if a user can foster
     * @param user_id The user's UUID
     * @return true if user exists and has a valid foster profile
     *
     * Checks:
     * - User exists and is active
     * - User has registered foster home
     * - Foster home is active and verified (if required)
     */
    bool canUserFoster(const std::string& user_id);

    /**
     * @brief Check if a dog is available for foster
     * @param dog_id The dog's UUID
     * @return true if dog can be fostered
     *
     * Checks:
     * - Dog exists
     * - Dog status is "adoptable"
     * - Dog is not already in foster
     */
    bool isDogAvailableForFoster(const std::string& dog_id);

    /**
     * @brief Check if a foster home has capacity
     * @param foster_home_id The foster home's UUID
     * @return true if foster home can accept more dogs
     */
    bool hasCapacity(const std::string& foster_home_id);

    /**
     * @brief Check if duplicate application exists
     * @param foster_home_id The foster home's UUID
     * @param dog_id The dog's UUID
     * @return true if pending application already exists
     */
    bool hasPendingApplication(const std::string& foster_home_id,
                                const std::string& dog_id);

    // ========================================================================
    // NOTIFICATIONS (PLACEHOLDER)
    // ========================================================================

    /**
     * @brief Send notification to applicant about application status
     * @param app The application
     * @param status New status (submitted, approved, rejected)
     *
     * Placeholder for future notification integration.
     * Would send email/push notification to foster applicant.
     */
    void notifyApplicant(const models::FosterApplication& app,
                         const std::string& status);

    /**
     * @brief Send notification to shelter about new application
     * @param app The application
     *
     * Placeholder for future notification integration.
     * Would send notification to shelter staff.
     */
    void notifyShelter(const models::FosterApplication& app);

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================

    FosterApplicationHandler() = default;
    ~FosterApplicationHandler() = default;

    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Get foster home ID for a user
     * @param user_id The user's UUID
     * @return Foster home ID if exists, nullopt otherwise
     */
    std::optional<std::string> getFosterHomeIdForUser(const std::string& user_id);

    /**
     * @brief Get shelter ID for a dog
     * @param dog_id The dog's UUID
     * @return Shelter ID if exists, nullopt otherwise
     */
    std::optional<std::string> getShelterIdForDog(const std::string& dog_id);
};

} // namespace wtl::core::services

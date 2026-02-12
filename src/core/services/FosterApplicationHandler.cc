/**
 * @file FosterApplicationHandler.cc
 * @brief Implementation of FosterApplicationHandler
 * @see FosterApplicationHandler.h for documentation
 */

#include "core/services/FosterApplicationHandler.h"

#include "core/services/FosterService.h"
#include "core/services/UserService.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::services {

// ============================================================================
// SINGLETON
// ============================================================================

FosterApplicationHandler& FosterApplicationHandler::getInstance() {
    static FosterApplicationHandler instance;
    return instance;
}

// ============================================================================
// APPLICATION WORKFLOW
// ============================================================================

FosterApplicationHandler::ApplicationResult FosterApplicationHandler::submit(
    const std::string& user_id,
    const std::string& dog_id,
    const std::string& message) {

    // Validate user can foster
    if (!canUserFoster(user_id)) {
        return ApplicationResult::Failure(
            "User cannot foster. Please complete foster home registration."
        );
    }

    // Get foster home ID
    auto foster_home_id = getFosterHomeIdForUser(user_id);
    if (!foster_home_id.has_value()) {
        return ApplicationResult::Failure(
            "Foster home not found for user. Please register as a foster."
        );
    }

    // Validate dog availability
    if (!isDogAvailableForFoster(dog_id)) {
        return ApplicationResult::Failure(
            "This dog is not currently available for fostering."
        );
    }

    // Validate capacity
    if (!hasCapacity(foster_home_id.value())) {
        return ApplicationResult::Failure(
            "Your foster home is at capacity. Please update your profile or end a current placement."
        );
    }

    // Check for duplicate application
    if (hasPendingApplication(foster_home_id.value(), dog_id)) {
        return ApplicationResult::Failure(
            "You already have a pending application for this dog."
        );
    }

    try {
        // Submit the application
        auto app = FosterService::getInstance().submitApplication(
            foster_home_id.value(),
            dog_id,
            message
        );

        // Notify relevant parties
        notifyApplicant(app, "submitted");
        notifyShelter(app);

        return ApplicationResult::Success(app);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::BUSINESS_LOGIC,
            "Failed to submit foster application: " + std::string(e.what()),
            {{"user_id", user_id}, {"dog_id", dog_id}}
        );
        return ApplicationResult::Failure(
            "An error occurred while submitting your application. Please try again."
        );
    }
}

FosterApplicationHandler::ApplicationResult FosterApplicationHandler::process(
    const std::string& app_id,
    bool approve,
    const std::string& response) {

    // Get the application
    auto app_opt = FosterService::getInstance().findApplicationById(app_id);
    if (!app_opt.has_value()) {
        return ApplicationResult::Failure("Application not found.");
    }

    auto app = app_opt.value();

    // Validate application is pending
    if (!app.isPending()) {
        return ApplicationResult::Failure(
            "This application has already been processed."
        );
    }

    if (approve) {
        // Additional validation for approval
        if (!isDogAvailableForFoster(app.dog_id)) {
            return ApplicationResult::Failure(
                "This dog is no longer available for fostering."
            );
        }

        if (!hasCapacity(app.foster_home_id)) {
            return ApplicationResult::Failure(
                "Foster home no longer has capacity. Consider rejecting or waiting."
            );
        }

        try {
            // Approve the application
            auto updated_app = FosterService::getInstance().approveApplication(
                app_id,
                response
            );

            // Notify applicant
            notifyApplicant(updated_app, "approved");

            return ApplicationResult::Success(updated_app);

        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::BUSINESS_LOGIC,
                "Failed to approve foster application: " + std::string(e.what()),
                {{"app_id", app_id}}
            );
            return ApplicationResult::Failure(
                "An error occurred while approving the application."
            );
        }
    } else {
        // Reject the application
        try {
            auto updated_app = FosterService::getInstance().rejectApplication(
                app_id,
                response
            );

            // Notify applicant
            notifyApplicant(updated_app, "rejected");

            return ApplicationResult::Success(updated_app);

        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::BUSINESS_LOGIC,
                "Failed to reject foster application: " + std::string(e.what()),
                {{"app_id", app_id}}
            );
            return ApplicationResult::Failure(
                "An error occurred while rejecting the application."
            );
        }
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

bool FosterApplicationHandler::canUserFoster(const std::string& user_id) {
    // Check if user exists
    auto user_opt = UserService::getInstance().findById(user_id);
    if (!user_opt.has_value()) {
        return false;
    }

    const auto& user = user_opt.value();

    // Check if user is active
    if (!user.is_active) {
        return false;
    }

    // Check if user has a foster home
    auto foster_home = FosterService::getInstance().findByUserId(user_id);
    if (!foster_home.has_value()) {
        return false;
    }

    // Check if foster home is active
    if (!foster_home->is_active) {
        return false;
    }

    // Optional: Check if verified (could be made configurable)
    // if (!foster_home->is_verified) {
    //     return false;
    // }

    return true;
}

bool FosterApplicationHandler::isDogAvailableForFoster(const std::string& dog_id) {
    // Check if dog exists
    auto dog_opt = DogService::getInstance().findById(dog_id);
    if (!dog_opt.has_value()) {
        return false;
    }

    const auto& dog = dog_opt.value();

    // Check status
    if (dog.status != "adoptable") {
        return false;
    }

    // Check availability flag
    if (!dog.is_available) {
        return false;
    }

    // Check if dog is already in active foster
    auto placements = FosterService::getInstance().getPlacementsByDog(dog_id);
    for (const auto& placement : placements) {
        if (placement.isActive()) {
            return false;  // Already in foster
        }
    }

    return true;
}

bool FosterApplicationHandler::hasCapacity(const std::string& foster_home_id) {
    auto home_opt = FosterService::getInstance().findById(foster_home_id);
    if (!home_opt.has_value()) {
        return false;
    }

    return home_opt->hasCapacity();
}

bool FosterApplicationHandler::hasPendingApplication(
    const std::string& foster_home_id,
    const std::string& dog_id) {

    auto applications = FosterService::getInstance().getApplicationsByHome(foster_home_id);

    for (const auto& app : applications) {
        if (app.dog_id == dog_id && app.isPending()) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// NOTIFICATIONS (PLACEHOLDER)
// ============================================================================

void FosterApplicationHandler::notifyApplicant(
    const models::FosterApplication& app,
    const std::string& status) {

    // Placeholder for future notification integration
    // This would:
    // 1. Get the user's email from foster_home -> user
    // 2. Get the dog's name for personalization
    // 3. Send appropriate email/push notification based on status

    // For now, just log the notification intent
    // In production, this would integrate with NotificationService

    /*
    Example implementation:
    auto home = FosterService::getInstance().findById(app.foster_home_id);
    if (home.has_value()) {
        auto user = UserService::getInstance().findById(home->user_id);
        if (user.has_value()) {
            NotificationService::getInstance().sendEmail(
                user->email,
                "Foster Application " + status,
                buildNotificationBody(app, status)
            );
        }
    }
    */
}

void FosterApplicationHandler::notifyShelter(const models::FosterApplication& app) {
    // Placeholder for future notification integration
    // This would:
    // 1. Get the shelter for the dog
    // 2. Get shelter staff emails
    // 3. Send notification about new application

    /*
    Example implementation:
    auto shelter_id = getShelterIdForDog(app.dog_id);
    if (shelter_id.has_value()) {
        auto shelter = ShelterService::getInstance().findById(shelter_id.value());
        if (shelter.has_value() && shelter->email.has_value()) {
            NotificationService::getInstance().sendEmail(
                shelter->email.value(),
                "New Foster Application",
                buildShelterNotificationBody(app)
            );
        }
    }
    */
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::optional<std::string> FosterApplicationHandler::getFosterHomeIdForUser(
    const std::string& user_id) {

    auto home = FosterService::getInstance().findByUserId(user_id);
    if (home.has_value()) {
        return home->id;
    }
    return std::nullopt;
}

std::optional<std::string> FosterApplicationHandler::getShelterIdForDog(
    const std::string& dog_id) {

    auto dog = DogService::getInstance().findById(dog_id);
    if (dog.has_value()) {
        return dog->shelter_id;
    }
    return std::nullopt;
}

} // namespace wtl::core::services

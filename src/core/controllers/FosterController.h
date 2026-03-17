/**
 * @file FosterController.h
 * @brief REST API controller for foster operations
 *
 * PURPOSE:
 * Provides HTTP endpoints for the Foster-First Pathway feature, enabling
 * users to register as foster homes, apply to foster dogs, and manage
 * placements. Research shows fostering makes dogs 14x more likely to be
 * adopted, making this a primary feature.
 *
 * ENDPOINTS:
 * User Endpoints (authenticated):
 *   POST /api/foster/register          - Register as foster home
 *   GET  /api/foster/profile           - Get foster profile
 *   PUT  /api/foster/profile           - Update foster profile
 *   GET  /api/foster/matches           - Get matched dogs
 *   GET  /api/foster/placements        - Get current placements
 *   POST /api/foster/apply/{dog_id}    - Apply to foster dog
 *   GET  /api/foster/applications      - Get my applications
 *   DELETE /api/foster/applications/{id} - Withdraw application
 *
 * Shelter/Admin Endpoints:
 *   GET  /api/foster/applications/pending      - Pending applications
 *   POST /api/foster/applications/{id}/approve - Approve application
 *   POST /api/foster/applications/{id}/reject  - Reject application
 *   GET  /api/foster/homes                     - List all foster homes
 *   GET  /api/foster/placements/all            - All placements
 *   GET  /api/foster/statistics                - Statistics
 *
 * DEPENDENCIES:
 * - FosterService (business logic)
 * - FosterMatcher (matching algorithm)
 * - FosterApplicationHandler (application workflow)
 * - AuthMiddleware (authentication)
 * - ApiResponse (response formatting)
 *
 * @author Agent 9 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::core::controllers {

/**
 * @class FosterController
 * @brief REST API controller for foster home and placement operations
 *
 * Provides all HTTP endpoints for the foster system including registration,
 * profile management, application processing, and administrative functions.
 */
class FosterController : public drogon::HttpController<FosterController> {
public:
    METHOD_LIST_BEGIN

    // ========================================================================
    // USER ENDPOINTS (Authenticated)
    // ========================================================================

    // Register as foster home
    ADD_METHOD_TO(FosterController::registerFoster,
                  "/api/foster/register",
                  drogon::Post);

    // Get foster profile
    ADD_METHOD_TO(FosterController::getProfile,
                  "/api/foster/profile",
                  drogon::Get);

    // Update foster profile
    ADD_METHOD_TO(FosterController::updateProfile,
                  "/api/foster/profile",
                  drogon::Put);

    // Get matched dogs for foster
    ADD_METHOD_TO(FosterController::getMatches,
                  "/api/foster/matches",
                  drogon::Get);

    // Get current placements
    ADD_METHOD_TO(FosterController::getPlacements,
                  "/api/foster/placements",
                  drogon::Get);

    // Apply to foster dog
    ADD_METHOD_TO(FosterController::applyToFoster,
                  "/api/foster/apply/{dog_id}",
                  drogon::Post);

    // Get my applications
    ADD_METHOD_TO(FosterController::getApplications,
                  "/api/foster/applications",
                  drogon::Get);

    // Withdraw application
    ADD_METHOD_TO(FosterController::withdrawApplication,
                  "/api/foster/applications/{id}",
                  drogon::Delete);

    // ========================================================================
    // SHELTER/ADMIN ENDPOINTS
    // ========================================================================

    // Get pending applications (shelter)
    ADD_METHOD_TO(FosterController::getPendingApplications,
                  "/api/foster/applications/pending",
                  drogon::Get);

    // Approve application (shelter)
    ADD_METHOD_TO(FosterController::approveApplication,
                  "/api/foster/applications/{id}/approve",
                  drogon::Post);

    // Reject application (shelter)
    ADD_METHOD_TO(FosterController::rejectApplication,
                  "/api/foster/applications/{id}/reject",
                  drogon::Post);

    // List all foster homes (admin)
    ADD_METHOD_TO(FosterController::listFosterHomes,
                  "/api/foster/homes",
                  drogon::Get);

    // Get all placements (admin)
    ADD_METHOD_TO(FosterController::getAllPlacements,
                  "/api/foster/placements/all",
                  drogon::Get);

    // Get statistics (admin)
    ADD_METHOD_TO(FosterController::getStatistics,
                  "/api/foster/statistics",
                  drogon::Get);

    METHOD_LIST_END

    // ========================================================================
    // USER ENDPOINT HANDLERS
    // ========================================================================

    /**
     * @brief Register current user as a foster home
     * @param req HTTP request with foster home data
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "max_dogs": 2,
     *   "has_yard": true,
     *   "has_other_dogs": false,
     *   "has_cats": false,
     *   "has_children": false,
     *   "children_ages": [],
     *   "ok_with_puppies": true,
     *   "ok_with_seniors": true,
     *   "ok_with_medical": false,
     *   "ok_with_behavioral": false,
     *   "preferred_sizes": ["medium", "large"],
     *   "preferred_breeds": [],
     *   "zip_code": "78701",
     *   "max_transport_miles": 50
     * }
     */
    void registerFoster(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get current user's foster profile
     * @param req HTTP request
     * @param callback Response callback
     */
    void getProfile(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update current user's foster profile
     * @param req HTTP request with updated data
     * @param callback Response callback
     */
    void updateProfile(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get matched dogs for current user's foster home
     * @param req HTTP request with optional limit param
     * @param callback Response callback
     */
    void getMatches(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get current user's active placements
     * @param req HTTP request
     * @param callback Response callback
     */
    void getPlacements(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Apply to foster a specific dog
     * @param req HTTP request with message
     * @param callback Response callback
     * @param dog_id The dog's UUID
     *
     * Request body:
     * {
     *   "message": "I would love to foster this dog!"
     * }
     */
    void applyToFoster(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& dog_id);

    /**
     * @brief Get current user's foster applications
     * @param req HTTP request
     * @param callback Response callback
     */
    void getApplications(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Withdraw a pending application
     * @param req HTTP request
     * @param callback Response callback
     * @param id The application UUID
     */
    void withdrawApplication(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             const std::string& id);

    // ========================================================================
    // SHELTER/ADMIN ENDPOINT HANDLERS
    // ========================================================================

    /**
     * @brief Get all pending applications (shelter staff)
     * @param req HTTP request
     * @param callback Response callback
     */
    void getPendingApplications(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Approve a foster application (shelter staff)
     * @param req HTTP request with response message
     * @param callback Response callback
     * @param id The application UUID
     */
    void approveApplication(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& id);

    /**
     * @brief Reject a foster application (shelter staff)
     * @param req HTTP request with response message
     * @param callback Response callback
     * @param id The application UUID
     */
    void rejectApplication(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           const std::string& id);

    /**
     * @brief List all foster homes (admin)
     * @param req HTTP request with pagination params
     * @param callback Response callback
     */
    void listFosterHomes(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get all placements (admin)
     * @param req HTTP request with optional status filter
     * @param callback Response callback
     */
    void getAllPlacements(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get foster statistics (admin)
     * @param req HTTP request
     * @param callback Response callback
     */
    void getStatistics(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::core::controllers

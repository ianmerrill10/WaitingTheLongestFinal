/**
 * @file NotificationController.h
 * @brief REST API controller for notification operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to user notifications including:
 * - Retrieving user notifications (with filtering and pagination)
 * - Marking notifications as read
 * - Managing notification preferences
 * - Device token registration for push notifications
 * - Notification engagement tracking
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET    /api/notifications              - List user's notifications
 * - GET    /api/notifications/{id}         - Get specific notification
 * - PUT    /api/notifications/{id}/read    - Mark as read
 * - PUT    /api/notifications/read-all     - Mark all as read
 * - DELETE /api/notifications/{id}         - Delete notification
 * - GET    /api/notifications/unread-count - Get unread count
 * - GET    /api/notifications/preferences  - Get preferences
 * - PUT    /api/notifications/preferences  - Update preferences
 * - POST   /api/notifications/register-device   - Register push token
 * - DELETE /api/notifications/unregister-device - Unregister push token
 * - POST   /api/notifications/{id}/opened   - Track notification opened
 * - POST   /api/notifications/{id}/clicked  - Track notification clicked
 *
 * DEPENDENCIES:
 * - NotificationService - CRUD operations and delivery
 * - AuthMiddleware - Authentication/authorization
 * - ApiResponse - Response formatting
 * - ErrorCapture - Error logging
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints to METHOD_LIST and implement handlers
 * - All endpoints require authentication (REQUIRE_AUTH)
 * - All errors use WTL_CAPTURE_ERROR macro
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::controllers {

/**
 * @class NotificationController
 * @brief HTTP controller for notification-related API endpoints
 *
 * All endpoints require authentication. Provides user-facing API
 * for managing notifications and preferences.
 */
class NotificationController : public drogon::HttpController<NotificationController> {
public:
    METHOD_LIST_BEGIN
    // ========================================================================
    // NOTIFICATION ENDPOINTS
    // ========================================================================

    // List user's notifications with pagination and filtering
    ADD_METHOD_TO(NotificationController::getNotifications, "/api/notifications", drogon::Get);

    // Get specific notification by ID
    ADD_METHOD_TO(NotificationController::getNotificationById, "/api/notifications/{id}", drogon::Get);

    // Mark notification as read
    ADD_METHOD_TO(NotificationController::markAsRead, "/api/notifications/{id}/read", drogon::Put);

    // Mark all notifications as read
    ADD_METHOD_TO(NotificationController::markAllAsRead, "/api/notifications/read-all", drogon::Put);

    // Delete a notification
    ADD_METHOD_TO(NotificationController::deleteNotification, "/api/notifications/{id}", drogon::Delete);

    // Get unread notification count
    ADD_METHOD_TO(NotificationController::getUnreadCount, "/api/notifications/unread-count", drogon::Get);

    // ========================================================================
    // ENGAGEMENT TRACKING
    // ========================================================================

    // Track notification opened
    ADD_METHOD_TO(NotificationController::trackOpened, "/api/notifications/{id}/opened", drogon::Post);

    // Track notification clicked
    ADD_METHOD_TO(NotificationController::trackClicked, "/api/notifications/{id}/clicked", drogon::Post);

    // ========================================================================
    // PREFERENCES ENDPOINTS
    // ========================================================================

    // Get user's notification preferences
    ADD_METHOD_TO(NotificationController::getPreferences, "/api/notifications/preferences", drogon::Get);

    // Update notification preferences
    ADD_METHOD_TO(NotificationController::updatePreferences, "/api/notifications/preferences", drogon::Put);

    // ========================================================================
    // DEVICE TOKEN ENDPOINTS
    // ========================================================================

    // Register device token for push notifications
    ADD_METHOD_TO(NotificationController::registerDevice, "/api/notifications/register-device", drogon::Post);

    // Unregister device token
    ADD_METHOD_TO(NotificationController::unregisterDevice, "/api/notifications/unregister-device", drogon::Delete);

    // Get user's registered devices
    ADD_METHOD_TO(NotificationController::getDevices, "/api/notifications/devices", drogon::Get);

    // ========================================================================
    // ADMIN ENDPOINTS
    // ========================================================================

    // Send notification (admin only)
    ADD_METHOD_TO(NotificationController::sendNotification, "/api/admin/notifications/send", drogon::Post);

    // Broadcast urgent notification (admin only)
    ADD_METHOD_TO(NotificationController::broadcastUrgent, "/api/admin/notifications/broadcast", drogon::Post);

    // Get notification statistics (admin only)
    ADD_METHOD_TO(NotificationController::getStats, "/api/admin/notifications/stats", drogon::Get);

    // Get worker status (admin only)
    ADD_METHOD_TO(NotificationController::getWorkerStatus, "/api/admin/notifications/worker", drogon::Get);

    METHOD_LIST_END

    // ========================================================================
    // NOTIFICATION HANDLERS
    // ========================================================================

    /**
     * @brief Get user's notifications
     *
     * Query params:
     * - page: Page number (default 1)
     * - per_page: Items per page (default 20, max 100)
     * - type: Filter by notification type
     * - unread_only: Only show unread (boolean)
     */
    void getNotifications(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get specific notification by ID
     */
    void getNotificationById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Mark notification as read
     */
    void markAsRead(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Mark all notifications as read
     */
    void markAllAsRead(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Delete a notification
     */
    void deleteNotification(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Get unread notification count
     */
    void getUnreadCount(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // ========================================================================
    // ENGAGEMENT TRACKING HANDLERS
    // ========================================================================

    /**
     * @brief Track notification opened
     */
    void trackOpened(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Track notification clicked (with optional action)
     *
     * Body:
     * - action: Optional action identifier
     */
    void trackClicked(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    // ========================================================================
    // PREFERENCES HANDLERS
    // ========================================================================

    /**
     * @brief Get user's notification preferences
     */
    void getPreferences(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Update notification preferences
     *
     * Body: NotificationPreferences JSON
     */
    void updatePreferences(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // ========================================================================
    // DEVICE TOKEN HANDLERS
    // ========================================================================

    /**
     * @brief Register device for push notifications
     *
     * Body:
     * - token: FCM token
     * - device_type: "ios", "android", "web"
     * - device_name: Optional device name
     */
    void registerDevice(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Unregister device token
     *
     * Body:
     * - token: FCM token to unregister
     */
    void unregisterDevice(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get user's registered devices
     */
    void getDevices(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // ========================================================================
    // ADMIN HANDLERS
    // ========================================================================

    /**
     * @brief Send notification to user (admin only)
     *
     * Body:
     * - user_id: Target user
     * - type: Notification type
     * - title: Notification title
     * - body: Notification body
     * - data: Optional data payload
     */
    void sendNotification(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Broadcast urgent notification (admin only)
     *
     * Body:
     * - dog_id: Dog to alert about
     * - radius_miles: Optional radius (uses default if not specified)
     */
    void broadcastUrgent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get notification statistics (admin only)
     */
    void getStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get notification worker status (admin only)
     */
    void getWorkerStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

private:
    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Parse pagination parameters from request
     */
    void parsePaginationParams(
        const drogon::HttpRequestPtr& req,
        int& page,
        int& per_page
    );

    /**
     * @brief Validate device registration data
     */
    bool validateDeviceData(
        const Json::Value& json,
        std::string& error_message
    );

    /**
     * @brief Validate notification send data
     */
    bool validateNotificationData(
        const Json::Value& json,
        std::string& error_message
    );
};

} // namespace wtl::controllers

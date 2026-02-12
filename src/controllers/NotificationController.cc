/**
 * @file NotificationController.cc
 * @brief Implementation of NotificationController
 * @see NotificationController.h for documentation
 */

#include "controllers/NotificationController.h"

// Standard library includes
#include <algorithm>

// Project includes
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/ApiResponse.h"
#include "notifications/NotificationService.h"
#include "notifications/NotificationWorker.h"
#include "notifications/UrgentAlertService.h"
#include "core/services/DogService.h"

namespace wtl::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::notifications;
using namespace ::wtl::core::auth;

// ============================================================================
// NOTIFICATION HANDLERS
// ============================================================================

void NotificationController::getNotifications(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        int page, per_page;
        parsePaginationParams(req, page, per_page);

        // Parse filters
        auto params = req->getParameters();
        std::string type_filter;
        bool unread_only = false;

        auto type_it = params.find("type");
        if (type_it != params.end()) {
            type_filter = type_it->second;
        }

        auto unread_it = params.find("unread_only");
        if (unread_it != params.end()) {
            unread_only = (unread_it->second == "true" || unread_it->second == "1");
        }

        // Get notifications
        auto& service = NotificationService::getInstance();
        int offset = (page - 1) * per_page;
        auto notifications = service.getUserNotifications(auth_token->user_id, per_page + 1, offset);

        // Check if there are more results
        bool has_more = notifications.size() > static_cast<size_t>(per_page);
        if (has_more) {
            notifications.pop_back();
        }

        // Apply type filter if specified
        if (!type_filter.empty()) {
            notifications.erase(
                std::remove_if(notifications.begin(), notifications.end(),
                    [&type_filter](const Notification& n) {
                        return notificationTypeToString(n.type) != type_filter;
                    }),
                notifications.end()
            );
        }

        // Apply unread filter if specified
        if (unread_only) {
            notifications.erase(
                std::remove_if(notifications.begin(), notifications.end(),
                    [](const Notification& n) {
                        return n.is_read;
                    }),
                notifications.end()
            );
        }

        // Build response
        Json::Value data(Json::arrayValue);
        for (const auto& notification : notifications) {
            data.append(notification.toJson());
        }

        // Get total count for pagination
        int total_unread = service.getUnreadCount(auth_token->user_id);

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["page"] = page;
        meta["per_page"] = per_page;
        meta["has_next"] = has_more;
        meta["has_prev"] = page > 1;
        meta["unread_count"] = total_unread;
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get notifications: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve notifications"));
    }
}

void NotificationController::getNotificationById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        auto notification = service.getNotification(id);

        if (!notification) {
            callback(ApiResponse::notFound("Notification"));
            return;
        }

        // Verify ownership
        if (notification->user_id != auth_token->user_id) {
            callback(ApiResponse::forbidden("Access denied"));
            return;
        }

        callback(ApiResponse::success(notification->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get notification: " + std::string(e.what()),
            {{"notification_id", id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve notification"));
    }
}

void NotificationController::markAsRead(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();

        // Get notification to verify ownership
        auto notification = service.getNotification(id);
        if (!notification) {
            callback(ApiResponse::notFound("Notification"));
            return;
        }

        if (notification->user_id != auth_token->user_id) {
            callback(ApiResponse::forbidden("Access denied"));
            return;
        }

        bool success = service.markAsRead(id);
        if (!success) {
            callback(ApiResponse::serverError("Failed to mark as read"));
            return;
        }

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to mark notification as read: " + std::string(e.what()),
            {{"notification_id", id}}
        );
        callback(ApiResponse::serverError("Failed to mark as read"));
    }
}

void NotificationController::markAllAsRead(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        int count = service.markAllAsRead(auth_token->user_id);

        Json::Value result;
        result["marked_as_read"] = count;

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to mark all notifications as read: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to mark all as read"));
    }
}

void NotificationController::deleteNotification(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();

        // Get notification to verify ownership
        auto notification = service.getNotification(id);
        if (!notification) {
            callback(ApiResponse::notFound("Notification"));
            return;
        }

        if (notification->user_id != auth_token->user_id) {
            callback(ApiResponse::forbidden("Access denied"));
            return;
        }

        bool success = service.deleteNotification(id);
        if (!success) {
            callback(ApiResponse::serverError("Failed to delete notification"));
            return;
        }

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to delete notification: " + std::string(e.what()),
            {{"notification_id", id}}
        );
        callback(ApiResponse::serverError("Failed to delete notification"));
    }
}

void NotificationController::getUnreadCount(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        int count = service.getUnreadCount(auth_token->user_id);

        Json::Value result;
        result["unread_count"] = count;

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get unread count"));
    }
}

// ============================================================================
// ENGAGEMENT TRACKING HANDLERS
// ============================================================================

void NotificationController::trackOpened(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();

        // Verify notification belongs to user
        auto notification = service.getNotification(id);
        if (!notification) {
            callback(ApiResponse::notFound("Notification"));
            return;
        }

        if (notification->user_id != auth_token->user_id) {
            callback(ApiResponse::forbidden("Access denied"));
            return;
        }

        service.recordOpened(id);
        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to track opened"));
    }
}

void NotificationController::trackClicked(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();

        // Verify notification belongs to user
        auto notification = service.getNotification(id);
        if (!notification) {
            callback(ApiResponse::notFound("Notification"));
            return;
        }

        if (notification->user_id != auth_token->user_id) {
            callback(ApiResponse::forbidden("Access denied"));
            return;
        }

        std::string action;
        auto json = req->getJsonObject();
        if (json && json->isMember("action")) {
            action = (*json)["action"].asString();
        }

        service.recordClicked(id, action);
        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to track click"));
    }
}

// ============================================================================
// PREFERENCES HANDLERS
// ============================================================================

void NotificationController::getPreferences(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        auto prefs = service.getPreferences(auth_token->user_id);

        if (!prefs) {
            // Create default preferences if none exist
            auto default_prefs = service.createDefaultPreferences(auth_token->user_id);
            callback(ApiResponse::success(default_prefs.toJson()));
            return;
        }

        callback(ApiResponse::success(prefs->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get preferences: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve preferences"));
    }
}

void NotificationController::updatePreferences(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Parse preferences from JSON
        UserNotificationPreferences prefs = UserNotificationPreferences::fromJson(*json);
        prefs.user_id = auth_token->user_id;

        auto& service = NotificationService::getInstance();
        bool success = service.updatePreferences(auth_token->user_id, prefs);

        if (!success) {
            callback(ApiResponse::serverError("Failed to update preferences"));
            return;
        }

        // Return updated preferences
        auto updated = service.getPreferences(auth_token->user_id);
        callback(ApiResponse::success(updated->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to update preferences: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to update preferences"));
    }
}

// ============================================================================
// DEVICE TOKEN HANDLERS
// ============================================================================

void NotificationController::registerDevice(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string error_message;
        if (!validateDeviceData(*json, error_message)) {
            callback(ApiResponse::badRequest(error_message));
            return;
        }

        std::string token = (*json)["token"].asString();
        std::string device_type = (*json)["device_type"].asString();
        std::string device_name = json->get("device_name", "").asString();

        auto& service = NotificationService::getInstance();
        std::string registration_id = service.registerDeviceToken(
            auth_token->user_id,
            token,
            device_type,
            device_name
        );

        Json::Value result;
        result["registration_id"] = registration_id;
        result["message"] = "Device registered successfully";

        callback(ApiResponse::created(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to register device: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to register device"));
    }
}

void NotificationController::unregisterDevice(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("token")) {
            callback(ApiResponse::badRequest("Token is required"));
            return;
        }

        std::string token = (*json)["token"].asString();

        auto& service = NotificationService::getInstance();
        service.unregisterDeviceToken(auth_token->user_id, token);

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to unregister device: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to unregister device"));
    }
}

void NotificationController::getDevices(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_AUTH(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        auto devices = service.getUserDeviceTokens(auth_token->user_id);

        Json::Value data(Json::arrayValue);
        for (const auto& device : devices) {
            data.append(device.toJson());
        }

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get devices: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve devices"));
    }
}

// ============================================================================
// ADMIN HANDLERS
// ============================================================================

void NotificationController::sendNotification(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string error_message;
        if (!validateNotificationData(*json, error_message)) {
            callback(ApiResponse::badRequest(error_message));
            return;
        }

        // Build notification from JSON
        Notification notification;
        notification.user_id = (*json)["user_id"].asString();
        notification.type = stringToNotificationType((*json)["type"].asString());
        notification.title = (*json)["title"].asString();
        notification.body = (*json)["body"].asString();

        if (json->isMember("dog_id")) {
            notification.dog_id = (*json)["dog_id"].asString();
        }
        if (json->isMember("shelter_id")) {
            notification.shelter_id = (*json)["shelter_id"].asString();
        }
        if (json->isMember("data")) {
            notification.data = (*json)["data"];
        }

        auto& service = NotificationService::getInstance();
        auto result = service.sendNotification(notification, notification.user_id);

        callback(ApiResponse::created(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to send notification: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to send notification"));
    }
}

void NotificationController::broadcastUrgent(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("dog_id")) {
            callback(ApiResponse::badRequest("dog_id is required"));
            return;
        }

        std::string dog_id = (*json)["dog_id"].asString();
        double radius_miles = 0;

        if (json->isMember("radius_miles")) {
            radius_miles = (*json)["radius_miles"].asDouble();
        }

        // Get the dog
        auto& dog_service = wtl::core::services::DogService::getInstance();
        auto dog = dog_service.findById(dog_id);

        if (!dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        // Broadcast urgent alert
        auto& urgent_service = UrgentAlertService::getInstance();
        auto result = urgent_service.blastNotification(*dog, radius_miles);

        callback(ApiResponse::created(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to broadcast urgent notification: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to broadcast urgent notification"));
    }
}

void NotificationController::getStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& service = NotificationService::getInstance();
        auto stats = service.getStats();

        // Also include urgent alert stats
        auto& urgent_service = UrgentAlertService::getInstance();
        auto urgent_stats = urgent_service.getStats();

        Json::Value result;
        result["notification_service"] = stats;
        result["urgent_alert_service"] = urgent_stats;

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get statistics"));
    }
}

void NotificationController::getWorkerStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& worker = NotificationWorker::getInstance();
        auto stats = worker.getStats();

        Json::Value result = stats.toJson();
        result["is_running"] = worker.isRunning();
        result["queue_size"] = static_cast<Json::UInt64>(worker.getQueueSize());

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get worker status"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void NotificationController::parsePaginationParams(
    const drogon::HttpRequestPtr& req,
    int& page,
    int& per_page
) {
    page = 1;
    per_page = 20;

    auto params = req->getParameters();

    auto page_it = params.find("page");
    if (page_it != params.end()) {
        try {
            page = std::max(1, std::stoi(page_it->second));
        } catch (const std::exception& e) {
            LOG_WARN << "NotificationController: " << e.what();
        } catch (...) {
            LOG_WARN << "NotificationController: unknown error";
        }
    }

    auto per_page_it = params.find("per_page");
    if (per_page_it != params.end()) {
        try {
            per_page = std::min(100, std::max(1, std::stoi(per_page_it->second)));
        } catch (const std::exception& e) {
            LOG_WARN << "NotificationController: " << e.what();
        } catch (...) {
            LOG_WARN << "NotificationController: unknown error";
        }
    }
}

bool NotificationController::validateDeviceData(
    const Json::Value& json,
    std::string& error_message
) {
    if (!json.isMember("token") || json["token"].asString().empty()) {
        error_message = "Token is required";
        return false;
    }

    if (!json.isMember("device_type") || json["device_type"].asString().empty()) {
        error_message = "Device type is required";
        return false;
    }

    std::string device_type = json["device_type"].asString();
    if (device_type != "ios" && device_type != "android" && device_type != "web") {
        error_message = "Device type must be ios, android, or web";
        return false;
    }

    return true;
}

bool NotificationController::validateNotificationData(
    const Json::Value& json,
    std::string& error_message
) {
    if (!json.isMember("user_id") || json["user_id"].asString().empty()) {
        error_message = "user_id is required";
        return false;
    }

    if (!json.isMember("type") || json["type"].asString().empty()) {
        error_message = "type is required";
        return false;
    }

    if (!json.isMember("title") || json["title"].asString().empty()) {
        error_message = "title is required";
        return false;
    }

    if (!json.isMember("body") || json["body"].asString().empty()) {
        error_message = "body is required";
        return false;
    }

    // Validate notification type
    try {
        stringToNotificationType(json["type"].asString());
    } catch (...) {
        error_message = "Invalid notification type";
        return false;
    }

    return true;
}

} // namespace wtl::controllers

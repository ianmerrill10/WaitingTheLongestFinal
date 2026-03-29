/**
 * @file ApiResponse.cc
 * @brief Implementation of ApiResponse utility
 * @see ApiResponse.h for documentation
 */

#include "core/utils/ApiResponse.h"

namespace wtl::core::utils {

// ============================================================================
// SUCCESS RESPONSES
// ============================================================================

drogon::HttpResponsePtr ApiResponse::success(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

drogon::HttpResponsePtr ApiResponse::success(const Json::Value& data,
                                              int total, int page, int per_page) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    // Calculate pagination metadata
    Json::Value meta;
    meta["total"] = total;
    meta["page"] = page;
    meta["per_page"] = per_page;
    meta["total_pages"] = (total + per_page - 1) / per_page; // Ceiling division
    meta["has_next"] = (page * per_page) < total;
    meta["has_prev"] = page > 1;

    response["meta"] = meta;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

drogon::HttpResponsePtr ApiResponse::created(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k201Created);
    return resp;
}

drogon::HttpResponsePtr ApiResponse::noContent() {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k204NoContent);
    return resp;
}

// ============================================================================
// ERROR RESPONSES
// ============================================================================

drogon::HttpResponsePtr ApiResponse::badRequest(const std::string& message) {
    return error("BAD_REQUEST", message, 400);
}

drogon::HttpResponsePtr ApiResponse::badRequest(const std::string& code, const std::string& message) {
    return error(code, message, 400);
}

drogon::HttpResponsePtr ApiResponse::unauthorized(const std::string& message) {
    return error("UNAUTHORIZED", message, 401);
}

drogon::HttpResponsePtr ApiResponse::forbidden(const std::string& message) {
    return error("FORBIDDEN", message, 403);
}

drogon::HttpResponsePtr ApiResponse::notFound(const std::string& resource) {
    std::string code = resource + "_NOT_FOUND";
    // Convert to uppercase
    for (auto& c : code) {
        c = std::toupper(c);
    }
    // Replace spaces with underscores
    for (auto& c : code) {
        if (c == ' ') c = '_';
    }

    std::string message = resource + " not found";
    return error(code, message, 404);
}

drogon::HttpResponsePtr ApiResponse::conflict(const std::string& message) {
    return error("CONFLICT", message, 409);
}

drogon::HttpResponsePtr ApiResponse::serverError(const std::string& message) {
    return error("INTERNAL_SERVER_ERROR", message, 500);
}

drogon::HttpResponsePtr ApiResponse::validationError(
    const std::vector<std::pair<std::string, std::string>>& errors) {

    Json::Value details;
    Json::Value fieldErrors(Json::arrayValue);

    for (const auto& [field, msg] : errors) {
        Json::Value fieldError;
        fieldError["field"] = field;
        fieldError["message"] = msg;
        fieldErrors.append(fieldError);
    }

    details["validation_errors"] = fieldErrors;

    return error("VALIDATION_ERROR", "One or more validation errors occurred", 422, details);
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

drogon::HttpResponsePtr ApiResponse::error(const std::string& code,
                                            const std::string& message,
                                            int httpStatus,
                                            const Json::Value& details) {
    Json::Value response;
    response["success"] = false;

    Json::Value errorObj;
    errorObj["code"] = code;
    errorObj["message"] = message;

    if (!details.isNull()) {
        errorObj["details"] = details;
    }

    response["error"] = errorObj;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);

    // Map integer status to Drogon status codes
    switch (httpStatus) {
        case 400:
            resp->setStatusCode(drogon::k400BadRequest);
            break;
        case 401:
            resp->setStatusCode(drogon::k401Unauthorized);
            break;
        case 403:
            resp->setStatusCode(drogon::k403Forbidden);
            break;
        case 404:
            resp->setStatusCode(drogon::k404NotFound);
            break;
        case 409:
            resp->setStatusCode(drogon::k409Conflict);
            break;
        case 422:
            resp->setStatusCode(drogon::k422UnprocessableEntity);
            break;
        case 500:
        default:
            resp->setStatusCode(drogon::k500InternalServerError);
            break;
    }

    return resp;
}

} // namespace wtl::core::utils

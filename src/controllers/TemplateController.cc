/**
 * @file TemplateController.cc
 * @brief Implementation of TemplateController
 * @see TemplateController.h for documentation
 */

#include "controllers/TemplateController.h"
#include "content/templates/TemplateEngine.h"
#include "content/templates/TemplateRepository.h"
#include "content/templates/TemplateContext.h"
#include "content/templates/TemplateHelpers.h"
#include "core/auth/AuthMiddleware.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::controllers {

using namespace drogon;
using namespace ::wtl::core::utils;
using namespace ::wtl::core::debug;
using namespace ::wtl::content::templates;

// ============================================================================
// PUBLIC ENDPOINTS
// ============================================================================

void TemplateController::getAllTemplates(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    try {
        auto& repo = TemplateRepository::getInstance();

        // Parse query parameters
        std::string category_filter = req->getParameter("category");
        std::string type_filter = req->getParameter("type");
        std::string urgency_filter = req->getParameter("urgency");
        bool active_only = req->getParameter("active_only") != "false";

        Json::Value templates_array(Json::arrayValue);

        // Get all categories
        auto categories = repo.getCategories();

        for (const auto& category : categories) {
            if (!category_filter.empty() && category != category_filter) {
                continue;
            }

            auto templates = repo.getTemplatesByCategory(category);
            for (const auto& tpl : templates) {
                // Apply filters
                if (active_only && !tpl.metadata.is_active) {
                    continue;
                }
                if (!type_filter.empty() && tpl.metadata.type != type_filter) {
                    continue;
                }
                if (!urgency_filter.empty()) {
                    bool matches_urgency = false;
                    for (const auto& u : tpl.metadata.use_for_urgency) {
                        if (u == urgency_filter) {
                            matches_urgency = true;
                            break;
                        }
                    }
                    if (!matches_urgency) continue;
                }

                templates_array.append(tpl.toJson());
            }
        }

        Json::Value response;
        response["templates"] = templates_array;
        response["count"] = static_cast<Json::UInt>(templates_array.size());

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error fetching templates: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Failed to fetch templates"));
    }
}

void TemplateController::getTemplatesByCategory(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& category) {

    try {
        auto& repo = TemplateRepository::getInstance();
        auto templates = repo.getTemplatesByCategory(category);

        if (templates.empty()) {
            callback(ApiResponse::notFound("Category not found: " + category));
            return;
        }

        Json::Value templates_array(Json::arrayValue);
        for (const auto& tpl : templates) {
            templates_array.append(tpl.toJson());
        }

        Json::Value response;
        response["category"] = category;
        response["templates"] = templates_array;
        response["count"] = static_cast<Json::UInt>(templates_array.size());

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error fetching templates by category: " + std::string(e.what()),
                         {{"category", category}});
        callback(ApiResponse::serverError("Failed to fetch templates"));
    }
}

void TemplateController::getTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& category,
    const std::string& name) {

    try {
        auto& repo = TemplateRepository::getInstance();
        auto tpl = repo.getTemplate(category, name);

        if (!tpl) {
            callback(ApiResponse::notFound("Template not found: " + category + "/" + name));
            return;
        }

        callback(ApiResponse::success(tpl->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error fetching template: " + std::string(e.what()),
                         {{"category", category}, {"name", name}});
        callback(ApiResponse::serverError("Failed to fetch template"));
    }
}

void TemplateController::getCategories(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    try {
        auto& repo = TemplateRepository::getInstance();
        auto categories = repo.getCategories();

        Json::Value categories_array(Json::arrayValue);
        for (const auto& cat : categories) {
            Json::Value cat_obj;
            cat_obj["name"] = cat;
            cat_obj["count"] = static_cast<Json::UInt>(repo.getCountByCategory(cat));
            categories_array.append(cat_obj);
        }

        Json::Value response;
        response["categories"] = categories_array;

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error fetching categories: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Failed to fetch categories"));
    }
}

void TemplateController::getStats(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    try {
        auto& repo = TemplateRepository::getInstance();
        auto& engine = TemplateEngine::getInstance();

        Json::Value stats;
        stats["repository"] = repo.getStats();
        stats["engine"] = engine.getStats();

        callback(ApiResponse::success(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error fetching stats: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Failed to fetch statistics"));
    }
}

// ============================================================================
// PREVIEW AND VALIDATION
// ============================================================================

void TemplateController::previewTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& engine = TemplateEngine::getInstance();
        auto& repo = TemplateRepository::getInstance();

        std::string rendered;
        std::string template_name = (*json).get("template_name", "").asString();
        std::string template_content = (*json).get("template_content", "").asString();
        Json::Value context = (*json).get("context", Json::Value(Json::objectValue));

        if (!template_name.empty()) {
            // Load and render named template
            size_t slash_pos = template_name.find('/');
            if (slash_pos == std::string::npos) {
                callback(ApiResponse::badRequest("Template name must be in format 'category/name'"));
                return;
            }

            std::string category = template_name.substr(0, slash_pos);
            std::string name = template_name.substr(slash_pos + 1);

            auto tpl = repo.getTemplate(category, name);
            if (!tpl) {
                callback(ApiResponse::notFound("Template not found: " + template_name));
                return;
            }

            rendered = engine.renderString(tpl->content, context);

        } else if (!template_content.empty()) {
            // Render provided content directly
            rendered = engine.renderString(template_content, context);
        } else {
            callback(ApiResponse::badRequest("Either template_name or template_content is required"));
            return;
        }

        Json::Value response;
        response["rendered"] = rendered;
        response["length"] = static_cast<Json::UInt>(rendered.length());

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error previewing template: " + std::string(e.what()), {});
        callback(ApiResponse::badRequest("Template rendering failed: " + std::string(e.what())));
    }
}

void TemplateController::validateTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string content = (*json).get("content", "").asString();
        if (content.empty()) {
            callback(ApiResponse::badRequest("content field is required"));
            return;
        }

        auto& repo = TemplateRepository::getInstance();
        auto result = repo.validateTemplate(content);

        Json::Value response;
        response["is_valid"] = result.is_valid;
        response["error_message"] = result.error_message;

        Json::Value warnings_array(Json::arrayValue);
        for (const auto& w : result.warnings) {
            warnings_array.append(w);
        }
        response["warnings"] = warnings_array;

        Json::Value vars_array(Json::arrayValue);
        for (const auto& v : result.required_variables) {
            vars_array.append(v);
        }
        response["required_variables"] = vars_array;

        response["estimated_render_length"] = result.estimated_render_length;

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::VALIDATION,
                         "Error validating template: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Validation failed"));
    }
}

// ============================================================================
// ADMIN ENDPOINTS
// ============================================================================

void TemplateController::createTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    // Require admin authentication
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Parse template from JSON
        Template tpl = Template::fromJson(*json);

        // Validate required fields
        if (tpl.metadata.name.empty()) {
            callback(ApiResponse::badRequest("name is required"));
            return;
        }
        if (tpl.metadata.category.empty()) {
            callback(ApiResponse::badRequest("category is required"));
            return;
        }
        if (tpl.content.empty()) {
            callback(ApiResponse::badRequest("content or caption is required"));
            return;
        }

        // Check if name is available
        auto& repo = TemplateRepository::getInstance();
        if (!repo.isNameAvailable(tpl.metadata.category, tpl.metadata.name)) {
            callback(ApiResponse::conflict("Template already exists: " +
                     tpl.metadata.category + "/" + tpl.metadata.name));
            return;
        }

        // Validate template content
        auto validation = repo.validateTemplate(tpl.content);
        if (!validation.is_valid) {
            callback(ApiResponse::badRequest("Invalid template: " + validation.error_message));
            return;
        }

        // Set created_by from auth token
        tpl.metadata.created_by = auth_token->user_id;

        // Save template
        auto saved = repo.saveTemplate(tpl);

        callback(ApiResponse::created(saved.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error creating template: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Failed to create template"));
    }
}

void TemplateController::updateTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& id) {

    // Require admin authentication
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& repo = TemplateRepository::getInstance();

        // Check if template exists
        auto existing = repo.getTemplateById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Template not found: " + id));
            return;
        }

        // Parse updated template
        Template tpl = Template::fromJson(*json);

        // Validate if content changed
        if (!tpl.content.empty() && tpl.content != existing->content) {
            auto validation = repo.validateTemplate(tpl.content);
            if (!validation.is_valid) {
                callback(ApiResponse::badRequest("Invalid template: " + validation.error_message));
                return;
            }
        }

        // Set updated_by from auth token
        tpl.metadata.updated_by = auth_token->user_id;

        // Update template
        auto updated = repo.updateTemplate(id, tpl);
        if (!updated) {
            callback(ApiResponse::serverError("Failed to update template"));
            return;
        }

        callback(ApiResponse::success(updated->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error updating template: " + std::string(e.what()),
                         {{"template_id", id}});
        callback(ApiResponse::serverError("Failed to update template"));
    }
}

void TemplateController::deleteTemplate(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& id) {

    // Require admin authentication
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& repo = TemplateRepository::getInstance();

        // Check if template exists
        auto existing = repo.getTemplateById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Template not found: " + id));
            return;
        }

        // Delete template
        if (repo.deleteTemplate(id)) {
            callback(ApiResponse::noContent());
        } else {
            callback(ApiResponse::serverError("Failed to delete template"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error deleting template: " + std::string(e.what()),
                         {{"template_id", id}});
        callback(ApiResponse::serverError("Failed to delete template"));
    }
}

void TemplateController::reloadTemplates(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    // Require admin authentication
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& repo = TemplateRepository::getInstance();
        int count = repo.reloadAll();

        // Also register helpers with engine
        auto& engine = TemplateEngine::getInstance();
        TemplateHelpers::registerAll(engine);

        Json::Value response;
        response["message"] = "Templates reloaded successfully";
        response["templates_loaded"] = count;

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Error reloading templates: " + std::string(e.what()), {});
        callback(ApiResponse::serverError("Failed to reload templates"));
    }
}

} // namespace wtl::controllers

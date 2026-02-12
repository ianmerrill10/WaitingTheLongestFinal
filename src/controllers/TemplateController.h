/**
 * @file TemplateController.h
 * @brief REST API controller for content template management
 *
 * PURPOSE:
 * Provides REST API endpoints for listing, retrieving, creating, updating,
 * and deleting content templates. Also supports template preview and validation.
 *
 * USAGE:
 * Controller auto-registers with Drogon via inheritance.
 *
 * ENDPOINTS:
 * - GET    /api/templates              - List all templates
 * - GET    /api/templates/{category}   - Get templates by category
 * - GET    /api/templates/{category}/{name} - Get specific template
 * - POST   /api/templates              - Create template (admin)
 * - PUT    /api/templates/{id}         - Update template (admin)
 * - DELETE /api/templates/{id}         - Delete template (admin)
 * - POST   /api/templates/preview      - Preview template with sample data
 * - POST   /api/templates/validate     - Validate template syntax
 * - GET    /api/templates/stats        - Get template statistics
 *
 * DEPENDENCIES:
 * - TemplateEngine (template rendering)
 * - TemplateRepository (template storage)
 * - AuthMiddleware (authentication)
 * - ApiResponse (response formatting)
 * - ErrorCapture (error logging)
 *
 * @author Agent 16 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::controllers {

/**
 * @class TemplateController
 * @brief REST API controller for template operations
 *
 * Handles all HTTP endpoints related to content templates.
 * Admin authentication required for create/update/delete operations.
 */
class TemplateController : public drogon::HttpController<TemplateController> {
public:
    METHOD_LIST_BEGIN

    // Public endpoints
    ADD_METHOD_TO(TemplateController::getAllTemplates,
                  "/api/templates", drogon::Get);

    ADD_METHOD_TO(TemplateController::getTemplatesByCategory,
                  "/api/templates/{category}", drogon::Get);

    ADD_METHOD_TO(TemplateController::getTemplate,
                  "/api/templates/{category}/{name}", drogon::Get);

    ADD_METHOD_TO(TemplateController::getCategories,
                  "/api/templates/categories", drogon::Get);

    ADD_METHOD_TO(TemplateController::getStats,
                  "/api/templates/stats", drogon::Get);

    // Preview and validation (authenticated)
    ADD_METHOD_TO(TemplateController::previewTemplate,
                  "/api/templates/preview", drogon::Post);

    ADD_METHOD_TO(TemplateController::validateTemplate,
                  "/api/templates/validate", drogon::Post);

    // Admin endpoints
    ADD_METHOD_TO(TemplateController::createTemplate,
                  "/api/templates", drogon::Post);

    ADD_METHOD_TO(TemplateController::updateTemplate,
                  "/api/templates/{id}", drogon::Put);

    ADD_METHOD_TO(TemplateController::deleteTemplate,
                  "/api/templates/{id}", drogon::Delete);

    ADD_METHOD_TO(TemplateController::reloadTemplates,
                  "/api/templates/reload", drogon::Post);

    METHOD_LIST_END

    // =========================================================================
    // PUBLIC ENDPOINTS
    // =========================================================================

    /**
     * @brief Get all templates
     *
     * GET /api/templates
     * Query params: category, type, urgency, active_only
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getAllTemplates(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get templates by category
     *
     * GET /api/templates/{category}
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param category Template category (tiktok, email, sms, etc.)
     */
    void getTemplatesByCategory(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& category);

    /**
     * @brief Get a specific template
     *
     * GET /api/templates/{category}/{name}
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param category Template category
     * @param name Template name
     */
    void getTemplate(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& category,
                    const std::string& name);

    /**
     * @brief Get available template categories
     *
     * GET /api/templates/categories
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getCategories(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get template statistics
     *
     * GET /api/templates/stats
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // PREVIEW AND VALIDATION
    // =========================================================================

    /**
     * @brief Preview template with sample data
     *
     * POST /api/templates/preview
     * Body: {
     *   "template_name": "tiktok/urgent_countdown",  // or "template_content"
     *   "context": { ... sample data ... }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void previewTemplate(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Validate template syntax
     *
     * POST /api/templates/validate
     * Body: { "content": "template content..." }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void validateTemplate(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // ADMIN ENDPOINTS
    // =========================================================================

    /**
     * @brief Create a new template (admin only)
     *
     * POST /api/templates
     * Body: Template JSON
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void createTemplate(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update an existing template (admin only)
     *
     * PUT /api/templates/{id}
     * Body: Updated template JSON
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Template UUID
     */
    void updateTemplate(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& id);

    /**
     * @brief Delete a template (admin only)
     *
     * DELETE /api/templates/{id}
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Template UUID
     */
    void deleteTemplate(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& id);

    /**
     * @brief Reload all templates from storage (admin only)
     *
     * POST /api/templates/reload
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void reloadTemplates(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::controllers

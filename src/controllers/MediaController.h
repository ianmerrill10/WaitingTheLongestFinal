/**
 * @file MediaController.h
 * @brief REST API controller for media operations
 *
 * PURPOSE:
 * Provides HTTP endpoints for media operations including video generation,
 * image processing, file uploads, and job status queries.
 *
 * ENDPOINTS:
 * POST /api/media/video/generate     - Start video generation job
 * POST /api/media/image/process      - Start image processing job
 * POST /api/media/share-card         - Generate share card
 * GET  /api/media/job/{id}           - Get job status
 * GET  /api/media/jobs/{dog_id}      - Get jobs for dog
 * GET  /api/media/{id}               - Get media record
 * GET  /api/media/dog/{dog_id}       - Get media for dog
 * DELETE /api/media/{id}             - Delete media
 * POST /api/media/upload             - Upload media file
 * GET  /api/media/queue/stats        - Get queue statistics
 *
 * DEPENDENCIES:
 * - Drogon HTTP framework
 * - MediaQueue, MediaStorage, VideoGenerator, ImageProcessor
 * - Auth middleware (for protected routes)
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Drogon includes
#include <drogon/HttpController.h>

// Third-party includes
#include <json/json.h>

namespace wtl::controllers {

/**
 * @class MediaController
 * @brief HTTP controller for media operations
 *
 * Handles all media-related API requests including video generation,
 * image processing, uploads, and status queries.
 */
class MediaController : public drogon::HttpController<MediaController> {
public:
    METHOD_LIST_BEGIN
    // Video operations
    ADD_METHOD_TO(MediaController::generateVideo, "/api/media/video/generate", drogon::Post);
    ADD_METHOD_TO(MediaController::addOverlay, "/api/media/video/overlay", drogon::Post);
    ADD_METHOD_TO(MediaController::processForPlatform, "/api/media/video/platform", drogon::Post);

    // Image operations
    ADD_METHOD_TO(MediaController::processImage, "/api/media/image/process", drogon::Post);
    ADD_METHOD_TO(MediaController::generateShareCard, "/api/media/share-card", drogon::Post);
    ADD_METHOD_TO(MediaController::generateThumbnail, "/api/media/thumbnail", drogon::Post);

    // Job management
    ADD_METHOD_TO(MediaController::getJobStatus, "/api/media/job/{id}", drogon::Get);
    ADD_METHOD_TO(MediaController::cancelJob, "/api/media/job/{id}", drogon::Delete);
    ADD_METHOD_TO(MediaController::retryJob, "/api/media/job/{id}/retry", drogon::Post);
    ADD_METHOD_TO(MediaController::getJobsForDog, "/api/media/jobs/{dog_id}", drogon::Get);

    // Media retrieval
    ADD_METHOD_TO(MediaController::getMedia, "/api/media/{id}", drogon::Get);
    ADD_METHOD_TO(MediaController::getMediaForDog, "/api/media/dog/{dog_id}", drogon::Get);
    ADD_METHOD_TO(MediaController::deleteMedia, "/api/media/{id}", drogon::Delete);

    // Upload
    ADD_METHOD_TO(MediaController::uploadMedia, "/api/media/upload", drogon::Post);

    // Queue management
    ADD_METHOD_TO(MediaController::getQueueStats, "/api/media/queue/stats", drogon::Get);
    ADD_METHOD_TO(MediaController::getPendingJobs, "/api/media/queue/pending", drogon::Get);
    ADD_METHOD_TO(MediaController::getProcessingJobs, "/api/media/queue/processing", drogon::Get);
    ADD_METHOD_TO(MediaController::getFailedJobs, "/api/media/queue/failed", drogon::Get);

    // Admin operations
    ADD_METHOD_TO(MediaController::cleanupJobs, "/api/media/admin/cleanup", drogon::Post);
    ADD_METHOD_TO(MediaController::resetStuckJobs, "/api/media/admin/reset-stuck", drogon::Post);
    ADD_METHOD_TO(MediaController::getWorkerStats, "/api/media/admin/worker-stats", drogon::Get);
    METHOD_LIST_END

    // =========================================================================
    // VIDEO OPERATIONS
    // =========================================================================

    /**
     * @brief Start video generation job
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "dog_id": "string",
     *   "photos": ["path1", "path2"],
     *   "template_id": "slideshow|ken_burns|countdown|story",
     *   "options": {
     *     "preprocess_photos": true,
     *     "add_wait_time_overlay": true,
     *     "add_urgency_badge": true,
     *     "add_music": true
     *   },
     *   "priority": "low|normal|high|urgent",
     *   "callback_url": "https://..."
     * }
     */
    void generateVideo(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Add overlay to existing video
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "video_path": "string",
     *   "dog_id": "string",
     *   "wait_time": "00:00:00:00:00:00",
     *   "countdown": "00:00:00",
     *   "urgency_level": "normal|warning|critical"
     * }
     */
    void addOverlay(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Process video for specific platform
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "video_path": "string",
     *   "dog_id": "string",
     *   "platforms": ["youtube", "instagram", "tiktok", "facebook"]
     * }
     */
    void processForPlatform(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // =========================================================================
    // IMAGE OPERATIONS
    // =========================================================================

    /**
     * @brief Start image processing job
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "dog_id": "string",
     *   "image_path": "string",
     *   "operation": "enhance|resize|optimize|wait_time_overlay|urgency_badge",
     *   "options": {...}
     * }
     */
    void processImage(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Generate share card for dog
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "dog_id": "string",
     *   "photo_path": "string",
     *   "platform": "facebook|twitter|instagram",
     *   "dog_info": {...}
     * }
     */
    void generateShareCard(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Generate thumbnail from video
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "dog_id": "string",
     *   "video_path": "string"
     * }
     */
    void generateThumbnail(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // =========================================================================
    // JOB MANAGEMENT
    // =========================================================================

    /**
     * @brief Get job status
     * @param req HTTP request
     * @param callback Response callback
     * @param id Job ID
     */
    void getJobStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Cancel a pending job
     * @param req HTTP request
     * @param callback Response callback
     * @param id Job ID
     */
    void cancelJob(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Retry a failed job
     * @param req HTTP request
     * @param callback Response callback
     * @param id Job ID
     */
    void retryJob(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Get all jobs for a dog
     * @param req HTTP request
     * @param callback Response callback
     * @param dog_id Dog ID
     */
    void getJobsForDog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id
    );

    // =========================================================================
    // MEDIA RETRIEVAL
    // =========================================================================

    /**
     * @brief Get media record by ID
     * @param req HTTP request
     * @param callback Response callback
     * @param id Media ID
     */
    void getMedia(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    /**
     * @brief Get all media for a dog
     * @param req HTTP request
     * @param callback Response callback
     * @param dog_id Dog ID
     */
    void getMediaForDog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id
    );

    /**
     * @brief Delete media by ID
     * @param req HTTP request
     * @param callback Response callback
     * @param id Media ID
     */
    void deleteMedia(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    // =========================================================================
    // UPLOAD
    // =========================================================================

    /**
     * @brief Upload media file
     * @param req HTTP request (multipart/form-data)
     * @param callback Response callback
     *
     * Form fields:
     * - file: The media file
     * - dog_id: Associated dog ID
     * - type: image|video|thumbnail
     * - metadata: JSON metadata (optional)
     */
    void uploadMedia(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // =========================================================================
    // QUEUE MANAGEMENT
    // =========================================================================

    /**
     * @brief Get queue statistics
     * @param req HTTP request
     * @param callback Response callback
     */
    void getQueueStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get pending jobs
     * @param req HTTP request
     * @param callback Response callback
     */
    void getPendingJobs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get processing jobs
     * @param req HTTP request
     * @param callback Response callback
     */
    void getProcessingJobs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get failed jobs
     * @param req HTTP request
     * @param callback Response callback
     */
    void getFailedJobs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // =========================================================================
    // ADMIN OPERATIONS
    // =========================================================================

    /**
     * @brief Clean up old jobs
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "days_old": 7
     * }
     */
    void cleanupJobs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Reset stuck processing jobs
     * @param req HTTP request
     * @param callback Response callback
     *
     * Request body:
     * {
     *   "timeout_minutes": 30
     * }
     */
    void resetStuckJobs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    /**
     * @brief Get worker statistics
     * @param req HTTP request
     * @param callback Response callback
     */
    void getWorkerStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

private:
    /**
     * @brief Create JSON error response
     * @param message Error message
     * @param status HTTP status code
     * @return HTTP response
     */
    static drogon::HttpResponsePtr errorResponse(
        const std::string& message,
        drogon::HttpStatusCode status = drogon::k400BadRequest
    );

    /**
     * @brief Create JSON success response
     * @param data Response data
     * @return HTTP response
     */
    static drogon::HttpResponsePtr successResponse(const Json::Value& data);

    /**
     * @brief Parse JSON from request body
     * @param req HTTP request
     * @param json Output JSON value
     * @return True if parsing successful
     */
    static bool parseJsonBody(const drogon::HttpRequestPtr& req, Json::Value& json);

    /**
     * @brief Validate required fields in JSON
     * @param json JSON object
     * @param fields Required field names
     * @param missing Output: first missing field name
     * @return True if all fields present
     */
    static bool validateRequiredFields(
        const Json::Value& json,
        const std::vector<std::string>& fields,
        std::string& missing
    );
};

} // namespace wtl::controllers

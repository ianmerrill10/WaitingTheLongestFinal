/**
 * @file MediaController.cc
 * @brief Implementation of media REST API controller
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "controllers/MediaController.h"

// Project includes
#include "content/media/ImageProcessor.h"
#include "content/media/MediaQueue.h"
#include "content/media/MediaStorage.h"
#include "content/media/MediaWorker.h"
#include "content/media/VideoGenerator.h"
#include "core/debug/ErrorCapture.h"

// Standard library includes
#include <fstream>

namespace wtl::controllers {

using namespace ::wtl::content::media;

// ============================================================================
// VIDEO OPERATIONS
// ============================================================================

void MediaController::generateVideo(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"dog_id", "photos"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    std::string dog_id = json["dog_id"].asString();

    // Extract photos
    std::vector<std::string> photos;
    if (!json["photos"].isArray()) {
        callback(errorResponse("photos must be an array"));
        return;
    }

    for (const auto& photo : json["photos"]) {
        photos.push_back(photo.asString());
    }

    if (photos.empty()) {
        callback(errorResponse("At least one photo is required"));
        return;
    }

    // Get template ID
    std::string template_id = json.get("template_id", "slideshow").asString();

    // Get priority
    std::string priority_str = json.get("priority", "normal").asString();
    JobPriority priority = stringToJobPriority(priority_str);

    // Enqueue job
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    std::string job_id = queue.enqueueVideoGeneration(dog_id, photos, template_id, priority);

    if (job_id.empty()) {
        callback(errorResponse("Failed to enqueue video generation job", drogon::k500InternalServerError));
        return;
    }

    // Update job with additional options if provided
    if (json.isMember("options") || json.isMember("callback_url") || json.isMember("dog_info")) {
        auto job_opt = queue.getStatus(job_id);
        if (job_opt) {
            MediaJob job = *job_opt;

            if (json.isMember("options")) {
                job.input_data["options"] = json["options"];
            }
            if (json.isMember("callback_url")) {
                job.callback_url = json["callback_url"].asString();
            }
            if (json.isMember("dog_info")) {
                job.input_data["dog_info"] = json["dog_info"];
            }
        }
    }

    Json::Value response;
    response["success"] = true;
    response["job_id"] = job_id;
    response["message"] = "Video generation job queued";

    callback(successResponse(response));
}

void MediaController::addOverlay(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"video_path", "dog_id"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    auto& generator = VideoGenerator::getInstance();
    if (!generator.isInitialized()) {
        generator.initializeFromConfig();
    }

    std::string video_path = json["video_path"].asString();
    std::string dog_id = json["dog_id"].asString();
    std::string wait_time = json.get("wait_time", "00:00:00:00:00:00").asString();

    std::optional<std::string> countdown;
    if (json.isMember("countdown")) {
        countdown = json["countdown"].asString();
    }

    // Generate output path
    std::string output_path = generator.generateOutputPath(dog_id + "_overlay");

    // Get overlay config
    OverlayConfig overlay_config = OverlayConfig::getDefault();

    auto result = generator.addOverlay(video_path, output_path, overlay_config, wait_time, countdown);

    if (result.success) {
        Json::Value response;
        response["success"] = true;
        response["output_path"] = result.output_path;
        response["thumbnail_path"] = result.thumbnail_path;
        callback(successResponse(response));
    } else {
        callback(errorResponse(result.error_message, drogon::k500InternalServerError));
    }
}

void MediaController::processForPlatform(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"video_path", "dog_id", "platforms"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    std::string dog_id = json["dog_id"].asString();
    std::string video_path = json["video_path"].asString();

    std::vector<std::string> platforms;
    if (json["platforms"].isArray()) {
        for (const auto& platform : json["platforms"]) {
            platforms.push_back(platform.asString());
        }
    }

    if (platforms.empty()) {
        callback(errorResponse("At least one platform is required"));
        return;
    }

    // Enqueue transcode job
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    std::string job_id = queue.enqueuePlatformTranscode(dog_id, video_path, platforms);

    if (job_id.empty()) {
        callback(errorResponse("Failed to enqueue transcode job", drogon::k500InternalServerError));
        return;
    }

    Json::Value response;
    response["success"] = true;
    response["job_id"] = job_id;
    response["message"] = "Platform transcode job queued";

    callback(successResponse(response));
}

// ============================================================================
// IMAGE OPERATIONS
// ============================================================================

void MediaController::processImage(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"dog_id", "image_path", "operation"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    std::string dog_id = json["dog_id"].asString();
    std::string image_path = json["image_path"].asString();
    std::string operation = json["operation"].asString();

    Json::Value options;
    if (json.isMember("options")) {
        options = json["options"];
    }

    // Enqueue image processing job
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    std::string job_id = queue.enqueueImageProcessing(dog_id, image_path, operation, options);

    if (job_id.empty()) {
        callback(errorResponse("Failed to enqueue image processing job", drogon::k500InternalServerError));
        return;
    }

    Json::Value response;
    response["success"] = true;
    response["job_id"] = job_id;
    response["message"] = "Image processing job queued";

    callback(successResponse(response));
}

void MediaController::generateShareCard(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"dog_id"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    std::string dog_id = json["dog_id"].asString();
    std::string platform = json.get("platform", "facebook").asString();

    // Enqueue share card job
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    MediaJob job;
    job.type = JobType::SHARE_CARD;
    job.dog_id = dog_id;
    job.input_data["platform"] = platform;

    if (json.isMember("photo_path")) {
        job.input_data["photo_path"] = json["photo_path"].asString();
    }
    if (json.isMember("dog_info")) {
        job.input_data["dog_info"] = json["dog_info"];
    }

    std::string job_id = queue.enqueue(job);

    if (job_id.empty()) {
        callback(errorResponse("Failed to enqueue share card job", drogon::k500InternalServerError));
        return;
    }

    Json::Value response;
    response["success"] = true;
    response["job_id"] = job_id;
    response["message"] = "Share card generation job queued";

    callback(successResponse(response));
}

void MediaController::generateThumbnail(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    if (!parseJsonBody(req, json)) {
        callback(errorResponse("Invalid JSON body"));
        return;
    }

    std::string missing;
    if (!validateRequiredFields(json, {"dog_id", "video_path"}, missing)) {
        callback(errorResponse("Missing required field: " + missing));
        return;
    }

    std::string dog_id = json["dog_id"].asString();
    std::string video_path = json["video_path"].asString();

    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    std::string job_id = queue.enqueueThumbnail(dog_id, video_path);

    if (job_id.empty()) {
        callback(errorResponse("Failed to enqueue thumbnail job", drogon::k500InternalServerError));
        return;
    }

    Json::Value response;
    response["success"] = true;
    response["job_id"] = job_id;
    response["message"] = "Thumbnail generation job queued";

    callback(successResponse(response));
}

// ============================================================================
// JOB MANAGEMENT
// ============================================================================

void MediaController::getJobStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    auto job_opt = queue.getStatus(id);

    if (!job_opt) {
        callback(errorResponse("Job not found", drogon::k404NotFound));
        return;
    }

    callback(successResponse(job_opt->toJson()));
}

void MediaController::cancelJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    bool cancelled = queue.cancel(id);

    Json::Value response;
    response["success"] = cancelled;
    response["job_id"] = id;

    if (cancelled) {
        response["message"] = "Job cancelled";
        callback(successResponse(response));
    } else {
        response["message"] = "Failed to cancel job (may be processing or not found)";
        callback(errorResponse("Failed to cancel job"));
    }
}

void MediaController::retryJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    bool retried = queue.retry(id);

    Json::Value response;
    response["success"] = retried;
    response["job_id"] = id;

    if (retried) {
        response["message"] = "Job requeued for retry";
        callback(successResponse(response));
    } else {
        response["message"] = "Failed to retry job (may be at max retries or not failed)";
        callback(errorResponse("Failed to retry job"));
    }
}

void MediaController::getJobsForDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    auto jobs = queue.getJobsForDog(dog_id);

    Json::Value response;
    response["success"] = true;
    response["dog_id"] = dog_id;
    response["count"] = static_cast<int>(jobs.size());

    Json::Value jobs_array(Json::arrayValue);
    for (const auto& job : jobs) {
        jobs_array.append(job.toJson());
    }
    response["jobs"] = jobs_array;

    callback(successResponse(response));
}

// ============================================================================
// MEDIA RETRIEVAL
// ============================================================================

void MediaController::getMedia(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& storage = MediaStorage::getInstance();
    if (!storage.isInitialized()) {
        storage.initializeFromConfig();
    }

    auto record_opt = storage.getMediaRecord(id);

    if (!record_opt) {
        callback(errorResponse("Media not found", drogon::k404NotFound));
        return;
    }

    callback(successResponse(record_opt->toJson()));
}

void MediaController::getMediaForDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id
) {
    auto& storage = MediaStorage::getInstance();
    if (!storage.isInitialized()) {
        storage.initializeFromConfig();
    }

    // Check for type filter
    std::optional<MediaType> type_filter;
    auto type_param = req->getParameter("type");
    if (!type_param.empty()) {
        type_filter = MediaStorage::stringToMediaType(type_param);
    }

    auto records = storage.getMediaForDog(dog_id, type_filter);

    Json::Value response;
    response["success"] = true;
    response["dog_id"] = dog_id;
    response["count"] = static_cast<int>(records.size());

    Json::Value media_array(Json::arrayValue);
    for (const auto& record : records) {
        media_array.append(record.toJson());
    }
    response["media"] = media_array;

    callback(successResponse(response));
}

void MediaController::deleteMedia(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& storage = MediaStorage::getInstance();
    if (!storage.isInitialized()) {
        storage.initializeFromConfig();
    }

    bool deleted = storage.deleteMedia(id);

    Json::Value response;
    response["success"] = deleted;
    response["media_id"] = id;

    if (deleted) {
        response["message"] = "Media deleted";
        callback(successResponse(response));
    } else {
        callback(errorResponse("Failed to delete media", drogon::k404NotFound));
    }
}

// ============================================================================
// UPLOAD
// ============================================================================

void MediaController::uploadMedia(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    // Handle multipart file upload
    drogon::MultiPartParser fileParser;
    if (fileParser.parse(req) != 0) {
        callback(errorResponse("Failed to parse multipart form data"));
        return;
    }

    auto& files = fileParser.getFiles();
    if (files.empty()) {
        callback(errorResponse("No file uploaded"));
        return;
    }

    auto& params = fileParser.getParameters();

    // Get dog_id
    std::string dog_id;
    auto dog_id_it = params.find("dog_id");
    if (dog_id_it == params.end()) {
        callback(errorResponse("Missing required field: dog_id"));
        return;
    }
    dog_id = dog_id_it->second;

    // Get type
    std::string type_str = "image";
    auto type_it = params.find("type");
    if (type_it != params.end()) {
        type_str = type_it->second;
    }

    MediaType type = MediaStorage::stringToMediaType(type_str);

    // Get metadata
    Json::Value metadata;
    auto metadata_it = params.find("metadata");
    if (metadata_it != params.end()) {
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        reader->parse(
            metadata_it->second.c_str(),
            metadata_it->second.c_str() + metadata_it->second.size(),
            &metadata,
            &errors
        );
    }

    auto& storage = MediaStorage::getInstance();
    if (!storage.isInitialized()) {
        storage.initializeFromConfig();
    }

    // Save uploaded file
    const auto& file = files[0];

    auto result = storage.saveMediaData(
        file.fileContent().data(),
        file.fileContent().size(),
        file.getFileName(),
        type,
        dog_id,
        metadata
    );

    if (result.success) {
        Json::Value response;
        response["success"] = true;
        response["media_id"] = result.media_id;
        response["url"] = result.url;
        response["file_size"] = static_cast<Json::Int64>(result.file_size);
        callback(successResponse(response));
    } else {
        callback(errorResponse(result.error_message, drogon::k500InternalServerError));
    }
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void MediaController::getQueueStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    auto stats = queue.getStats();

    Json::Value response;
    response["success"] = true;
    response["stats"] = stats.toJson();

    callback(successResponse(response));
}

void MediaController::getPendingJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    int limit = 100;
    auto limit_param = req->getParameter("limit");
    if (!limit_param.empty()) {
        limit = std::stoi(limit_param);
    }

    auto jobs = queue.getPendingJobs(limit);

    Json::Value response;
    response["success"] = true;
    response["count"] = static_cast<int>(jobs.size());

    Json::Value jobs_array(Json::arrayValue);
    for (const auto& job : jobs) {
        jobs_array.append(job.toJson());
    }
    response["jobs"] = jobs_array;

    callback(successResponse(response));
}

void MediaController::getProcessingJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    auto jobs = queue.getProcessingJobs();

    Json::Value response;
    response["success"] = true;
    response["count"] = static_cast<int>(jobs.size());

    Json::Value jobs_array(Json::arrayValue);
    for (const auto& job : jobs) {
        jobs_array.append(job.toJson());
    }
    response["jobs"] = jobs_array;

    callback(successResponse(response));
}

void MediaController::getFailedJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    int limit = 100;
    auto limit_param = req->getParameter("limit");
    if (!limit_param.empty()) {
        limit = std::stoi(limit_param);
    }

    auto jobs = queue.getFailedJobs(limit);

    Json::Value response;
    response["success"] = true;
    response["count"] = static_cast<int>(jobs.size());

    Json::Value jobs_array(Json::arrayValue);
    for (const auto& job : jobs) {
        jobs_array.append(job.toJson());
    }
    response["jobs"] = jobs_array;

    callback(successResponse(response));
}

// ============================================================================
// ADMIN OPERATIONS
// ============================================================================

void MediaController::cleanupJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    parseJsonBody(req, json);  // Optional body

    int days_old = json.get("days_old", 7).asInt();

    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    int cleaned = queue.cleanup(days_old);

    Json::Value response;
    response["success"] = true;
    response["jobs_cleaned"] = cleaned;
    response["days_old"] = days_old;

    callback(successResponse(response));
}

void MediaController::resetStuckJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    Json::Value json;
    parseJsonBody(req, json);  // Optional body

    int timeout_minutes = json.get("timeout_minutes", 30).asInt();

    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        queue.initializeFromConfig();
    }

    int reset = queue.resetStuckJobs(timeout_minutes);

    Json::Value response;
    response["success"] = true;
    response["jobs_reset"] = reset;
    response["timeout_minutes"] = timeout_minutes;

    callback(successResponse(response));
}

void MediaController::getWorkerStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto& worker = MediaWorker::getInstance();

    Json::Value response;
    response["success"] = true;
    response["running"] = worker.isRunning();
    response["active_threads"] = worker.getActiveThreadCount();
    response["worker_stats"] = worker.getStats().toJson();
    response["queue_stats"] = worker.getQueueStats().toJson();
    response["config"] = worker.getConfig().toJson();

    callback(successResponse(response));
}

// ============================================================================
// HELPER METHODS
// ============================================================================

drogon::HttpResponsePtr MediaController::errorResponse(
    const std::string& message,
    drogon::HttpStatusCode status
) {
    Json::Value json;
    json["success"] = false;
    json["error"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status);
    return resp;
}

drogon::HttpResponsePtr MediaController::successResponse(const Json::Value& data) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

bool MediaController::parseJsonBody(const drogon::HttpRequestPtr& req, Json::Value& json) {
    auto body = req->getBody();
    if (body.empty()) {
        return false;
    }

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;

    return reader->parse(body.data(), body.data() + body.size(), &json, &errors);
}

bool MediaController::validateRequiredFields(
    const Json::Value& json,
    const std::vector<std::string>& fields,
    std::string& missing
) {
    for (const auto& field : fields) {
        if (!json.isMember(field) || json[field].isNull()) {
            missing = field;
            return false;
        }
    }
    return true;
}

} // namespace wtl::controllers

/**
 * @file MediaWorker.cc
 * @brief Implementation of background media processing worker
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "content/media/MediaWorker.h"

// Project includes
#include "content/media/ImageProcessor.h"
#include "content/media/MediaQueue.h"
#include "content/media/MediaStorage.h"
#include "content/media/VideoGenerator.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

// Standard library includes
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace wtl::content::media {

// ============================================================================
// WORKER CONFIG IMPLEMENTATION
// ============================================================================

Json::Value WorkerConfig::toJson() const {
    Json::Value json;
    json["num_threads"] = num_threads;
    json["poll_interval_ms"] = poll_interval_ms;
    json["job_timeout_minutes"] = job_timeout_minutes;
    json["max_retries"] = max_retries;
    json["process_video"] = process_video;
    json["process_image"] = process_image;
    json["process_transcode"] = process_transcode;
    json["auto_cleanup"] = auto_cleanup;
    json["cleanup_interval_hours"] = cleanup_interval_hours;
    json["cleanup_days_old"] = cleanup_days_old;
    return json;
}

WorkerConfig WorkerConfig::fromJson(const Json::Value& json) {
    WorkerConfig config;
    config.num_threads = json.get("num_threads", 4).asInt();
    config.poll_interval_ms = json.get("poll_interval_ms", 1000).asInt();
    config.job_timeout_minutes = json.get("job_timeout_minutes", 30).asInt();
    config.max_retries = json.get("max_retries", 3).asInt();
    config.process_video = json.get("process_video", true).asBool();
    config.process_image = json.get("process_image", true).asBool();
    config.process_transcode = json.get("process_transcode", true).asBool();
    config.auto_cleanup = json.get("auto_cleanup", true).asBool();
    config.cleanup_interval_hours = json.get("cleanup_interval_hours", 24).asInt();
    config.cleanup_days_old = json.get("cleanup_days_old", 7).asInt();
    return config;
}

WorkerConfig WorkerConfig::defaults() {
    return WorkerConfig();
}

// ============================================================================
// WORKER STATS IMPLEMENTATION
// ============================================================================

Json::Value WorkerStats::toJson() const {
    Json::Value json;
    json["active_threads"] = active_threads;
    json["jobs_processed"] = jobs_processed;
    json["jobs_succeeded"] = jobs_succeeded;
    json["jobs_failed"] = jobs_failed;
    json["jobs_retried"] = jobs_retried;
    json["avg_processing_time_ms"] = avg_processing_time_ms;
    json["last_error"] = last_error;

    // Format timestamps
    auto format_time = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    };

    json["started_at"] = format_time(started_at);
    json["last_activity"] = format_time(last_activity);

    return json;
}

// ============================================================================
// MEDIA WORKER IMPLEMENTATION
// ============================================================================

MediaWorker& MediaWorker::getInstance() {
    static MediaWorker instance;
    return instance;
}

MediaWorker::~MediaWorker() {
    if (running_) {
        stop(true);
    }
}

bool MediaWorker::start(int num_threads) {
    WorkerConfig config;
    config.num_threads = num_threads;
    return start(config);
}

bool MediaWorker::start(const WorkerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return true;  // Already running
    }

    config_ = config;

    // Verify dependencies
    auto& queue = MediaQueue::getInstance();
    if (!queue.isInitialized()) {
        if (!queue.initializeFromConfig()) {
            WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaWorker::start - Failed to initialize MediaQueue", {});
            return false;
        }
    }

    // Reset statistics
    jobs_processed_ = 0;
    jobs_succeeded_ = 0;
    jobs_failed_ = 0;
    jobs_retried_ = 0;
    total_processing_time_ms_ = 0;
    started_at_ = std::chrono::system_clock::now();
    last_activity_ = started_at_;
    last_error_.clear();

    // Set enabled types
    video_enabled_ = config.process_video;
    image_enabled_ = config.process_image;
    transcode_enabled_ = config.process_transcode;

    running_ = true;

    // Start worker threads
    for (int i = 0; i < config.num_threads; ++i) {
        worker_threads_.emplace_back(&MediaWorker::workerThread, this, i);
    }

    // Start cleanup thread if enabled
    if (config.auto_cleanup) {
        cleanup_thread_ = std::thread(&MediaWorker::cleanupThread, this);
    }

    return true;
}

bool MediaWorker::startFromConfig() {
    WorkerConfig config;

    std::ifstream config_file("config/media_config.json");
    if (config_file.is_open()) {
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::string errors;

        if (Json::parseFromStream(builder, config_file, &json, &errors)) {
            if (json.isMember("worker")) {
                config = WorkerConfig::fromJson(json["worker"]);
            }
        }
        config_file.close();
    }

    return start(config);
}

void MediaWorker::stop(bool wait_for_completion) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }

    // Notify all waiting threads
    cv_.notify_all();
    MediaQueue::getInstance().notifyActivity();

    // Wait for worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            if (wait_for_completion) {
                thread.join();
            } else {
                thread.detach();
            }
        }
    }
    worker_threads_.clear();

    // Wait for cleanup thread
    if (cleanup_thread_.joinable()) {
        if (wait_for_completion) {
            cleanup_thread_.join();
        } else {
            cleanup_thread_.detach();
        }
    }
}

bool MediaWorker::isRunning() const {
    return running_;
}

int MediaWorker::getActiveThreadCount() const {
    return active_threads_;
}

void MediaWorker::setCompletionCallback(JobCompletionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    completion_callback_ = std::move(callback);
}

void MediaWorker::setProcessableTypes(const std::vector<JobType>& types) {
    video_enabled_ = false;
    image_enabled_ = false;
    share_card_enabled_ = false;
    thumbnail_enabled_ = false;
    transcode_enabled_ = false;
    batch_enabled_ = false;
    cleanup_enabled_ = false;

    for (const auto& type : types) {
        setTypeEnabled(type, true);
    }
}

void MediaWorker::setTypeEnabled(JobType type, bool enabled) {
    switch (type) {
        case JobType::VIDEO_GENERATION:
            video_enabled_ = enabled;
            break;
        case JobType::IMAGE_PROCESSING:
            image_enabled_ = enabled;
            break;
        case JobType::SHARE_CARD:
            share_card_enabled_ = enabled;
            break;
        case JobType::THUMBNAIL:
            thumbnail_enabled_ = enabled;
            break;
        case JobType::PLATFORM_TRANSCODE:
            transcode_enabled_ = enabled;
            break;
        case JobType::BATCH_PROCESS:
            batch_enabled_ = enabled;
            break;
        case JobType::CLEANUP:
            cleanup_enabled_ = enabled;
            break;
    }
}

WorkerStats MediaWorker::getStats() const {
    WorkerStats stats;
    stats.active_threads = active_threads_;
    stats.jobs_processed = jobs_processed_;
    stats.jobs_succeeded = jobs_succeeded_;
    stats.jobs_failed = jobs_failed_;
    stats.jobs_retried = jobs_retried_;

    if (jobs_processed_ > 0) {
        stats.avg_processing_time_ms =
            static_cast<double>(total_processing_time_ms_) / jobs_processed_;
    }

    stats.started_at = started_at_;
    stats.last_activity = last_activity_;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats.last_error = last_error_;
    }

    return stats;
}

WorkerConfig MediaWorker::getConfig() const {
    return config_;
}

QueueStats MediaWorker::getQueueStats() const {
    return MediaQueue::getInstance().getStats();
}

bool MediaWorker::processJob(const std::string& job_id) {
    auto& queue = MediaQueue::getInstance();
    auto job_opt = queue.getStatus(job_id);

    if (!job_opt) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaWorker::processJob - Job not found: " + job_id, {});
        return false;
    }

    MediaJob job = *job_opt;

    // Update status to processing
    queue.updateStatus(job.id, JobStatus::PROCESSING);
    job.status = JobStatus::PROCESSING;
    job.started_at = std::chrono::system_clock::now();

    return processJobInternal(job);
}

int MediaWorker::runCleanup() {
    auto& queue = MediaQueue::getInstance();
    return queue.cleanup(config_.cleanup_days_old);
}

int MediaWorker::resetStuckJobs() {
    auto& queue = MediaQueue::getInstance();
    return queue.resetStuckJobs(config_.job_timeout_minutes);
}

void MediaWorker::triggerProcessing() {
    cv_.notify_all();
    MediaQueue::getInstance().notifyActivity();
}

// ============================================================================
// PRIVATE THREAD FUNCTIONS
// ============================================================================

void MediaWorker::workerThread(int thread_id) {
    active_threads_++;

    auto& queue = MediaQueue::getInstance();

    while (running_) {
        // Wait for jobs
        bool has_job = queue.waitForActivity(
            std::chrono::milliseconds(config_.poll_interval_ms)
        );

        if (!running_) {
            break;
        }

        if (!has_job) {
            continue;
        }

        // Try to dequeue a job
        auto job_opt = queue.dequeue();
        if (!job_opt) {
            continue;
        }

        MediaJob job = *job_opt;

        // Check if we should process this type
        if (!isTypeEnabled(job.type)) {
            // Put it back (by updating status to pending)
            queue.updateStatus(job.id, JobStatus::PENDING);
            continue;
        }

        // Process the job
        auto start_time = std::chrono::steady_clock::now();

        bool success = processJobInternal(job);

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );

        // Update statistics
        jobs_processed_++;
        total_processing_time_ms_ += duration.count();
        last_activity_ = std::chrono::system_clock::now();

        if (success) {
            jobs_succeeded_++;
        } else {
            jobs_failed_++;
        }

        // Call completion callback
        JobCompletionCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = completion_callback_;
        }

        if (callback) {
            try {
                callback(job, success);
            } catch (const std::exception& e) {
                WTL_CAPTURE_ERROR(ErrorCategory::CONTENT,
                    std::string("MediaWorker::completionCallback - Callback error: ") + e.what(), {});
            }
        }

        // Send webhook if configured
        if (!job.callback_url.empty()) {
            sendWebhookNotification(job, success);
        }
    }

    active_threads_--;
}

void MediaWorker::cleanupThread() {
    auto& queue = MediaQueue::getInstance();
    auto cleanup_interval = std::chrono::hours(config_.cleanup_interval_hours);

    while (running_) {
        // Wait for cleanup interval
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, cleanup_interval, [this]() { return !running_.load(); });

        if (!running_) {
            break;
        }

        lock.unlock();

        // Run cleanup
        try {
            int cleaned = queue.cleanup(config_.cleanup_days_old);
            if (cleaned > 0) {
                // Log cleanup activity
            }

            // Reset stuck jobs
            int reset = queue.resetStuckJobs(config_.job_timeout_minutes);
            if (reset > 0) {
                // Log reset activity
            }
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(ErrorCategory::CONTENT,
                std::string("MediaWorker::cleanupThread - Cleanup error: ") + e.what(), {});
        }
    }
}

// ============================================================================
// JOB PROCESSING
// ============================================================================

bool MediaWorker::processJobInternal(MediaJob& job) {
    auto& queue = MediaQueue::getInstance();

    try {
        bool success = false;

        switch (job.type) {
            case JobType::VIDEO_GENERATION:
                success = processVideoGeneration(job);
                break;
            case JobType::IMAGE_PROCESSING:
                success = processImageProcessing(job);
                break;
            case JobType::SHARE_CARD:
                success = processShareCard(job);
                break;
            case JobType::THUMBNAIL:
                success = processThumbnail(job);
                break;
            case JobType::PLATFORM_TRANSCODE:
                success = processPlatformTranscode(job);
                break;
            case JobType::BATCH_PROCESS:
                success = processBatch(job);
                break;
            case JobType::CLEANUP:
                success = processCleanup(job);
                break;
        }

        if (success) {
            queue.updateStatus(job.id, JobStatus::COMPLETED, job.result_data);
        } else {
            // Check if we should retry
            if (job.retry_count < job.max_retries) {
                jobs_retried_++;
                queue.retry(job.id);
            } else {
                queue.updateStatus(job.id, JobStatus::FAILED, Json::Value(), job.error_message);
            }
        }

        return success;

    } catch (const std::exception& e) {
        std::string error = std::string("Job processing error: ") + e.what();
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaWorker::processJobInternal - ") + error, {});

        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = error;
        }

        job.error_message = error;

        // Check if we should retry
        if (job.retry_count < job.max_retries) {
            jobs_retried_++;
            queue.retry(job.id);
        } else {
            queue.updateStatus(job.id, JobStatus::FAILED, Json::Value(), error);
        }

        return false;
    }
}

bool MediaWorker::processVideoGeneration(MediaJob& job) {
    auto& generator = VideoGenerator::getInstance();
    auto& queue = MediaQueue::getInstance();

    // Ensure generator is initialized
    if (!generator.isInitialized()) {
        if (!generator.initializeFromConfig()) {
            job.error_message = "VideoGenerator not initialized";
            return false;
        }
    }

    // Extract photos from input data
    std::vector<std::string> photos;
    if (job.input_data.isMember("photos") && job.input_data["photos"].isArray()) {
        for (const auto& photo : job.input_data["photos"]) {
            photos.push_back(photo.asString());
        }
    }

    if (photos.empty()) {
        job.error_message = "No photos provided for video generation";
        return false;
    }

    // Build dog info
    DogVideoInfo dog_info;
    dog_info.id = job.dog_id;

    if (job.input_data.isMember("dog_info")) {
        dog_info = DogVideoInfo::fromJson(job.input_data["dog_info"]);
    }

    // Get template
    std::string template_id = job.input_data.get("template_id", "slideshow").asString();
    TemplateType template_type = TemplateType::SLIDESHOW;

    if (template_id == "ken_burns") template_type = TemplateType::KEN_BURNS;
    else if (template_id == "countdown") template_type = TemplateType::COUNTDOWN;
    else if (template_id == "story") template_type = TemplateType::STORY;
    else if (template_id == "promotional") template_type = TemplateType::PROMOTIONAL;
    else if (template_id == "quick_share") template_type = TemplateType::QUICK_SHARE;
    else if (template_id == "adoption_appeal") template_type = TemplateType::ADOPTION_APPEAL;

    VideoTemplate video_template = VideoTemplate::get(template_type);

    // Progress callback
    auto progress_callback = [&job, &queue](const std::string& stage, double progress) {
        queue.updateProgress(job.id, progress, stage);
    };

    // Generate video
    auto result = generator.generateFromPhotos(
        photos,
        dog_info,
        video_template,
        "",  // Auto-generate output path
        VideoGenerationOptions::defaults(),
        progress_callback
    );

    if (result.success) {
        job.result_data["output_path"] = result.output_path;
        job.result_data["thumbnail_path"] = result.thumbnail_path;
        job.result_data["duration"] = result.duration;
        job.result_data["width"] = result.width;
        job.result_data["height"] = result.height;
        job.result_data["file_size"] = static_cast<Json::Int64>(result.file_size);
        return true;
    } else {
        job.error_message = result.error_message;
        return false;
    }
}

bool MediaWorker::processImageProcessing(MediaJob& job) {
    auto& processor = ImageProcessor::getInstance();
    auto& queue = MediaQueue::getInstance();

    // Ensure processor is initialized
    if (!processor.isInitialized()) {
        if (!processor.initializeFromConfig()) {
            job.error_message = "ImageProcessor not initialized";
            return false;
        }
    }

    std::string image_path = job.input_data.get("image_path", "").asString();
    std::string operation = job.input_data.get("operation", "enhance").asString();

    if (image_path.empty()) {
        job.error_message = "No image path provided";
        return false;
    }

    queue.updateProgress(job.id, 10.0, "Processing image");

    ImageResult result;

    if (operation == "enhance") {
        EnhanceOptions options;
        if (job.input_data.isMember("options")) {
            options = EnhanceOptions::fromJson(job.input_data["options"]);
        }
        result = processor.enhance(image_path, "", options);
    } else if (operation == "wait_time_overlay") {
        std::string wait_time = job.input_data.get("wait_time", "00:00:00:00:00:00").asString();
        result = processor.addWaitTimeOverlay(image_path, "", wait_time);
    } else if (operation == "urgency_badge") {
        std::string urgency = job.input_data.get("urgency_level", "normal").asString();
        result = processor.addUrgencyBadge(image_path, "", urgency);
    } else if (operation == "resize") {
        int width = job.input_data.get("width", 800).asInt();
        int height = job.input_data.get("height", 600).asInt();
        Dimensions dims;
        dims.width = width;
        dims.height = height;
        result = processor.resize(image_path, "", dims);
    } else if (operation == "optimize") {
        OptimizeOptions options;
        if (job.input_data.isMember("options")) {
            options = OptimizeOptions::fromJson(job.input_data["options"]);
        }
        result = processor.optimize(image_path, "", options);
    } else {
        job.error_message = "Unknown operation: " + operation;
        return false;
    }

    queue.updateProgress(job.id, 100.0, "Complete");

    if (result.success) {
        job.result_data["output_path"] = result.output_path;
        job.result_data["width"] = result.width;
        job.result_data["height"] = result.height;
        job.result_data["file_size"] = static_cast<Json::Int64>(result.file_size);
        return true;
    } else {
        job.error_message = result.error_message;
        return false;
    }
}

bool MediaWorker::processShareCard(MediaJob& job) {
    auto& processor = ImageProcessor::getInstance();
    auto& queue = MediaQueue::getInstance();

    if (!processor.isInitialized()) {
        if (!processor.initializeFromConfig()) {
            job.error_message = "ImageProcessor not initialized";
            return false;
        }
    }

    queue.updateProgress(job.id, 10.0, "Generating share card");

    // Build dog info
    DogInfo dog_info;
    dog_info.id = job.dog_id;

    if (job.input_data.isMember("dog_info")) {
        dog_info = DogInfo::fromJson(job.input_data["dog_info"]);
    }

    std::string photo_path = job.input_data.get("photo_path", "").asString();
    std::string platform = job.input_data.get("platform", "facebook").asString();

    ShareCardOptions options;
    if (platform == "twitter") {
        options = ShareCardOptions::forTwitter();
    } else if (platform == "instagram") {
        options = ShareCardOptions::forInstagram();
    } else {
        options = ShareCardOptions::forFacebook();
    }

    queue.updateProgress(job.id, 30.0, "Creating card layout");

    auto result = processor.generateShareCardFromPhoto(photo_path, dog_info, "", options);

    queue.updateProgress(job.id, 100.0, "Complete");

    if (result.success) {
        job.result_data["output_path"] = result.output_path;
        job.result_data["width"] = result.width;
        job.result_data["height"] = result.height;
        job.result_data["file_size"] = static_cast<Json::Int64>(result.file_size);
        return true;
    } else {
        job.error_message = result.error_message;
        return false;
    }
}

bool MediaWorker::processThumbnail(MediaJob& job) {
    auto& generator = VideoGenerator::getInstance();
    auto& queue = MediaQueue::getInstance();

    if (!generator.isInitialized()) {
        if (!generator.initializeFromConfig()) {
            job.error_message = "VideoGenerator not initialized";
            return false;
        }
    }

    std::string video_path = job.input_data.get("video_path", "").asString();

    if (video_path.empty()) {
        job.error_message = "No video path provided";
        return false;
    }

    queue.updateProgress(job.id, 20.0, "Extracting thumbnail");

    auto result = generator.generateBestThumbnail(video_path);

    queue.updateProgress(job.id, 100.0, "Complete");

    if (result.success) {
        job.result_data["thumbnail_path"] = result.thumbnail_path;
        job.result_data["width"] = result.width;
        job.result_data["height"] = result.height;
        return true;
    } else {
        job.error_message = result.error_message;
        return false;
    }
}

bool MediaWorker::processPlatformTranscode(MediaJob& job) {
    auto& generator = VideoGenerator::getInstance();
    auto& queue = MediaQueue::getInstance();

    if (!generator.isInitialized()) {
        if (!generator.initializeFromConfig()) {
            job.error_message = "VideoGenerator not initialized";
            return false;
        }
    }

    std::string video_path = job.input_data.get("video_path", "").asString();

    if (video_path.empty()) {
        job.error_message = "No video path provided";
        return false;
    }

    // Get platforms
    std::vector<std::string> platforms;
    if (job.input_data.isMember("platforms") && job.input_data["platforms"].isArray()) {
        for (const auto& platform : job.input_data["platforms"]) {
            platforms.push_back(platform.asString());
        }
    }

    if (platforms.empty()) {
        platforms = {"youtube", "instagram", "tiktok", "facebook"};
    }

    // Generate output directory
    std::string output_dir = "media/videos/" + job.dog_id + "/platforms";

    int total = static_cast<int>(platforms.size());
    int current = 0;

    Json::Value results(Json::objectValue);

    for (const auto& platform : platforms) {
        queue.updateProgress(
            job.id,
            (current * 100.0) / total,
            "Processing for " + platform
        );

        std::string output_path = output_dir + "/" + platform + ".mp4";
        auto result = generator.processForPlatform(video_path, output_path, platform);

        if (result.success) {
            results[platform]["success"] = true;
            results[platform]["output_path"] = result.output_path;
            results[platform]["file_size"] = static_cast<Json::Int64>(result.file_size);
        } else {
            results[platform]["success"] = false;
            results[platform]["error"] = result.error_message;
        }

        current++;
    }

    queue.updateProgress(job.id, 100.0, "Complete");

    job.result_data["platforms"] = results;
    return true;
}

bool MediaWorker::processBatch(MediaJob& job) {
    auto& queue = MediaQueue::getInstance();

    // Batch processing creates multiple sub-jobs
    if (!job.input_data.isMember("jobs") || !job.input_data["jobs"].isArray()) {
        job.error_message = "No jobs array in batch";
        return false;
    }

    int total = job.input_data["jobs"].size();
    int completed = 0;
    int failed = 0;

    Json::Value results(Json::arrayValue);

    for (const auto& sub_job_json : job.input_data["jobs"]) {
        MediaJob sub_job = MediaJob::fromJson(sub_job_json);
        sub_job.dog_id = job.dog_id;

        queue.updateProgress(
            job.id,
            (completed * 100.0) / total,
            "Processing job " + std::to_string(completed + 1) + "/" + std::to_string(total)
        );

        bool success = processJobInternal(sub_job);

        Json::Value result;
        result["success"] = success;
        result["job_type"] = jobTypeToString(sub_job.type);
        if (success) {
            result["result"] = sub_job.result_data;
        } else {
            result["error"] = sub_job.error_message;
            failed++;
        }
        results.append(result);

        completed++;
    }

    queue.updateProgress(job.id, 100.0, "Complete");

    job.result_data["results"] = results;
    job.result_data["total"] = total;
    job.result_data["completed"] = completed;
    job.result_data["failed"] = failed;

    return failed == 0;
}

bool MediaWorker::processCleanup(MediaJob& job) {
    auto& queue = MediaQueue::getInstance();
    auto& storage = MediaStorage::getInstance();

    queue.updateProgress(job.id, 10.0, "Cleaning up old jobs");

    int days_old = job.input_data.get("days_old", 7).asInt();

    // Clean up old jobs
    int jobs_cleaned = queue.cleanup(days_old);

    queue.updateProgress(job.id, 50.0, "Cleaning up expired media");

    // Clean up expired media
    int media_cleaned = 0;
    if (storage.isInitialized()) {
        media_cleaned = storage.deleteExpiredMedia();
    }

    queue.updateProgress(job.id, 100.0, "Complete");

    job.result_data["jobs_cleaned"] = jobs_cleaned;
    job.result_data["media_cleaned"] = media_cleaned;

    return true;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void MediaWorker::sendWebhookNotification(const MediaJob& job, bool success) {
    // Build webhook payload
    Json::Value payload;
    payload["job_id"] = job.id;
    payload["status"] = success ? "completed" : "failed";
    payload["dog_id"] = job.dog_id;
    payload["job_type"] = jobTypeToString(job.type);

    if (success) {
        payload["result"] = job.result_data;
    } else {
        payload["error"] = job.error_message;
    }

    Json::StreamWriterBuilder writer;
    std::string json_body = Json::writeString(writer, payload);

    // Send webhook using curl
    std::string command = "curl -s -X POST -H 'Content-Type: application/json' -d '" +
        json_body + "' '" + job.callback_url + "'";

    int result = std::system(command.c_str());
    if (result != 0) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT,
            "MediaWorker::sendWebhookNotification - Webhook failed for job " + job.id, {});
    }
}

bool MediaWorker::isTypeEnabled(JobType type) const {
    switch (type) {
        case JobType::VIDEO_GENERATION:
            return video_enabled_;
        case JobType::IMAGE_PROCESSING:
            return image_enabled_;
        case JobType::SHARE_CARD:
            return share_card_enabled_;
        case JobType::THUMBNAIL:
            return thumbnail_enabled_;
        case JobType::PLATFORM_TRANSCODE:
            return transcode_enabled_;
        case JobType::BATCH_PROCESS:
            return batch_enabled_;
        case JobType::CLEANUP:
            return cleanup_enabled_;
        default:
            return false;
    }
}

void MediaWorker::updateProgress(const std::string& job_id, double progress, const std::string& stage) {
    MediaQueue::getInstance().updateProgress(job_id, progress, stage);
}

} // namespace wtl::content::media

/**
 * @file VideoGenerator.cc
 * @brief Implementation of video generation from photos
 * @see VideoGenerator.h for documentation
 */

#include "content/media/VideoGenerator.h"

// Standard library includes
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>

// Project includes
#include "content/media/FFmpegWrapper.h"
#include "content/media/ImageProcessor.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::media {

namespace fs = std::filesystem;

// ============================================================================
// VIDEO GENERATION OPTIONS IMPLEMENTATION
// ============================================================================

Json::Value VideoGenerationOptions::toJson() const {
    Json::Value json;
    json["preprocess_photos"] = preprocess_photos;
    json["add_wait_time_overlay"] = add_wait_time_overlay;
    json["add_urgency_badge"] = add_urgency_badge;
    json["add_music"] = add_music;
    json["add_captions"] = add_captions;
    json["include_intro"] = include_intro;
    json["include_outro"] = include_outro;
    return json;
}

VideoGenerationOptions VideoGenerationOptions::fromJson(const Json::Value& json) {
    VideoGenerationOptions opts;
    opts.preprocess_photos = json.get("preprocess_photos", true).asBool();
    opts.add_wait_time_overlay = json.get("add_wait_time_overlay", true).asBool();
    opts.add_urgency_badge = json.get("add_urgency_badge", true).asBool();
    opts.add_music = json.get("add_music", true).asBool();
    opts.add_captions = json.get("add_captions", true).asBool();
    opts.include_intro = json.get("include_intro", false).asBool();
    opts.include_outro = json.get("include_outro", false).asBool();
    return opts;
}

VideoGenerationOptions VideoGenerationOptions::defaults() {
    return VideoGenerationOptions();
}

VideoGenerationOptions VideoGenerationOptions::minimal() {
    VideoGenerationOptions opts;
    opts.preprocess_photos = false;
    opts.add_wait_time_overlay = true;
    opts.add_urgency_badge = false;
    opts.add_music = false;
    opts.add_captions = false;
    opts.include_intro = false;
    opts.include_outro = false;
    return opts;
}

VideoGenerationOptions VideoGenerationOptions::full() {
    VideoGenerationOptions opts;
    opts.preprocess_photos = true;
    opts.add_wait_time_overlay = true;
    opts.add_urgency_badge = true;
    opts.add_music = true;
    opts.add_captions = true;
    opts.include_intro = true;
    opts.include_outro = true;
    return opts;
}

// ============================================================================
// VIDEO RESULT IMPLEMENTATION
// ============================================================================

Json::Value VideoResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["output_path"] = output_path;
    json["thumbnail_path"] = thumbnail_path;
    json["error_message"] = error_message;
    json["duration"] = duration;
    json["width"] = width;
    json["height"] = height;
    json["file_size"] = static_cast<Json::Int64>(file_size);
    json["processing_time_ms"] = static_cast<Json::Int64>(processing_time.count());
    return json;
}

// ============================================================================
// DOG VIDEO INFO IMPLEMENTATION
// ============================================================================

Json::Value DogVideoInfo::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["breed"] = breed;
    json["age"] = age;
    json["gender"] = gender;
    json["size"] = size;
    json["description"] = description;
    json["wait_time"] = wait_time;
    json["urgency_level"] = urgency_level;
    json["shelter_name"] = shelter_name;
    json["profile_url"] = profile_url;
    if (countdown) {
        json["countdown"] = *countdown;
    }
    return json;
}

DogVideoInfo DogVideoInfo::fromJson(const Json::Value& json) {
    DogVideoInfo info;
    info.id = json.get("id", "").asString();
    info.name = json.get("name", "").asString();
    info.breed = json.get("breed", "").asString();
    info.age = json.get("age", "").asString();
    info.gender = json.get("gender", "").asString();
    info.size = json.get("size", "").asString();
    info.description = json.get("description", "").asString();
    info.wait_time = json.get("wait_time", "").asString();
    info.urgency_level = json.get("urgency_level", "normal").asString();
    info.shelter_name = json.get("shelter_name", "").asString();
    info.profile_url = json.get("profile_url", "").asString();
    if (json.isMember("countdown") && !json["countdown"].isNull()) {
        info.countdown = json["countdown"].asString();
    }
    return info;
}

// ============================================================================
// VIDEO GENERATOR IMPLEMENTATION
// ============================================================================

VideoGenerator& VideoGenerator::getInstance() {
    static VideoGenerator instance;
    return instance;
}

VideoGenerator::~VideoGenerator() {
    cleanupTempFiles();
}

bool VideoGenerator::initialize(
    const std::string& assets_dir,
    const std::string& temp_dir,
    const std::string& output_dir
) {
    std::lock_guard<std::mutex> lock(mutex_);

    assets_dir_ = assets_dir;
    temp_dir_ = temp_dir;
    output_dir_ = output_dir;
    music_dir_ = assets_dir + "/music";

    // Create directories
    try {
        if (!fs::exists(temp_dir_)) {
            fs::create_directories(temp_dir_);
        }
        if (!fs::exists(output_dir_)) {
            fs::create_directories(output_dir_);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::FILE_IO,
            "Failed to create directories: " + std::string(e.what()),
            {{"temp_dir", temp_dir_}}
        );
        return false;
    }

    // Verify FFmpeg is available
    if (!FFmpegWrapper::getInstance().isAvailable()) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "FFmpeg not available for video generation",
            {}
        );
        return false;
    }

    initialized_ = true;
    return true;
}

bool VideoGenerator::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string assets = config.getString("media.video_assets_dir", "static/video");
        std::string temp = config.getString("media.temp_dir", "/tmp/wtl_media");
        std::string output = config.getString("media.video_output_dir", "media/videos");

        return initialize(assets, temp, output);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize VideoGenerator from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

bool VideoGenerator::isInitialized() const {
    return initialized_.load();
}

VideoResult VideoGenerator::generateFromPhotos(
    const std::vector<std::string>& photos,
    const DogVideoInfo& dog,
    const VideoTemplate& template_config,
    const std::string& output_path,
    const VideoGenerationOptions& options,
    VideoProgressCallback progress
) {
    VideoResult result;
    auto start_time = std::chrono::steady_clock::now();
    cancelled_ = false;

    // Validate inputs
    if (photos.empty()) {
        result.error_message = "No photos provided";
        generation_errors_++;
        return result;
    }

    if (static_cast<int>(photos.size()) < template_config.min_photos) {
        result.error_message = "Not enough photos. Minimum required: " +
                               std::to_string(template_config.min_photos);
        generation_errors_++;
        return result;
    }

    // Generate output path if not provided
    std::string final_output = output_path;
    if (final_output.empty()) {
        final_output = generateOutputPath(dog.id, "mp4");
    }

    try {
        // Stage 1: Preprocess photos
        if (progress) progress("Preprocessing photos", 0.0);

        Dimensions target_dims{template_config.output.width, template_config.output.height};
        std::vector<std::string> processed_photos = preprocessPhotos(
            photos, target_dims, options.preprocess_photos
        );

        if (cancelled_) {
            result.error_message = "Generation cancelled";
            return result;
        }

        if (progress) progress("Preprocessing photos", 100.0);

        // Stage 2: Build base video
        if (progress) progress("Building video", 0.0);

        std::string base_video = getTempPath("mp4");
        VideoResult build_result = buildVideo(
            processed_photos, template_config, base_video,
            [&progress](const std::string& stage, double p) {
                if (progress) progress("Building video", p);
            }
        );

        if (!build_result.success) {
            result.error_message = "Failed to build video: " + build_result.error_message;
            generation_errors_++;
            return result;
        }

        if (cancelled_) {
            result.error_message = "Generation cancelled";
            return result;
        }

        std::string current_video = base_video;

        // Stage 3: Apply overlays
        if (options.add_wait_time_overlay || options.add_urgency_badge) {
            if (progress) progress("Adding overlays", 0.0);

            std::string overlayed_video = getTempPath("mp4");
            VideoResult overlay_result = applyOverlays(
                current_video, overlayed_video, dog, template_config.overlay_config
            );

            if (overlay_result.success) {
                current_video = overlayed_video;
            }

            if (progress) progress("Adding overlays", 100.0);
        }

        if (cancelled_) {
            result.error_message = "Generation cancelled";
            return result;
        }

        // Stage 4: Apply text placements
        if (options.add_captions && !template_config.text_placements.empty()) {
            if (progress) progress("Adding captions", 0.0);

            std::string captioned_video = getTempPath("mp4");
            VideoResult text_result = applyTextPlacements(
                current_video, captioned_video, template_config, dog
            );

            if (text_result.success) {
                current_video = captioned_video;
            }

            if (progress) progress("Adding captions", 100.0);
        }

        if (cancelled_) {
            result.error_message = "Generation cancelled";
            return result;
        }

        // Stage 5: Add music
        if (options.add_music && template_config.music.style != MusicStyle::NONE) {
            if (progress) progress("Adding music", 0.0);

            std::string music_video = getTempPath("mp4");
            VideoResult music_result = addMusic(
                current_video, music_video, "", template_config.music
            );

            if (music_result.success) {
                current_video = music_video;
            }

            if (progress) progress("Adding music", 100.0);
        }

        if (cancelled_) {
            result.error_message = "Generation cancelled";
            return result;
        }

        // Stage 6: Copy to final output
        if (progress) progress("Finalizing", 0.0);

        try {
            fs::copy_file(current_video, final_output, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            result.error_message = "Failed to save output: " + std::string(e.what());
            generation_errors_++;
            return result;
        }

        // Get video info
        auto& ffmpeg = FFmpegWrapper::getInstance();
        MediaInfo info = ffmpeg.getMediaInfo(final_output);

        result.success = true;
        result.output_path = final_output;
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(final_output);

        // Generate thumbnail
        std::string thumb_path = final_output;
        size_t ext_pos = thumb_path.rfind('.');
        if (ext_pos != std::string::npos) {
            thumb_path = thumb_path.substr(0, ext_pos) + "_thumb.jpg";
        }

        auto thumb_result = generateThumbnail(final_output, thumb_path);
        if (thumb_result.success) {
            result.thumbnail_path = thumb_result.thumbnail_path;
            thumbnails_generated_++;
        }

        if (progress) progress("Finalizing", 100.0);

        auto end_time = std::chrono::steady_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );

        videos_generated_++;
        total_processing_time_ms_ += result.processing_time.count();

    } catch (const std::exception& e) {
        result.error_message = "Unexpected error: " + std::string(e.what());
        generation_errors_++;

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Video generation failed: " + std::string(e.what()),
            {{"dog_id", dog.id}}
        );
    }

    return result;
}

VideoResult VideoGenerator::generateWithTemplate(
    const std::vector<std::string>& photos,
    const DogVideoInfo& dog,
    TemplateType template_type,
    const std::string& output_path
) {
    VideoTemplate template_config = VideoTemplate::get(template_type);
    return generateFromPhotos(photos, dog, template_config, output_path);
}

VideoResult VideoGenerator::addOverlay(
    const std::string& video_path,
    const std::string& output_path,
    const OverlayConfig& overlay_config,
    const std::string& wait_time,
    const std::optional<std::string>& countdown
) {
    VideoResult result;

    if (!fs::exists(video_path)) {
        result.error_message = "Input video not found: " + video_path;
        return result;
    }

    auto& ffmpeg = FFmpegWrapper::getInstance();

    // Build filter complex for overlays
    std::ostringstream filter;
    std::vector<std::string> filter_parts;

    // LED wait time counter
    if (overlay_config.led_counter.visible && !wait_time.empty()) {
        auto& cfg = overlay_config.led_counter;

        std::ostringstream text_filter;
        text_filter << "drawtext=text='" << wait_time << "'";
        text_filter << ":fontcolor=" << cfg.digit_color.toFFmpegFormat();
        text_filter << ":fontsize=" << cfg.font_size;

        // Position
        if (cfg.position == Position::TOP_RIGHT) {
            text_filter << ":x=w-text_w-" << cfg.margin_x;
            text_filter << ":y=" << cfg.margin_y;
        } else if (cfg.position == Position::TOP_LEFT) {
            text_filter << ":x=" << cfg.margin_x;
            text_filter << ":y=" << cfg.margin_y;
        }

        text_filter << ":box=1:boxcolor=" << cfg.background_color.toFFmpegFormat();
        text_filter << ":boxborderw=" << cfg.padding;

        filter_parts.push_back(text_filter.str());
    }

    // Countdown counter
    if (overlay_config.countdown_counter.visible && countdown.has_value()) {
        auto& cfg = overlay_config.countdown_counter;

        std::ostringstream text_filter;
        text_filter << "drawtext=text='" << *countdown << "'";
        text_filter << ":fontcolor=" << cfg.digit_color.toFFmpegFormat();
        text_filter << ":fontsize=" << cfg.font_size;

        if (cfg.position == Position::TOP_LEFT) {
            text_filter << ":x=" << cfg.margin_x;
            text_filter << ":y=" << cfg.margin_y;
        } else if (cfg.position == Position::TOP_CENTER) {
            text_filter << ":x=(w-text_w)/2";
            text_filter << ":y=" << cfg.margin_y;
        }

        text_filter << ":box=1:boxcolor=" << cfg.background_color.toFFmpegFormat();
        text_filter << ":boxborderw=" << cfg.padding;

        filter_parts.push_back(text_filter.str());
    }

    // Urgency badge
    if (overlay_config.urgency_badge.visible) {
        auto& cfg = overlay_config.urgency_badge;
        auto [bg_color, text_color, border_color] = cfg.getAutoColors();

        std::string badge_text = cfg.urgency_level;
        std::transform(badge_text.begin(), badge_text.end(), badge_text.begin(), ::toupper);

        // Draw background box
        std::ostringstream box_filter;
        int box_x = cfg.margin_x;
        int box_y = cfg.margin_y;

        box_filter << "drawbox=x=" << box_x << ":y=" << box_y;
        box_filter << ":w=" << cfg.size.width << ":h=" << cfg.size.height;
        box_filter << ":color=" << bg_color.toFFmpegFormat() << ":t=fill";
        filter_parts.push_back(box_filter.str());

        // Draw text
        std::ostringstream text_filter;
        text_filter << "drawtext=text='" << badge_text << "'";
        text_filter << ":fontcolor=" << text_color.toFFmpegFormat();
        text_filter << ":fontsize=" << cfg.font_size;
        text_filter << ":x=" << (box_x + cfg.size.width / 2) << "-text_w/2";
        text_filter << ":y=" << (box_y + cfg.size.height / 2) << "-text_h/2";
        filter_parts.push_back(text_filter.str());
    }

    if (filter_parts.empty()) {
        // No overlays to add, just copy
        try {
            fs::copy_file(video_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy video: " + std::string(e.what());
            return result;
        }
    }

    // Join filter parts
    for (size_t i = 0; i < filter_parts.size(); ++i) {
        if (i > 0) filter << ",";
        filter << filter_parts[i];
    }

    // Execute FFmpeg
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << video_path << "\" -vf \"" << filter.str()
        << "\" -c:a copy \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        generation_errors_++;
        return result;
    }

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int exit_code = pclose(pipe);

    if (exit_code == 0 && fs::exists(output_path)) {
        result.success = true;
        result.output_path = output_path;

        MediaInfo info = ffmpeg.getMediaInfo(output_path);
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(output_path);
    } else {
        result.error_message = "Failed to add overlays";
        generation_errors_++;
    }

    return result;
}

VideoResult VideoGenerator::addWaitTimeCounter(
    const std::string& video_path,
    const std::string& output_path,
    const std::string& wait_time,
    const LEDCounterConfig& config
) {
    OverlayConfig overlay;
    overlay.led_counter = config;
    overlay.countdown_counter.visible = false;
    overlay.urgency_badge.visible = false;

    return addOverlay(video_path, output_path, overlay, wait_time);
}

VideoResult VideoGenerator::addMusic(
    const std::string& video_path,
    const std::string& output_path,
    const std::string& music_id,
    const MusicConfig& config
) {
    VideoResult result;

    // Get music file path
    std::string music_path = music_id.empty() ? config.getMusicFilePath() : music_id;

    if (music_path.empty() || !fs::exists(music_path)) {
        // No music file, just copy video
        try {
            fs::copy_file(video_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy video: " + std::string(e.what());
            return result;
        }
    }

    auto& ffmpeg = FFmpegWrapper::getInstance();

    AudioOptions audio_opts;
    audio_opts.audio_file = music_path;
    audio_opts.volume = config.volume;
    audio_opts.loop = config.loop;
    audio_opts.fade_in = config.fade_in;
    audio_opts.fade_out = config.fade_out;
    audio_opts.fade_duration = config.fade_duration;

    CommandResult cmd_result = ffmpeg.addAudio(video_path, output_path, audio_opts);

    if (cmd_result.success) {
        result.success = true;
        result.output_path = output_path;

        MediaInfo info = ffmpeg.getMediaInfo(output_path);
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(output_path);
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

VideoResult VideoGenerator::addMusicByStyle(
    const std::string& video_path,
    const std::string& output_path,
    MusicStyle style,
    float volume
) {
    MusicConfig config;
    config.style = style;
    config.volume = volume;
    config.fade_in = true;
    config.fade_out = true;

    return addMusic(video_path, output_path, "", config);
}

VideoResult VideoGenerator::addText(
    const std::string& video_path,
    const std::string& output_path,
    const TextOverlayConfig& text_config
) {
    VideoResult result;

    if (!text_config.visible || text_config.text.empty()) {
        try {
            fs::copy_file(video_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy video: " + std::string(e.what());
            return result;
        }
    }

    auto& ffmpeg = FFmpegWrapper::getInstance();
    CommandResult cmd_result = ffmpeg.addTextOverlay(video_path, output_path, text_config);

    if (cmd_result.success) {
        result.success = true;
        result.output_path = output_path;

        MediaInfo info = ffmpeg.getMediaInfo(output_path);
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(output_path);
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

VideoResult VideoGenerator::addTextOverlays(
    const std::string& video_path,
    const std::string& output_path,
    const std::vector<TextOverlayConfig>& text_configs
) {
    VideoResult result;

    if (text_configs.empty()) {
        try {
            fs::copy_file(video_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy video: " + std::string(e.what());
            return result;
        }
    }

    std::string current_input = video_path;

    for (size_t i = 0; i < text_configs.size(); ++i) {
        std::string temp_output = (i == text_configs.size() - 1) ?
                                  output_path : getTempPath("mp4");

        result = addText(current_input, temp_output, text_configs[i]);

        if (!result.success) {
            return result;
        }

        current_input = temp_output;
    }

    return result;
}

VideoResult VideoGenerator::generateThumbnail(
    const std::string& video_path,
    const std::string& output_path,
    const ThumbnailOptions& options
) {
    VideoResult result;

    std::string thumb_output = output_path;
    if (thumb_output.empty()) {
        thumb_output = getTempPath("jpg");
    }

    auto& ffmpeg = FFmpegWrapper::getInstance();
    CommandResult cmd_result = ffmpeg.extractThumbnail(video_path, thumb_output, options);

    if (cmd_result.success) {
        result.success = true;
        result.thumbnail_path = thumb_output;
        result.file_size = fs::file_size(thumb_output);

        auto& processor = ImageProcessor::getInstance();
        Dimensions dims = processor.getImageDimensions(thumb_output);
        result.width = dims.width;
        result.height = dims.height;
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

VideoResult VideoGenerator::generateBestThumbnail(
    const std::string& video_path,
    const std::string& output_path
) {
    VideoResult result;

    std::string thumb_output = output_path;
    if (thumb_output.empty()) {
        thumb_output = getTempPath("jpg");
    }

    auto& ffmpeg = FFmpegWrapper::getInstance();
    CommandResult cmd_result = ffmpeg.extractBestThumbnail(video_path, thumb_output);

    if (cmd_result.success) {
        result.success = true;
        result.thumbnail_path = thumb_output;
        result.file_size = fs::file_size(thumb_output);
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

VideoResult VideoGenerator::processForPlatform(
    const std::string& video_path,
    const std::string& output_path,
    const std::string& platform
) {
    VideoResult result;

    auto& ffmpeg = FFmpegWrapper::getInstance();
    TranscodeOptions opts = TranscodeOptions::forPlatform(platform);

    CommandResult cmd_result = ffmpeg.transcode(video_path, output_path, opts);

    if (cmd_result.success) {
        result.success = true;
        result.output_path = output_path;

        MediaInfo info = ffmpeg.getMediaInfo(output_path);
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(output_path);
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

std::map<std::string, VideoResult> VideoGenerator::processForPlatforms(
    const std::string& video_path,
    const std::string& output_dir,
    const std::vector<std::string>& platforms
) {
    std::map<std::string, VideoResult> results;

    try {
        if (!fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }
    } catch (const std::exception& e) {
        VideoResult error_result;
        error_result.error_message = "Failed to create output directory";
        for (const auto& platform : platforms) {
            results[platform] = error_result;
        }
        return results;
    }

    std::string base_name = fs::path(video_path).stem().string();

    for (const auto& platform : platforms) {
        std::string output_path = output_dir + "/" + base_name + "_" + platform + ".mp4";
        results[platform] = processForPlatform(video_path, output_path, platform);
    }

    return results;
}

std::vector<std::string> VideoGenerator::getAvailableMusic() const {
    std::vector<std::string> music;

    try {
        if (fs::exists(music_dir_)) {
            for (const auto& entry : fs::directory_iterator(music_dir_)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".mp3" || ext == ".wav" || ext == ".m4a") {
                        music.push_back(entry.path().stem().string());
                    }
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return music;
}

std::string VideoGenerator::getMusicPath(MusicStyle style) const {
    MusicConfig config;
    config.style = style;
    return config.getMusicFilePath();
}

std::string VideoGenerator::generateOutputPath(
    const std::string& dog_id,
    const std::string& extension
) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    std::ostringstream path;
    path << output_dir_ << "/" << dog_id << "_" << timestamp << "." << extension;

    return path.str();
}

void VideoGenerator::cleanupTempFiles() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& path : temp_files_) {
        try {
            if (fs::exists(path)) {
                fs::remove(path);
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    temp_files_.clear();
}

void VideoGenerator::cancelGeneration() {
    cancelled_ = true;
    FFmpegWrapper::getInstance().cancelCurrentOperation();
}

bool VideoGenerator::isCancelled() const {
    return cancelled_.load();
}

Json::Value VideoGenerator::getStats() const {
    Json::Value stats;
    stats["videos_generated"] = static_cast<Json::UInt64>(videos_generated_.load());
    stats["thumbnails_generated"] = static_cast<Json::UInt64>(thumbnails_generated_.load());
    stats["generation_errors"] = static_cast<Json::UInt64>(generation_errors_.load());
    stats["total_processing_time_ms"] = static_cast<Json::UInt64>(total_processing_time_ms_.load());
    stats["temp_files_count"] = static_cast<Json::UInt>(temp_files_.size());
    return stats;
}

std::vector<std::string> VideoGenerator::preprocessPhotos(
    const std::vector<std::string>& photos,
    const Dimensions& dimensions,
    bool enhance
) {
    std::vector<std::string> processed;
    auto& processor = ImageProcessor::getInstance();

    for (const auto& photo : photos) {
        if (!fs::exists(photo)) {
            continue;
        }

        std::string output = getTempPath("jpg");

        // Crop and resize to target dimensions
        auto result = processor.cropAndResize(photo, output, dimensions);

        if (result.success) {
            if (enhance) {
                std::string enhanced = getTempPath("jpg");
                auto enhance_result = processor.autoEnhance(output, enhanced);
                if (enhance_result.success) {
                    processed.push_back(enhanced);
                } else {
                    processed.push_back(output);
                }
            } else {
                processed.push_back(output);
            }
        }
    }

    return processed;
}

VideoResult VideoGenerator::buildVideo(
    const std::vector<std::string>& photos,
    const VideoTemplate& template_config,
    const std::string& output_path,
    VideoProgressCallback progress
) {
    VideoResult result;

    auto& ffmpeg = FFmpegWrapper::getInstance();

    ConcatOptions opts;
    opts.duration_per_image = template_config.photo_duration;
    opts.transition = template_config.transition;
    opts.motion = template_config.motion;
    opts.output_width = template_config.output.width;
    opts.output_height = template_config.output.height;
    opts.fps = template_config.output.fps;
    opts.codec = template_config.output.codec;
    opts.crf = template_config.output.crf;
    opts.preset = template_config.output.preset;

    CommandResult cmd_result = ffmpeg.concat(
        photos, output_path, opts,
        [&progress](double p, int eta) {
            if (progress) progress("Encoding", p);
        }
    );

    if (cmd_result.success) {
        result.success = true;
        result.output_path = output_path;

        MediaInfo info = ffmpeg.getMediaInfo(output_path);
        result.duration = info.duration;
        result.width = info.width;
        result.height = info.height;
        result.file_size = fs::file_size(output_path);
    } else {
        result.error_message = cmd_result.error_message;
    }

    return result;
}

VideoResult VideoGenerator::applyOverlays(
    const std::string& video_path,
    const std::string& output_path,
    const DogVideoInfo& dog,
    const OverlayConfig& overlay_config
) {
    return addOverlay(video_path, output_path, overlay_config, dog.wait_time, dog.countdown);
}

VideoResult VideoGenerator::applyTextPlacements(
    const std::string& video_path,
    const std::string& output_path,
    const VideoTemplate& template_config,
    const DogVideoInfo& dog
) {
    std::vector<TextOverlayConfig> text_configs;

    for (const auto& placement : template_config.text_placements) {
        TextOverlayConfig config = placement.text_config;

        // Replace placeholders
        config.text = replacePlaceholders(config.text, dog);

        // Set timing
        config.start_time = placement.start_time;
        if (placement.end_time > 0) {
            config.display_duration = placement.end_time - placement.start_time;
        }

        if (!config.text.empty()) {
            text_configs.push_back(config);
        }
    }

    return addTextOverlays(video_path, output_path, text_configs);
}

std::string VideoGenerator::replacePlaceholders(
    const std::string& text,
    const DogVideoInfo& dog
) {
    std::string result = text;

    // Replace all placeholders
    auto replace = [&result](const std::string& placeholder, const std::string& value) {
        size_t pos;
        while ((pos = result.find(placeholder)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
        }
    };

    replace("{{dog_name}}", dog.name);
    replace("{{breed}}", dog.breed);
    replace("{{age}}", dog.age);
    replace("{{gender}}", dog.gender);
    replace("{{size}}", dog.size);
    replace("{{wait_time}}", dog.wait_time);
    replace("{{urgency_level}}", dog.urgency_level);
    replace("{{shelter_name}}", dog.shelter_name);

    if (dog.countdown) {
        replace("{{countdown}}", *dog.countdown);
    }

    return result;
}

std::string VideoGenerator::getTempPath(const std::string& extension) {
    std::lock_guard<std::mutex> lock(mutex_);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(100000, 999999);

    std::string filename = "vid_" + std::to_string(dist(gen)) + "." + extension;
    std::string path = temp_dir_ + "/" + filename;

    temp_files_.push_back(path);
    return path;
}

} // namespace wtl::content::media

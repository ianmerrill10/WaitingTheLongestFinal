/**
 * @file ImageProcessor.cc
 * @brief Implementation of image processing operations
 * @see ImageProcessor.h for documentation
 */

#include "content/media/ImageProcessor.h"

// Standard library includes
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

// Project includes
#include "content/media/FFmpegWrapper.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::media {

namespace fs = std::filesystem;

// ============================================================================
// ENHANCE OPTIONS IMPLEMENTATION
// ============================================================================

Json::Value EnhanceOptions::toJson() const {
    Json::Value json;
    json["auto_levels"] = auto_levels;
    json["auto_contrast"] = auto_contrast;
    json["auto_saturation"] = auto_saturation;
    json["brightness_adjust"] = brightness_adjust;
    json["contrast_adjust"] = contrast_adjust;
    json["saturation_adjust"] = saturation_adjust;
    json["sharpness"] = sharpness;
    json["denoise"] = denoise;
    json["remove_red_eye"] = remove_red_eye;
    return json;
}

EnhanceOptions EnhanceOptions::fromJson(const Json::Value& json) {
    EnhanceOptions opts;
    opts.auto_levels = json.get("auto_levels", true).asBool();
    opts.auto_contrast = json.get("auto_contrast", true).asBool();
    opts.auto_saturation = json.get("auto_saturation", false).asBool();
    opts.brightness_adjust = static_cast<float>(json.get("brightness_adjust", 0.0).asDouble());
    opts.contrast_adjust = static_cast<float>(json.get("contrast_adjust", 0.0).asDouble());
    opts.saturation_adjust = static_cast<float>(json.get("saturation_adjust", 0.0).asDouble());
    opts.sharpness = static_cast<float>(json.get("sharpness", 0.0).asDouble());
    opts.denoise = json.get("denoise", false).asBool();
    opts.remove_red_eye = json.get("remove_red_eye", false).asBool();
    return opts;
}

EnhanceOptions EnhanceOptions::autoEnhance() {
    EnhanceOptions opts;
    opts.auto_levels = true;
    opts.auto_contrast = true;
    opts.sharpness = 0.3f;
    return opts;
}

EnhanceOptions EnhanceOptions::subtle() {
    EnhanceOptions opts;
    opts.auto_levels = false;
    opts.auto_contrast = false;
    opts.brightness_adjust = 0.05f;
    opts.contrast_adjust = 0.1f;
    opts.saturation_adjust = 0.05f;
    opts.sharpness = 0.2f;
    return opts;
}

// ============================================================================
// OPTIMIZE OPTIONS IMPLEMENTATION
// ============================================================================

Json::Value OptimizeOptions::toJson() const {
    Json::Value json;
    json["quality"] = quality;
    json["strip_metadata"] = strip_metadata;
    json["progressive"] = progressive;
    json["max_file_size_kb"] = max_file_size_kb;
    return json;
}

OptimizeOptions OptimizeOptions::fromJson(const Json::Value& json) {
    OptimizeOptions opts;
    opts.quality = json.get("quality", 85).asInt();
    opts.strip_metadata = json.get("strip_metadata", true).asBool();
    opts.progressive = json.get("progressive", true).asBool();
    opts.max_file_size_kb = json.get("max_file_size_kb", 0).asInt();
    return opts;
}

OptimizeOptions OptimizeOptions::forWeb() {
    OptimizeOptions opts;
    opts.quality = 82;
    opts.strip_metadata = true;
    opts.progressive = true;
    return opts;
}

OptimizeOptions OptimizeOptions::highQuality() {
    OptimizeOptions opts;
    opts.quality = 95;
    opts.strip_metadata = false;
    opts.progressive = true;
    return opts;
}

// ============================================================================
// SHARE CARD OPTIONS IMPLEMENTATION
// ============================================================================

Json::Value ShareCardOptions::toJson() const {
    Json::Value json;
    json["width"] = width;
    json["height"] = height;
    json["template_style"] = template_style;
    json["show_wait_time"] = show_wait_time;
    json["show_urgency"] = show_urgency;
    json["show_shelter_logo"] = show_shelter_logo;
    json["show_qr_code"] = show_qr_code;
    json["background_color"] = background_color.toJson();
    json["font_family"] = font_family;
    return json;
}

ShareCardOptions ShareCardOptions::fromJson(const Json::Value& json) {
    ShareCardOptions opts;
    opts.width = json.get("width", 1200).asInt();
    opts.height = json.get("height", 630).asInt();
    opts.template_style = json.get("template_style", "default").asString();
    opts.show_wait_time = json.get("show_wait_time", true).asBool();
    opts.show_urgency = json.get("show_urgency", true).asBool();
    opts.show_shelter_logo = json.get("show_shelter_logo", false).asBool();
    opts.show_qr_code = json.get("show_qr_code", false).asBool();
    if (json.isMember("background_color")) {
        opts.background_color = Color::fromJson(json["background_color"]);
    }
    opts.font_family = json.get("font_family", "Arial").asString();
    return opts;
}

ShareCardOptions ShareCardOptions::forFacebook() {
    ShareCardOptions opts;
    opts.width = 1200;
    opts.height = 630;
    return opts;
}

ShareCardOptions ShareCardOptions::forTwitter() {
    ShareCardOptions opts;
    opts.width = 1200;
    opts.height = 675;  // Twitter card ratio
    return opts;
}

ShareCardOptions ShareCardOptions::forInstagram() {
    ShareCardOptions opts;
    opts.width = 1080;
    opts.height = 1080;  // Square format
    return opts;
}

// ============================================================================
// IMAGE RESULT IMPLEMENTATION
// ============================================================================

Json::Value ImageResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["output_path"] = output_path;
    json["error_message"] = error_message;
    json["width"] = width;
    json["height"] = height;
    json["file_size"] = static_cast<Json::Int64>(file_size);
    return json;
}

// ============================================================================
// DOG INFO IMPLEMENTATION
// ============================================================================

Json::Value DogInfo::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["breed"] = breed;
    json["age"] = age;
    json["wait_time"] = wait_time;
    json["urgency_level"] = urgency_level;
    json["shelter_name"] = shelter_name;
    json["photo_url"] = photo_url;
    json["profile_url"] = profile_url;
    if (countdown) {
        json["countdown"] = *countdown;
    }
    return json;
}

DogInfo DogInfo::fromJson(const Json::Value& json) {
    DogInfo info;
    info.id = json.get("id", "").asString();
    info.name = json.get("name", "").asString();
    info.breed = json.get("breed", "").asString();
    info.age = json.get("age", "").asString();
    info.wait_time = json.get("wait_time", "").asString();
    info.urgency_level = json.get("urgency_level", "normal").asString();
    info.shelter_name = json.get("shelter_name", "").asString();
    info.photo_url = json.get("photo_url", "").asString();
    info.profile_url = json.get("profile_url", "").asString();
    if (json.isMember("countdown") && !json["countdown"].isNull()) {
        info.countdown = json["countdown"].asString();
    }
    return info;
}

// ============================================================================
// IMAGE PROCESSOR IMPLEMENTATION
// ============================================================================

ImageProcessor& ImageProcessor::getInstance() {
    static ImageProcessor instance;
    return instance;
}

ImageProcessor::~ImageProcessor() {
    cleanupTempFiles();
}

bool ImageProcessor::initialize(
    const std::string& assets_dir,
    const std::string& temp_dir
) {
    std::lock_guard<std::mutex> lock(mutex_);

    assets_dir_ = assets_dir;
    temp_dir_ = temp_dir;
    overlays_dir_ = assets_dir + "/overlays";

    // Create directories if they do not exist
    try {
        if (!fs::exists(temp_dir_)) {
            fs::create_directories(temp_dir_);
        }
        if (!fs::exists(overlays_dir_)) {
            fs::create_directories(overlays_dir_);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::FILE_IO,
            "Failed to create directories: " + std::string(e.what()),
            {{"temp_dir", temp_dir_}}
        );
        return false;
    }

    // Ensure FFmpeg is available
    if (!FFmpegWrapper::getInstance().isAvailable()) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "FFmpeg not available for image processing",
            {}
        );
        return false;
    }

    initialized_ = true;
    return true;
}

bool ImageProcessor::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string assets = config.getString("media.assets_dir", "static/images");
        std::string temp = config.getString("media.temp_dir", "/tmp/wtl_media");

        return initialize(assets, temp);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize ImageProcessor from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

bool ImageProcessor::isInitialized() const {
    return initialized_.load();
}

ImageResult ImageProcessor::enhance(
    const std::string& input_path,
    const std::string& output_path,
    const EnhanceOptions& options
) {
    ImageResult result;

    if (!fs::exists(input_path)) {
        result.error_message = "Input file does not exist: " + input_path;
        processing_errors_++;
        return result;
    }

    // Build enhancement filter
    std::string filter = buildEnhanceFilter(options);

    // Execute with FFmpeg
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);

    if (!filter.empty()) {
        args.push_back("-vf");
        args.push_back(filter);
    }

    args.push_back(output_path);

    // Use FFmpegWrapper's low-level execution
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\"";
    if (!filter.empty()) {
        cmd << " -vf \"" << filter << "\"";
    }
    cmd << " \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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

        // Get output dimensions
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);

        images_processed_++;
    } else {
        result.error_message = "Enhancement failed: " + output;
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::autoEnhance(
    const std::string& input_path,
    const std::string& output_path
) {
    return enhance(input_path, output_path, EnhanceOptions::autoEnhance());
}

ImageResult ImageProcessor::addWaitTimeOverlay(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& wait_time,
    const LEDCounterConfig& config
) {
    ImageResult result;

    if (!config.visible) {
        // Just copy the file
        try {
            fs::copy_file(input_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy file: " + std::string(e.what());
            return result;
        }
    }

    // Get input image dimensions
    Dimensions input_dims = getImageDimensions(input_path);
    if (input_dims.width == 0) {
        result.error_message = "Failed to get input image dimensions";
        return result;
    }

    // Calculate overlay position
    int overlay_width = static_cast<int>(wait_time.length() * config.font_size * 0.6);
    int overlay_height = config.font_size + config.padding * 2;

    Coordinates pos = calculatePosition(
        config.position,
        overlay_width,
        overlay_height,
        input_dims.width,
        input_dims.height,
        config.margin_x,
        config.margin_y
    );

    if (config.position == Position::CUSTOM) {
        pos = config.custom_position;
    }

    // Build FFmpeg filter for text overlay
    std::ostringstream filter;
    filter << "drawtext=text='" << wait_time << "'";
    filter << ":fontcolor=" << config.digit_color.toFFmpegFormat();
    filter << ":fontsize=" << config.font_size;
    filter << ":x=" << pos.x;
    filter << ":y=" << pos.y;
    filter << ":box=1:boxcolor=" << config.background_color.toFFmpegFormat();
    filter << ":boxborderw=" << config.padding;

    // Execute
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\" -vf \"" << filter.str()
        << "\" \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = "Failed to add wait time overlay";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::addUrgencyBadge(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& urgency_level,
    const UrgencyBadgeConfig& config
) {
    ImageResult result;

    if (!config.visible) {
        try {
            fs::copy_file(input_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy file: " + std::string(e.what());
            return result;
        }
    }

    // Get badge colors
    auto [bg_color, text_color, border_color] = config.getAutoColors();

    if (config.background_color) bg_color = *config.background_color;
    if (config.text_color) text_color = *config.text_color;

    // Get input dimensions
    Dimensions input_dims = getImageDimensions(input_path);

    // Calculate position
    Coordinates pos = calculatePosition(
        config.position,
        config.size.width,
        config.size.height,
        input_dims.width,
        input_dims.height,
        config.margin_x,
        config.margin_y
    );

    // Format urgency text
    std::string badge_text = urgency_level;
    std::transform(badge_text.begin(), badge_text.end(), badge_text.begin(), ::toupper);

    // Build filter
    std::ostringstream filter;

    // Draw background rectangle
    filter << "drawbox=x=" << pos.x << ":y=" << pos.y
           << ":w=" << config.size.width << ":h=" << config.size.height
           << ":color=" << bg_color.toFFmpegFormat()
           << ":t=fill";

    // Add text
    filter << ",drawtext=text='" << badge_text << "'";
    filter << ":fontcolor=" << text_color.toFFmpegFormat();
    filter << ":fontsize=" << config.font_size;
    filter << ":x=" << (pos.x + config.size.width / 2) << "-text_w/2";
    filter << ":y=" << (pos.y + config.size.height / 2) << "-text_h/2";

    // Execute
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\" -vf \"" << filter.str()
        << "\" \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = "Failed to add urgency badge";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::addTextOverlay(
    const std::string& input_path,
    const std::string& output_path,
    const TextOverlayConfig& config
) {
    ImageResult result;

    if (!config.visible || config.text.empty()) {
        try {
            fs::copy_file(input_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy file: " + std::string(e.what());
            return result;
        }
    }

    Dimensions input_dims = getImageDimensions(input_path);

    // Build drawtext filter
    std::ostringstream filter;
    filter << "drawtext=text='" << config.text << "'";
    filter << ":fontcolor=" << config.text_color.toFFmpegFormat();
    filter << ":fontsize=" << config.font_size;

    // Position
    if (config.position == Position::CENTER) {
        filter << ":x=(w-text_w)/2:y=(h-text_h)/2";
    } else if (config.position == Position::BOTTOM_CENTER) {
        filter << ":x=(w-text_w)/2:y=h-text_h-" << config.margin_y;
    } else if (config.position == Position::TOP_CENTER) {
        filter << ":x=(w-text_w)/2:y=" << config.margin_y;
    } else if (config.position == Position::CUSTOM) {
        filter << ":x=" << config.custom_position.x << ":y=" << config.custom_position.y;
    } else {
        // Calculate for other positions
        Coordinates pos = calculatePosition(
            config.position, 0, 0, input_dims.width, input_dims.height,
            config.margin_x, config.margin_y
        );
        filter << ":x=" << pos.x << ":y=" << pos.y;
    }

    // Background box
    if (config.show_background) {
        filter << ":box=1:boxcolor=" << config.background_color.toFFmpegFormat();
        filter << ":boxborderw=" << config.background_padding;
    }

    // Shadow
    if (config.enable_shadow) {
        filter << ":shadowcolor=" << config.shadow_color.toFFmpegFormat();
        filter << ":shadowx=" << config.shadow_offset_x;
        filter << ":shadowy=" << config.shadow_offset_y;
    }

    // Execute
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\" -vf \"" << filter.str()
        << "\" \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = "Failed to add text overlay";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::addOverlays(
    const std::string& input_path,
    const std::string& output_path,
    const OverlayConfig& overlay_config,
    const std::string& wait_time,
    const std::optional<std::string>& countdown
) {
    ImageResult result;

    if (!overlay_config.enable_overlays) {
        try {
            fs::copy_file(input_path, output_path, fs::copy_options::overwrite_existing);
            result.success = true;
            result.output_path = output_path;
            return result;
        } catch (const std::exception& e) {
            result.error_message = "Failed to copy file: " + std::string(e.what());
            return result;
        }
    }

    // Process overlays in sequence using temp files
    std::string current_input = input_path;
    std::string temp_output;
    int step = 0;

    // Add wait time counter
    if (overlay_config.led_counter.visible && !wait_time.empty()) {
        temp_output = getTempPath("jpg");
        auto wait_result = addWaitTimeOverlay(
            current_input, temp_output, wait_time, overlay_config.led_counter
        );

        if (!wait_result.success) {
            result.error_message = "Failed at wait time overlay: " + wait_result.error_message;
            return result;
        }

        current_input = temp_output;
        step++;
    }

    // Add countdown counter
    if (overlay_config.countdown_counter.visible && countdown.has_value()) {
        temp_output = getTempPath("jpg");
        auto countdown_result = addWaitTimeOverlay(
            current_input, temp_output, *countdown, overlay_config.countdown_counter
        );

        if (!countdown_result.success) {
            result.error_message = "Failed at countdown overlay: " + countdown_result.error_message;
            return result;
        }

        current_input = temp_output;
        step++;
    }

    // Add urgency badge
    if (overlay_config.urgency_badge.visible) {
        temp_output = getTempPath("jpg");
        auto badge_result = addUrgencyBadge(
            current_input, temp_output,
            overlay_config.urgency_badge.urgency_level,
            overlay_config.urgency_badge
        );

        if (!badge_result.success) {
            result.error_message = "Failed at urgency badge: " + badge_result.error_message;
            return result;
        }

        current_input = temp_output;
        step++;
    }

    // Add text overlays
    for (const auto& text_config : overlay_config.text_overlays) {
        if (!text_config.visible || text_config.text.empty()) continue;

        temp_output = getTempPath("jpg");
        auto text_result = addTextOverlay(current_input, temp_output, text_config);

        if (!text_result.success) {
            result.error_message = "Failed at text overlay: " + text_result.error_message;
            return result;
        }

        current_input = temp_output;
        step++;
    }

    // Copy final result to output path
    try {
        if (current_input != input_path) {
            fs::copy_file(current_input, output_path, fs::copy_options::overwrite_existing);
        } else {
            fs::copy_file(input_path, output_path, fs::copy_options::overwrite_existing);
        }

        result.success = true;
        result.output_path = output_path;
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
    } catch (const std::exception& e) {
        result.error_message = "Failed to save final output: " + std::string(e.what());
    }

    return result;
}

ImageResult ImageProcessor::generateShareCard(
    const DogInfo& dog,
    const std::string& output_path,
    const ShareCardOptions& options
) {
    ImageResult result;

    // If we have a photo URL/path, use it
    if (!dog.photo_url.empty() && fs::exists(dog.photo_url)) {
        return generateShareCardFromPhoto(dog.photo_url, dog, output_path, options);
    }

    // Generate a simple text-based card
    // Create a solid color background
    std::ostringstream cmd;
    cmd << "ffmpeg -y -f lavfi -i color=c="
        << options.background_color.toFFmpegFormat() << ":s="
        << options.width << "x" << options.height << ":d=1";

    // Add dog name
    cmd << " -vf \"drawtext=text='" << dog.name << "'";
    cmd << ":fontsize=72:fontcolor=black:x=(w-text_w)/2:y=h/3";

    // Add breed
    cmd << ",drawtext=text='" << dog.breed << "'";
    cmd << ":fontsize=36:fontcolor=gray:x=(w-text_w)/2:y=h/3+100";

    // Add wait time if enabled
    if (options.show_wait_time && !dog.wait_time.empty()) {
        cmd << ",drawtext=text='Waiting\\: " << dog.wait_time << "'";
        cmd << ":fontsize=48:fontcolor=green:x=(w-text_w)/2:y=h*2/3";
    }

    cmd << "\" -frames:v 1 \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        result.width = options.width;
        result.height = options.height;
        result.file_size = fs::file_size(output_path);
        share_cards_generated_++;
    } else {
        result.error_message = "Failed to generate share card";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::generateShareCardFromPhoto(
    const std::string& photo_path,
    const DogInfo& dog,
    const std::string& output_path,
    const ShareCardOptions& options
) {
    ImageResult result;

    // First, resize/crop photo to fit card dimensions
    std::string temp_photo = getTempPath("jpg");
    auto resize_result = cropAndResize(
        photo_path, temp_photo, Dimensions{options.width, options.height}
    );

    if (!resize_result.success) {
        result.error_message = "Failed to resize photo: " + resize_result.error_message;
        return result;
    }

    // Create overlay config for share card
    OverlayConfig overlay;
    overlay.enable_overlays = true;
    overlay.target_width = options.width;
    overlay.target_height = options.height;

    // Wait time counter
    if (options.show_wait_time) {
        overlay.led_counter = LEDCounterConfig::defaultWaitTime();
        overlay.led_counter.position = Position::TOP_RIGHT;
        overlay.led_counter.font_size = 36;
    } else {
        overlay.led_counter.visible = false;
    }

    // Urgency badge
    if (options.show_urgency) {
        overlay.urgency_badge.visible = true;
        overlay.urgency_badge.urgency_level = dog.urgency_level;
        overlay.urgency_badge.position = Position::TOP_LEFT;
    } else {
        overlay.urgency_badge.visible = false;
    }

    // Dog name text
    TextOverlayConfig name_text;
    name_text.visible = true;
    name_text.text = dog.name;
    name_text.position = Position::BOTTOM_CENTER;
    name_text.font_size = 48;
    name_text.text_color = Color::white();
    name_text.show_background = true;
    name_text.background_color = Color{0, 0, 0, 180};
    name_text.background_padding = 16;
    name_text.margin_y = 80;
    overlay.text_overlays.push_back(name_text);

    // Breed text
    TextOverlayConfig breed_text;
    breed_text.visible = true;
    breed_text.text = dog.breed + " | " + dog.age;
    breed_text.position = Position::BOTTOM_CENTER;
    breed_text.font_size = 24;
    breed_text.text_color = Color::white();
    breed_text.show_background = true;
    breed_text.background_color = Color{0, 0, 0, 150};
    breed_text.background_padding = 10;
    breed_text.margin_y = 30;
    overlay.text_overlays.push_back(breed_text);

    // Apply overlays
    result = addOverlays(temp_photo, output_path, overlay, dog.wait_time, dog.countdown);

    if (result.success) {
        share_cards_generated_++;
    }

    return result;
}

ImageResult ImageProcessor::resize(
    const std::string& input_path,
    const std::string& output_path,
    const Dimensions& dimensions,
    bool maintain_aspect
) {
    ImageResult result;

    auto& ffmpeg = FFmpegWrapper::getInstance();
    auto cmd_result = ffmpeg.resizeImage(
        input_path, output_path, dimensions.width, dimensions.height, maintain_aspect
    );

    if (cmd_result.success) {
        result.success = true;
        result.output_path = output_path;
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = cmd_result.error_message;
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::resizeToWidth(
    const std::string& input_path,
    const std::string& output_path,
    int width
) {
    return resize(input_path, output_path, Dimensions{width, -1}, true);
}

ImageResult ImageProcessor::resizeToHeight(
    const std::string& input_path,
    const std::string& output_path,
    int height
) {
    return resize(input_path, output_path, Dimensions{-1, height}, true);
}

ImageResult ImageProcessor::cropAndResize(
    const std::string& input_path,
    const std::string& output_path,
    const Dimensions& dimensions
) {
    ImageResult result;

    // Build filter for center crop and resize
    std::ostringstream filter;
    filter << "scale=w='if(gt(a," << (static_cast<double>(dimensions.width) / dimensions.height)
           << ")," << dimensions.height << "*a," << dimensions.width << ")'";
    filter << ":h='if(gt(a," << (static_cast<double>(dimensions.width) / dimensions.height)
           << ")," << dimensions.height << "," << dimensions.width << "/a)'";
    filter << ",crop=" << dimensions.width << ":" << dimensions.height;

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\" -vf \"" << filter.str()
        << "\" \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        result.width = dimensions.width;
        result.height = dimensions.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = "Failed to crop and resize";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::optimize(
    const std::string& input_path,
    const std::string& output_path,
    const OptimizeOptions& options
) {
    ImageResult result;

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\"";

    // Quality setting
    cmd << " -q:v " << (100 - options.quality) / 3;  // FFmpeg scale

    // Strip metadata
    if (options.strip_metadata) {
        cmd << " -map_metadata -1";
    }

    cmd << " \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);

        // Check max file size constraint
        if (options.max_file_size_kb > 0 &&
            result.file_size > options.max_file_size_kb * 1024) {
            // Re-compress with lower quality
            OptimizeOptions retry_opts = options;
            retry_opts.quality = std::max(50, options.quality - 15);
            retry_opts.max_file_size_kb = 0;  // Prevent recursion
            return optimize(output_path, output_path, retry_opts);
        }

        images_processed_++;
    } else {
        result.error_message = "Failed to optimize image";
        processing_errors_++;
    }

    return result;
}

ImageResult ImageProcessor::convert(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& format,
    int quality
) {
    ImageResult result;

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << input_path << "\"";

    if (format == "jpg" || format == "jpeg") {
        cmd << " -q:v " << (100 - quality) / 3;
    } else if (format == "webp") {
        cmd << " -quality " << quality;
    }

    cmd << " \"" << output_path << "\" 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute FFmpeg";
        processing_errors_++;
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
        auto dims = getImageDimensions(output_path);
        result.width = dims.width;
        result.height = dims.height;
        result.file_size = fs::file_size(output_path);
        images_processed_++;
    } else {
        result.error_message = "Failed to convert image";
        processing_errors_++;
    }

    return result;
}

Dimensions ImageProcessor::getImageDimensions(const std::string& path) {
    auto& ffmpeg = FFmpegWrapper::getInstance();
    MediaInfo info = ffmpeg.getMediaInfo(path);
    return Dimensions{info.width, info.height};
}

bool ImageProcessor::isValidImage(const std::string& path) {
    auto& ffmpeg = FFmpegWrapper::getInstance();
    return ffmpeg.isValidImage(path);
}

std::string ImageProcessor::getTempPath(const std::string& extension) {
    std::lock_guard<std::mutex> lock(mutex_);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(100000, 999999);

    std::string filename = "img_" + std::to_string(dist(gen)) + "." + extension;
    std::string path = temp_dir_ + "/" + filename;

    temp_files_.push_back(path);
    return path;
}

void ImageProcessor::cleanupTempFiles() {
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

Json::Value ImageProcessor::getStats() const {
    Json::Value stats;
    stats["images_processed"] = static_cast<Json::UInt64>(images_processed_.load());
    stats["share_cards_generated"] = static_cast<Json::UInt64>(share_cards_generated_.load());
    stats["processing_errors"] = static_cast<Json::UInt64>(processing_errors_.load());
    stats["temp_files_count"] = static_cast<Json::UInt>(temp_files_.size());
    return stats;
}

std::string ImageProcessor::buildEnhanceFilter(const EnhanceOptions& options) {
    std::ostringstream filter;
    std::vector<std::string> filters;

    // Auto levels/normalize
    if (options.auto_levels) {
        filters.push_back("normalize");
    }

    // Brightness and contrast
    float eq_brightness = options.brightness_adjust;
    float eq_contrast = 1.0f + options.contrast_adjust;
    float eq_saturation = 1.0f + options.saturation_adjust;

    if (eq_brightness != 0.0f || eq_contrast != 1.0f || eq_saturation != 1.0f) {
        std::ostringstream eq;
        eq << "eq=brightness=" << eq_brightness
           << ":contrast=" << eq_contrast
           << ":saturation=" << eq_saturation;
        filters.push_back(eq.str());
    }

    // Sharpness
    if (options.sharpness > 0) {
        float amount = options.sharpness * 2.0f;
        std::ostringstream sharp;
        sharp << "unsharp=5:5:" << amount << ":5:5:" << (amount / 2.0f);
        filters.push_back(sharp.str());
    }

    // Denoise
    if (options.denoise) {
        filters.push_back("hqdn3d=4:3:6:4");
    }

    // Join filters
    for (size_t i = 0; i < filters.size(); ++i) {
        if (i > 0) filter << ",";
        filter << filters[i];
    }

    return filter.str();
}

std::string ImageProcessor::getUrgencyBadgePath(const std::string& urgency_level) {
    return overlays_dir_ + "/urgency-" + urgency_level + ".png";
}

} // namespace wtl::content::media

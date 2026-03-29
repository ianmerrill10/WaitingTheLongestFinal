/**
 * @file FFmpegWrapper.cc
 * @brief Implementation of FFmpeg command wrapper
 * @see FFmpegWrapper.h for documentation
 */

#include "content/media/FFmpegWrapper.h"

// Standard library includes
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::media {

namespace fs = std::filesystem;

// ============================================================================
// COMMAND RESULT IMPLEMENTATION
// ============================================================================

Json::Value CommandResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["exit_code"] = exit_code;
    json["output_path"] = output_path;
    json["command"] = command;
    json["duration_ms"] = static_cast<Json::Int64>(duration.count());
    json["error_message"] = error_message;
    return json;
}

// ============================================================================
// MEDIA INFO IMPLEMENTATION
// ============================================================================

Json::Value MediaInfo::toJson() const {
    Json::Value json;
    json["path"] = path;
    json["format"] = format;
    json["duration"] = duration;
    json["width"] = width;
    json["height"] = height;
    json["video_codec"] = video_codec;
    json["audio_codec"] = audio_codec;
    json["fps"] = fps;
    json["bitrate"] = bitrate;
    json["file_size"] = static_cast<Json::Int64>(file_size);
    json["has_video"] = has_video;
    json["has_audio"] = has_audio;
    return json;
}

MediaInfo MediaInfo::fromFFprobeJson(const Json::Value& json) {
    MediaInfo info;

    if (json.isMember("format")) {
        const auto& fmt = json["format"];
        info.format = fmt.get("format_name", "").asString();
        info.duration = std::stod(fmt.get("duration", "0").asString());
        info.bitrate = std::stoi(fmt.get("bit_rate", "0").asString()) / 1000;
        info.file_size = std::stoll(fmt.get("size", "0").asString());
    }

    if (json.isMember("streams") && json["streams"].isArray()) {
        for (const auto& stream : json["streams"]) {
            std::string codec_type = stream.get("codec_type", "").asString();

            if (codec_type == "video") {
                info.has_video = true;
                info.video_codec = stream.get("codec_name", "").asString();
                info.width = stream.get("width", 0).asInt();
                info.height = stream.get("height", 0).asInt();

                // Parse frame rate (can be "30/1" format)
                std::string fps_str = stream.get("r_frame_rate", "0/1").asString();
                size_t slash = fps_str.find('/');
                if (slash != std::string::npos) {
                    int num = std::stoi(fps_str.substr(0, slash));
                    int den = std::stoi(fps_str.substr(slash + 1));
                    info.fps = den > 0 ? num / den : 0;
                }
            } else if (codec_type == "audio") {
                info.has_audio = true;
                info.audio_codec = stream.get("codec_name", "").asString();
            }
        }
    }

    return info;
}

// ============================================================================
// OPTIONS IMPLEMENTATIONS
// ============================================================================

Json::Value ConcatOptions::toJson() const {
    Json::Value json;
    json["duration_per_image"] = duration_per_image;
    json["transition"] = transition.toJson();
    json["motion"] = motion.toJson();
    json["output_width"] = output_width;
    json["output_height"] = output_height;
    json["fps"] = fps;
    json["codec"] = codec;
    json["crf"] = crf;
    json["preset"] = preset;
    json["pixel_format"] = pixel_format;
    return json;
}

ConcatOptions ConcatOptions::fromJson(const Json::Value& json) {
    ConcatOptions opts;
    opts.duration_per_image = json.get("duration_per_image", 5.0).asDouble();
    if (json.isMember("transition")) {
        opts.transition = TransitionConfig::fromJson(json["transition"]);
    }
    if (json.isMember("motion")) {
        opts.motion = MotionConfig::fromJson(json["motion"]);
    }
    opts.output_width = json.get("output_width", 1920).asInt();
    opts.output_height = json.get("output_height", 1080).asInt();
    opts.fps = json.get("fps", 30).asInt();
    opts.codec = json.get("codec", "libx264").asString();
    opts.crf = json.get("crf", 23).asInt();
    opts.preset = json.get("preset", "medium").asString();
    opts.pixel_format = json.get("pixel_format", "yuv420p").asString();
    return opts;
}

Json::Value OverlayOptions::toJson() const {
    Json::Value json;
    json["overlay_image"] = overlay_image;
    json["x"] = x;
    json["y"] = y;
    json["opacity"] = opacity;
    json["start_time"] = start_time;
    json["end_time"] = end_time;
    return json;
}

Json::Value AudioOptions::toJson() const {
    Json::Value json;
    json["audio_file"] = audio_file;
    json["volume"] = volume;
    json["loop"] = loop;
    json["fade_in"] = fade_in;
    json["fade_out"] = fade_out;
    json["fade_duration"] = fade_duration;
    json["codec"] = codec;
    json["bitrate"] = bitrate;
    return json;
}

Json::Value TranscodeOptions::toJson() const {
    Json::Value json;
    json["width"] = width;
    json["height"] = height;
    json["fps"] = fps;
    json["codec"] = codec;
    json["crf"] = crf;
    json["preset"] = preset;
    json["format"] = format;
    json["audio_codec"] = audio_codec;
    json["audio_bitrate"] = audio_bitrate;
    json["copy_audio"] = copy_audio;
    return json;
}

TranscodeOptions TranscodeOptions::forPlatform(const std::string& platform) {
    TranscodeOptions opts;

    if (platform == "youtube") {
        opts.width = 1920;
        opts.height = 1080;
        opts.fps = 30;
        opts.codec = "libx264";
        opts.crf = 21;
        opts.preset = "medium";
        opts.audio_bitrate = 192;
    } else if (platform == "instagram" || platform == "instagram_feed") {
        opts.width = 1080;
        opts.height = 1350;
        opts.fps = 30;
        opts.codec = "libx264";
        opts.crf = 23;
        opts.preset = "medium";
        opts.audio_bitrate = 128;
    } else if (platform == "instagram_stories" || platform == "tiktok" || platform == "reels") {
        opts.width = 1080;
        opts.height = 1920;
        opts.fps = 30;
        opts.codec = "libx264";
        opts.crf = 23;
        opts.preset = "fast";
        opts.audio_bitrate = 128;
    } else if (platform == "twitter") {
        opts.width = 1280;
        opts.height = 720;
        opts.fps = 30;
        opts.codec = "libx264";
        opts.crf = 23;
        opts.preset = "fast";
        opts.audio_bitrate = 128;
    } else if (platform == "facebook") {
        opts.width = 1280;
        opts.height = 720;
        opts.fps = 30;
        opts.codec = "libx264";
        opts.crf = 22;
        opts.preset = "medium";
        opts.audio_bitrate = 160;
    }

    return opts;
}

Json::Value ThumbnailOptions::toJson() const {
    Json::Value json;
    json["timestamp"] = timestamp;
    json["width"] = width;
    json["height"] = height;
    json["format"] = format;
    json["quality"] = quality;
    json["scale_to_fit"] = scale_to_fit;
    return json;
}

// ============================================================================
// FFMPEG WRAPPER IMPLEMENTATION
// ============================================================================

FFmpegWrapper& FFmpegWrapper::getInstance() {
    static FFmpegWrapper instance;
    return instance;
}

FFmpegWrapper::~FFmpegWrapper() {
    cleanupTempFiles();
}

bool FFmpegWrapper::initialize(
    const std::string& ffmpeg_path,
    const std::string& ffprobe_path,
    const std::string& temp_dir
) {
    std::lock_guard<std::mutex> lock(mutex_);

    ffmpeg_path_ = ffmpeg_path;
    ffprobe_path_ = ffprobe_path;
    temp_dir_ = temp_dir;

    // Create temp directory if it does not exist
    try {
        if (!fs::exists(temp_dir_)) {
            fs::create_directories(temp_dir_);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::FILE_IO,
            "Failed to create temp directory: " + std::string(e.what()),
            {{"temp_dir", temp_dir_}}
        );
        return false;
    }

    // Verify FFmpeg is available
    if (!isAvailable()) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "FFmpeg not found or not working",
            {{"ffmpeg_path", ffmpeg_path_}}
        );
        return false;
    }

    initialized_ = true;
    return true;
}

bool FFmpegWrapper::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string ffmpeg = config.getString("media.ffmpeg_path", "ffmpeg");
        std::string ffprobe = config.getString("media.ffprobe_path", "ffprobe");
        std::string temp = config.getString("media.temp_dir", "/tmp/wtl_media");

        return initialize(ffmpeg, ffprobe, temp);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize FFmpeg from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

bool FFmpegWrapper::isAvailable() const {
    std::string cmd = ffmpeg_path_ + " -version";

#ifdef _WIN32
    cmd += " 2>nul";
#else
    cmd += " 2>/dev/null";
#endif

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    return (status == 0) && (result.find("ffmpeg version") != std::string::npos);
}

std::string FFmpegWrapper::getVersion() const {
    std::string cmd = ffmpeg_path_ + " -version";

#ifdef _WIN32
    cmd += " 2>nul";
#else
    cmd += " 2>/dev/null";
#endif

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "unknown";

    char buffer[256];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }

    pclose(pipe);

    // Extract version from "ffmpeg version X.X.X ..."
    std::regex version_regex("ffmpeg version ([\\d\\.]+)");
    std::smatch match;
    if (std::regex_search(result, match, version_regex)) {
        return match[1].str();
    }

    return "unknown";
}

MediaInfo FFmpegWrapper::getMediaInfo(const std::string& path) {
    MediaInfo info;
    info.path = path;

    Json::Value probe_result = executeFFprobe(path);
    if (!probe_result.isNull()) {
        info = MediaInfo::fromFFprobeJson(probe_result);
        info.path = path;
    }

    return info;
}

bool FFmpegWrapper::isValidImage(const std::string& path) {
    if (!fs::exists(path)) return false;

    MediaInfo info = getMediaInfo(path);
    return info.has_video && !info.has_audio && info.duration == 0.0;
}

bool FFmpegWrapper::isValidVideo(const std::string& path) {
    if (!fs::exists(path)) return false;

    MediaInfo info = getMediaInfo(path);
    return info.has_video && info.duration > 0.0;
}

CommandResult FFmpegWrapper::concat(
    const std::vector<std::string>& images,
    const std::string& output_path,
    const ConcatOptions& options,
    ProgressCallback progress
) {
    if (images.empty()) {
        CommandResult result;
        result.success = false;
        result.error_message = "No images provided";
        return result;
    }

    // Build FFmpeg command
    std::vector<std::string> args;
    args.push_back("-y");  // Overwrite output

    // Add input images
    for (const auto& img : images) {
        args.push_back("-loop");
        args.push_back("1");
        args.push_back("-t");
        args.push_back(std::to_string(options.duration_per_image));
        args.push_back("-i");
        args.push_back(img);
    }

    // Build filter complex
    std::string filter = buildSlideshowFilter(static_cast<int>(images.size()), options);

    args.push_back("-filter_complex");
    args.push_back(filter);

    // Output options
    args.push_back("-c:v");
    args.push_back(options.codec);
    args.push_back("-crf");
    args.push_back(std::to_string(options.crf));
    args.push_back("-preset");
    args.push_back(options.preset);
    args.push_back("-pix_fmt");
    args.push_back(options.pixel_format);
    args.push_back("-r");
    args.push_back(std::to_string(options.fps));

    args.push_back(output_path);

    CommandResult result = executeCommand(args, progress);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::concatVideos(
    const std::vector<std::string>& videos,
    const std::string& output_path,
    double transition_duration,
    ProgressCallback progress
) {
    if (videos.empty()) {
        CommandResult result;
        result.success = false;
        result.error_message = "No videos provided";
        return result;
    }

    // Create a concat list file
    std::string list_path = getTempFilePath("txt");
    std::ofstream list_file(list_path);

    for (const auto& video : videos) {
        list_file << "file '" << video << "'\n";
    }
    list_file.close();

    temp_files_.push_back(list_path);

    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-f");
    args.push_back("concat");
    args.push_back("-safe");
    args.push_back("0");
    args.push_back("-i");
    args.push_back(list_path);
    args.push_back("-c");
    args.push_back("copy");
    args.push_back(output_path);

    CommandResult result = executeCommand(args, progress);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::overlay(
    const std::string& video_path,
    const std::string& output_path,
    const OverlayOptions& options
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);
    args.push_back("-i");
    args.push_back(options.overlay_image);

    // Build overlay filter
    std::ostringstream filter;
    filter << "[1:v]format=rgba";

    if (options.opacity < 1.0f) {
        filter << ",colorchannelmixer=aa=" << options.opacity;
    }

    filter << "[ovr];[0:v][ovr]overlay=" << options.x << ":" << options.y;

    if (options.start_time > 0 || options.end_time > 0) {
        filter << ":enable='between(t," << options.start_time << ",";
        if (options.end_time > 0) {
            filter << options.end_time;
        } else {
            filter << "999999";
        }
        filter << ")'";
    }

    args.push_back("-filter_complex");
    args.push_back(filter.str());
    args.push_back("-c:a");
    args.push_back("copy");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::overlayMultiple(
    const std::string& video_path,
    const std::string& output_path,
    const std::vector<OverlayOptions>& overlays
) {
    if (overlays.empty()) {
        CommandResult result;
        result.success = false;
        result.error_message = "No overlays provided";
        return result;
    }

    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);

    // Add all overlay inputs
    for (const auto& ovr : overlays) {
        args.push_back("-i");
        args.push_back(ovr.overlay_image);
    }

    // Build filter complex for all overlays
    std::ostringstream filter;
    std::string last_output = "[0:v]";

    for (size_t i = 0; i < overlays.size(); ++i) {
        const auto& ovr = overlays[i];
        std::string ovr_input = "[" + std::to_string(i + 1) + ":v]";
        std::string output_label = "[ovr" + std::to_string(i) + "]";

        // Format overlay input
        filter << ovr_input << "format=rgba";
        if (ovr.opacity < 1.0f) {
            filter << ",colorchannelmixer=aa=" << ovr.opacity;
        }
        filter << "[scaled" << i << "];";

        // Apply overlay
        filter << last_output << "[scaled" << i << "]overlay=" << ovr.x << ":" << ovr.y;

        if (ovr.start_time > 0 || ovr.end_time > 0) {
            filter << ":enable='between(t," << ovr.start_time << ",";
            if (ovr.end_time > 0) {
                filter << ovr.end_time;
            } else {
                filter << "999999";
            }
            filter << ")'";
        }

        if (i < overlays.size() - 1) {
            filter << output_label << ";";
            last_output = output_label;
        }
    }

    args.push_back("-filter_complex");
    args.push_back(filter.str());
    args.push_back("-c:a");
    args.push_back("copy");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::addTextOverlay(
    const std::string& video_path,
    const std::string& output_path,
    const TextOverlayConfig& text_config
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);

    // Build drawtext filter
    std::ostringstream filter;
    filter << "drawtext=";
    filter << "text='" << escapeFilterString(text_config.text) << "'";
    filter << ":" << buildFontSpec(text_config);
    filter << ":fontcolor=" << text_config.text_color.toFFmpegFormat();
    filter << ":fontsize=" << text_config.font_size;

    // Position
    if (text_config.position == Position::CENTER) {
        filter << ":x=(w-text_w)/2:y=(h-text_h)/2";
    } else if (text_config.position == Position::BOTTOM_CENTER) {
        filter << ":x=(w-text_w)/2:y=h-text_h-" << text_config.margin_y;
    } else if (text_config.position == Position::TOP_CENTER) {
        filter << ":x=(w-text_w)/2:y=" << text_config.margin_y;
    } else if (text_config.position == Position::CUSTOM) {
        filter << ":x=" << text_config.custom_position.x << ":y=" << text_config.custom_position.y;
    }

    // Shadow
    if (text_config.enable_shadow) {
        filter << ":shadowcolor=" << text_config.shadow_color.toFFmpegFormat();
        filter << ":shadowx=" << text_config.shadow_offset_x;
        filter << ":shadowy=" << text_config.shadow_offset_y;
    }

    // Background box
    if (text_config.show_background) {
        filter << ":box=1:boxcolor=" << text_config.background_color.toFFmpegFormat();
        filter << ":boxborderw=" << text_config.background_padding;
    }

    // Timing
    if (text_config.start_time > 0 || text_config.display_duration > 0) {
        filter << ":enable='between(t," << text_config.start_time << ",";
        if (text_config.display_duration > 0) {
            filter << (text_config.start_time + text_config.display_duration);
        } else {
            filter << "999999";
        }
        filter << ")'";
    }

    args.push_back("-vf");
    args.push_back(filter.str());
    args.push_back("-c:a");
    args.push_back("copy");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::addAudio(
    const std::string& video_path,
    const std::string& output_path,
    const AudioOptions& options
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);

    if (options.loop) {
        args.push_back("-stream_loop");
        args.push_back("-1");
    }

    args.push_back("-i");
    args.push_back(options.audio_file);

    // Get video duration for proper trimming
    MediaInfo video_info = getMediaInfo(video_path);

    // Audio filter for volume and fades
    std::ostringstream audio_filter;
    audio_filter << "[1:a]";

    bool has_filter = false;

    if (options.volume != 1.0f) {
        audio_filter << "volume=" << options.volume;
        has_filter = true;
    }

    if (options.fade_in) {
        if (has_filter) audio_filter << ",";
        audio_filter << "afade=t=in:st=0:d=" << options.fade_duration;
        has_filter = true;
    }

    if (options.fade_out && video_info.duration > 0) {
        if (has_filter) audio_filter << ",";
        double fade_start = video_info.duration - options.fade_duration;
        audio_filter << "afade=t=out:st=" << fade_start << ":d=" << options.fade_duration;
        has_filter = true;
    }

    if (has_filter) {
        audio_filter << "[aout]";
        args.push_back("-filter_complex");
        args.push_back(audio_filter.str());
        args.push_back("-map");
        args.push_back("0:v");
        args.push_back("-map");
        args.push_back("[aout]");
    } else {
        args.push_back("-map");
        args.push_back("0:v");
        args.push_back("-map");
        args.push_back("1:a");
    }

    args.push_back("-c:v");
    args.push_back("copy");
    args.push_back("-c:a");
    args.push_back(options.codec);
    args.push_back("-b:a");
    args.push_back(std::to_string(options.bitrate) + "k");
    args.push_back("-shortest");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::replaceAudio(
    const std::string& video_path,
    const std::string& audio_path,
    const std::string& output_path
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);
    args.push_back("-i");
    args.push_back(audio_path);
    args.push_back("-map");
    args.push_back("0:v");
    args.push_back("-map");
    args.push_back("1:a");
    args.push_back("-c:v");
    args.push_back("copy");
    args.push_back("-c:a");
    args.push_back("aac");
    args.push_back("-shortest");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::removeAudio(
    const std::string& video_path,
    const std::string& output_path
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);
    args.push_back("-c:v");
    args.push_back("copy");
    args.push_back("-an");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::transcode(
    const std::string& input_path,
    const std::string& output_path,
    const TranscodeOptions& options,
    ProgressCallback progress
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);

    // Video options
    std::ostringstream vf;
    bool has_vf = false;

    if (options.width > 0 || options.height > 0) {
        vf << "scale=";
        if (options.width > 0) {
            vf << options.width;
        } else {
            vf << "-2";
        }
        vf << ":";
        if (options.height > 0) {
            vf << options.height;
        } else {
            vf << "-2";
        }
        has_vf = true;
    }

    if (has_vf) {
        args.push_back("-vf");
        args.push_back(vf.str());
    }

    args.push_back("-c:v");
    args.push_back(options.codec);
    args.push_back("-crf");
    args.push_back(std::to_string(options.crf));
    args.push_back("-preset");
    args.push_back(options.preset);

    if (options.fps > 0) {
        args.push_back("-r");
        args.push_back(std::to_string(options.fps));
    }

    // Audio options
    if (options.copy_audio) {
        args.push_back("-c:a");
        args.push_back("copy");
    } else {
        args.push_back("-c:a");
        args.push_back(options.audio_codec);
        args.push_back("-b:a");
        args.push_back(std::to_string(options.audio_bitrate) + "k");
    }

    args.push_back(output_path);

    CommandResult result = executeCommand(args, progress);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::resize(
    const std::string& input_path,
    const std::string& output_path,
    int width,
    int height,
    bool maintain_aspect
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);

    std::ostringstream vf;
    vf << "scale=";

    if (maintain_aspect) {
        vf << width << ":" << height << ":force_original_aspect_ratio=decrease";
        vf << ",pad=" << width << ":" << height << ":(ow-iw)/2:(oh-ih)/2";
    } else {
        vf << width << ":" << height;
    }

    args.push_back("-vf");
    args.push_back(vf.str());
    args.push_back("-c:a");
    args.push_back("copy");
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::extractThumbnail(
    const std::string& video_path,
    const std::string& output_path,
    const ThumbnailOptions& options
) {
    std::vector<std::string> args;
    args.push_back("-y");

    if (options.timestamp >= 0) {
        args.push_back("-ss");
        args.push_back(std::to_string(options.timestamp));
    }

    args.push_back("-i");
    args.push_back(video_path);
    args.push_back("-vframes");
    args.push_back("1");

    // Scale filter
    std::ostringstream vf;
    vf << "scale=";
    if (options.scale_to_fit) {
        vf << options.width << ":" << options.height << ":force_original_aspect_ratio=decrease";
    } else {
        vf << options.width << ":" << options.height;
    }

    args.push_back("-vf");
    args.push_back(vf.str());

    if (options.format == "jpg" || options.format == "jpeg") {
        args.push_back("-q:v");
        args.push_back(std::to_string(100 - options.quality));  // FFmpeg uses inverse scale
    }

    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::extractBestThumbnail(
    const std::string& video_path,
    const std::string& output_path,
    const ThumbnailOptions& options
) {
    // Get video duration
    MediaInfo info = getMediaInfo(video_path);

    // Select timestamp at 1/3 of video (usually a good representative frame)
    ThumbnailOptions opts = options;
    opts.timestamp = info.duration > 0 ? info.duration / 3.0 : 0;

    return extractThumbnail(video_path, output_path, opts);
}

CommandResult FFmpegWrapper::extractThumbnails(
    const std::string& video_path,
    const std::string& output_dir,
    int count,
    const ThumbnailOptions& options
) {
    // Create output directory
    try {
        fs::create_directories(output_dir);
    } catch (const std::exception& e) {
        CommandResult result;
        result.success = false;
        result.error_message = "Failed to create output directory: " + std::string(e.what());
        return result;
    }

    MediaInfo info = getMediaInfo(video_path);
    double interval = info.duration / (count + 1);

    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(video_path);

    // Build select filter for evenly spaced frames
    std::ostringstream select;
    select << "select='";
    for (int i = 1; i <= count; ++i) {
        if (i > 1) select << "+";
        select << "lt(prev_pts*TB," << (interval * i) << ")*gte(pts*TB," << (interval * i) << ")";
    }
    select << "',scale=" << options.width << ":" << options.height;

    args.push_back("-vf");
    args.push_back(select.str());
    args.push_back("-vsync");
    args.push_back("vfr");

    std::string output_pattern = output_dir + "/thumb_%03d." + options.format;
    args.push_back(output_pattern);

    CommandResult result = executeCommand(args);
    result.output_path = output_dir;

    return result;
}

CommandResult FFmpegWrapper::resizeImage(
    const std::string& input_path,
    const std::string& output_path,
    int width,
    int height,
    bool maintain_aspect
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);

    std::ostringstream vf;
    vf << "scale=";

    if (maintain_aspect) {
        vf << width << ":" << height << ":force_original_aspect_ratio=decrease";
    } else {
        vf << width << ":" << height;
    }

    args.push_back("-vf");
    args.push_back(vf.str());
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::cropImage(
    const std::string& input_path,
    const std::string& output_path,
    int x, int y, int width, int height
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);

    std::ostringstream vf;
    vf << "crop=" << width << ":" << height << ":" << x << ":" << y;

    args.push_back("-vf");
    args.push_back(vf.str());
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

CommandResult FFmpegWrapper::overlayImage(
    const std::string& input_path,
    const std::string& overlay_path,
    const std::string& output_path,
    int x, int y
) {
    std::vector<std::string> args;
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(input_path);
    args.push_back("-i");
    args.push_back(overlay_path);

    std::ostringstream filter;
    filter << "[0:v][1:v]overlay=" << x << ":" << y;

    args.push_back("-filter_complex");
    args.push_back(filter.str());
    args.push_back(output_path);

    CommandResult result = executeCommand(args);
    result.output_path = output_path;

    return result;
}

std::string FFmpegWrapper::buildSlideshowFilter(int image_count, const ConcatOptions& options) {
    std::ostringstream filter;

    // Scale all inputs to same size
    for (int i = 0; i < image_count; ++i) {
        filter << "[" << i << ":v]"
               << "scale=" << options.output_width << ":" << options.output_height
               << ":force_original_aspect_ratio=decrease,"
               << "pad=" << options.output_width << ":" << options.output_height
               << ":(ow-iw)/2:(oh-ih)/2,"
               << "setsar=1,"
               << "fps=" << options.fps;

        // Add Ken Burns effect if configured
        if (options.motion.type != MotionType::NONE) {
            filter << "," << buildKenBurnsFilter(i, options.motion, options);
        }

        filter << "[v" << i << "];";
    }

    // Concatenate with transitions
    if (image_count == 1) {
        filter << "[v0]null";
    } else {
        // Apply xfade transitions between clips
        std::string last_output = "[v0]";
        double offset = options.duration_per_image - options.transition.duration;

        for (int i = 1; i < image_count; ++i) {
            std::string current_input = "[v" + std::to_string(i) + "]";
            std::string output_label = "[xf" + std::to_string(i) + "]";

            filter << last_output << current_input
                   << "xfade=transition=" << options.transition.getFFmpegFilter()
                   << ":duration=" << options.transition.duration
                   << ":offset=" << offset;

            if (i < image_count - 1) {
                filter << output_label << ";";
                last_output = output_label;
                offset += options.duration_per_image - options.transition.duration;
            }
        }
    }

    return filter.str();
}

std::string FFmpegWrapper::buildKenBurnsFilter(int index, const MotionConfig& motion, const ConcatOptions& options) {
    std::ostringstream filter;

    // Calculate start and end zoom/position based on motion type
    float start_zoom = 1.0f;
    float end_zoom = motion.zoom_factor;
    std::string x_expr, y_expr;

    MotionType effective_type = motion.type;
    if (effective_type == MotionType::RANDOM) {
        // Cycle through motion types based on index
        int motion_index = index % 4;
        switch (motion_index) {
            case 0: effective_type = MotionType::ZOOM_IN; break;
            case 1: effective_type = MotionType::ZOOM_OUT; break;
            case 2: effective_type = MotionType::PAN_LEFT; break;
            case 3: effective_type = MotionType::PAN_RIGHT; break;
        }
    }

    int frames = static_cast<int>(options.duration_per_image * options.fps);

    switch (effective_type) {
        case MotionType::ZOOM_IN:
            filter << "zoompan=z='min(" << end_zoom << ",pzoom+0.001)':"
                   << "d=" << frames << ":x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
                   << "s=" << options.output_width << "x" << options.output_height;
            break;

        case MotionType::ZOOM_OUT:
            filter << "zoompan=z='if(lte(zoom," << start_zoom << ")," << end_zoom << ",max(" << start_zoom << ",pzoom-0.001))':"
                   << "d=" << frames << ":x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
                   << "s=" << options.output_width << "x" << options.output_height;
            break;

        case MotionType::PAN_LEFT:
            filter << "zoompan=z='" << motion.zoom_factor << "':"
                   << "d=" << frames << ":"
                   << "x='if(lte(on,1),iw*" << motion.pan_percentage << ",(px-iw*" << motion.pan_percentage << "*2/" << frames << "))':"
                   << "y='ih/2-(ih/zoom/2)':"
                   << "s=" << options.output_width << "x" << options.output_height;
            break;

        case MotionType::PAN_RIGHT:
            filter << "zoompan=z='" << motion.zoom_factor << "':"
                   << "d=" << frames << ":"
                   << "x='if(lte(on,1),0,(px+iw*" << motion.pan_percentage << "*2/" << frames << "))':"
                   << "y='ih/2-(ih/zoom/2)':"
                   << "s=" << options.output_width << "x" << options.output_height;
            break;

        case MotionType::ZOOM_PAN:
            // Combined zoom and pan effect
            filter << "zoompan=z='min(" << end_zoom << ",pzoom+0.0005)':"
                   << "d=" << frames << ":"
                   << "x='iw/2-(iw/zoom/2)+sin(on/" << frames << "*PI)*iw*" << motion.pan_percentage << "/zoom':"
                   << "y='ih/2-(ih/zoom/2)':"
                   << "s=" << options.output_width << "x" << options.output_height;
            break;

        default:
            return "";
    }

    return filter.str();
}

std::string FFmpegWrapper::buildTransitionFilter(const TransitionConfig& transition) {
    return "xfade=transition=" + transition.getFFmpegFilter() +
           ":duration=" + std::to_string(transition.duration);
}

std::string FFmpegWrapper::getTempFilePath(const std::string& extension) {
    std::lock_guard<std::mutex> lock(mutex_);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(100000, 999999);

    std::string filename = "wtl_" + std::to_string(dist(gen)) + "." + extension;
    std::string path = temp_dir_ + "/" + filename;

    temp_files_.push_back(path);
    return path;
}

void FFmpegWrapper::cleanupTempFiles() {
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

void FFmpegWrapper::cancelCurrentOperation() {
    cancelled_ = true;

#ifndef _WIN32
    if (current_process_id_ > 0) {
        kill(current_process_id_, SIGTERM);
    }
#endif
}

bool FFmpegWrapper::isCancelled() const {
    return cancelled_.load();
}

Json::Value FFmpegWrapper::getStats() const {
    Json::Value stats;
    stats["total_commands"] = static_cast<Json::UInt64>(total_commands_.load());
    stats["successful_commands"] = static_cast<Json::UInt64>(successful_commands_.load());
    stats["failed_commands"] = static_cast<Json::UInt64>(failed_commands_.load());
    stats["total_processing_time_ms"] = static_cast<Json::UInt64>(total_processing_time_ms_.load());
    stats["ffmpeg_version"] = getVersion();
    stats["temp_files_count"] = static_cast<Json::UInt>(temp_files_.size());
    return stats;
}

CommandResult FFmpegWrapper::executeCommand(
    const std::vector<std::string>& args,
    ProgressCallback progress_callback
) {
    CommandResult result;
    auto start_time = std::chrono::steady_clock::now();

    cancelled_ = false;

    // Build command string
    std::ostringstream cmd;
    cmd << ffmpeg_path_;

    // Add progress output if callback provided
    if (progress_callback) {
        cmd << " -progress pipe:1";
    }

    for (const auto& arg : args) {
        // Quote arguments with spaces
        if (arg.find(' ') != std::string::npos || arg.find('\'') != std::string::npos) {
            cmd << " \"" << arg << "\"";
        } else {
            cmd << " " << arg;
        }
    }

    // Redirect stderr to stdout for parsing
    cmd << " 2>&1";

    result.command = cmd.str();
    total_commands_++;

    // Execute command
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        result.success = false;
        result.error_message = "Failed to execute FFmpeg command";
        failed_commands_++;
        return result;
    }

    // Read output
    std::array<char, 4096> buffer;
    std::string output;

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        if (cancelled_) {
            pclose(pipe);
            result.success = false;
            result.error_message = "Operation cancelled";
            failed_commands_++;
            return result;
        }

        output += buffer.data();

        // Parse progress if callback provided
        if (progress_callback) {
            // Look for "out_time_ms=" in output
            std::string line(buffer.data());
            if (line.find("out_time_ms=") != std::string::npos) {
                // Extract progress (simplified - real impl would track total duration)
                double progress = parseProgress(line, 0);
                progress_callback(progress, 0);
            }
        }
    }

    result.exit_code = pclose(pipe);
    result.stderr_output = output;

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    total_processing_time_ms_ += result.duration.count();

    // Check for success
    result.success = (result.exit_code == 0);

    if (result.success) {
        successful_commands_++;
    } else {
        failed_commands_++;

        // Extract error message from output
        std::regex error_regex("Error|error|Invalid|invalid|fail|Fail");
        std::smatch match;
        if (std::regex_search(output, match, error_regex)) {
            // Find the line containing the error
            size_t pos = output.find(match[0].str());
            if (pos != std::string::npos) {
                size_t line_end = output.find('\n', pos);
                result.error_message = output.substr(pos, line_end - pos);
            }
        }

        if (result.error_message.empty()) {
            result.error_message = "FFmpeg command failed with exit code " + std::to_string(result.exit_code);
        }

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "FFmpeg command failed: " + result.error_message,
            {{"command", result.command}, {"exit_code", std::to_string(result.exit_code)}}
        );
    }

    return result;
}

Json::Value FFmpegWrapper::executeFFprobe(const std::string& path) {
    std::ostringstream cmd;
    cmd << ffprobe_path_
        << " -v quiet -print_format json -show_format -show_streams \""
        << path << "\"";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return Json::Value::null;
    }

    std::string output;
    std::array<char, 4096> buffer;

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    pclose(pipe);

    // Parse JSON
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(output);

    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        return Json::Value::null;
    }

    return root;
}

double FFmpegWrapper::parseProgress(const std::string& line, double total_duration) {
    std::regex time_regex("out_time_ms=(\\d+)");
    std::smatch match;

    if (std::regex_search(line, match, time_regex)) {
        double current_ms = std::stod(match[1].str());
        if (total_duration > 0) {
            return (current_ms / 1000.0 / total_duration) * 100.0;
        }
    }

    return 0.0;
}

std::string FFmpegWrapper::escapeFilterString(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);

    for (char c : str) {
        switch (c) {
            case '\'':
                result += "'\\''";
                break;
            case '\\':
                result += "\\\\";
                break;
            case ':':
                result += "\\:";
                break;
            case '[':
                result += "\\[";
                break;
            case ']':
                result += "\\]";
                break;
            default:
                result += c;
        }
    }

    return result;
}

std::string FFmpegWrapper::buildFontSpec(const TextOverlayConfig& config) {
    std::ostringstream spec;

    // Try to find system font
    spec << "fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans";

    switch (config.font_style) {
        case FontStyle::BOLD:
            spec << "-Bold";
            break;
        case FontStyle::ITALIC:
            spec << "-Oblique";
            break;
        case FontStyle::BOLD_ITALIC:
            spec << "-BoldOblique";
            break;
        default:
            break;
    }

    spec << ".ttf";

    return spec.str();
}

} // namespace wtl::content::media

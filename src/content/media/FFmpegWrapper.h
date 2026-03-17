/**
 * @file FFmpegWrapper.h
 * @brief Wrapper for FFmpeg command-line operations
 *
 * PURPOSE:
 * Provides a C++ wrapper around FFmpeg shell commands for video/image
 * processing. Handles command building, execution, error handling, and
 * progress tracking.
 *
 * USAGE:
 * auto& ffmpeg = FFmpegWrapper::getInstance();
 * ffmpeg.initialize("/usr/bin/ffmpeg", "/usr/bin/ffprobe");
 *
 * auto result = ffmpeg.concat({"img1.jpg", "img2.jpg"}, "output.mp4", options);
 * if (result.success) {
 *     std::cout << "Video created: " << result.output_path << std::endl;
 * }
 *
 * DEPENDENCIES:
 * - FFmpeg/FFprobe installed on system
 * - popen/pclose for command execution
 * - ErrorCapture, SelfHealing for error handling
 *
 * MODIFICATION GUIDE:
 * - Add new FFmpeg operations as methods
 * - Extend CommandResult for additional result data
 * - Add new filter types to buildFilterComplex()
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/media/OverlayConfig.h"
#include "content/media/VideoTemplate.h"

namespace wtl::content::media {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct CommandResult
 * @brief Result of an FFmpeg command execution
 */
struct CommandResult {
    bool success = false;              ///< Whether command succeeded
    int exit_code = -1;                ///< Process exit code
    std::string output_path;           ///< Path to output file (if applicable)
    std::string stdout_output;         ///< Standard output
    std::string stderr_output;         ///< Standard error (FFmpeg outputs here)
    std::string command;               ///< The executed command
    std::chrono::milliseconds duration; ///< Execution duration
    std::string error_message;         ///< Error message if failed

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct MediaInfo
 * @brief Information about a media file
 */
struct MediaInfo {
    std::string path;                  ///< File path
    std::string format;                ///< Container format
    double duration = 0.0;             ///< Duration in seconds
    int width = 0;                     ///< Width in pixels
    int height = 0;                    ///< Height in pixels
    std::string video_codec;           ///< Video codec
    std::string audio_codec;           ///< Audio codec
    int fps = 0;                       ///< Frames per second
    int bitrate = 0;                   ///< Bitrate in kbps
    int64_t file_size = 0;             ///< File size in bytes
    bool has_video = false;            ///< Has video stream
    bool has_audio = false;            ///< Has audio stream

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from FFprobe JSON output
     * @param json FFprobe output
     * @return MediaInfo object
     */
    static MediaInfo fromFFprobeJson(const Json::Value& json);
};

/**
 * @struct ConcatOptions
 * @brief Options for concatenating images/videos
 */
struct ConcatOptions {
    double duration_per_image = 5.0;   ///< Duration per image in seconds
    TransitionConfig transition;        ///< Transition configuration
    MotionConfig motion;               ///< Motion effect configuration
    int output_width = 1920;           ///< Output width
    int output_height = 1080;          ///< Output height
    int fps = 30;                      ///< Output FPS
    std::string codec = "libx264";     ///< Video codec
    int crf = 23;                      ///< Quality (lower = better)
    std::string preset = "medium";     ///< Encoding preset
    std::string pixel_format = "yuv420p"; ///< Pixel format

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static ConcatOptions fromJson(const Json::Value& json);
};

/**
 * @struct OverlayOptions
 * @brief Options for adding overlay to video
 */
struct OverlayOptions {
    std::string overlay_image;         ///< Path to overlay image
    int x = 0;                         ///< X position
    int y = 0;                         ///< Y position
    float opacity = 1.0f;              ///< Opacity (0.0 - 1.0)
    double start_time = 0.0;           ///< When to start showing
    double end_time = 0.0;             ///< When to stop (0 = end)

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct AudioOptions
 * @brief Options for adding/mixing audio
 */
struct AudioOptions {
    std::string audio_file;            ///< Path to audio file
    float volume = 1.0f;               ///< Volume level (0.0 - 1.0)
    bool loop = false;                 ///< Loop audio to match video
    bool fade_in = false;              ///< Fade in at start
    bool fade_out = false;             ///< Fade out at end
    double fade_duration = 2.0;        ///< Fade duration in seconds
    std::string codec = "aac";         ///< Audio codec
    int bitrate = 192;                 ///< Bitrate in kbps

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct TranscodeOptions
 * @brief Options for transcoding video
 */
struct TranscodeOptions {
    int width = 0;                     ///< Target width (0 = keep)
    int height = 0;                    ///< Target height (0 = keep)
    int fps = 0;                       ///< Target FPS (0 = keep)
    std::string codec = "libx264";     ///< Video codec
    int crf = 23;                      ///< Quality
    std::string preset = "medium";     ///< Encoding preset
    std::string format = "mp4";        ///< Output format
    std::string audio_codec = "aac";   ///< Audio codec
    int audio_bitrate = 192;           ///< Audio bitrate
    bool copy_audio = false;           ///< Copy audio without re-encoding

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Get preset for platform
     * @param platform Platform name
     * @return Transcode options for platform
     */
    static TranscodeOptions forPlatform(const std::string& platform);
};

/**
 * @struct ThumbnailOptions
 * @brief Options for extracting thumbnails
 */
struct ThumbnailOptions {
    double timestamp = -1.0;           ///< Specific timestamp (-1 = auto best)
    int width = 640;                   ///< Thumbnail width
    int height = 480;                  ///< Thumbnail height
    std::string format = "jpg";        ///< Output format
    int quality = 85;                  ///< JPEG quality (1-100)
    bool scale_to_fit = true;          ///< Scale to fit dimensions

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @brief Progress callback function type
 * @param progress Progress percentage (0.0 - 100.0)
 * @param eta Estimated time remaining in seconds
 */
using ProgressCallback = std::function<void(double progress, int eta_seconds)>;

// ============================================================================
// FFMPEG WRAPPER CLASS
// ============================================================================

/**
 * @class FFmpegWrapper
 * @brief Singleton wrapper for FFmpeg command-line operations
 *
 * Provides methods for common video/image operations using FFmpeg.
 * All operations are executed as shell commands with proper error handling.
 */
class FFmpegWrapper {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the FFmpegWrapper singleton
     */
    static FFmpegWrapper& getInstance();

    // Delete copy/move constructors
    FFmpegWrapper(const FFmpegWrapper&) = delete;
    FFmpegWrapper& operator=(const FFmpegWrapper&) = delete;
    FFmpegWrapper(FFmpegWrapper&&) = delete;
    FFmpegWrapper& operator=(FFmpegWrapper&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize FFmpeg wrapper
     * @param ffmpeg_path Path to ffmpeg executable
     * @param ffprobe_path Path to ffprobe executable
     * @param temp_dir Temporary directory for intermediate files
     * @return True if initialization successful
     */
    bool initialize(
        const std::string& ffmpeg_path = "ffmpeg",
        const std::string& ffprobe_path = "ffprobe",
        const std::string& temp_dir = "/tmp/wtl_media"
    );

    /**
     * @brief Initialize from configuration file
     * @return True if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if FFmpeg is available
     * @return True if FFmpeg is installed and working
     */
    bool isAvailable() const;

    /**
     * @brief Get FFmpeg version
     * @return Version string
     */
    std::string getVersion() const;

    // =========================================================================
    // MEDIA INFORMATION
    // =========================================================================

    /**
     * @brief Get information about a media file
     * @param path Path to media file
     * @return MediaInfo struct
     */
    MediaInfo getMediaInfo(const std::string& path);

    /**
     * @brief Check if file is a valid image
     * @param path Path to file
     * @return True if valid image
     */
    bool isValidImage(const std::string& path);

    /**
     * @brief Check if file is a valid video
     * @param path Path to file
     * @return True if valid video
     */
    bool isValidVideo(const std::string& path);

    // =========================================================================
    // CONCATENATION
    // =========================================================================

    /**
     * @brief Concatenate images into video
     * @param images List of image paths
     * @param output_path Output video path
     * @param options Concatenation options
     * @param progress Optional progress callback
     * @return CommandResult
     */
    CommandResult concat(
        const std::vector<std::string>& images,
        const std::string& output_path,
        const ConcatOptions& options = {},
        ProgressCallback progress = nullptr
    );

    /**
     * @brief Concatenate videos together
     * @param videos List of video paths
     * @param output_path Output video path
     * @param transition_duration Transition duration in seconds
     * @param progress Optional progress callback
     * @return CommandResult
     */
    CommandResult concatVideos(
        const std::vector<std::string>& videos,
        const std::string& output_path,
        double transition_duration = 1.0,
        ProgressCallback progress = nullptr
    );

    // =========================================================================
    // OVERLAY
    // =========================================================================

    /**
     * @brief Add image overlay to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param options Overlay options
     * @return CommandResult
     */
    CommandResult overlay(
        const std::string& video_path,
        const std::string& output_path,
        const OverlayOptions& options
    );

    /**
     * @brief Add multiple overlays to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param overlays List of overlay options
     * @return CommandResult
     */
    CommandResult overlayMultiple(
        const std::string& video_path,
        const std::string& output_path,
        const std::vector<OverlayOptions>& overlays
    );

    /**
     * @brief Add text overlay to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param text_config Text overlay configuration
     * @return CommandResult
     */
    CommandResult addTextOverlay(
        const std::string& video_path,
        const std::string& output_path,
        const TextOverlayConfig& text_config
    );

    // =========================================================================
    // AUDIO
    // =========================================================================

    /**
     * @brief Add audio track to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param options Audio options
     * @return CommandResult
     */
    CommandResult addAudio(
        const std::string& video_path,
        const std::string& output_path,
        const AudioOptions& options
    );

    /**
     * @brief Replace audio track in video
     * @param video_path Input video path
     * @param audio_path New audio file path
     * @param output_path Output video path
     * @return CommandResult
     */
    CommandResult replaceAudio(
        const std::string& video_path,
        const std::string& audio_path,
        const std::string& output_path
    );

    /**
     * @brief Remove audio from video
     * @param video_path Input video path
     * @param output_path Output video path
     * @return CommandResult
     */
    CommandResult removeAudio(
        const std::string& video_path,
        const std::string& output_path
    );

    // =========================================================================
    // TRANSCODING
    // =========================================================================

    /**
     * @brief Transcode video to different format/codec
     * @param input_path Input video path
     * @param output_path Output video path
     * @param options Transcode options
     * @param progress Optional progress callback
     * @return CommandResult
     */
    CommandResult transcode(
        const std::string& input_path,
        const std::string& output_path,
        const TranscodeOptions& options,
        ProgressCallback progress = nullptr
    );

    /**
     * @brief Resize video
     * @param input_path Input video path
     * @param output_path Output video path
     * @param width Target width
     * @param height Target height
     * @param maintain_aspect Keep aspect ratio
     * @return CommandResult
     */
    CommandResult resize(
        const std::string& input_path,
        const std::string& output_path,
        int width,
        int height,
        bool maintain_aspect = true
    );

    // =========================================================================
    // THUMBNAILS
    // =========================================================================

    /**
     * @brief Extract thumbnail from video
     * @param video_path Input video path
     * @param output_path Output image path
     * @param options Thumbnail options
     * @return CommandResult
     */
    CommandResult extractThumbnail(
        const std::string& video_path,
        const std::string& output_path,
        const ThumbnailOptions& options = {}
    );

    /**
     * @brief Extract best thumbnail automatically
     * @param video_path Input video path
     * @param output_path Output image path
     * @param options Thumbnail options
     * @return CommandResult
     */
    CommandResult extractBestThumbnail(
        const std::string& video_path,
        const std::string& output_path,
        const ThumbnailOptions& options = {}
    );

    /**
     * @brief Extract multiple thumbnails
     * @param video_path Input video path
     * @param output_dir Output directory
     * @param count Number of thumbnails
     * @param options Thumbnail options
     * @return CommandResult (output_path will be the directory)
     */
    CommandResult extractThumbnails(
        const std::string& video_path,
        const std::string& output_dir,
        int count,
        const ThumbnailOptions& options = {}
    );

    // =========================================================================
    // IMAGE PROCESSING
    // =========================================================================

    /**
     * @brief Resize image
     * @param input_path Input image path
     * @param output_path Output image path
     * @param width Target width
     * @param height Target height
     * @param maintain_aspect Keep aspect ratio
     * @return CommandResult
     */
    CommandResult resizeImage(
        const std::string& input_path,
        const std::string& output_path,
        int width,
        int height,
        bool maintain_aspect = true
    );

    /**
     * @brief Crop image
     * @param input_path Input image path
     * @param output_path Output image path
     * @param x Crop X position
     * @param y Crop Y position
     * @param width Crop width
     * @param height Crop height
     * @return CommandResult
     */
    CommandResult cropImage(
        const std::string& input_path,
        const std::string& output_path,
        int x, int y, int width, int height
    );

    /**
     * @brief Add overlay to image
     * @param input_path Input image path
     * @param overlay_path Overlay image path
     * @param output_path Output image path
     * @param x X position
     * @param y Y position
     * @return CommandResult
     */
    CommandResult overlayImage(
        const std::string& input_path,
        const std::string& overlay_path,
        const std::string& output_path,
        int x, int y
    );

    // =========================================================================
    // UTILITY METHODS
    // =========================================================================

    /**
     * @brief Build filter complex string for image slideshow
     * @param image_count Number of images
     * @param options Concat options
     * @return Filter complex string
     */
    std::string buildSlideshowFilter(int image_count, const ConcatOptions& options);

    /**
     * @brief Build filter for Ken Burns effect
     * @param index Image index
     * @param motion Motion configuration
     * @param options Concat options
     * @return Filter string
     */
    std::string buildKenBurnsFilter(int index, const MotionConfig& motion, const ConcatOptions& options);

    /**
     * @brief Build transition filter
     * @param transition Transition configuration
     * @return Filter string
     */
    std::string buildTransitionFilter(const TransitionConfig& transition);

    /**
     * @brief Generate unique temporary file path
     * @param extension File extension
     * @return Temporary file path
     */
    std::string getTempFilePath(const std::string& extension);

    /**
     * @brief Clean up temporary files
     */
    void cleanupTempFiles();

    /**
     * @brief Cancel currently running operation
     */
    void cancelCurrentOperation();

    /**
     * @brief Check if operation was cancelled
     * @return True if cancelled
     */
    bool isCancelled() const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get processing statistics
     * @return JSON with statistics
     */
    Json::Value getStats() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    FFmpegWrapper() = default;
    ~FFmpegWrapper();

    /**
     * @brief Execute FFmpeg command
     * @param args Command arguments
     * @param progress_callback Progress callback
     * @return CommandResult
     */
    CommandResult executeCommand(
        const std::vector<std::string>& args,
        ProgressCallback progress_callback = nullptr
    );

    /**
     * @brief Execute FFprobe command and get JSON output
     * @param path Media file path
     * @return JSON output
     */
    Json::Value executeFFprobe(const std::string& path);

    /**
     * @brief Parse FFmpeg progress output
     * @param line Output line
     * @param total_duration Total expected duration
     * @return Progress percentage (0-100)
     */
    double parseProgress(const std::string& line, double total_duration);

    /**
     * @brief Escape string for FFmpeg filter
     * @param str Input string
     * @return Escaped string
     */
    std::string escapeFilterString(const std::string& str);

    /**
     * @brief Build font specification for drawtext filter
     * @param config Text overlay config
     * @return Font specification string
     */
    std::string buildFontSpec(const TextOverlayConfig& config);

    // Configuration
    std::string ffmpeg_path_ = "ffmpeg";
    std::string ffprobe_path_ = "ffprobe";
    std::string temp_dir_ = "/tmp/wtl_media";

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<int> current_process_id_{-1};

    // Statistics
    std::atomic<uint64_t> total_commands_{0};
    std::atomic<uint64_t> successful_commands_{0};
    std::atomic<uint64_t> failed_commands_{0};
    std::atomic<uint64_t> total_processing_time_ms_{0};

    // Temp file tracking
    std::vector<std::string> temp_files_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::media

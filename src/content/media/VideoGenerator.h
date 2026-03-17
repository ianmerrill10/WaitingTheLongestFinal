/**
 * @file VideoGenerator.h
 * @brief Video generation from dog photos with templates and overlays
 *
 * PURPOSE:
 * Provides video generation capabilities for creating promotional videos
 * from dog photos. Supports templates, LED counter overlays, music,
 * captions, and platform-specific formatting.
 *
 * USAGE:
 * auto& generator = VideoGenerator::getInstance();
 * auto result = generator.generateFromPhotos(photos, dog, template);
 * auto thumbnail = generator.generateThumbnail(video_path);
 *
 * DEPENDENCIES:
 * - FFmpegWrapper (for video operations)
 * - ImageProcessor (for image preprocessing)
 * - VideoTemplate, OverlayConfig (for configuration)
 * - ErrorCapture, SelfHealing (for error handling)
 *
 * MODIFICATION GUIDE:
 * - Add new generation modes to generateFromPhotos()
 * - Extend platform support in processForPlatform()
 * - Add new overlay types in addOverlay()
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/media/FFmpegWrapper.h"
#include "content/media/ImageProcessor.h"
#include "content/media/OverlayConfig.h"
#include "content/media/VideoTemplate.h"

namespace wtl::content::media {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct VideoGenerationOptions
 * @brief Options for video generation
 */
struct VideoGenerationOptions {
    bool preprocess_photos = true;     ///< Auto-enhance photos before use
    bool add_wait_time_overlay = true; ///< Add LED wait time counter
    bool add_urgency_badge = true;     ///< Add urgency badge if applicable
    bool add_music = true;             ///< Add background music
    bool add_captions = true;          ///< Add text captions
    bool include_intro = false;        ///< Include intro sequence
    bool include_outro = false;        ///< Include outro with CTA

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static VideoGenerationOptions fromJson(const Json::Value& json);

    /**
     * @brief Get default options
     */
    static VideoGenerationOptions defaults();

    /**
     * @brief Get minimal options (fast generation)
     */
    static VideoGenerationOptions minimal();

    /**
     * @brief Get full options (all features)
     */
    static VideoGenerationOptions full();
};

/**
 * @struct VideoResult
 * @brief Result of video generation operation
 */
struct VideoResult {
    bool success = false;              ///< Whether generation succeeded
    std::string output_path;           ///< Path to output video
    std::string thumbnail_path;        ///< Path to generated thumbnail
    std::string error_message;         ///< Error message if failed
    double duration = 0.0;             ///< Video duration in seconds
    int width = 0;                     ///< Video width
    int height = 0;                    ///< Video height
    int64_t file_size = 0;             ///< File size in bytes
    std::chrono::milliseconds processing_time; ///< Time taken to generate

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct DogVideoInfo
 * @brief Dog information for video generation
 */
struct DogVideoInfo {
    std::string id;                    ///< Dog ID
    std::string name;                  ///< Dog name
    std::string breed;                 ///< Primary breed
    std::string age;                   ///< Age description
    std::string gender;                ///< Gender
    std::string size;                  ///< Size category
    std::string description;           ///< Description text
    std::string wait_time;             ///< Formatted wait time (YY:MM:DD:HH:MM:SS)
    std::string urgency_level;         ///< Urgency level
    std::string shelter_name;          ///< Shelter name
    std::string profile_url;           ///< URL to dog profile
    std::optional<std::string> countdown; ///< Countdown time if applicable

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static DogVideoInfo fromJson(const Json::Value& json);
};

/**
 * @brief Progress callback function type
 * @param stage Current processing stage name
 * @param progress Progress percentage (0.0 - 100.0)
 */
using VideoProgressCallback = std::function<void(const std::string& stage, double progress)>;

// ============================================================================
// VIDEO GENERATOR CLASS
// ============================================================================

/**
 * @class VideoGenerator
 * @brief Singleton class for generating videos from dog photos
 *
 * Provides comprehensive video generation capabilities including
 * slideshows, Ken Burns effects, overlays, music, and platform optimization.
 */
class VideoGenerator {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the VideoGenerator singleton
     */
    static VideoGenerator& getInstance();

    // Delete copy/move constructors
    VideoGenerator(const VideoGenerator&) = delete;
    VideoGenerator& operator=(const VideoGenerator&) = delete;
    VideoGenerator(VideoGenerator&&) = delete;
    VideoGenerator& operator=(VideoGenerator&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the video generator
     * @param assets_dir Directory containing video assets (music, intros, etc.)
     * @param temp_dir Temporary directory for intermediate files
     * @param output_dir Default output directory
     * @return True if initialization successful
     */
    bool initialize(
        const std::string& assets_dir = "static/video",
        const std::string& temp_dir = "/tmp/wtl_media",
        const std::string& output_dir = "media/videos"
    );

    /**
     * @brief Initialize from configuration
     * @return True if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if generator is initialized
     * @return True if initialized
     */
    bool isInitialized() const;

    // =========================================================================
    // VIDEO GENERATION
    // =========================================================================

    /**
     * @brief Generate video from photos
     * @param photos List of photo paths
     * @param dog Dog information
     * @param template_config Video template to use
     * @param output_path Output video path (optional, auto-generated if empty)
     * @param options Generation options
     * @param progress Optional progress callback
     * @return VideoResult
     */
    VideoResult generateFromPhotos(
        const std::vector<std::string>& photos,
        const DogVideoInfo& dog,
        const VideoTemplate& template_config,
        const std::string& output_path = "",
        const VideoGenerationOptions& options = VideoGenerationOptions::defaults(),
        VideoProgressCallback progress = nullptr
    );

    /**
     * @brief Generate video with default template
     * @param photos List of photo paths
     * @param dog Dog information
     * @param template_type Template type to use
     * @param output_path Output video path
     * @return VideoResult
     */
    VideoResult generateWithTemplate(
        const std::vector<std::string>& photos,
        const DogVideoInfo& dog,
        TemplateType template_type = TemplateType::SLIDESHOW,
        const std::string& output_path = ""
    );

    // =========================================================================
    // OVERLAY OPERATIONS
    // =========================================================================

    /**
     * @brief Add overlay to existing video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param overlay_config Overlay configuration
     * @param wait_time Wait time for LED counter
     * @param countdown Optional countdown time
     * @return VideoResult
     */
    VideoResult addOverlay(
        const std::string& video_path,
        const std::string& output_path,
        const OverlayConfig& overlay_config,
        const std::string& wait_time,
        const std::optional<std::string>& countdown = std::nullopt
    );

    /**
     * @brief Add LED counter overlay to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param wait_time Wait time string
     * @param config Counter configuration
     * @return VideoResult
     */
    VideoResult addWaitTimeCounter(
        const std::string& video_path,
        const std::string& output_path,
        const std::string& wait_time,
        const LEDCounterConfig& config = LEDCounterConfig::defaultWaitTime()
    );

    // =========================================================================
    // AUDIO OPERATIONS
    // =========================================================================

    /**
     * @brief Add music to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param music_id Music track ID or path
     * @param config Music configuration
     * @return VideoResult
     */
    VideoResult addMusic(
        const std::string& video_path,
        const std::string& output_path,
        const std::string& music_id,
        const MusicConfig& config = MusicConfig()
    );

    /**
     * @brief Add music by style
     * @param video_path Input video path
     * @param output_path Output video path
     * @param style Music style to use
     * @param volume Volume level (0.0 - 1.0)
     * @return VideoResult
     */
    VideoResult addMusicByStyle(
        const std::string& video_path,
        const std::string& output_path,
        MusicStyle style,
        float volume = 0.3f
    );

    // =========================================================================
    // TEXT OPERATIONS
    // =========================================================================

    /**
     * @brief Add text captions to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param text_config Text overlay configuration
     * @return VideoResult
     */
    VideoResult addText(
        const std::string& video_path,
        const std::string& output_path,
        const TextOverlayConfig& text_config
    );

    /**
     * @brief Add multiple text overlays
     * @param video_path Input video path
     * @param output_path Output video path
     * @param text_configs List of text configurations
     * @return VideoResult
     */
    VideoResult addTextOverlays(
        const std::string& video_path,
        const std::string& output_path,
        const std::vector<TextOverlayConfig>& text_configs
    );

    // =========================================================================
    // THUMBNAIL OPERATIONS
    // =========================================================================

    /**
     * @brief Generate thumbnail from video
     * @param video_path Input video path
     * @param output_path Output image path (optional)
     * @param options Thumbnail options
     * @return VideoResult (with thumbnail_path set)
     */
    VideoResult generateThumbnail(
        const std::string& video_path,
        const std::string& output_path = "",
        const ThumbnailOptions& options = ThumbnailOptions()
    );

    /**
     * @brief Generate best thumbnail automatically
     * @param video_path Input video path
     * @param output_path Output image path (optional)
     * @return VideoResult
     */
    VideoResult generateBestThumbnail(
        const std::string& video_path,
        const std::string& output_path = ""
    );

    // =========================================================================
    // PLATFORM PROCESSING
    // =========================================================================

    /**
     * @brief Process video for specific platform
     * @param video_path Input video path
     * @param output_path Output video path
     * @param platform Platform name (youtube, instagram, tiktok, facebook, twitter)
     * @return VideoResult
     */
    VideoResult processForPlatform(
        const std::string& video_path,
        const std::string& output_path,
        const std::string& platform
    );

    /**
     * @brief Generate versions for multiple platforms
     * @param video_path Input video path
     * @param output_dir Output directory
     * @param platforms List of platform names
     * @return Map of platform to VideoResult
     */
    std::map<std::string, VideoResult> processForPlatforms(
        const std::string& video_path,
        const std::string& output_dir,
        const std::vector<std::string>& platforms
    );

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Get available music tracks
     * @return List of music track IDs/names
     */
    std::vector<std::string> getAvailableMusic() const;

    /**
     * @brief Get music path for style
     * @param style Music style
     * @return Path to music file
     */
    std::string getMusicPath(MusicStyle style) const;

    /**
     * @brief Generate unique output path
     * @param dog_id Dog ID
     * @param extension File extension
     * @return Generated path
     */
    std::string generateOutputPath(
        const std::string& dog_id,
        const std::string& extension = "mp4"
    );

    /**
     * @brief Clean up temporary files
     */
    void cleanupTempFiles();

    /**
     * @brief Cancel current generation
     */
    void cancelGeneration();

    /**
     * @brief Check if generation was cancelled
     * @return True if cancelled
     */
    bool isCancelled() const;

    /**
     * @brief Get processing statistics
     * @return JSON with statistics
     */
    Json::Value getStats() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    VideoGenerator() = default;
    ~VideoGenerator();

    /**
     * @brief Preprocess photos for video
     * @param photos Input photo paths
     * @param dimensions Target dimensions
     * @param enhance Apply auto-enhancement
     * @return List of preprocessed photo paths
     */
    std::vector<std::string> preprocessPhotos(
        const std::vector<std::string>& photos,
        const Dimensions& dimensions,
        bool enhance = true
    );

    /**
     * @brief Build video from preprocessed photos
     * @param photos Preprocessed photo paths
     * @param template_config Video template
     * @param output_path Output video path
     * @param progress Progress callback
     * @return VideoResult
     */
    VideoResult buildVideo(
        const std::vector<std::string>& photos,
        const VideoTemplate& template_config,
        const std::string& output_path,
        VideoProgressCallback progress = nullptr
    );

    /**
     * @brief Apply overlays to video
     * @param video_path Input video path
     * @param output_path Output video path
     * @param dog Dog information
     * @param overlay_config Overlay configuration
     * @return VideoResult
     */
    VideoResult applyOverlays(
        const std::string& video_path,
        const std::string& output_path,
        const DogVideoInfo& dog,
        const OverlayConfig& overlay_config
    );

    /**
     * @brief Apply text placements from template
     * @param video_path Input video path
     * @param output_path Output video path
     * @param template_config Template with text placements
     * @param dog Dog information for placeholders
     * @return VideoResult
     */
    VideoResult applyTextPlacements(
        const std::string& video_path,
        const std::string& output_path,
        const VideoTemplate& template_config,
        const DogVideoInfo& dog
    );

    /**
     * @brief Replace placeholders in text
     * @param text Text with placeholders
     * @param dog Dog information
     * @return Text with replaced placeholders
     */
    std::string replacePlaceholders(
        const std::string& text,
        const DogVideoInfo& dog
    );

    /**
     * @brief Get temp file path
     * @param extension File extension
     * @return Temp file path
     */
    std::string getTempPath(const std::string& extension);

    // Configuration
    std::string assets_dir_ = "static/video";
    std::string temp_dir_ = "/tmp/wtl_media";
    std::string output_dir_ = "media/videos";
    std::string music_dir_ = "static/audio/music";

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> cancelled_{false};

    // Temp file tracking
    std::vector<std::string> temp_files_;

    // Statistics
    std::atomic<uint64_t> videos_generated_{0};
    std::atomic<uint64_t> thumbnails_generated_{0};
    std::atomic<uint64_t> generation_errors_{0};
    std::atomic<uint64_t> total_processing_time_ms_{0};

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::media

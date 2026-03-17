/**
 * @file VideoTemplate.h
 * @brief Video template definitions for automated video generation
 *
 * PURPOSE:
 * Defines video templates that control how dog photos are assembled into
 * promotional videos. Templates specify duration, transitions, overlay
 * configurations, music styles, and text placements.
 *
 * USAGE:
 * VideoTemplate template = VideoTemplate::get(TemplateType::KEN_BURNS);
 * template.duration = 30.0;
 * template.overlay_config.led_counter.visible = true;
 *
 * DEPENDENCIES:
 * - OverlayConfig (overlay definitions)
 * - jsoncpp (Json::Value for serialization)
 *
 * MODIFICATION GUIDE:
 * - Add new template types to TemplateType enum
 * - Create corresponding factory method in get()
 * - Add new transition styles to TransitionStyle enum
 * - Extend MusicStyle for new music categories
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/media/OverlayConfig.h"

namespace wtl::content::media {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum TemplateType
 * @brief Pre-defined video template types
 */
enum class TemplateType {
    SLIDESHOW = 0,    ///< Simple slideshow with crossfade
    KEN_BURNS,        ///< Ken Burns effect (pan and zoom)
    COUNTDOWN,        ///< Urgency countdown focus
    STORY,            ///< Vertical story format (9:16)
    PROMOTIONAL,      ///< Full promotional video
    QUICK_SHARE,      ///< Quick 15-second share
    ADOPTION_APPEAL,  ///< Emotional adoption appeal
    BEFORE_AFTER,     ///< Before/after transformation
    CUSTOM            ///< Custom user-defined template
};

/**
 * @enum TransitionStyle
 * @brief Video transition styles between photos
 */
enum class TransitionStyle {
    NONE = 0,         ///< Cut (no transition)
    CROSSFADE,        ///< Dissolve between images
    FADE_BLACK,       ///< Fade to black between images
    FADE_WHITE,       ///< Fade to white between images
    SLIDE_LEFT,       ///< Slide in from right
    SLIDE_RIGHT,      ///< Slide in from left
    SLIDE_UP,         ///< Slide in from bottom
    SLIDE_DOWN,       ///< Slide in from top
    ZOOM_IN,          ///< Zoom into next image
    ZOOM_OUT,         ///< Zoom out to next image
    WIPE_LEFT,        ///< Wipe from right to left
    WIPE_RIGHT,       ///< Wipe from left to right
    CIRCLE_OPEN,      ///< Circle reveal opening
    CIRCLE_CLOSE,     ///< Circle reveal closing
    RANDOM            ///< Random transition selection
};

/**
 * @enum MusicStyle
 * @brief Background music style categories
 */
enum class MusicStyle {
    NONE = 0,         ///< No background music
    UPBEAT,           ///< Happy, energetic music
    EMOTIONAL,        ///< Emotional, tugging heartstrings
    CALM,             ///< Calm, peaceful music
    URGENT,           ///< Urgent, time-sensitive music
    INSPIRATIONAL,    ///< Uplifting, inspiring music
    PLAYFUL,          ///< Fun, playful music for puppies
    CUSTOM            ///< User-provided music
};

/**
 * @enum AspectRatio
 * @brief Video aspect ratios
 */
enum class AspectRatio {
    LANDSCAPE_16_9 = 0,  ///< 16:9 landscape (YouTube, standard)
    LANDSCAPE_4_3,       ///< 4:3 landscape (older format)
    PORTRAIT_9_16,       ///< 9:16 portrait (Stories, Reels, TikTok)
    PORTRAIT_4_5,        ///< 4:5 portrait (Instagram feed)
    SQUARE_1_1,          ///< 1:1 square (Instagram feed)
    CUSTOM               ///< Custom aspect ratio
};

/**
 * @enum MotionType
 * @brief Motion effect types for static images
 */
enum class MotionType {
    NONE = 0,         ///< No motion effect
    PAN_LEFT,         ///< Pan from right to left
    PAN_RIGHT,        ///< Pan from left to right
    PAN_UP,           ///< Pan from bottom to top
    PAN_DOWN,         ///< Pan from top to bottom
    ZOOM_IN,          ///< Slowly zoom in
    ZOOM_OUT,         ///< Slowly zoom out
    ZOOM_PAN,         ///< Combined zoom and pan (Ken Burns)
    RANDOM            ///< Random motion selection
};

// ============================================================================
// TEXT PLACEMENT STRUCTURE
// ============================================================================

/**
 * @struct TextPlacement
 * @brief Configuration for dynamic text placement in videos
 */
struct TextPlacement {
    std::string placeholder;           ///< Placeholder name (e.g., "{{dog_name}}")
    TextOverlayConfig text_config;     ///< Text styling and position
    double start_time = 0.0;           ///< When to show (seconds)
    double end_time = 0.0;             ///< When to hide (0 = end of video)
    bool use_animation = true;         ///< Enable entrance animation

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return TextPlacement object
     */
    static TextPlacement fromJson(const Json::Value& json);
};

// ============================================================================
// MOTION CONFIG STRUCTURE
// ============================================================================

/**
 * @struct MotionConfig
 * @brief Configuration for image motion effects
 */
struct MotionConfig {
    MotionType type = MotionType::ZOOM_PAN;  ///< Motion effect type
    float zoom_factor = 1.2f;                ///< Zoom scale (1.0 = no zoom)
    float pan_percentage = 0.15f;            ///< Pan distance as percentage of image
    double duration = 5.0;                   ///< Duration of motion in seconds
    bool ease_in_out = true;                 ///< Use easing for smooth motion

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return MotionConfig object
     */
    static MotionConfig fromJson(const Json::Value& json);
};

// ============================================================================
// TRANSITION CONFIG STRUCTURE
// ============================================================================

/**
 * @struct TransitionConfig
 * @brief Configuration for transitions between photos
 */
struct TransitionConfig {
    TransitionStyle style = TransitionStyle::CROSSFADE;  ///< Transition style
    double duration = 1.0;                               ///< Transition duration in seconds
    bool use_random = false;                             ///< Use random transitions

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return TransitionConfig object
     */
    static TransitionConfig fromJson(const Json::Value& json);

    /**
     * @brief Get FFmpeg filter string for transition
     * @return FFmpeg xfade filter expression
     */
    std::string getFFmpegFilter() const;
};

// ============================================================================
// MUSIC CONFIG STRUCTURE
// ============================================================================

/**
 * @struct MusicConfig
 * @brief Configuration for background music
 */
struct MusicConfig {
    MusicStyle style = MusicStyle::UPBEAT;   ///< Music style
    std::string music_file;                  ///< Path to specific music file (if custom)
    float volume = 0.3f;                     ///< Music volume (0.0 - 1.0)
    bool fade_in = true;                     ///< Fade in at start
    bool fade_out = true;                    ///< Fade out at end
    double fade_duration = 2.0;              ///< Fade duration in seconds
    bool loop = true;                        ///< Loop music if video is longer

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return MusicConfig object
     */
    static MusicConfig fromJson(const Json::Value& json);

    /**
     * @brief Get music file path for style
     * @return Path to music file
     */
    std::string getMusicFilePath() const;
};

// ============================================================================
// OUTPUT CONFIG STRUCTURE
// ============================================================================

/**
 * @struct OutputConfig
 * @brief Configuration for video output format
 */
struct OutputConfig {
    int width = 1920;                        ///< Output width in pixels
    int height = 1080;                       ///< Output height in pixels
    AspectRatio aspect_ratio = AspectRatio::LANDSCAPE_16_9;  ///< Aspect ratio
    int fps = 30;                            ///< Frames per second
    int bitrate_kbps = 8000;                 ///< Video bitrate in kbps
    std::string codec = "libx264";           ///< Video codec
    std::string preset = "medium";           ///< Encoding preset (speed vs quality)
    std::string format = "mp4";              ///< Output format
    int crf = 23;                            ///< Constant Rate Factor (quality, lower = better)
    std::string audio_codec = "aac";         ///< Audio codec
    int audio_bitrate_kbps = 192;            ///< Audio bitrate

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return OutputConfig object
     */
    static OutputConfig fromJson(const Json::Value& json);

    /**
     * @brief Get dimensions from aspect ratio
     * @param max_dimension Maximum dimension (width or height)
     * @return Dimensions struct
     */
    static Dimensions getDimensionsForAspectRatio(AspectRatio ratio, int max_dimension = 1920);

    /**
     * @brief Get preset for specific platform
     * @param platform Platform name (youtube, instagram, tiktok, facebook, twitter)
     * @return OutputConfig optimized for platform
     */
    static OutputConfig forPlatform(const std::string& platform);
};

// ============================================================================
// MAIN VIDEO TEMPLATE STRUCTURE
// ============================================================================

/**
 * @struct VideoTemplate
 * @brief Complete video template configuration
 *
 * Defines all aspects of video generation including duration,
 * transitions, overlays, music, and output format.
 */
struct VideoTemplate {
    // Identity
    std::string id;                          ///< Template ID
    std::string name;                        ///< Template display name
    std::string description;                 ///< Template description
    TemplateType type = TemplateType::SLIDESHOW;  ///< Template type

    // Timing
    double total_duration = 30.0;            ///< Total video duration in seconds
    double photo_duration = 5.0;             ///< Duration per photo in seconds
    int min_photos = 3;                      ///< Minimum required photos
    int max_photos = 10;                     ///< Maximum recommended photos

    // Effects
    TransitionConfig transition;             ///< Transition configuration
    MotionConfig motion;                     ///< Motion effect configuration
    MusicConfig music;                       ///< Background music configuration

    // Overlays
    OverlayConfig overlay_config;            ///< Overlay configuration

    // Text placements
    std::vector<TextPlacement> text_placements;  ///< Dynamic text placements

    // Output
    OutputConfig output;                     ///< Output configuration

    // Intro/Outro
    bool include_intro = false;              ///< Include intro sequence
    double intro_duration = 3.0;             ///< Intro duration in seconds
    std::string intro_video_path;            ///< Path to intro video

    bool include_outro = false;              ///< Include outro sequence
    double outro_duration = 3.0;             ///< Outro duration in seconds
    std::string outro_video_path;            ///< Path to outro video

    // Branding
    bool include_watermark = true;           ///< Include watermark
    std::string watermark_path;              ///< Path to watermark image

    // ========================================================================
    // METHODS
    // ========================================================================

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return VideoTemplate object
     */
    static VideoTemplate fromJson(const Json::Value& json);

    /**
     * @brief Get template by type
     * @param type Template type
     * @return Pre-configured template
     */
    static VideoTemplate get(TemplateType type);

    /**
     * @brief Get template by name
     * @param name Template name
     * @return Pre-configured template
     */
    static VideoTemplate getByName(const std::string& name);

    /**
     * @brief Get all available templates
     * @return Vector of all template types with names
     */
    static std::vector<std::pair<TemplateType, std::string>> getAvailableTemplates();

    /**
     * @brief Calculate total duration based on photo count
     * @param photo_count Number of photos
     * @return Calculated duration in seconds
     */
    double calculateDuration(int photo_count) const;

    /**
     * @brief Validate template configuration
     * @return True if valid, false otherwise
     */
    bool validate() const;

    /**
     * @brief Add text placement
     * @param placement Text placement configuration
     */
    void addTextPlacement(const TextPlacement& placement);

    /**
     * @brief Clear all text placements
     */
    void clearTextPlacements();

    /**
     * @brief Apply platform-specific settings
     * @param platform Platform name
     */
    void applyPlatformSettings(const std::string& platform);

    /**
     * @brief Clone template
     * @return Copy of template
     */
    VideoTemplate clone() const;

    // ========================================================================
    // FACTORY METHODS FOR SPECIFIC TEMPLATES
    // ========================================================================

    /**
     * @brief Create slideshow template
     * @return Slideshow template
     */
    static VideoTemplate createSlideshow();

    /**
     * @brief Create Ken Burns template
     * @return Ken Burns template
     */
    static VideoTemplate createKenBurns();

    /**
     * @brief Create countdown template
     * @return Countdown template
     */
    static VideoTemplate createCountdown();

    /**
     * @brief Create story template (9:16)
     * @return Story template
     */
    static VideoTemplate createStory();

    /**
     * @brief Create promotional template
     * @return Promotional template
     */
    static VideoTemplate createPromotional();

    /**
     * @brief Create quick share template
     * @return Quick share template
     */
    static VideoTemplate createQuickShare();

    /**
     * @brief Create adoption appeal template
     * @return Adoption appeal template
     */
    static VideoTemplate createAdoptionAppeal();
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert TemplateType enum to string
 * @param type TemplateType value
 * @return String representation
 */
std::string templateTypeToString(TemplateType type);

/**
 * @brief Convert string to TemplateType enum
 * @param str String representation
 * @return TemplateType value
 */
TemplateType stringToTemplateType(const std::string& str);

/**
 * @brief Convert TransitionStyle enum to string
 * @param style TransitionStyle value
 * @return String representation
 */
std::string transitionStyleToString(TransitionStyle style);

/**
 * @brief Convert string to TransitionStyle enum
 * @param str String representation
 * @return TransitionStyle value
 */
TransitionStyle stringToTransitionStyle(const std::string& str);

/**
 * @brief Convert MusicStyle enum to string
 * @param style MusicStyle value
 * @return String representation
 */
std::string musicStyleToString(MusicStyle style);

/**
 * @brief Convert string to MusicStyle enum
 * @param str String representation
 * @return MusicStyle value
 */
MusicStyle stringToMusicStyle(const std::string& str);

/**
 * @brief Convert AspectRatio enum to string
 * @param ratio AspectRatio value
 * @return String representation
 */
std::string aspectRatioToString(AspectRatio ratio);

/**
 * @brief Convert string to AspectRatio enum
 * @param str String representation
 * @return AspectRatio value
 */
AspectRatio stringToAspectRatio(const std::string& str);

/**
 * @brief Convert MotionType enum to string
 * @param type MotionType value
 * @return String representation
 */
std::string motionTypeToString(MotionType type);

/**
 * @brief Convert string to MotionType enum
 * @param str String representation
 * @return MotionType value
 */
MotionType stringToMotionType(const std::string& str);

} // namespace wtl::content::media

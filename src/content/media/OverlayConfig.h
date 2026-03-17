/**
 * @file OverlayConfig.h
 * @brief Configuration structures for video and image overlays
 *
 * PURPOSE:
 * Defines configuration structures for LED counter overlays, urgency badges,
 * text overlays, and logo placements used in video and image generation.
 * These configurations control the visual appearance and positioning of
 * overlay elements.
 *
 * USAGE:
 * OverlayConfig config;
 * config.led_counter.position = Position::TOP_RIGHT;
 * config.led_counter.font_size = 48;
 * config.urgency_badge.visible = true;
 * config.urgency_badge.urgency_level = "critical";
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value for serialization)
 *
 * MODIFICATION GUIDE:
 * - Add new overlay types by creating new config structs
 * - Extend Position enum for more placement options
 * - Add new fields to existing configs as needed
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

namespace wtl::content::media {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum Position
 * @brief Screen position for overlay placement
 */
enum class Position {
    TOP_LEFT = 0,
    TOP_CENTER,
    TOP_RIGHT,
    CENTER_LEFT,
    CENTER,
    CENTER_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
    CUSTOM
};

/**
 * @enum TextAlign
 * @brief Text alignment within overlay
 */
enum class TextAlign {
    LEFT = 0,
    CENTER,
    RIGHT
};

/**
 * @enum FontStyle
 * @brief Font style for text overlays
 */
enum class FontStyle {
    NORMAL = 0,
    BOLD,
    ITALIC,
    BOLD_ITALIC
};

// ============================================================================
// COLOR STRUCTURE
// ============================================================================

/**
 * @struct Color
 * @brief RGBA color representation
 */
struct Color {
    uint8_t r = 0;     ///< Red component (0-255)
    uint8_t g = 0;     ///< Green component (0-255)
    uint8_t b = 0;     ///< Blue component (0-255)
    uint8_t a = 255;   ///< Alpha component (0-255)

    /**
     * @brief Create color from hex string (e.g., "#FF0000" or "FF0000")
     * @param hex Hex color string
     * @return Color object
     */
    static Color fromHex(const std::string& hex);

    /**
     * @brief Convert to hex string
     * @param include_alpha Whether to include alpha in output
     * @return Hex color string (e.g., "#FF0000FF")
     */
    std::string toHex(bool include_alpha = false) const;

    /**
     * @brief Convert to FFmpeg color format
     * @return FFmpeg color string (e.g., "0xFF0000")
     */
    std::string toFFmpegFormat() const;

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return Color object
     */
    static Color fromJson(const Json::Value& json);

    // Predefined colors
    static Color white();
    static Color black();
    static Color red();
    static Color green();
    static Color blue();
    static Color yellow();
    static Color orange();
    static Color transparent();
};

// ============================================================================
// CUSTOM COORDINATES
// ============================================================================

/**
 * @struct Coordinates
 * @brief Custom X/Y coordinates for positioning
 */
struct Coordinates {
    int x = 0;   ///< X position in pixels
    int y = 0;   ///< Y position in pixels

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static Coordinates fromJson(const Json::Value& json);
};

/**
 * @struct Dimensions
 * @brief Width and height dimensions
 */
struct Dimensions {
    int width = 0;    ///< Width in pixels
    int height = 0;   ///< Height in pixels

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static Dimensions fromJson(const Json::Value& json);

    /**
     * @brief Calculate aspect ratio
     * @return Aspect ratio as width/height
     */
    double aspectRatio() const;

    /**
     * @brief Scale dimensions to fit within max bounds while preserving aspect ratio
     * @param max_width Maximum width
     * @param max_height Maximum height
     * @return Scaled dimensions
     */
    Dimensions scaleToFit(int max_width, int max_height) const;
};

// ============================================================================
// LED COUNTER CONFIG
// ============================================================================

/**
 * @struct LEDCounterConfig
 * @brief Configuration for LED-style wait time counter overlay
 */
struct LEDCounterConfig {
    bool visible = true;                    ///< Whether counter is visible
    Position position = Position::TOP_RIGHT; ///< Screen position
    Coordinates custom_position;            ///< Custom position if position == CUSTOM

    // Visual appearance
    int font_size = 36;                     ///< Font size in pixels
    Color digit_color = Color::green();    ///< Color of LED digits
    Color background_color = Color::black(); ///< Background color
    Color glow_color;                       ///< Glow effect color (if enabled)
    bool enable_glow = true;                ///< Enable LED glow effect
    float glow_intensity = 0.6f;            ///< Glow intensity (0.0 - 1.0)

    // Counter format
    bool show_years = true;                 ///< Show years in YY:MM:DD:HH:MM:SS
    bool show_seconds = true;               ///< Show seconds
    std::string separator = ":";            ///< Separator between components
    int segment_width = 2;                  ///< Width of LED segments

    // Margins and padding
    int margin_x = 20;                      ///< Horizontal margin from edge
    int margin_y = 20;                      ///< Vertical margin from edge
    int padding = 10;                       ///< Padding inside background

    // Animation
    bool animate_digits = false;            ///< Animate digit changes
    int animation_fps = 30;                 ///< Animation frame rate

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return LEDCounterConfig object
     */
    static LEDCounterConfig fromJson(const Json::Value& json);

    /**
     * @brief Get default wait time counter config (green)
     * @return Default config for wait time display
     */
    static LEDCounterConfig defaultWaitTime();

    /**
     * @brief Get default countdown counter config (red)
     * @return Default config for countdown display
     */
    static LEDCounterConfig defaultCountdown();
};

// ============================================================================
// URGENCY BADGE CONFIG
// ============================================================================

/**
 * @struct UrgencyBadgeConfig
 * @brief Configuration for urgency level badge overlay
 */
struct UrgencyBadgeConfig {
    bool visible = true;                       ///< Whether badge is visible
    Position position = Position::TOP_LEFT;    ///< Screen position
    Coordinates custom_position;               ///< Custom position if position == CUSTOM

    // Badge content
    std::string urgency_level = "normal";      ///< Urgency level (normal, medium, high, critical)
    bool show_text = true;                     ///< Show text label
    bool show_icon = true;                     ///< Show urgency icon

    // Visual appearance
    Dimensions size = {120, 40};               ///< Badge size
    int corner_radius = 8;                     ///< Rounded corner radius
    int border_width = 2;                      ///< Border width

    // Colors (auto-set based on urgency_level if not specified)
    std::optional<Color> background_color;     ///< Override background color
    std::optional<Color> text_color;           ///< Override text color
    std::optional<Color> border_color;         ///< Override border color

    // Font
    int font_size = 14;                        ///< Font size for text
    FontStyle font_style = FontStyle::BOLD;    ///< Font style

    // Margins
    int margin_x = 15;                         ///< Horizontal margin from edge
    int margin_y = 15;                         ///< Vertical margin from edge

    // Animation
    bool pulse_animation = false;              ///< Enable pulse animation for critical
    bool flash_animation = false;              ///< Enable flash animation

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return UrgencyBadgeConfig object
     */
    static UrgencyBadgeConfig fromJson(const Json::Value& json);

    /**
     * @brief Get badge image path for current urgency level
     * @return Path to badge overlay image
     */
    std::string getBadgeImagePath() const;

    /**
     * @brief Get auto color scheme for urgency level
     * @return Tuple of (background, text, border) colors
     */
    std::tuple<Color, Color, Color> getAutoColors() const;
};

// ============================================================================
// TEXT OVERLAY CONFIG
// ============================================================================

/**
 * @struct TextOverlayConfig
 * @brief Configuration for text overlays (captions, titles, etc.)
 */
struct TextOverlayConfig {
    bool visible = true;                       ///< Whether text is visible
    std::string text;                          ///< The text to display
    Position position = Position::BOTTOM_CENTER; ///< Screen position
    Coordinates custom_position;               ///< Custom position if position == CUSTOM

    // Font settings
    std::string font_family = "Arial";         ///< Font family name
    int font_size = 24;                        ///< Font size in pixels
    FontStyle font_style = FontStyle::NORMAL;  ///< Font style
    Color text_color = Color::white();         ///< Text color

    // Background
    bool show_background = true;               ///< Show background behind text
    Color background_color;                    ///< Background color
    int background_padding = 8;                ///< Padding around text
    int background_radius = 4;                 ///< Background corner radius

    // Shadow/outline
    bool enable_shadow = true;                 ///< Enable drop shadow
    Color shadow_color = Color::black();       ///< Shadow color
    int shadow_offset_x = 2;                   ///< Shadow X offset
    int shadow_offset_y = 2;                   ///< Shadow Y offset
    int shadow_blur = 4;                       ///< Shadow blur radius

    bool enable_outline = false;               ///< Enable text outline
    Color outline_color = Color::black();      ///< Outline color
    int outline_width = 2;                     ///< Outline width

    // Alignment and wrapping
    TextAlign alignment = TextAlign::CENTER;   ///< Text alignment
    int max_width = 0;                         ///< Max width for wrapping (0 = no limit)
    bool word_wrap = true;                     ///< Enable word wrapping

    // Animation
    double fade_in_duration = 0.0;             ///< Fade in duration in seconds
    double fade_out_duration = 0.0;            ///< Fade out duration in seconds
    double display_duration = 0.0;             ///< How long to show (0 = entire video)
    double start_time = 0.0;                   ///< When to start showing (seconds)

    // Margins
    int margin_x = 20;                         ///< Horizontal margin from edge
    int margin_y = 40;                         ///< Vertical margin from edge

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return TextOverlayConfig object
     */
    static TextOverlayConfig fromJson(const Json::Value& json);

    /**
     * @brief Create title text config
     * @param text Title text
     * @return Config suitable for title overlay
     */
    static TextOverlayConfig createTitle(const std::string& text);

    /**
     * @brief Create caption text config
     * @param text Caption text
     * @return Config suitable for caption overlay
     */
    static TextOverlayConfig createCaption(const std::string& text);

    /**
     * @brief Create call-to-action text config
     * @param text CTA text
     * @return Config suitable for CTA overlay
     */
    static TextOverlayConfig createCallToAction(const std::string& text);
};

// ============================================================================
// LOGO OVERLAY CONFIG
// ============================================================================

/**
 * @struct LogoOverlayConfig
 * @brief Configuration for logo/watermark overlay
 */
struct LogoOverlayConfig {
    bool visible = true;                          ///< Whether logo is visible
    std::string logo_path;                        ///< Path to logo image
    Position position = Position::BOTTOM_RIGHT;   ///< Screen position
    Coordinates custom_position;                  ///< Custom position if position == CUSTOM

    // Size
    Dimensions size = {100, 100};                 ///< Logo size
    bool maintain_aspect_ratio = true;            ///< Maintain original aspect ratio

    // Appearance
    float opacity = 0.8f;                         ///< Opacity (0.0 - 1.0)
    bool grayscale = false;                       ///< Convert to grayscale

    // Margins
    int margin_x = 15;                            ///< Horizontal margin from edge
    int margin_y = 15;                            ///< Vertical margin from edge

    // Animation
    double fade_in_duration = 0.5;                ///< Fade in duration in seconds

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return LogoOverlayConfig object
     */
    static LogoOverlayConfig fromJson(const Json::Value& json);
};

// ============================================================================
// MAIN OVERLAY CONFIG
// ============================================================================

/**
 * @struct OverlayConfig
 * @brief Complete overlay configuration for video/image generation
 *
 * Contains all overlay configurations: LED counter, urgency badge,
 * text overlays, and logo placement.
 */
struct OverlayConfig {
    // Individual overlay configs
    LEDCounterConfig led_counter;              ///< LED wait time counter
    LEDCounterConfig countdown_counter;        ///< LED countdown timer (for euthanasia)
    UrgencyBadgeConfig urgency_badge;          ///< Urgency level badge
    std::vector<TextOverlayConfig> text_overlays; ///< Text overlays (captions, titles)
    LogoOverlayConfig logo;                    ///< Logo/watermark

    // Global settings
    bool enable_overlays = true;               ///< Master switch for all overlays
    int target_width = 1920;                   ///< Target video/image width
    int target_height = 1080;                  ///< Target video/image height

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return OverlayConfig object
     */
    static OverlayConfig fromJson(const Json::Value& json);

    /**
     * @brief Get default overlay config
     * @return Default configuration
     */
    static OverlayConfig getDefault();

    /**
     * @brief Get minimal overlay config (just counter)
     * @return Minimal configuration
     */
    static OverlayConfig getMinimal();

    /**
     * @brief Get full overlay config (all overlays enabled)
     * @return Full configuration
     */
    static OverlayConfig getFull();

    /**
     * @brief Create config for social media share card
     * @return Config optimized for share cards
     */
    static OverlayConfig forShareCard();

    /**
     * @brief Create config for story format (9:16)
     * @return Config optimized for stories
     */
    static OverlayConfig forStory();

    /**
     * @brief Add a text overlay
     * @param config Text overlay configuration
     */
    void addTextOverlay(const TextOverlayConfig& config);

    /**
     * @brief Clear all text overlays
     */
    void clearTextOverlays();

    /**
     * @brief Validate configuration
     * @return True if valid, false otherwise
     */
    bool validate() const;

    /**
     * @brief Scale all overlay positions for different output sizes
     * @param width New target width
     * @param height New target height
     */
    void scaleToSize(int width, int height);
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert Position enum to string
 * @param pos Position value
 * @return String representation
 */
std::string positionToString(Position pos);

/**
 * @brief Convert string to Position enum
 * @param str String representation
 * @return Position value
 */
Position stringToPosition(const std::string& str);

/**
 * @brief Convert TextAlign enum to string
 * @param align TextAlign value
 * @return String representation
 */
std::string textAlignToString(TextAlign align);

/**
 * @brief Convert string to TextAlign enum
 * @param str String representation
 * @return TextAlign value
 */
TextAlign stringToTextAlign(const std::string& str);

/**
 * @brief Convert FontStyle enum to string
 * @param style FontStyle value
 * @return String representation
 */
std::string fontStyleToString(FontStyle style);

/**
 * @brief Convert string to FontStyle enum
 * @param str String representation
 * @return FontStyle value
 */
FontStyle stringToFontStyle(const std::string& str);

/**
 * @brief Calculate screen coordinates from Position enum
 * @param pos Position enum value
 * @param element_width Width of element to place
 * @param element_height Height of element to place
 * @param screen_width Total screen width
 * @param screen_height Total screen height
 * @param margin_x Horizontal margin
 * @param margin_y Vertical margin
 * @return Coordinates for element placement
 */
Coordinates calculatePosition(
    Position pos,
    int element_width,
    int element_height,
    int screen_width,
    int screen_height,
    int margin_x = 0,
    int margin_y = 0
);

} // namespace wtl::content::media

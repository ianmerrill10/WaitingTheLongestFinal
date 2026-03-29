/**
 * @file OverlayConfig.cc
 * @brief Implementation of overlay configuration structures
 * @see OverlayConfig.h for documentation
 */

#include "content/media/OverlayConfig.h"

// Standard library includes
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace wtl::content::media {

// ============================================================================
// COLOR IMPLEMENTATION
// ============================================================================

Color Color::fromHex(const std::string& hex) {
    Color color;
    std::string h = hex;

    // Remove # prefix if present
    if (!h.empty() && h[0] == '#') {
        h = h.substr(1);
    }

    if (h.length() >= 6) {
        color.r = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
        color.g = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
        color.b = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));

        if (h.length() >= 8) {
            color.a = static_cast<uint8_t>(std::stoi(h.substr(6, 2), nullptr, 16));
        }
    }

    return color;
}

std::string Color::toHex(bool include_alpha) const {
    std::ostringstream oss;
    oss << "#" << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(r)
        << std::setw(2) << static_cast<int>(g)
        << std::setw(2) << static_cast<int>(b);

    if (include_alpha) {
        oss << std::setw(2) << static_cast<int>(a);
    }

    return oss.str();
}

std::string Color::toFFmpegFormat() const {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(r)
        << std::setw(2) << static_cast<int>(g)
        << std::setw(2) << static_cast<int>(b);

    if (a < 255) {
        oss << "@" << std::fixed << std::setprecision(2)
            << (static_cast<float>(a) / 255.0f);
    }

    return oss.str();
}

Json::Value Color::toJson() const {
    Json::Value json;
    json["r"] = r;
    json["g"] = g;
    json["b"] = b;
    json["a"] = a;
    json["hex"] = toHex(true);
    return json;
}

Color Color::fromJson(const Json::Value& json) {
    Color color;

    if (json.isString()) {
        return fromHex(json.asString());
    }

    if (json.isMember("hex")) {
        return fromHex(json["hex"].asString());
    }

    color.r = static_cast<uint8_t>(json.get("r", 0).asInt());
    color.g = static_cast<uint8_t>(json.get("g", 0).asInt());
    color.b = static_cast<uint8_t>(json.get("b", 0).asInt());
    color.a = static_cast<uint8_t>(json.get("a", 255).asInt());

    return color;
}

Color Color::white() { return Color{255, 255, 255, 255}; }
Color Color::black() { return Color{0, 0, 0, 255}; }
Color Color::red() { return Color{255, 0, 0, 255}; }
Color Color::green() { return Color{0, 255, 0, 255}; }
Color Color::blue() { return Color{0, 0, 255, 255}; }
Color Color::yellow() { return Color{255, 255, 0, 255}; }
Color Color::orange() { return Color{255, 165, 0, 255}; }
Color Color::transparent() { return Color{0, 0, 0, 0}; }

// ============================================================================
// COORDINATES IMPLEMENTATION
// ============================================================================

Json::Value Coordinates::toJson() const {
    Json::Value json;
    json["x"] = x;
    json["y"] = y;
    return json;
}

Coordinates Coordinates::fromJson(const Json::Value& json) {
    Coordinates coords;
    coords.x = json.get("x", 0).asInt();
    coords.y = json.get("y", 0).asInt();
    return coords;
}

// ============================================================================
// DIMENSIONS IMPLEMENTATION
// ============================================================================

Json::Value Dimensions::toJson() const {
    Json::Value json;
    json["width"] = width;
    json["height"] = height;
    return json;
}

Dimensions Dimensions::fromJson(const Json::Value& json) {
    Dimensions dims;
    dims.width = json.get("width", 0).asInt();
    dims.height = json.get("height", 0).asInt();
    return dims;
}

double Dimensions::aspectRatio() const {
    if (height == 0) return 0.0;
    return static_cast<double>(width) / static_cast<double>(height);
}

Dimensions Dimensions::scaleToFit(int max_width, int max_height) const {
    if (width == 0 || height == 0) {
        return Dimensions{0, 0};
    }

    double scale_x = static_cast<double>(max_width) / width;
    double scale_y = static_cast<double>(max_height) / height;
    double scale = std::min(scale_x, scale_y);

    return Dimensions{
        static_cast<int>(width * scale),
        static_cast<int>(height * scale)
    };
}

// ============================================================================
// LED COUNTER CONFIG IMPLEMENTATION
// ============================================================================

Json::Value LEDCounterConfig::toJson() const {
    Json::Value json;
    json["visible"] = visible;
    json["position"] = positionToString(position);
    json["custom_position"] = custom_position.toJson();
    json["font_size"] = font_size;
    json["digit_color"] = digit_color.toJson();
    json["background_color"] = background_color.toJson();
    json["glow_color"] = glow_color.toJson();
    json["enable_glow"] = enable_glow;
    json["glow_intensity"] = glow_intensity;
    json["show_years"] = show_years;
    json["show_seconds"] = show_seconds;
    json["separator"] = separator;
    json["segment_width"] = segment_width;
    json["margin_x"] = margin_x;
    json["margin_y"] = margin_y;
    json["padding"] = padding;
    json["animate_digits"] = animate_digits;
    json["animation_fps"] = animation_fps;
    return json;
}

LEDCounterConfig LEDCounterConfig::fromJson(const Json::Value& json) {
    LEDCounterConfig config;

    config.visible = json.get("visible", true).asBool();
    config.position = stringToPosition(json.get("position", "top_right").asString());

    if (json.isMember("custom_position")) {
        config.custom_position = Coordinates::fromJson(json["custom_position"]);
    }

    config.font_size = json.get("font_size", 36).asInt();

    if (json.isMember("digit_color")) {
        config.digit_color = Color::fromJson(json["digit_color"]);
    }
    if (json.isMember("background_color")) {
        config.background_color = Color::fromJson(json["background_color"]);
    }
    if (json.isMember("glow_color")) {
        config.glow_color = Color::fromJson(json["glow_color"]);
    }

    config.enable_glow = json.get("enable_glow", true).asBool();
    config.glow_intensity = static_cast<float>(json.get("glow_intensity", 0.6).asDouble());
    config.show_years = json.get("show_years", true).asBool();
    config.show_seconds = json.get("show_seconds", true).asBool();
    config.separator = json.get("separator", ":").asString();
    config.segment_width = json.get("segment_width", 2).asInt();
    config.margin_x = json.get("margin_x", 20).asInt();
    config.margin_y = json.get("margin_y", 20).asInt();
    config.padding = json.get("padding", 10).asInt();
    config.animate_digits = json.get("animate_digits", false).asBool();
    config.animation_fps = json.get("animation_fps", 30).asInt();

    return config;
}

LEDCounterConfig LEDCounterConfig::defaultWaitTime() {
    LEDCounterConfig config;
    config.visible = true;
    config.position = Position::TOP_RIGHT;
    config.font_size = 36;
    config.digit_color = Color::green();
    config.background_color = Color{0, 0, 0, 200};
    config.glow_color = Color{0, 255, 0, 128};
    config.enable_glow = true;
    config.glow_intensity = 0.6f;
    config.show_years = true;
    config.show_seconds = true;
    return config;
}

LEDCounterConfig LEDCounterConfig::defaultCountdown() {
    LEDCounterConfig config;
    config.visible = true;
    config.position = Position::TOP_LEFT;
    config.font_size = 36;
    config.digit_color = Color::red();
    config.background_color = Color{0, 0, 0, 200};
    config.glow_color = Color{255, 0, 0, 128};
    config.enable_glow = true;
    config.glow_intensity = 0.8f;
    config.show_years = false;
    config.show_seconds = true;
    return config;
}

// ============================================================================
// URGENCY BADGE CONFIG IMPLEMENTATION
// ============================================================================

Json::Value UrgencyBadgeConfig::toJson() const {
    Json::Value json;
    json["visible"] = visible;
    json["position"] = positionToString(position);
    json["custom_position"] = custom_position.toJson();
    json["urgency_level"] = urgency_level;
    json["show_text"] = show_text;
    json["show_icon"] = show_icon;
    json["size"] = size.toJson();
    json["corner_radius"] = corner_radius;
    json["border_width"] = border_width;

    if (background_color) {
        json["background_color"] = background_color->toJson();
    }
    if (text_color) {
        json["text_color"] = text_color->toJson();
    }
    if (border_color) {
        json["border_color"] = border_color->toJson();
    }

    json["font_size"] = font_size;
    json["font_style"] = fontStyleToString(font_style);
    json["margin_x"] = margin_x;
    json["margin_y"] = margin_y;
    json["pulse_animation"] = pulse_animation;
    json["flash_animation"] = flash_animation;

    return json;
}

UrgencyBadgeConfig UrgencyBadgeConfig::fromJson(const Json::Value& json) {
    UrgencyBadgeConfig config;

    config.visible = json.get("visible", true).asBool();
    config.position = stringToPosition(json.get("position", "top_left").asString());

    if (json.isMember("custom_position")) {
        config.custom_position = Coordinates::fromJson(json["custom_position"]);
    }

    config.urgency_level = json.get("urgency_level", "normal").asString();
    config.show_text = json.get("show_text", true).asBool();
    config.show_icon = json.get("show_icon", true).asBool();

    if (json.isMember("size")) {
        config.size = Dimensions::fromJson(json["size"]);
    }

    config.corner_radius = json.get("corner_radius", 8).asInt();
    config.border_width = json.get("border_width", 2).asInt();

    if (json.isMember("background_color")) {
        config.background_color = Color::fromJson(json["background_color"]);
    }
    if (json.isMember("text_color")) {
        config.text_color = Color::fromJson(json["text_color"]);
    }
    if (json.isMember("border_color")) {
        config.border_color = Color::fromJson(json["border_color"]);
    }

    config.font_size = json.get("font_size", 14).asInt();
    config.font_style = stringToFontStyle(json.get("font_style", "bold").asString());
    config.margin_x = json.get("margin_x", 15).asInt();
    config.margin_y = json.get("margin_y", 15).asInt();
    config.pulse_animation = json.get("pulse_animation", false).asBool();
    config.flash_animation = json.get("flash_animation", false).asBool();

    return config;
}

std::string UrgencyBadgeConfig::getBadgeImagePath() const {
    return "static/images/overlays/urgency-" + urgency_level + ".png";
}

std::tuple<Color, Color, Color> UrgencyBadgeConfig::getAutoColors() const {
    // Return (background, text, border) colors based on urgency level
    if (urgency_level == "critical") {
        return {
            Color{220, 38, 38, 255},   // Dark red background
            Color::white(),             // White text
            Color{185, 28, 28, 255}    // Darker red border
        };
    } else if (urgency_level == "high") {
        return {
            Color{249, 115, 22, 255},  // Orange background
            Color::white(),             // White text
            Color{234, 88, 12, 255}    // Darker orange border
        };
    } else if (urgency_level == "medium") {
        return {
            Color{250, 204, 21, 255},  // Yellow background
            Color::black(),             // Black text
            Color{202, 138, 4, 255}    // Darker yellow border
        };
    } else {
        // Normal
        return {
            Color{34, 197, 94, 255},   // Green background
            Color::white(),             // White text
            Color{22, 163, 74, 255}    // Darker green border
        };
    }
}

// ============================================================================
// TEXT OVERLAY CONFIG IMPLEMENTATION
// ============================================================================

Json::Value TextOverlayConfig::toJson() const {
    Json::Value json;
    json["visible"] = visible;
    json["text"] = text;
    json["position"] = positionToString(position);
    json["custom_position"] = custom_position.toJson();
    json["font_family"] = font_family;
    json["font_size"] = font_size;
    json["font_style"] = fontStyleToString(font_style);
    json["text_color"] = text_color.toJson();
    json["show_background"] = show_background;
    json["background_color"] = background_color.toJson();
    json["background_padding"] = background_padding;
    json["background_radius"] = background_radius;
    json["enable_shadow"] = enable_shadow;
    json["shadow_color"] = shadow_color.toJson();
    json["shadow_offset_x"] = shadow_offset_x;
    json["shadow_offset_y"] = shadow_offset_y;
    json["shadow_blur"] = shadow_blur;
    json["enable_outline"] = enable_outline;
    json["outline_color"] = outline_color.toJson();
    json["outline_width"] = outline_width;
    json["alignment"] = textAlignToString(alignment);
    json["max_width"] = max_width;
    json["word_wrap"] = word_wrap;
    json["fade_in_duration"] = fade_in_duration;
    json["fade_out_duration"] = fade_out_duration;
    json["display_duration"] = display_duration;
    json["start_time"] = start_time;
    json["margin_x"] = margin_x;
    json["margin_y"] = margin_y;
    return json;
}

TextOverlayConfig TextOverlayConfig::fromJson(const Json::Value& json) {
    TextOverlayConfig config;

    config.visible = json.get("visible", true).asBool();
    config.text = json.get("text", "").asString();
    config.position = stringToPosition(json.get("position", "bottom_center").asString());

    if (json.isMember("custom_position")) {
        config.custom_position = Coordinates::fromJson(json["custom_position"]);
    }

    config.font_family = json.get("font_family", "Arial").asString();
    config.font_size = json.get("font_size", 24).asInt();
    config.font_style = stringToFontStyle(json.get("font_style", "normal").asString());

    if (json.isMember("text_color")) {
        config.text_color = Color::fromJson(json["text_color"]);
    }

    config.show_background = json.get("show_background", true).asBool();

    if (json.isMember("background_color")) {
        config.background_color = Color::fromJson(json["background_color"]);
    }

    config.background_padding = json.get("background_padding", 8).asInt();
    config.background_radius = json.get("background_radius", 4).asInt();
    config.enable_shadow = json.get("enable_shadow", true).asBool();

    if (json.isMember("shadow_color")) {
        config.shadow_color = Color::fromJson(json["shadow_color"]);
    }

    config.shadow_offset_x = json.get("shadow_offset_x", 2).asInt();
    config.shadow_offset_y = json.get("shadow_offset_y", 2).asInt();
    config.shadow_blur = json.get("shadow_blur", 4).asInt();
    config.enable_outline = json.get("enable_outline", false).asBool();

    if (json.isMember("outline_color")) {
        config.outline_color = Color::fromJson(json["outline_color"]);
    }

    config.outline_width = json.get("outline_width", 2).asInt();
    config.alignment = stringToTextAlign(json.get("alignment", "center").asString());
    config.max_width = json.get("max_width", 0).asInt();
    config.word_wrap = json.get("word_wrap", true).asBool();
    config.fade_in_duration = json.get("fade_in_duration", 0.0).asDouble();
    config.fade_out_duration = json.get("fade_out_duration", 0.0).asDouble();
    config.display_duration = json.get("display_duration", 0.0).asDouble();
    config.start_time = json.get("start_time", 0.0).asDouble();
    config.margin_x = json.get("margin_x", 20).asInt();
    config.margin_y = json.get("margin_y", 40).asInt();

    return config;
}

TextOverlayConfig TextOverlayConfig::createTitle(const std::string& text) {
    TextOverlayConfig config;
    config.text = text;
    config.position = Position::TOP_CENTER;
    config.font_size = 48;
    config.font_style = FontStyle::BOLD;
    config.text_color = Color::white();
    config.show_background = true;
    config.background_color = Color{0, 0, 0, 180};
    config.background_padding = 16;
    config.background_radius = 8;
    config.enable_shadow = true;
    config.fade_in_duration = 0.5;
    config.margin_y = 60;
    return config;
}

TextOverlayConfig TextOverlayConfig::createCaption(const std::string& text) {
    TextOverlayConfig config;
    config.text = text;
    config.position = Position::BOTTOM_CENTER;
    config.font_size = 24;
    config.font_style = FontStyle::NORMAL;
    config.text_color = Color::white();
    config.show_background = true;
    config.background_color = Color{0, 0, 0, 160};
    config.background_padding = 10;
    config.background_radius = 4;
    config.enable_shadow = true;
    config.margin_y = 40;
    return config;
}

TextOverlayConfig TextOverlayConfig::createCallToAction(const std::string& text) {
    TextOverlayConfig config;
    config.text = text;
    config.position = Position::BOTTOM_CENTER;
    config.font_size = 32;
    config.font_style = FontStyle::BOLD;
    config.text_color = Color::white();
    config.show_background = true;
    config.background_color = Color{59, 130, 246, 255}; // Blue
    config.background_padding = 16;
    config.background_radius = 8;
    config.enable_shadow = true;
    config.margin_y = 80;
    return config;
}

// ============================================================================
// LOGO OVERLAY CONFIG IMPLEMENTATION
// ============================================================================

Json::Value LogoOverlayConfig::toJson() const {
    Json::Value json;
    json["visible"] = visible;
    json["logo_path"] = logo_path;
    json["position"] = positionToString(position);
    json["custom_position"] = custom_position.toJson();
    json["size"] = size.toJson();
    json["maintain_aspect_ratio"] = maintain_aspect_ratio;
    json["opacity"] = opacity;
    json["grayscale"] = grayscale;
    json["margin_x"] = margin_x;
    json["margin_y"] = margin_y;
    json["fade_in_duration"] = fade_in_duration;
    return json;
}

LogoOverlayConfig LogoOverlayConfig::fromJson(const Json::Value& json) {
    LogoOverlayConfig config;

    config.visible = json.get("visible", true).asBool();
    config.logo_path = json.get("logo_path", "").asString();
    config.position = stringToPosition(json.get("position", "bottom_right").asString());

    if (json.isMember("custom_position")) {
        config.custom_position = Coordinates::fromJson(json["custom_position"]);
    }
    if (json.isMember("size")) {
        config.size = Dimensions::fromJson(json["size"]);
    }

    config.maintain_aspect_ratio = json.get("maintain_aspect_ratio", true).asBool();
    config.opacity = static_cast<float>(json.get("opacity", 0.8).asDouble());
    config.grayscale = json.get("grayscale", false).asBool();
    config.margin_x = json.get("margin_x", 15).asInt();
    config.margin_y = json.get("margin_y", 15).asInt();
    config.fade_in_duration = json.get("fade_in_duration", 0.5).asDouble();

    return config;
}

// ============================================================================
// MAIN OVERLAY CONFIG IMPLEMENTATION
// ============================================================================

Json::Value OverlayConfig::toJson() const {
    Json::Value json;
    json["led_counter"] = led_counter.toJson();
    json["countdown_counter"] = countdown_counter.toJson();
    json["urgency_badge"] = urgency_badge.toJson();

    Json::Value text_array(Json::arrayValue);
    for (const auto& text_config : text_overlays) {
        text_array.append(text_config.toJson());
    }
    json["text_overlays"] = text_array;

    json["logo"] = logo.toJson();
    json["enable_overlays"] = enable_overlays;
    json["target_width"] = target_width;
    json["target_height"] = target_height;

    return json;
}

OverlayConfig OverlayConfig::fromJson(const Json::Value& json) {
    OverlayConfig config;

    if (json.isMember("led_counter")) {
        config.led_counter = LEDCounterConfig::fromJson(json["led_counter"]);
    }
    if (json.isMember("countdown_counter")) {
        config.countdown_counter = LEDCounterConfig::fromJson(json["countdown_counter"]);
    }
    if (json.isMember("urgency_badge")) {
        config.urgency_badge = UrgencyBadgeConfig::fromJson(json["urgency_badge"]);
    }

    if (json.isMember("text_overlays") && json["text_overlays"].isArray()) {
        for (const auto& text_json : json["text_overlays"]) {
            config.text_overlays.push_back(TextOverlayConfig::fromJson(text_json));
        }
    }

    if (json.isMember("logo")) {
        config.logo = LogoOverlayConfig::fromJson(json["logo"]);
    }

    config.enable_overlays = json.get("enable_overlays", true).asBool();
    config.target_width = json.get("target_width", 1920).asInt();
    config.target_height = json.get("target_height", 1080).asInt();

    return config;
}

OverlayConfig OverlayConfig::getDefault() {
    OverlayConfig config;
    config.led_counter = LEDCounterConfig::defaultWaitTime();
    config.countdown_counter = LEDCounterConfig::defaultCountdown();
    config.countdown_counter.visible = false; // Hide by default
    config.urgency_badge.visible = true;
    config.logo.visible = true;
    config.logo.logo_path = "static/images/logo.png";
    config.enable_overlays = true;
    config.target_width = 1920;
    config.target_height = 1080;
    return config;
}

OverlayConfig OverlayConfig::getMinimal() {
    OverlayConfig config;
    config.led_counter = LEDCounterConfig::defaultWaitTime();
    config.countdown_counter.visible = false;
    config.urgency_badge.visible = false;
    config.logo.visible = false;
    config.enable_overlays = true;
    config.target_width = 1920;
    config.target_height = 1080;
    return config;
}

OverlayConfig OverlayConfig::getFull() {
    OverlayConfig config = getDefault();

    // Enable countdown for dogs with euthanasia date
    config.countdown_counter.visible = true;

    // Add title
    auto title = TextOverlayConfig::createTitle("Meet Your New Best Friend");
    config.text_overlays.push_back(title);

    // Add CTA
    auto cta = TextOverlayConfig::createCallToAction("Adopt Me Today!");
    cta.start_time = 3.0;  // Show after 3 seconds
    config.text_overlays.push_back(cta);

    return config;
}

OverlayConfig OverlayConfig::forShareCard() {
    OverlayConfig config;
    config.target_width = 1200;
    config.target_height = 630;  // Facebook/Twitter recommended size

    config.led_counter = LEDCounterConfig::defaultWaitTime();
    config.led_counter.font_size = 48;
    config.led_counter.position = Position::TOP_RIGHT;

    config.urgency_badge.visible = true;
    config.urgency_badge.position = Position::TOP_LEFT;
    config.urgency_badge.size = {150, 50};
    config.urgency_badge.font_size = 18;

    config.logo.visible = true;
    config.logo.position = Position::BOTTOM_RIGHT;
    config.logo.size = {80, 80};

    config.countdown_counter.visible = false;

    return config;
}

OverlayConfig OverlayConfig::forStory() {
    OverlayConfig config;
    config.target_width = 1080;
    config.target_height = 1920;  // 9:16 aspect ratio for stories

    config.led_counter = LEDCounterConfig::defaultWaitTime();
    config.led_counter.font_size = 42;
    config.led_counter.position = Position::TOP_CENTER;
    config.led_counter.margin_y = 100;  // Account for status bar

    config.urgency_badge.visible = true;
    config.urgency_badge.position = Position::TOP_LEFT;
    config.urgency_badge.margin_y = 100;

    config.logo.visible = true;
    config.logo.position = Position::BOTTOM_CENTER;
    config.logo.size = {120, 120};
    config.logo.margin_y = 100;  // Account for swipe up area

    config.countdown_counter.visible = false;

    return config;
}

void OverlayConfig::addTextOverlay(const TextOverlayConfig& config) {
    text_overlays.push_back(config);
}

void OverlayConfig::clearTextOverlays() {
    text_overlays.clear();
}

bool OverlayConfig::validate() const {
    // Validate dimensions
    if (target_width <= 0 || target_height <= 0) {
        return false;
    }

    // Validate font sizes
    if (led_counter.font_size <= 0 || led_counter.font_size > 200) {
        return false;
    }

    if (urgency_badge.font_size <= 0 || urgency_badge.font_size > 100) {
        return false;
    }

    // Validate text overlays
    for (const auto& text_config : text_overlays) {
        if (text_config.font_size <= 0 || text_config.font_size > 200) {
            return false;
        }
    }

    return true;
}

void OverlayConfig::scaleToSize(int width, int height) {
    if (target_width == 0 || target_height == 0) return;

    double scale_x = static_cast<double>(width) / target_width;
    double scale_y = static_cast<double>(height) / target_height;
    double scale = std::min(scale_x, scale_y);

    // Scale LED counter
    led_counter.font_size = static_cast<int>(led_counter.font_size * scale);
    led_counter.margin_x = static_cast<int>(led_counter.margin_x * scale);
    led_counter.margin_y = static_cast<int>(led_counter.margin_y * scale);
    led_counter.padding = static_cast<int>(led_counter.padding * scale);

    // Scale countdown counter
    countdown_counter.font_size = static_cast<int>(countdown_counter.font_size * scale);
    countdown_counter.margin_x = static_cast<int>(countdown_counter.margin_x * scale);
    countdown_counter.margin_y = static_cast<int>(countdown_counter.margin_y * scale);
    countdown_counter.padding = static_cast<int>(countdown_counter.padding * scale);

    // Scale urgency badge
    urgency_badge.size.width = static_cast<int>(urgency_badge.size.width * scale);
    urgency_badge.size.height = static_cast<int>(urgency_badge.size.height * scale);
    urgency_badge.font_size = static_cast<int>(urgency_badge.font_size * scale);
    urgency_badge.margin_x = static_cast<int>(urgency_badge.margin_x * scale);
    urgency_badge.margin_y = static_cast<int>(urgency_badge.margin_y * scale);

    // Scale text overlays
    for (auto& text_config : text_overlays) {
        text_config.font_size = static_cast<int>(text_config.font_size * scale);
        text_config.margin_x = static_cast<int>(text_config.margin_x * scale);
        text_config.margin_y = static_cast<int>(text_config.margin_y * scale);
        text_config.background_padding = static_cast<int>(text_config.background_padding * scale);
        if (text_config.max_width > 0) {
            text_config.max_width = static_cast<int>(text_config.max_width * scale);
        }
    }

    // Scale logo
    logo.size.width = static_cast<int>(logo.size.width * scale);
    logo.size.height = static_cast<int>(logo.size.height * scale);
    logo.margin_x = static_cast<int>(logo.margin_x * scale);
    logo.margin_y = static_cast<int>(logo.margin_y * scale);

    // Update target dimensions
    target_width = width;
    target_height = height;
}

// ============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// ============================================================================

std::string positionToString(Position pos) {
    switch (pos) {
        case Position::TOP_LEFT:      return "top_left";
        case Position::TOP_CENTER:    return "top_center";
        case Position::TOP_RIGHT:     return "top_right";
        case Position::CENTER_LEFT:   return "center_left";
        case Position::CENTER:        return "center";
        case Position::CENTER_RIGHT:  return "center_right";
        case Position::BOTTOM_LEFT:   return "bottom_left";
        case Position::BOTTOM_CENTER: return "bottom_center";
        case Position::BOTTOM_RIGHT:  return "bottom_right";
        case Position::CUSTOM:        return "custom";
        default:                      return "top_right";
    }
}

Position stringToPosition(const std::string& str) {
    if (str == "top_left")      return Position::TOP_LEFT;
    if (str == "top_center")    return Position::TOP_CENTER;
    if (str == "top_right")     return Position::TOP_RIGHT;
    if (str == "center_left")   return Position::CENTER_LEFT;
    if (str == "center")        return Position::CENTER;
    if (str == "center_right")  return Position::CENTER_RIGHT;
    if (str == "bottom_left")   return Position::BOTTOM_LEFT;
    if (str == "bottom_center") return Position::BOTTOM_CENTER;
    if (str == "bottom_right")  return Position::BOTTOM_RIGHT;
    if (str == "custom")        return Position::CUSTOM;
    return Position::TOP_RIGHT;
}

std::string textAlignToString(TextAlign align) {
    switch (align) {
        case TextAlign::LEFT:   return "left";
        case TextAlign::CENTER: return "center";
        case TextAlign::RIGHT:  return "right";
        default:                return "center";
    }
}

TextAlign stringToTextAlign(const std::string& str) {
    if (str == "left")   return TextAlign::LEFT;
    if (str == "center") return TextAlign::CENTER;
    if (str == "right")  return TextAlign::RIGHT;
    return TextAlign::CENTER;
}

std::string fontStyleToString(FontStyle style) {
    switch (style) {
        case FontStyle::NORMAL:      return "normal";
        case FontStyle::BOLD:        return "bold";
        case FontStyle::ITALIC:      return "italic";
        case FontStyle::BOLD_ITALIC: return "bold_italic";
        default:                     return "normal";
    }
}

FontStyle stringToFontStyle(const std::string& str) {
    if (str == "normal")      return FontStyle::NORMAL;
    if (str == "bold")        return FontStyle::BOLD;
    if (str == "italic")      return FontStyle::ITALIC;
    if (str == "bold_italic") return FontStyle::BOLD_ITALIC;
    return FontStyle::NORMAL;
}

Coordinates calculatePosition(
    Position pos,
    int element_width,
    int element_height,
    int screen_width,
    int screen_height,
    int margin_x,
    int margin_y
) {
    Coordinates coords;

    switch (pos) {
        case Position::TOP_LEFT:
            coords.x = margin_x;
            coords.y = margin_y;
            break;

        case Position::TOP_CENTER:
            coords.x = (screen_width - element_width) / 2;
            coords.y = margin_y;
            break;

        case Position::TOP_RIGHT:
            coords.x = screen_width - element_width - margin_x;
            coords.y = margin_y;
            break;

        case Position::CENTER_LEFT:
            coords.x = margin_x;
            coords.y = (screen_height - element_height) / 2;
            break;

        case Position::CENTER:
            coords.x = (screen_width - element_width) / 2;
            coords.y = (screen_height - element_height) / 2;
            break;

        case Position::CENTER_RIGHT:
            coords.x = screen_width - element_width - margin_x;
            coords.y = (screen_height - element_height) / 2;
            break;

        case Position::BOTTOM_LEFT:
            coords.x = margin_x;
            coords.y = screen_height - element_height - margin_y;
            break;

        case Position::BOTTOM_CENTER:
            coords.x = (screen_width - element_width) / 2;
            coords.y = screen_height - element_height - margin_y;
            break;

        case Position::BOTTOM_RIGHT:
            coords.x = screen_width - element_width - margin_x;
            coords.y = screen_height - element_height - margin_y;
            break;

        case Position::CUSTOM:
            // Custom position should be set directly
            break;
    }

    return coords;
}

} // namespace wtl::content::media

/**
 * @file VideoTemplate.cc
 * @brief Implementation of video template definitions
 * @see VideoTemplate.h for documentation
 */

#include "content/media/VideoTemplate.h"

// Standard library includes
#include <algorithm>
#include <random>

namespace wtl::content::media {

// ============================================================================
// TEXT PLACEMENT IMPLEMENTATION
// ============================================================================

Json::Value TextPlacement::toJson() const {
    Json::Value json;
    json["placeholder"] = placeholder;
    json["text_config"] = text_config.toJson();
    json["start_time"] = start_time;
    json["end_time"] = end_time;
    json["use_animation"] = use_animation;
    return json;
}

TextPlacement TextPlacement::fromJson(const Json::Value& json) {
    TextPlacement placement;
    placement.placeholder = json.get("placeholder", "").asString();
    if (json.isMember("text_config")) {
        placement.text_config = TextOverlayConfig::fromJson(json["text_config"]);
    }
    placement.start_time = json.get("start_time", 0.0).asDouble();
    placement.end_time = json.get("end_time", 0.0).asDouble();
    placement.use_animation = json.get("use_animation", true).asBool();
    return placement;
}

// ============================================================================
// MOTION CONFIG IMPLEMENTATION
// ============================================================================

Json::Value MotionConfig::toJson() const {
    Json::Value json;
    json["type"] = motionTypeToString(type);
    json["zoom_factor"] = zoom_factor;
    json["pan_percentage"] = pan_percentage;
    json["duration"] = duration;
    json["ease_in_out"] = ease_in_out;
    return json;
}

MotionConfig MotionConfig::fromJson(const Json::Value& json) {
    MotionConfig config;
    config.type = stringToMotionType(json.get("type", "zoom_pan").asString());
    config.zoom_factor = static_cast<float>(json.get("zoom_factor", 1.2).asDouble());
    config.pan_percentage = static_cast<float>(json.get("pan_percentage", 0.15).asDouble());
    config.duration = json.get("duration", 5.0).asDouble();
    config.ease_in_out = json.get("ease_in_out", true).asBool();
    return config;
}

// ============================================================================
// TRANSITION CONFIG IMPLEMENTATION
// ============================================================================

Json::Value TransitionConfig::toJson() const {
    Json::Value json;
    json["style"] = transitionStyleToString(style);
    json["duration"] = duration;
    json["use_random"] = use_random;
    return json;
}

TransitionConfig TransitionConfig::fromJson(const Json::Value& json) {
    TransitionConfig config;
    config.style = stringToTransitionStyle(json.get("style", "crossfade").asString());
    config.duration = json.get("duration", 1.0).asDouble();
    config.use_random = json.get("use_random", false).asBool();
    return config;
}

std::string TransitionConfig::getFFmpegFilter() const {
    TransitionStyle effective_style = style;

    // If random, pick a random transition
    if (use_random || style == TransitionStyle::RANDOM) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dist(0, 13); // Number of transitions - 1
        effective_style = static_cast<TransitionStyle>(dist(gen));
        if (effective_style == TransitionStyle::RANDOM) {
            effective_style = TransitionStyle::CROSSFADE;
        }
    }

    // Map to FFmpeg xfade transition names
    switch (effective_style) {
        case TransitionStyle::NONE:
            return "";
        case TransitionStyle::CROSSFADE:
            return "fade";
        case TransitionStyle::FADE_BLACK:
            return "fadeblack";
        case TransitionStyle::FADE_WHITE:
            return "fadewhite";
        case TransitionStyle::SLIDE_LEFT:
            return "slideleft";
        case TransitionStyle::SLIDE_RIGHT:
            return "slideright";
        case TransitionStyle::SLIDE_UP:
            return "slideup";
        case TransitionStyle::SLIDE_DOWN:
            return "slidedown";
        case TransitionStyle::ZOOM_IN:
            return "zoomin";
        case TransitionStyle::ZOOM_OUT:
            return "fadefast";  // No direct equivalent, use fadefast
        case TransitionStyle::WIPE_LEFT:
            return "wipeleft";
        case TransitionStyle::WIPE_RIGHT:
            return "wiperight";
        case TransitionStyle::CIRCLE_OPEN:
            return "circleopen";
        case TransitionStyle::CIRCLE_CLOSE:
            return "circleclose";
        default:
            return "fade";
    }
}

// ============================================================================
// MUSIC CONFIG IMPLEMENTATION
// ============================================================================

Json::Value MusicConfig::toJson() const {
    Json::Value json;
    json["style"] = musicStyleToString(style);
    json["music_file"] = music_file;
    json["volume"] = volume;
    json["fade_in"] = fade_in;
    json["fade_out"] = fade_out;
    json["fade_duration"] = fade_duration;
    json["loop"] = loop;
    return json;
}

MusicConfig MusicConfig::fromJson(const Json::Value& json) {
    MusicConfig config;
    config.style = stringToMusicStyle(json.get("style", "upbeat").asString());
    config.music_file = json.get("music_file", "").asString();
    config.volume = static_cast<float>(json.get("volume", 0.3).asDouble());
    config.fade_in = json.get("fade_in", true).asBool();
    config.fade_out = json.get("fade_out", true).asBool();
    config.fade_duration = json.get("fade_duration", 2.0).asDouble();
    config.loop = json.get("loop", true).asBool();
    return config;
}

std::string MusicConfig::getMusicFilePath() const {
    if (!music_file.empty()) {
        return music_file;
    }

    // Return stock music path based on style
    switch (style) {
        case MusicStyle::NONE:
            return "";
        case MusicStyle::UPBEAT:
            return "static/audio/music/upbeat.mp3";
        case MusicStyle::EMOTIONAL:
            return "static/audio/music/emotional.mp3";
        case MusicStyle::CALM:
            return "static/audio/music/calm.mp3";
        case MusicStyle::URGENT:
            return "static/audio/music/urgent.mp3";
        case MusicStyle::INSPIRATIONAL:
            return "static/audio/music/inspirational.mp3";
        case MusicStyle::PLAYFUL:
            return "static/audio/music/playful.mp3";
        case MusicStyle::CUSTOM:
            return music_file;
        default:
            return "static/audio/music/upbeat.mp3";
    }
}

// ============================================================================
// OUTPUT CONFIG IMPLEMENTATION
// ============================================================================

Json::Value OutputConfig::toJson() const {
    Json::Value json;
    json["width"] = width;
    json["height"] = height;
    json["aspect_ratio"] = aspectRatioToString(aspect_ratio);
    json["fps"] = fps;
    json["bitrate_kbps"] = bitrate_kbps;
    json["codec"] = codec;
    json["preset"] = preset;
    json["format"] = format;
    json["crf"] = crf;
    json["audio_codec"] = audio_codec;
    json["audio_bitrate_kbps"] = audio_bitrate_kbps;
    return json;
}

OutputConfig OutputConfig::fromJson(const Json::Value& json) {
    OutputConfig config;
    config.width = json.get("width", 1920).asInt();
    config.height = json.get("height", 1080).asInt();
    config.aspect_ratio = stringToAspectRatio(json.get("aspect_ratio", "16:9").asString());
    config.fps = json.get("fps", 30).asInt();
    config.bitrate_kbps = json.get("bitrate_kbps", 8000).asInt();
    config.codec = json.get("codec", "libx264").asString();
    config.preset = json.get("preset", "medium").asString();
    config.format = json.get("format", "mp4").asString();
    config.crf = json.get("crf", 23).asInt();
    config.audio_codec = json.get("audio_codec", "aac").asString();
    config.audio_bitrate_kbps = json.get("audio_bitrate_kbps", 192).asInt();
    return config;
}

Dimensions OutputConfig::getDimensionsForAspectRatio(AspectRatio ratio, int max_dimension) {
    switch (ratio) {
        case AspectRatio::LANDSCAPE_16_9:
            return Dimensions{max_dimension, max_dimension * 9 / 16};
        case AspectRatio::LANDSCAPE_4_3:
            return Dimensions{max_dimension, max_dimension * 3 / 4};
        case AspectRatio::PORTRAIT_9_16:
            return Dimensions{max_dimension * 9 / 16, max_dimension};
        case AspectRatio::PORTRAIT_4_5:
            return Dimensions{max_dimension * 4 / 5, max_dimension};
        case AspectRatio::SQUARE_1_1:
            return Dimensions{max_dimension, max_dimension};
        default:
            return Dimensions{max_dimension, max_dimension * 9 / 16};
    }
}

OutputConfig OutputConfig::forPlatform(const std::string& platform) {
    OutputConfig config;

    if (platform == "youtube") {
        config.width = 1920;
        config.height = 1080;
        config.aspect_ratio = AspectRatio::LANDSCAPE_16_9;
        config.fps = 30;
        config.bitrate_kbps = 8000;
        config.crf = 21;
    } else if (platform == "instagram_feed") {
        config.width = 1080;
        config.height = 1350;
        config.aspect_ratio = AspectRatio::PORTRAIT_4_5;
        config.fps = 30;
        config.bitrate_kbps = 5000;
        config.crf = 23;
    } else if (platform == "instagram_stories" || platform == "tiktok" || platform == "reels") {
        config.width = 1080;
        config.height = 1920;
        config.aspect_ratio = AspectRatio::PORTRAIT_9_16;
        config.fps = 30;
        config.bitrate_kbps = 5000;
        config.crf = 23;
    } else if (platform == "facebook") {
        config.width = 1280;
        config.height = 720;
        config.aspect_ratio = AspectRatio::LANDSCAPE_16_9;
        config.fps = 30;
        config.bitrate_kbps = 6000;
        config.crf = 22;
    } else if (platform == "twitter") {
        config.width = 1280;
        config.height = 720;
        config.aspect_ratio = AspectRatio::LANDSCAPE_16_9;
        config.fps = 30;
        config.bitrate_kbps = 5000;
        config.crf = 23;
    } else {
        // Default to YouTube settings
        config.width = 1920;
        config.height = 1080;
        config.aspect_ratio = AspectRatio::LANDSCAPE_16_9;
        config.fps = 30;
        config.bitrate_kbps = 8000;
        config.crf = 23;
    }

    return config;
}

// ============================================================================
// VIDEO TEMPLATE IMPLEMENTATION
// ============================================================================

Json::Value VideoTemplate::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["description"] = description;
    json["type"] = templateTypeToString(type);
    json["total_duration"] = total_duration;
    json["photo_duration"] = photo_duration;
    json["min_photos"] = min_photos;
    json["max_photos"] = max_photos;
    json["transition"] = transition.toJson();
    json["motion"] = motion.toJson();
    json["music"] = music.toJson();
    json["overlay_config"] = overlay_config.toJson();

    Json::Value text_array(Json::arrayValue);
    for (const auto& placement : text_placements) {
        text_array.append(placement.toJson());
    }
    json["text_placements"] = text_array;

    json["output"] = output.toJson();
    json["include_intro"] = include_intro;
    json["intro_duration"] = intro_duration;
    json["intro_video_path"] = intro_video_path;
    json["include_outro"] = include_outro;
    json["outro_duration"] = outro_duration;
    json["outro_video_path"] = outro_video_path;
    json["include_watermark"] = include_watermark;
    json["watermark_path"] = watermark_path;

    return json;
}

VideoTemplate VideoTemplate::fromJson(const Json::Value& json) {
    VideoTemplate template_obj;

    template_obj.id = json.get("id", "").asString();
    template_obj.name = json.get("name", "").asString();
    template_obj.description = json.get("description", "").asString();
    template_obj.type = stringToTemplateType(json.get("type", "slideshow").asString());
    template_obj.total_duration = json.get("total_duration", 30.0).asDouble();
    template_obj.photo_duration = json.get("photo_duration", 5.0).asDouble();
    template_obj.min_photos = json.get("min_photos", 3).asInt();
    template_obj.max_photos = json.get("max_photos", 10).asInt();

    if (json.isMember("transition")) {
        template_obj.transition = TransitionConfig::fromJson(json["transition"]);
    }
    if (json.isMember("motion")) {
        template_obj.motion = MotionConfig::fromJson(json["motion"]);
    }
    if (json.isMember("music")) {
        template_obj.music = MusicConfig::fromJson(json["music"]);
    }
    if (json.isMember("overlay_config")) {
        template_obj.overlay_config = OverlayConfig::fromJson(json["overlay_config"]);
    }

    if (json.isMember("text_placements") && json["text_placements"].isArray()) {
        for (const auto& placement_json : json["text_placements"]) {
            template_obj.text_placements.push_back(TextPlacement::fromJson(placement_json));
        }
    }

    if (json.isMember("output")) {
        template_obj.output = OutputConfig::fromJson(json["output"]);
    }

    template_obj.include_intro = json.get("include_intro", false).asBool();
    template_obj.intro_duration = json.get("intro_duration", 3.0).asDouble();
    template_obj.intro_video_path = json.get("intro_video_path", "").asString();
    template_obj.include_outro = json.get("include_outro", false).asBool();
    template_obj.outro_duration = json.get("outro_duration", 3.0).asDouble();
    template_obj.outro_video_path = json.get("outro_video_path", "").asString();
    template_obj.include_watermark = json.get("include_watermark", true).asBool();
    template_obj.watermark_path = json.get("watermark_path", "").asString();

    return template_obj;
}

VideoTemplate VideoTemplate::get(TemplateType type) {
    switch (type) {
        case TemplateType::SLIDESHOW:
            return createSlideshow();
        case TemplateType::KEN_BURNS:
            return createKenBurns();
        case TemplateType::COUNTDOWN:
            return createCountdown();
        case TemplateType::STORY:
            return createStory();
        case TemplateType::PROMOTIONAL:
            return createPromotional();
        case TemplateType::QUICK_SHARE:
            return createQuickShare();
        case TemplateType::ADOPTION_APPEAL:
            return createAdoptionAppeal();
        default:
            return createSlideshow();
    }
}

VideoTemplate VideoTemplate::getByName(const std::string& name) {
    if (name == "slideshow") return createSlideshow();
    if (name == "ken_burns") return createKenBurns();
    if (name == "countdown") return createCountdown();
    if (name == "story") return createStory();
    if (name == "promotional") return createPromotional();
    if (name == "quick_share") return createQuickShare();
    if (name == "adoption_appeal") return createAdoptionAppeal();
    return createSlideshow();
}

std::vector<std::pair<TemplateType, std::string>> VideoTemplate::getAvailableTemplates() {
    return {
        {TemplateType::SLIDESHOW, "Slideshow"},
        {TemplateType::KEN_BURNS, "Ken Burns"},
        {TemplateType::COUNTDOWN, "Countdown"},
        {TemplateType::STORY, "Story (9:16)"},
        {TemplateType::PROMOTIONAL, "Promotional"},
        {TemplateType::QUICK_SHARE, "Quick Share"},
        {TemplateType::ADOPTION_APPEAL, "Adoption Appeal"}
    };
}

double VideoTemplate::calculateDuration(int photo_count) const {
    double content_duration = photo_count * photo_duration;
    double transition_duration = (photo_count - 1) * transition.duration;

    double total = content_duration - transition_duration; // Transitions overlap

    if (include_intro) total += intro_duration;
    if (include_outro) total += outro_duration;

    return std::max(total, 5.0); // Minimum 5 seconds
}

bool VideoTemplate::validate() const {
    if (total_duration <= 0) return false;
    if (photo_duration <= 0) return false;
    if (min_photos <= 0 || max_photos < min_photos) return false;
    if (output.width <= 0 || output.height <= 0) return false;
    if (output.fps <= 0 || output.fps > 120) return false;

    return overlay_config.validate();
}

void VideoTemplate::addTextPlacement(const TextPlacement& placement) {
    text_placements.push_back(placement);
}

void VideoTemplate::clearTextPlacements() {
    text_placements.clear();
}

void VideoTemplate::applyPlatformSettings(const std::string& platform) {
    output = OutputConfig::forPlatform(platform);

    // Adjust overlay config for new dimensions
    overlay_config.scaleToSize(output.width, output.height);

    // Platform-specific adjustments
    if (platform == "instagram_stories" || platform == "tiktok" || platform == "reels") {
        // Use story overlay config
        overlay_config = OverlayConfig::forStory();
    } else if (platform == "twitter") {
        // Twitter videos should be shorter
        total_duration = std::min(total_duration, 140.0);
    }
}

VideoTemplate VideoTemplate::clone() const {
    return fromJson(toJson());
}

VideoTemplate VideoTemplate::createSlideshow() {
    VideoTemplate t;
    t.id = "slideshow";
    t.name = "Slideshow";
    t.description = "Simple slideshow with crossfade transitions";
    t.type = TemplateType::SLIDESHOW;
    t.total_duration = 30.0;
    t.photo_duration = 5.0;
    t.min_photos = 3;
    t.max_photos = 10;

    t.transition.style = TransitionStyle::CROSSFADE;
    t.transition.duration = 1.0;

    t.motion.type = MotionType::NONE;

    t.music.style = MusicStyle::UPBEAT;
    t.music.volume = 0.3f;

    t.overlay_config = OverlayConfig::getDefault();

    // Add dog name title
    TextPlacement name_text;
    name_text.placeholder = "{{dog_name}}";
    name_text.text_config = TextOverlayConfig::createTitle("");
    name_text.start_time = 0.0;
    name_text.end_time = 5.0;
    t.text_placements.push_back(name_text);

    t.include_watermark = true;
    t.watermark_path = "static/images/logo.png";

    return t;
}

VideoTemplate VideoTemplate::createKenBurns() {
    VideoTemplate t;
    t.id = "ken_burns";
    t.name = "Ken Burns";
    t.description = "Elegant pan and zoom effect on each photo";
    t.type = TemplateType::KEN_BURNS;
    t.total_duration = 45.0;
    t.photo_duration = 6.0;
    t.min_photos = 4;
    t.max_photos = 12;

    t.transition.style = TransitionStyle::CROSSFADE;
    t.transition.duration = 1.5;

    t.motion.type = MotionType::ZOOM_PAN;
    t.motion.zoom_factor = 1.3f;
    t.motion.pan_percentage = 0.2f;
    t.motion.ease_in_out = true;

    t.music.style = MusicStyle::EMOTIONAL;
    t.music.volume = 0.35f;

    t.overlay_config = OverlayConfig::getDefault();
    t.overlay_config.led_counter.font_size = 42;

    // Add intro text
    TextPlacement intro_text;
    intro_text.placeholder = "{{intro}}";
    intro_text.text_config = TextOverlayConfig::createTitle("Meet {{dog_name}}");
    intro_text.start_time = 0.0;
    intro_text.end_time = 4.0;
    t.text_placements.push_back(intro_text);

    // Add wait time text
    TextPlacement wait_text;
    wait_text.placeholder = "{{wait_message}}";
    wait_text.text_config = TextOverlayConfig::createCaption("Waiting for a home");
    wait_text.text_config.position = Position::BOTTOM_CENTER;
    wait_text.start_time = 4.0;
    t.text_placements.push_back(wait_text);

    t.include_watermark = true;

    return t;
}

VideoTemplate VideoTemplate::createCountdown() {
    VideoTemplate t;
    t.id = "countdown";
    t.name = "Countdown";
    t.description = "Urgency-focused with prominent countdown timer";
    t.type = TemplateType::COUNTDOWN;
    t.total_duration = 30.0;
    t.photo_duration = 4.0;
    t.min_photos = 3;
    t.max_photos = 8;

    t.transition.style = TransitionStyle::FADE_BLACK;
    t.transition.duration = 0.5;

    t.motion.type = MotionType::ZOOM_IN;
    t.motion.zoom_factor = 1.15f;

    t.music.style = MusicStyle::URGENT;
    t.music.volume = 0.4f;

    t.overlay_config = OverlayConfig::getDefault();

    // Enable countdown counter prominently
    t.overlay_config.countdown_counter = LEDCounterConfig::defaultCountdown();
    t.overlay_config.countdown_counter.visible = true;
    t.overlay_config.countdown_counter.font_size = 56;
    t.overlay_config.countdown_counter.position = Position::TOP_CENTER;

    // LED wait time counter smaller
    t.overlay_config.led_counter.font_size = 28;
    t.overlay_config.led_counter.position = Position::TOP_RIGHT;

    // Urgency badge prominent
    t.overlay_config.urgency_badge.visible = true;
    t.overlay_config.urgency_badge.pulse_animation = true;
    t.overlay_config.urgency_badge.size = {160, 50};

    // Urgent CTA
    TextPlacement cta;
    cta.placeholder = "{{cta}}";
    cta.text_config = TextOverlayConfig::createCallToAction("Save Me Now!");
    cta.text_config.background_color = Color::red();
    cta.start_time = 5.0;
    t.text_placements.push_back(cta);

    return t;
}

VideoTemplate VideoTemplate::createStory() {
    VideoTemplate t;
    t.id = "story";
    t.name = "Story (9:16)";
    t.description = "Vertical format for Instagram/TikTok stories";
    t.type = TemplateType::STORY;
    t.total_duration = 15.0;
    t.photo_duration = 3.0;
    t.min_photos = 3;
    t.max_photos = 5;

    t.output = OutputConfig::forPlatform("instagram_stories");

    t.transition.style = TransitionStyle::SLIDE_UP;
    t.transition.duration = 0.5;

    t.motion.type = MotionType::ZOOM_IN;
    t.motion.zoom_factor = 1.1f;

    t.music.style = MusicStyle::UPBEAT;
    t.music.volume = 0.35f;

    t.overlay_config = OverlayConfig::forStory();

    // Story-optimized text placement
    TextPlacement name_text;
    name_text.placeholder = "{{dog_name}}";
    name_text.text_config = TextOverlayConfig::createTitle("");
    name_text.text_config.font_size = 56;
    name_text.text_config.position = Position::CENTER;
    name_text.text_config.margin_y = 200;
    name_text.start_time = 0.0;
    name_text.end_time = 3.0;
    t.text_placements.push_back(name_text);

    // Swipe up CTA
    TextPlacement swipe_cta;
    swipe_cta.placeholder = "{{swipe_cta}}";
    swipe_cta.text_config.text = "Swipe up to adopt";
    swipe_cta.text_config.position = Position::BOTTOM_CENTER;
    swipe_cta.text_config.margin_y = 120;
    swipe_cta.text_config.font_size = 24;
    swipe_cta.start_time = 3.0;
    t.text_placements.push_back(swipe_cta);

    return t;
}

VideoTemplate VideoTemplate::createPromotional() {
    VideoTemplate t;
    t.id = "promotional";
    t.name = "Promotional";
    t.description = "Full promotional video with intro and outro";
    t.type = TemplateType::PROMOTIONAL;
    t.total_duration = 60.0;
    t.photo_duration = 5.0;
    t.min_photos = 5;
    t.max_photos = 15;

    t.transition.style = TransitionStyle::CROSSFADE;
    t.transition.duration = 1.0;
    t.transition.use_random = true;

    t.motion.type = MotionType::ZOOM_PAN;
    t.motion.zoom_factor = 1.2f;

    t.music.style = MusicStyle::INSPIRATIONAL;
    t.music.volume = 0.35f;
    t.music.fade_in = true;
    t.music.fade_out = true;

    t.overlay_config = OverlayConfig::getFull();

    t.include_intro = true;
    t.intro_duration = 3.0;
    t.intro_video_path = "static/video/intro.mp4";

    t.include_outro = true;
    t.outro_duration = 5.0;
    t.outro_video_path = "static/video/outro.mp4";

    // Multiple text placements
    TextPlacement intro_text;
    intro_text.placeholder = "{{intro}}";
    intro_text.text_config = TextOverlayConfig::createTitle("Meet Your New Best Friend");
    intro_text.start_time = 3.0;
    intro_text.end_time = 7.0;
    t.text_placements.push_back(intro_text);

    TextPlacement name_text;
    name_text.placeholder = "{{dog_name}}";
    name_text.text_config = TextOverlayConfig::createTitle("");
    name_text.text_config.font_size = 64;
    name_text.start_time = 7.0;
    name_text.end_time = 12.0;
    t.text_placements.push_back(name_text);

    TextPlacement story_text;
    story_text.placeholder = "{{story}}";
    story_text.text_config = TextOverlayConfig::createCaption("");
    story_text.start_time = 15.0;
    story_text.end_time = 25.0;
    t.text_placements.push_back(story_text);

    TextPlacement cta;
    cta.placeholder = "{{cta}}";
    cta.text_config = TextOverlayConfig::createCallToAction("Adopt Today at WaitingTheLongest.com");
    cta.start_time = 50.0;
    t.text_placements.push_back(cta);

    return t;
}

VideoTemplate VideoTemplate::createQuickShare() {
    VideoTemplate t;
    t.id = "quick_share";
    t.name = "Quick Share";
    t.description = "Fast 15-second video for quick social sharing";
    t.type = TemplateType::QUICK_SHARE;
    t.total_duration = 15.0;
    t.photo_duration = 3.0;
    t.min_photos = 3;
    t.max_photos = 5;

    t.transition.style = TransitionStyle::CROSSFADE;
    t.transition.duration = 0.5;

    t.motion.type = MotionType::ZOOM_IN;
    t.motion.zoom_factor = 1.1f;

    t.music.style = MusicStyle::UPBEAT;
    t.music.volume = 0.3f;

    t.overlay_config = OverlayConfig::getMinimal();

    // Simple name and CTA
    TextPlacement name_text;
    name_text.placeholder = "{{dog_name}}";
    name_text.text_config = TextOverlayConfig::createTitle("");
    name_text.text_config.font_size = 48;
    name_text.start_time = 0.0;
    name_text.end_time = 5.0;
    t.text_placements.push_back(name_text);

    TextPlacement cta;
    cta.placeholder = "{{cta}}";
    cta.text_config = TextOverlayConfig::createCallToAction("Adopt Me!");
    cta.start_time = 10.0;
    t.text_placements.push_back(cta);

    return t;
}

VideoTemplate VideoTemplate::createAdoptionAppeal() {
    VideoTemplate t;
    t.id = "adoption_appeal";
    t.name = "Adoption Appeal";
    t.description = "Emotional appeal video to encourage adoption";
    t.type = TemplateType::ADOPTION_APPEAL;
    t.total_duration = 45.0;
    t.photo_duration = 5.0;
    t.min_photos = 5;
    t.max_photos = 12;

    t.transition.style = TransitionStyle::FADE_BLACK;
    t.transition.duration = 1.5;

    t.motion.type = MotionType::ZOOM_PAN;
    t.motion.zoom_factor = 1.25f;
    t.motion.ease_in_out = true;

    t.music.style = MusicStyle::EMOTIONAL;
    t.music.volume = 0.4f;
    t.music.fade_in = true;
    t.music.fade_out = true;
    t.music.fade_duration = 3.0;

    t.overlay_config = OverlayConfig::getDefault();
    t.overlay_config.led_counter.font_size = 48;
    t.overlay_config.urgency_badge.visible = true;

    // Emotional text placements
    TextPlacement waiting_text;
    waiting_text.placeholder = "{{waiting}}";
    waiting_text.text_config = TextOverlayConfig::createCaption("Still waiting...");
    waiting_text.text_config.font_size = 36;
    waiting_text.start_time = 0.0;
    waiting_text.end_time = 5.0;
    t.text_placements.push_back(waiting_text);

    TextPlacement name_text;
    name_text.placeholder = "{{dog_name}}";
    name_text.text_config = TextOverlayConfig::createTitle("");
    name_text.text_config.font_size = 56;
    name_text.start_time = 5.0;
    name_text.end_time = 12.0;
    t.text_placements.push_back(name_text);

    TextPlacement story_text;
    story_text.placeholder = "{{story}}";
    story_text.text_config = TextOverlayConfig::createCaption("");
    story_text.start_time = 15.0;
    story_text.end_time = 30.0;
    t.text_placements.push_back(story_text);

    TextPlacement appeal_text;
    appeal_text.placeholder = "{{appeal}}";
    appeal_text.text_config.text = "Will you be the one?";
    appeal_text.text_config = TextOverlayConfig::createTitle("");
    appeal_text.text_config.font_size = 48;
    appeal_text.start_time = 32.0;
    appeal_text.end_time = 40.0;
    t.text_placements.push_back(appeal_text);

    TextPlacement cta;
    cta.placeholder = "{{cta}}";
    cta.text_config = TextOverlayConfig::createCallToAction("Give {{dog_name}} a Forever Home");
    cta.start_time = 38.0;
    t.text_placements.push_back(cta);

    return t;
}

// ============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// ============================================================================

std::string templateTypeToString(TemplateType type) {
    switch (type) {
        case TemplateType::SLIDESHOW:       return "slideshow";
        case TemplateType::KEN_BURNS:       return "ken_burns";
        case TemplateType::COUNTDOWN:       return "countdown";
        case TemplateType::STORY:           return "story";
        case TemplateType::PROMOTIONAL:     return "promotional";
        case TemplateType::QUICK_SHARE:     return "quick_share";
        case TemplateType::ADOPTION_APPEAL: return "adoption_appeal";
        case TemplateType::BEFORE_AFTER:    return "before_after";
        case TemplateType::CUSTOM:          return "custom";
        default:                            return "slideshow";
    }
}

TemplateType stringToTemplateType(const std::string& str) {
    if (str == "slideshow")       return TemplateType::SLIDESHOW;
    if (str == "ken_burns")       return TemplateType::KEN_BURNS;
    if (str == "countdown")       return TemplateType::COUNTDOWN;
    if (str == "story")           return TemplateType::STORY;
    if (str == "promotional")     return TemplateType::PROMOTIONAL;
    if (str == "quick_share")     return TemplateType::QUICK_SHARE;
    if (str == "adoption_appeal") return TemplateType::ADOPTION_APPEAL;
    if (str == "before_after")    return TemplateType::BEFORE_AFTER;
    if (str == "custom")          return TemplateType::CUSTOM;
    return TemplateType::SLIDESHOW;
}

std::string transitionStyleToString(TransitionStyle style) {
    switch (style) {
        case TransitionStyle::NONE:         return "none";
        case TransitionStyle::CROSSFADE:    return "crossfade";
        case TransitionStyle::FADE_BLACK:   return "fade_black";
        case TransitionStyle::FADE_WHITE:   return "fade_white";
        case TransitionStyle::SLIDE_LEFT:   return "slide_left";
        case TransitionStyle::SLIDE_RIGHT:  return "slide_right";
        case TransitionStyle::SLIDE_UP:     return "slide_up";
        case TransitionStyle::SLIDE_DOWN:   return "slide_down";
        case TransitionStyle::ZOOM_IN:      return "zoom_in";
        case TransitionStyle::ZOOM_OUT:     return "zoom_out";
        case TransitionStyle::WIPE_LEFT:    return "wipe_left";
        case TransitionStyle::WIPE_RIGHT:   return "wipe_right";
        case TransitionStyle::CIRCLE_OPEN:  return "circle_open";
        case TransitionStyle::CIRCLE_CLOSE: return "circle_close";
        case TransitionStyle::RANDOM:       return "random";
        default:                            return "crossfade";
    }
}

TransitionStyle stringToTransitionStyle(const std::string& str) {
    if (str == "none")         return TransitionStyle::NONE;
    if (str == "crossfade")    return TransitionStyle::CROSSFADE;
    if (str == "fade_black")   return TransitionStyle::FADE_BLACK;
    if (str == "fade_white")   return TransitionStyle::FADE_WHITE;
    if (str == "slide_left")   return TransitionStyle::SLIDE_LEFT;
    if (str == "slide_right")  return TransitionStyle::SLIDE_RIGHT;
    if (str == "slide_up")     return TransitionStyle::SLIDE_UP;
    if (str == "slide_down")   return TransitionStyle::SLIDE_DOWN;
    if (str == "zoom_in")      return TransitionStyle::ZOOM_IN;
    if (str == "zoom_out")     return TransitionStyle::ZOOM_OUT;
    if (str == "wipe_left")    return TransitionStyle::WIPE_LEFT;
    if (str == "wipe_right")   return TransitionStyle::WIPE_RIGHT;
    if (str == "circle_open")  return TransitionStyle::CIRCLE_OPEN;
    if (str == "circle_close") return TransitionStyle::CIRCLE_CLOSE;
    if (str == "random")       return TransitionStyle::RANDOM;
    return TransitionStyle::CROSSFADE;
}

std::string musicStyleToString(MusicStyle style) {
    switch (style) {
        case MusicStyle::NONE:          return "none";
        case MusicStyle::UPBEAT:        return "upbeat";
        case MusicStyle::EMOTIONAL:     return "emotional";
        case MusicStyle::CALM:          return "calm";
        case MusicStyle::URGENT:        return "urgent";
        case MusicStyle::INSPIRATIONAL: return "inspirational";
        case MusicStyle::PLAYFUL:       return "playful";
        case MusicStyle::CUSTOM:        return "custom";
        default:                        return "upbeat";
    }
}

MusicStyle stringToMusicStyle(const std::string& str) {
    if (str == "none")          return MusicStyle::NONE;
    if (str == "upbeat")        return MusicStyle::UPBEAT;
    if (str == "emotional")     return MusicStyle::EMOTIONAL;
    if (str == "calm")          return MusicStyle::CALM;
    if (str == "urgent")        return MusicStyle::URGENT;
    if (str == "inspirational") return MusicStyle::INSPIRATIONAL;
    if (str == "playful")       return MusicStyle::PLAYFUL;
    if (str == "custom")        return MusicStyle::CUSTOM;
    return MusicStyle::UPBEAT;
}

std::string aspectRatioToString(AspectRatio ratio) {
    switch (ratio) {
        case AspectRatio::LANDSCAPE_16_9: return "16:9";
        case AspectRatio::LANDSCAPE_4_3:  return "4:3";
        case AspectRatio::PORTRAIT_9_16:  return "9:16";
        case AspectRatio::PORTRAIT_4_5:   return "4:5";
        case AspectRatio::SQUARE_1_1:     return "1:1";
        case AspectRatio::CUSTOM:         return "custom";
        default:                          return "16:9";
    }
}

AspectRatio stringToAspectRatio(const std::string& str) {
    if (str == "16:9")   return AspectRatio::LANDSCAPE_16_9;
    if (str == "4:3")    return AspectRatio::LANDSCAPE_4_3;
    if (str == "9:16")   return AspectRatio::PORTRAIT_9_16;
    if (str == "4:5")    return AspectRatio::PORTRAIT_4_5;
    if (str == "1:1")    return AspectRatio::SQUARE_1_1;
    if (str == "custom") return AspectRatio::CUSTOM;
    return AspectRatio::LANDSCAPE_16_9;
}

std::string motionTypeToString(MotionType type) {
    switch (type) {
        case MotionType::NONE:      return "none";
        case MotionType::PAN_LEFT:  return "pan_left";
        case MotionType::PAN_RIGHT: return "pan_right";
        case MotionType::PAN_UP:    return "pan_up";
        case MotionType::PAN_DOWN:  return "pan_down";
        case MotionType::ZOOM_IN:   return "zoom_in";
        case MotionType::ZOOM_OUT:  return "zoom_out";
        case MotionType::ZOOM_PAN:  return "zoom_pan";
        case MotionType::RANDOM:    return "random";
        default:                    return "none";
    }
}

MotionType stringToMotionType(const std::string& str) {
    if (str == "none")      return MotionType::NONE;
    if (str == "pan_left")  return MotionType::PAN_LEFT;
    if (str == "pan_right") return MotionType::PAN_RIGHT;
    if (str == "pan_up")    return MotionType::PAN_UP;
    if (str == "pan_down")  return MotionType::PAN_DOWN;
    if (str == "zoom_in")   return MotionType::ZOOM_IN;
    if (str == "zoom_out")  return MotionType::ZOOM_OUT;
    if (str == "zoom_pan")  return MotionType::ZOOM_PAN;
    if (str == "random")    return MotionType::RANDOM;
    return MotionType::NONE;
}

} // namespace wtl::content::media

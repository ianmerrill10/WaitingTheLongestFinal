/**
 * @file ShareCardGenerator.cc
 * @brief Implementation of share card generator for social media
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "content/social/ShareCardGenerator.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"

#include <pqxx/pqxx>
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <future>
#include <sstream>
#include <iomanip>
#include <cmath>

// For image processing (using stb_image for portability)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

namespace wtl::content::social {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string cardStyleToString(CardStyle style) {
    switch (style) {
        case CardStyle::STANDARD: return "standard";
        case CardStyle::URGENT: return "urgent";
        case CardStyle::MILESTONE: return "milestone";
        case CardStyle::SUCCESS_STORY: return "success_story";
        case CardStyle::EVENT: return "event";
        case CardStyle::COMPARISON: return "comparison";
        case CardStyle::CAROUSEL_ITEM: return "carousel_item";
        case CardStyle::STORY: return "story";
        default: return "unknown";
    }
}

std::string urgencyBadgeToString(UrgencyBadge badge) {
    switch (badge) {
        case UrgencyBadge::NONE: return "none";
        case UrgencyBadge::NEW_ARRIVAL: return "new_arrival";
        case UrgencyBadge::LONG_WAIT: return "long_wait";
        case UrgencyBadge::URGENT: return "urgent";
        case UrgencyBadge::CRITICAL: return "critical";
        case UrgencyBadge::SENIOR: return "senior";
        case UrgencyBadge::SPECIAL_NEEDS: return "special_needs";
        case UrgencyBadge::BONDED_PAIR: return "bonded_pair";
        default: return "unknown";
    }
}

// ============================================================================
// CARD DIMENSIONS IMPLEMENTATION
// ============================================================================

CardDimensions CardDimensions::forPlatform(Platform platform, CardStyle style) {
    CardDimensions dims;
    dims.format = "png";
    dims.quality = 95;

    // Story format is always vertical
    if (style == CardStyle::STORY) {
        dims.width = 1080;
        dims.height = 1920;
        dims.aspect_ratio = 9.0f / 16.0f;
        return dims;
    }

    switch (platform) {
        case Platform::INSTAGRAM:
            // Instagram square or 4:5 portrait
            dims.width = 1080;
            dims.height = 1350;  // 4:5 portrait
            dims.aspect_ratio = 4.0f / 5.0f;
            break;

        case Platform::FACEBOOK:
            // Facebook feed optimal
            dims.width = 1200;
            dims.height = 630;
            dims.aspect_ratio = 1.91f;
            break;

        case Platform::TWITTER:
            // Twitter in-stream photo
            dims.width = 1200;
            dims.height = 675;
            dims.aspect_ratio = 16.0f / 9.0f;
            break;

        case Platform::PINTEREST:
            // Pinterest optimal vertical
            dims.width = 1000;
            dims.height = 1500;
            dims.aspect_ratio = 2.0f / 3.0f;
            break;

        case Platform::LINKEDIN:
            // LinkedIn shared image
            dims.width = 1200;
            dims.height = 627;
            dims.aspect_ratio = 1.91f;
            break;

        case Platform::TIKTOK:
            // TikTok cover image
            dims.width = 1080;
            dims.height = 1920;
            dims.aspect_ratio = 9.0f / 16.0f;
            break;

        case Platform::THREADS:
            // Same as Instagram
            dims.width = 1080;
            dims.height = 1350;
            dims.aspect_ratio = 4.0f / 5.0f;
            break;

        case Platform::YOUTUBE:
            // YouTube thumbnail
            dims.width = 1280;
            dims.height = 720;
            dims.aspect_ratio = 16.0f / 9.0f;
            break;

        default:
            // Default square
            dims.width = 1080;
            dims.height = 1080;
            dims.aspect_ratio = 1.0f;
            break;
    }

    return dims;
}

// ============================================================================
// CARD COLORS IMPLEMENTATION
// ============================================================================

CardColors CardColors::defaultScheme() {
    CardColors colors;
    colors.primary = "#4A90D9";       // Blue
    colors.secondary = "#2C3E50";     // Dark blue-gray
    colors.accent = "#E74C3C";        // Red for urgency
    colors.text_light = "#FFFFFF";    // White
    colors.text_dark = "#2C3E50";     // Dark
    colors.overlay = "#000000AA";     // Semi-transparent black
    colors.urgent_badge = "#E74C3C";  // Red
    colors.success_badge = "#27AE60"; // Green
    return colors;
}

CardColors CardColors::fromJson(const Json::Value& json) {
    CardColors colors = defaultScheme();

    if (json.isMember("primary")) colors.primary = json["primary"].asString();
    if (json.isMember("secondary")) colors.secondary = json["secondary"].asString();
    if (json.isMember("accent")) colors.accent = json["accent"].asString();
    if (json.isMember("text_light")) colors.text_light = json["text_light"].asString();
    if (json.isMember("text_dark")) colors.text_dark = json["text_dark"].asString();
    if (json.isMember("overlay")) colors.overlay = json["overlay"].asString();
    if (json.isMember("urgent_badge")) colors.urgent_badge = json["urgent_badge"].asString();
    if (json.isMember("success_badge")) colors.success_badge = json["success_badge"].asString();

    return colors;
}

// ============================================================================
// CARD FONTS IMPLEMENTATION
// ============================================================================

CardFonts CardFonts::defaultFonts() {
    CardFonts fonts;
    fonts.heading_family = "Montserrat-Bold";
    fonts.body_family = "OpenSans-Regular";
    fonts.heading_size = 72;
    fonts.subheading_size = 48;
    fonts.body_size = 36;
    fonts.badge_size = 28;
    fonts.cta_size = 32;
    return fonts;
}

// ============================================================================
// CARD CONFIG IMPLEMENTATION
// ============================================================================

CardConfig CardConfig::defaultForPlatform(Platform platform) {
    CardConfig config;
    config.dimensions = CardDimensions::forPlatform(platform);
    config.colors = CardColors::defaultScheme();
    config.fonts = CardFonts::defaultFonts();
    config.show_wait_time = true;
    config.show_urgency_badge = true;
    config.show_qr_code = (platform == Platform::PINTEREST);  // QR good for Pinterest
    config.show_shelter_logo = true;
    config.show_cta = true;
    config.show_website = true;
    config.cta_text = "Adopt Me Today!";
    config.website_url = "waitingthelongest.com";
    return config;
}

CardConfig CardConfig::fromJson(const Json::Value& json) {
    CardConfig config;

    if (json.isMember("dimensions")) {
        const auto& dims = json["dimensions"];
        config.dimensions.width = dims.get("width", 1080).asInt();
        config.dimensions.height = dims.get("height", 1080).asInt();
        config.dimensions.format = dims.get("format", "png").asString();
        config.dimensions.quality = dims.get("quality", 95).asInt();
    }

    if (json.isMember("colors")) {
        config.colors = CardColors::fromJson(json["colors"]);
    }

    config.show_wait_time = json.get("show_wait_time", true).asBool();
    config.show_urgency_badge = json.get("show_urgency_badge", true).asBool();
    config.show_qr_code = json.get("show_qr_code", false).asBool();
    config.show_shelter_logo = json.get("show_shelter_logo", true).asBool();
    config.show_cta = json.get("show_cta", true).asBool();
    config.show_website = json.get("show_website", true).asBool();
    config.logo_path = json.get("logo_path", "").asString();
    config.cta_text = json.get("cta_text", "Adopt Me Today!").asString();
    config.website_url = json.get("website_url", "").asString();

    return config;
}

Json::Value CardConfig::toJson() const {
    Json::Value json;

    Json::Value dims;
    dims["width"] = dimensions.width;
    dims["height"] = dimensions.height;
    dims["format"] = dimensions.format;
    dims["quality"] = dimensions.quality;
    json["dimensions"] = dims;

    json["show_wait_time"] = show_wait_time;
    json["show_urgency_badge"] = show_urgency_badge;
    json["show_qr_code"] = show_qr_code;
    json["show_shelter_logo"] = show_shelter_logo;
    json["show_cta"] = show_cta;
    json["show_website"] = show_website;
    json["logo_path"] = logo_path;
    json["cta_text"] = cta_text;
    json["website_url"] = website_url;

    return json;
}

// ============================================================================
// GENERATED CARD IMPLEMENTATION
// ============================================================================

Json::Value GeneratedCard::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["error_message"] = error_message;
    json["image_format"] = image_format;
    json["width"] = width;
    json["height"] = height;
    json["file_size"] = static_cast<Json::UInt64>(file_size);
    json["dog_id"] = dog_id;
    json["dog_name"] = dog_name;
    json["days_waiting"] = days_waiting;
    json["badge"] = urgencyBadgeToString(badge);
    json["style"] = cardStyleToString(style);
    json["platform"] = platformToString(platform);
    json["image_url"] = image_url;
    json["qr_code_url"] = qr_code_url;
    json["generated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        generated_at.time_since_epoch()).count();
    json["generation_time_ms"] = static_cast<Json::Int64>(generation_time.count());
    return json;
}

// ============================================================================
// DOG CARD DATA IMPLEMENTATION
// ============================================================================

UrgencyBadge DogCardData::determineUrgencyBadge() const {
    // Priority order: special conditions first, then wait time
    if (is_bonded_pair) return UrgencyBadge::BONDED_PAIR;
    if (is_special_needs) return UrgencyBadge::SPECIAL_NEEDS;
    if (is_senior) return UrgencyBadge::SENIOR;

    // Wait time badges
    if (days_waiting >= 730) return UrgencyBadge::CRITICAL;      // 2+ years
    if (days_waiting >= 365) return UrgencyBadge::URGENT;        // 1+ year
    if (days_waiting >= 180) return UrgencyBadge::LONG_WAIT;     // 6+ months
    if (days_waiting <= 7) return UrgencyBadge::NEW_ARRIVAL;     // New

    return UrgencyBadge::NONE;
}

std::optional<DogCardData> DogCardData::loadFromDatabase(const std::string& dog_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT d.id, d.name, d.breed, d.age_years, d.age_months, "
            "d.sex, d.size, d.primary_photo_url, d.intake_date, "
            "d.is_senior, d.is_special_needs, d.is_bonded_pair, "
            "d.bonded_pair_name, d.short_bio, "
            "s.name as shelter_name "
            "FROM dogs d "
            "LEFT JOIN shelters s ON d.shelter_id = s.id "
            "WHERE d.id = $1",
            dog_id);

        if (result.empty()) {
            return std::nullopt;
        }

        const auto& row = result[0];
        DogCardData data;
        data.id = row["id"].as<std::string>();
        data.name = row["name"].as<std::string>();
        data.breed = row["breed"].as<std::string>("");
        data.sex = row["sex"].as<std::string>("Unknown");
        data.size = row["size"].as<std::string>("Medium");
        data.primary_photo_url = row["primary_photo_url"].as<std::string>("");
        data.is_senior = row["is_senior"].as<bool>(false);
        data.is_special_needs = row["is_special_needs"].as<bool>(false);
        data.is_bonded_pair = row["is_bonded_pair"].as<bool>(false);
        data.bonded_pair_name = row["bonded_pair_name"].as<std::string>("");
        data.short_bio = row["short_bio"].as<std::string>("");
        data.shelter_name = row["shelter_name"].as<std::string>("Local Shelter");

        // Calculate age display
        int years = row["age_years"].as<int>(0);
        int months = row["age_months"].as<int>(0);
        if (years > 0) {
            data.age_display = std::to_string(years) + " year" + (years > 1 ? "s" : "") + " old";
        } else if (months > 0) {
            data.age_display = std::to_string(months) + " month" + (months > 1 ? "s" : "") + " old";
        } else {
            data.age_display = "Age unknown";
        }

        // Calculate days waiting
        std::string intake_date = row["intake_date"].as<std::string>("");
        if (!intake_date.empty()) {
            auto now = std::chrono::system_clock::now();
            auto now_t = std::chrono::system_clock::to_time_t(now);

            std::tm intake_tm = {};
            std::istringstream ss(intake_date);
            ss >> std::get_time(&intake_tm, "%Y-%m-%d");
            auto intake_t = std::mktime(&intake_tm);

            data.days_waiting = static_cast<int>(
                std::difftime(now_t, intake_t) / (60 * 60 * 24));
        } else {
            data.days_waiting = 0;
        }

        // Load additional photos
        auto photos_result = txn.exec_params(
            "SELECT photo_url FROM dog_photos WHERE dog_id = $1 "
            "ORDER BY display_order LIMIT 10",
            dog_id);

        for (const auto& photo_row : photos_result) {
            data.additional_photos.push_back(photo_row[0].as<std::string>());
        }

        // Build profile URL
        data.profile_url = "https://waitingthelongest.com/dogs/" + dog_id;

        return data;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to load dog data",
            {{"dog_id", dog_id}, {"error", e.what()}});
        return std::nullopt;
    }
}

Json::Value DogCardData::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["breed"] = breed;
    json["age_display"] = age_display;
    json["sex"] = sex;
    json["size"] = size;
    json["primary_photo_url"] = primary_photo_url;
    json["days_waiting"] = days_waiting;
    json["is_senior"] = is_senior;
    json["is_special_needs"] = is_special_needs;
    json["is_bonded_pair"] = is_bonded_pair;
    json["bonded_pair_name"] = bonded_pair_name;
    json["short_bio"] = short_bio;
    json["shelter_name"] = shelter_name;
    json["profile_url"] = profile_url;

    Json::Value photos(Json::arrayValue);
    for (const auto& url : additional_photos) {
        photos.append(url);
    }
    json["additional_photos"] = photos;

    return json;
}

// ============================================================================
// SHARE CARD GENERATOR IMPLEMENTATION
// ============================================================================

ShareCardGenerator& ShareCardGenerator::getInstance() {
    static ShareCardGenerator instance;
    return instance;
}

void ShareCardGenerator::initialize(const std::string& assets_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    assets_path_ = assets_path;
    default_colors_ = CardColors::defaultScheme();
    default_fonts_ = CardFonts::defaultFonts();

    initialized_ = true;

    LOG_INFO << "ShareCardGenerator: Initialized with assets path: " + assets_path;
}

void ShareCardGenerator::initializeFromConfig() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT config_value FROM system_config WHERE config_key = 'share_card_generator'");

        if (!result.empty() && !result[0][0].is_null()) {
            Json::Value json;
            Json::Reader reader;
            if (reader.parse(result[0][0].as<std::string>(), json)) {
                std::string assets = json.get("assets_path", "assets/social").asString();
                initialize(assets);

                if (json.isMember("colors")) {
                    default_colors_ = CardColors::fromJson(json["colors"]);
                }
                if (json.isMember("logo_path")) {
                    default_logo_path_ = json["logo_path"].asString();
                }
                if (json.isMember("website_url")) {
                    default_website_ = json["website_url"].asString();
                }
                if (json.isMember("cta_text")) {
                    default_cta_ = json["cta_text"].asString();
                }
                return;
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to load config",
            {{"error", e.what()}});
    }

    initialize("assets/social");
}

bool ShareCardGenerator::isInitialized() const {
    return initialized_;
}

// ============================================================================
// CARD GENERATION
// ============================================================================

GeneratedCard ShareCardGenerator::generateCard(
    const std::string& dog_id,
    Platform platform,
    CardStyle style,
    const std::optional<CardConfig>& config) {

    GeneratedCard card;
    card.dog_id = dog_id;
    card.platform = platform;
    card.style = style;
    card.generated_at = std::chrono::system_clock::now();

    // Check cache first
    auto cached = getCachedCard(dog_id, platform, style);
    if (cached) {
        return *cached;
    }

    // Load dog data
    auto dog_data = DogCardData::loadFromDatabase(dog_id);
    if (!dog_data) {
        card.success = false;
        card.error_message = "Dog not found: " + dog_id;
        return card;
    }

    return generateCard(*dog_data, platform, style, config);
}

GeneratedCard ShareCardGenerator::generateCard(
    const DogCardData& dog_data,
    Platform platform,
    CardStyle style,
    const std::optional<CardConfig>& config) {

    auto start_time = std::chrono::steady_clock::now();

    GeneratedCard card;
    card.dog_id = dog_data.id;
    card.dog_name = dog_data.name;
    card.days_waiting = dog_data.days_waiting;
    card.platform = platform;
    card.style = style;
    card.generated_at = std::chrono::system_clock::now();

    try {
        // Build configuration
        CardConfig card_config = config.value_or(CardConfig::defaultForPlatform(platform));

        // Apply defaults
        if (card_config.logo_path.empty()) {
            card_config.logo_path = default_logo_path_;
        }
        if (card_config.website_url.empty()) {
            card_config.website_url = default_website_;
        }
        if (card_config.cta_text.empty()) {
            card_config.cta_text = default_cta_;
        }
        if (card_config.cta_text.empty()) {
            card_config.cta_text = "Adopt Me Today!";
        }

        // Determine badge
        card.badge = dog_data.determineUrgencyBadge();

        // Render based on style
        std::vector<uint8_t> image_data;

        switch (style) {
            case CardStyle::URGENT:
                image_data = renderUrgentCard(dog_data, card_config);
                break;

            case CardStyle::STORY:
                image_data = renderStoryCard(dog_data, card_config);
                break;

            case CardStyle::MILESTONE:
                image_data = renderMilestoneCard(dog_data, "default", card_config);
                break;

            case CardStyle::STANDARD:
            default:
                image_data = renderStandardCard(dog_data, card_config);
                break;
        }

        card.image_data = std::move(image_data);
        card.image_format = card_config.dimensions.format;
        card.width = card_config.dimensions.width;
        card.height = card_config.dimensions.height;
        card.file_size = card.image_data.size();
        card.success = true;

        // Build QR code URL
        card.qr_code_url = dog_data.profile_url;

        // Cache the result
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::string cache_key = generateCacheKey(dog_data.id, platform, style);
            card_cache_[cache_key] = card;
        }

    } catch (const std::exception& e) {
        card.success = false;
        card.error_message = e.what();
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Card generation failed",
            {{"dog_id", dog_data.id}, {"error", e.what()}});
    }

    auto end_time = std::chrono::steady_clock::now();
    card.generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return card;
}

std::map<Platform, GeneratedCard> ShareCardGenerator::generateMultiPlatformCards(
    const std::string& dog_id,
    const std::vector<Platform>& platforms,
    CardStyle style) {

    std::map<Platform, GeneratedCard> results;

    // Load dog data once
    auto dog_data = DogCardData::loadFromDatabase(dog_id);
    if (!dog_data) {
        for (const auto& platform : platforms) {
            GeneratedCard card;
            card.success = false;
            card.error_message = "Dog not found: " + dog_id;
            card.platform = platform;
            results[platform] = card;
        }
        return results;
    }

    // Generate for each platform
    for (const auto& platform : platforms) {
        results[platform] = generateCard(*dog_data, platform, style);
    }

    return results;
}

std::vector<GeneratedCard> ShareCardGenerator::generateCarousel(
    const std::string& dog_id,
    Platform platform,
    int max_cards) {

    std::vector<GeneratedCard> cards;

    auto dog_data = DogCardData::loadFromDatabase(dog_id);
    if (!dog_data) {
        GeneratedCard error_card;
        error_card.success = false;
        error_card.error_message = "Dog not found: " + dog_id;
        cards.push_back(error_card);
        return cards;
    }

    // First card: main photo with info
    cards.push_back(generateCard(*dog_data, platform, CardStyle::STANDARD));

    // Additional cards from additional photos
    int photo_count = std::min(
        static_cast<int>(dog_data->additional_photos.size()),
        max_cards - 1);

    for (int i = 0; i < photo_count; ++i) {
        DogCardData photo_data = *dog_data;
        photo_data.primary_photo_url = dog_data->additional_photos[i];

        cards.push_back(generateCard(photo_data, platform, CardStyle::CAROUSEL_ITEM));
    }

    return cards;
}

GeneratedCard ShareCardGenerator::generateMilestoneCard(
    const std::string& dog_id,
    const std::string& milestone_type,
    Platform platform) {

    auto dog_data = DogCardData::loadFromDatabase(dog_id);
    if (!dog_data) {
        GeneratedCard card;
        card.success = false;
        card.error_message = "Dog not found: " + dog_id;
        return card;
    }

    auto start_time = std::chrono::steady_clock::now();

    GeneratedCard card;
    card.dog_id = dog_data->id;
    card.dog_name = dog_data->name;
    card.days_waiting = dog_data->days_waiting;
    card.platform = platform;
    card.style = CardStyle::MILESTONE;
    card.generated_at = std::chrono::system_clock::now();
    card.badge = UrgencyBadge::URGENT;

    try {
        CardConfig config = CardConfig::defaultForPlatform(platform);
        card.image_data = renderMilestoneCard(*dog_data, milestone_type, config);
        card.image_format = config.dimensions.format;
        card.width = config.dimensions.width;
        card.height = config.dimensions.height;
        card.file_size = card.image_data.size();
        card.success = true;

    } catch (const std::exception& e) {
        card.success = false;
        card.error_message = e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    card.generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return card;
}

GeneratedCard ShareCardGenerator::generateSuccessStoryCard(
    const std::string& dog_id,
    const std::string& before_photo_url,
    const std::string& after_photo_url,
    Platform platform) {

    auto dog_data = DogCardData::loadFromDatabase(dog_id);
    if (!dog_data) {
        GeneratedCard card;
        card.success = false;
        card.error_message = "Dog not found: " + dog_id;
        return card;
    }

    auto start_time = std::chrono::steady_clock::now();

    GeneratedCard card;
    card.dog_id = dog_data->id;
    card.dog_name = dog_data->name;
    card.days_waiting = dog_data->days_waiting;
    card.platform = platform;
    card.style = CardStyle::SUCCESS_STORY;
    card.generated_at = std::chrono::system_clock::now();
    card.badge = UrgencyBadge::NONE;

    try {
        CardConfig config = CardConfig::defaultForPlatform(platform);
        card.image_data = renderSuccessStoryCard(*dog_data, before_photo_url, after_photo_url, config);
        card.image_format = config.dimensions.format;
        card.width = config.dimensions.width;
        card.height = config.dimensions.height;
        card.file_size = card.image_data.size();
        card.success = true;

    } catch (const std::exception& e) {
        card.success = false;
        card.error_message = e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    card.generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return card;
}

// ============================================================================
// QR CODE GENERATION
// ============================================================================

std::vector<uint8_t> ShareCardGenerator::generateQRCode(
    const std::string& profile_url,
    int size) {

    // QR code generation using a simple implementation
    // In production, use a library like libqrencode
    std::vector<uint8_t> qr_data;

    try {
        // Placeholder: create a simple QR-like pattern
        // Real implementation would use libqrencode

        int module_count = 25;  // Standard QR size
        int module_size = size / module_count;
        int actual_size = module_count * module_size;

        // Create simple placeholder image (white with black pattern)
        qr_data.resize(actual_size * actual_size * 4);  // RGBA

        // Fill with white
        for (size_t i = 0; i < qr_data.size(); i += 4) {
            qr_data[i] = 255;      // R
            qr_data[i + 1] = 255;  // G
            qr_data[i + 2] = 255;  // B
            qr_data[i + 3] = 255;  // A
        }

        // Add finder patterns (corners)
        auto drawFinderPattern = [&](int start_x, int start_y) {
            for (int y = 0; y < 7 * module_size; y++) {
                for (int x = 0; x < 7 * module_size; x++) {
                    int mx = x / module_size;
                    int my = y / module_size;

                    bool is_black = (mx == 0 || mx == 6 || my == 0 || my == 6) ||
                                   (mx >= 2 && mx <= 4 && my >= 2 && my <= 4);

                    if (is_black) {
                        int idx = ((start_y + y) * actual_size + (start_x + x)) * 4;
                        if (idx >= 0 && idx + 3 < static_cast<int>(qr_data.size())) {
                            qr_data[idx] = 0;
                            qr_data[idx + 1] = 0;
                            qr_data[idx + 2] = 0;
                        }
                    }
                }
            }
        };

        drawFinderPattern(0, 0);  // Top-left
        drawFinderPattern(actual_size - 7 * module_size, 0);  // Top-right
        drawFinderPattern(0, actual_size - 7 * module_size);  // Bottom-left

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "QR code generation failed",
            {{"url", profile_url}, {"error", e.what()}});
    }

    return qr_data;
}

// ============================================================================
// UPLOAD
// ============================================================================

std::string ShareCardGenerator::uploadCard(GeneratedCard& card) {
    if (!card.success || card.image_data.empty()) {
        return "";
    }

    try {
        // Upload to CDN (implementation depends on CDN provider)
        // This is a placeholder for the actual upload logic

        // Generate a unique filename
        std::stringstream ss;
        ss << "share_cards/" << card.dog_id << "_"
           << platformToString(card.platform) << "_"
           << cardStyleToString(card.style) << "_"
           << std::chrono::duration_cast<std::chrono::seconds>(
                  card.generated_at.time_since_epoch()).count()
           << "." << card.image_format;

        std::string filename = ss.str();

        // In production, upload to S3, Cloudinary, etc.
        // For now, return a placeholder URL
        card.image_url = "https://cdn.waitingthelongest.com/" + filename;

        return card.image_url;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Card upload failed",
            {{"dog_id", card.dog_id}, {"error", e.what()}});
        return "";
    }
}

int ShareCardGenerator::uploadCards(std::vector<GeneratedCard>& cards) {
    int count = 0;

    for (auto& card : cards) {
        if (!uploadCard(card).empty()) {
            count++;
        }
    }

    return count;
}

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

std::optional<GeneratedCard> ShareCardGenerator::getCachedCard(
    const std::string& dog_id,
    Platform platform,
    CardStyle style) const {

    std::lock_guard<std::mutex> lock(cache_mutex_);

    std::string key = generateCacheKey(dog_id, platform, style);
    auto it = card_cache_.find(key);

    if (it != card_cache_.end()) {
        // Check if cache is still valid (less than 1 hour old)
        auto age = std::chrono::system_clock::now() - it->second.generated_at;
        if (age < std::chrono::hours(1)) {
            return it->second;
        }
        // Remove stale entry
        card_cache_.erase(it);
    }

    return std::nullopt;
}

void ShareCardGenerator::invalidateCache(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Remove all cached cards for this dog
    for (auto it = card_cache_.begin(); it != card_cache_.end();) {
        if (it->second.dog_id == dog_id) {
            it = card_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void ShareCardGenerator::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    card_cache_.clear();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ShareCardGenerator::setDefaultColors(const CardColors& colors) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_colors_ = colors;
}

void ShareCardGenerator::setDefaultFonts(const CardFonts& fonts) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_fonts_ = fonts;
}

void ShareCardGenerator::setShelterBranding(
    const std::string& logo_path,
    const std::string& website_url,
    const std::string& cta_text) {

    std::lock_guard<std::mutex> lock(mutex_);
    default_logo_path_ = logo_path;
    default_website_ = website_url;
    default_cta_ = cta_text;
}

// ============================================================================
// INTERNAL RENDERING METHODS
// ============================================================================

std::vector<uint8_t> ShareCardGenerator::renderStandardCard(
    const DogCardData& dog,
    const CardConfig& config) {

    std::vector<uint8_t> image;

    try {
        // Download and resize the dog photo
        auto photo_data = downloadImage(dog.primary_photo_url);
        if (photo_data.empty()) {
            throw std::runtime_error("Failed to download dog photo");
        }

        image = resizeImage(photo_data, config.dimensions.width, config.dimensions.height);

        // Apply gradient overlay at bottom for text readability
        applyGradientOverlay(image, config.colors.overlay, 0.7f, true);

        // Apply dog name
        int name_y = config.dimensions.height - 200;
        applyTextOverlay(image, dog.name, 50, name_y,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.text_light);

        // Apply breed and age
        std::string info = dog.breed + " | " + dog.age_display;
        applyTextOverlay(image, info, 50, name_y + 80,
            config.fonts.body_family, config.fonts.body_size,
            config.colors.text_light);

        // Apply wait time badge if enabled
        if (config.show_wait_time && dog.days_waiting > 0) {
            applyWaitTimeBadge(image, dog.days_waiting, config);
        }

        // Apply urgency badge if enabled
        if (config.show_urgency_badge) {
            UrgencyBadge badge = dog.determineUrgencyBadge();
            if (badge != UrgencyBadge::NONE) {
                applyUrgencyBadge(image, badge, config);
            }
        }

        // Apply CTA text if enabled
        if (config.show_cta) {
            int cta_y = config.dimensions.height - 50;
            applyTextOverlay(image, config.cta_text, 50, cta_y,
                config.fonts.body_family, config.fonts.cta_size,
                config.colors.accent);
        }

        // Apply shelter logo if enabled
        if (config.show_shelter_logo && !config.logo_path.empty()) {
            applyShelterLogo(image, config);
        }

        // Apply QR code if enabled
        if (config.show_qr_code) {
            int qr_size = 150;
            int qr_x = config.dimensions.width - qr_size - 30;
            int qr_y = config.dimensions.height - qr_size - 30;
            applyQRCode(image, dog.profile_url, qr_x, qr_y, qr_size);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Standard card render failed",
            {{"dog_id", dog.id}, {"error", e.what()}});
        throw;
    }

    return image;
}

std::vector<uint8_t> ShareCardGenerator::renderUrgentCard(
    const DogCardData& dog,
    const CardConfig& config) {

    std::vector<uint8_t> image;

    try {
        // Similar to standard but with urgent styling
        auto photo_data = downloadImage(dog.primary_photo_url);
        if (photo_data.empty()) {
            throw std::runtime_error("Failed to download dog photo");
        }

        image = resizeImage(photo_data, config.dimensions.width, config.dimensions.height);

        // Apply stronger overlay with urgent color tint
        applyGradientOverlay(image, config.colors.urgent_badge, 0.3f, false);
        applyGradientOverlay(image, config.colors.overlay, 0.7f, true);

        // Large "HELP ME FIND A HOME" text
        int urgent_y = 100;
        applyTextOverlay(image, "HELP ME FIND A HOME", 50, urgent_y,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.accent);

        // Dog name
        int name_y = config.dimensions.height - 250;
        applyTextOverlay(image, dog.name, 50, name_y,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.text_light);

        // Waiting time emphasized
        std::string wait_text = "Waiting " + std::to_string(dog.days_waiting) + " days for a family";
        applyTextOverlay(image, wait_text, 50, name_y + 80,
            config.fonts.body_family, config.fonts.subheading_size,
            config.colors.text_light);

        // Always show urgency badge on urgent cards
        applyUrgencyBadge(image, dog.determineUrgencyBadge(), config);

        // CTA
        if (config.show_cta) {
            int cta_y = config.dimensions.height - 50;
            applyTextOverlay(image, "Please Share - Every Share Helps!", 50, cta_y,
                config.fonts.body_family, config.fonts.cta_size,
                config.colors.accent);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Urgent card render failed",
            {{"dog_id", dog.id}, {"error", e.what()}});
        throw;
    }

    return image;
}

std::vector<uint8_t> ShareCardGenerator::renderMilestoneCard(
    const DogCardData& dog,
    const std::string& milestone_type,
    const CardConfig& config) {

    std::vector<uint8_t> image;

    try {
        auto photo_data = downloadImage(dog.primary_photo_url);
        if (photo_data.empty()) {
            throw std::runtime_error("Failed to download dog photo");
        }

        image = resizeImage(photo_data, config.dimensions.width, config.dimensions.height);
        applyGradientOverlay(image, config.colors.overlay, 0.6f, true);

        // Milestone text
        std::string milestone_text;
        if (milestone_type == "1_year") {
            milestone_text = "1 YEAR WAITING";
        } else if (milestone_type == "2_years") {
            milestone_text = "2 YEARS WAITING";
        } else if (milestone_type == "500_days") {
            milestone_text = "500 DAYS WAITING";
        } else {
            milestone_text = std::to_string(dog.days_waiting) + " DAYS";
        }

        int milestone_y = 100;
        applyTextOverlay(image, milestone_text, 50, milestone_y,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.accent);

        // Dog name
        int name_y = config.dimensions.height - 200;
        applyTextOverlay(image, dog.name, 50, name_y,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.text_light);

        // Emotional appeal
        std::string appeal = "Still hoping for a forever home";
        applyTextOverlay(image, appeal, 50, name_y + 80,
            config.fonts.body_family, config.fonts.subheading_size,
            config.colors.text_light);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Milestone card render failed",
            {{"dog_id", dog.id}, {"error", e.what()}});
        throw;
    }

    return image;
}

std::vector<uint8_t> ShareCardGenerator::renderSuccessStoryCard(
    const DogCardData& dog,
    const std::string& before_photo,
    const std::string& after_photo,
    const CardConfig& config) {

    std::vector<uint8_t> image;

    try {
        // For success story, we create a side-by-side comparison
        int half_width = config.dimensions.width / 2;

        auto before_data = downloadImage(before_photo);
        auto after_data = downloadImage(after_photo);

        if (before_data.empty() || after_data.empty()) {
            throw std::runtime_error("Failed to download comparison photos");
        }

        auto before_resized = resizeImage(before_data, half_width, config.dimensions.height);
        auto after_resized = resizeImage(after_data, half_width, config.dimensions.height);

        // Combine images side by side
        // This is a simplified placeholder - real implementation would properly composite
        image.resize(config.dimensions.width * config.dimensions.height * 4);

        // Copy before image to left half
        // Copy after image to right half
        // (Simplified - actual implementation would use proper image compositing)

        // Apply "ADOPTED!" banner
        applyTextOverlay(image, "ADOPTED!", config.dimensions.width / 2 - 100, 50,
            config.fonts.heading_family, config.fonts.heading_size,
            config.colors.success_badge);

        // Add labels
        applyTextOverlay(image, "Before", 50, config.dimensions.height - 50,
            config.fonts.body_family, config.fonts.body_size,
            config.colors.text_light);

        applyTextOverlay(image, "After", half_width + 50, config.dimensions.height - 50,
            config.fonts.body_family, config.fonts.body_size,
            config.colors.text_light);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Success story card render failed",
            {{"dog_id", dog.id}, {"error", e.what()}});
        throw;
    }

    return image;
}

std::vector<uint8_t> ShareCardGenerator::renderStoryCard(
    const DogCardData& dog,
    const CardConfig& config) {

    // Story cards are vertical (9:16) format
    CardConfig story_config = config;
    story_config.dimensions = CardDimensions::forPlatform(Platform::TIKTOK, CardStyle::STORY);

    return renderStandardCard(dog, story_config);
}

// ============================================================================
// IMAGE PROCESSING HELPERS
// ============================================================================

std::vector<uint8_t> ShareCardGenerator::downloadImage(const std::string& url) {
    std::vector<uint8_t> data;

    if (url.empty()) {
        return data;
    }

    try {
        // Use drogon HTTP client to download
        auto client = drogon::HttpClient::newHttpClient(url);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);

        // Synchronous request (in production, might want async)
        std::promise<std::vector<uint8_t>> promise;
        auto future = promise.get_future();

        client->sendRequest(req, [&promise](drogon::ReqResult result,
                                            const drogon::HttpResponsePtr& response) {
            if (result == drogon::ReqResult::Ok && response) {
                std::string body(response->body());
                std::vector<uint8_t> data(body.begin(), body.end());
                promise.set_value(std::move(data));
            } else {
                promise.set_value({});
            }
        });

        data = future.get();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Image download failed",
            {{"url", url}, {"error", e.what()}});
    }

    return data;
}

std::vector<uint8_t> ShareCardGenerator::resizeImage(
    const std::vector<uint8_t>& image_data,
    int width,
    int height,
    bool crop) {

    // Placeholder for actual image resizing
    // In production, use stb_image_resize or similar library

    std::vector<uint8_t> resized;

    // For now, return a placeholder colored image
    resized.resize(width * height * 4);  // RGBA

    // Fill with a gradient as placeholder
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            resized[idx] = static_cast<uint8_t>(100 + (y * 50 / height));      // R
            resized[idx + 1] = static_cast<uint8_t>(100 + (x * 50 / width));   // G
            resized[idx + 2] = 150;                                             // B
            resized[idx + 3] = 255;                                             // A
        }
    }

    return resized;
}

void ShareCardGenerator::applyTextOverlay(
    std::vector<uint8_t>& image,
    const std::string& text,
    int x,
    int y,
    const std::string& font_family,
    int font_size,
    const std::string& color) {

    // Placeholder for text rendering
    // In production, use FreeType or similar library

    // This would render text onto the image at the specified position
    // For now, this is a no-op placeholder
}

void ShareCardGenerator::applyWaitTimeBadge(
    std::vector<uint8_t>& image,
    int days_waiting,
    const CardConfig& config) {

    // Apply a badge showing days waiting
    std::string badge_text = std::to_string(days_waiting) + " days waiting";

    // Position in top-left corner
    int badge_x = 30;
    int badge_y = 30;

    // Draw badge background (rounded rectangle)
    // Then overlay text
    applyTextOverlay(image, badge_text, badge_x + 10, badge_y + 25,
        config.fonts.body_family, config.fonts.badge_size,
        config.colors.text_light);
}

void ShareCardGenerator::applyUrgencyBadge(
    std::vector<uint8_t>& image,
    UrgencyBadge badge,
    const CardConfig& config) {

    if (badge == UrgencyBadge::NONE) {
        return;
    }

    std::string badge_text;
    std::string badge_color;

    switch (badge) {
        case UrgencyBadge::NEW_ARRIVAL:
            badge_text = "NEW!";
            badge_color = config.colors.success_badge;
            break;
        case UrgencyBadge::LONG_WAIT:
            badge_text = "LONG WAIT";
            badge_color = config.colors.accent;
            break;
        case UrgencyBadge::URGENT:
            badge_text = "URGENT";
            badge_color = config.colors.urgent_badge;
            break;
        case UrgencyBadge::CRITICAL:
            badge_text = "CRITICAL";
            badge_color = config.colors.urgent_badge;
            break;
        case UrgencyBadge::SENIOR:
            badge_text = "SENIOR";
            badge_color = config.colors.primary;
            break;
        case UrgencyBadge::SPECIAL_NEEDS:
            badge_text = "SPECIAL NEEDS";
            badge_color = config.colors.primary;
            break;
        case UrgencyBadge::BONDED_PAIR:
            badge_text = "BONDED PAIR";
            badge_color = config.colors.primary;
            break;
        default:
            return;
    }

    // Position in top-right corner
    int badge_x = config.dimensions.width - 200;
    int badge_y = 30;

    applyTextOverlay(image, badge_text, badge_x, badge_y + 25,
        config.fonts.body_family, config.fonts.badge_size,
        badge_color);
}

void ShareCardGenerator::applyQRCode(
    std::vector<uint8_t>& image,
    const std::string& url,
    int x,
    int y,
    int size) {

    auto qr_data = generateQRCode(url, size);

    // Composite QR code onto main image at position (x, y)
    // Placeholder - actual implementation would blend pixels
}

void ShareCardGenerator::applyShelterLogo(
    std::vector<uint8_t>& image,
    const CardConfig& config) {

    if (config.logo_path.empty()) {
        return;
    }

    // Load and resize logo, then composite
    // Placeholder implementation
}

void ShareCardGenerator::applyGradientOverlay(
    std::vector<uint8_t>& image,
    const std::string& color,
    float opacity,
    bool from_bottom) {

    // Apply a gradient overlay for text readability
    // Placeholder implementation - will modify pixel values in image buffer

    (void)opacity;
    (void)from_bottom;

    // Parse hex color
    int r = 0, g = 0, b = 0;
    if (color.length() >= 7 && color[0] == '#') {
        r = std::stoi(color.substr(1, 2), nullptr, 16);
        g = std::stoi(color.substr(3, 2), nullptr, 16);
        b = std::stoi(color.substr(5, 2), nullptr, 16);
    }

    // TODO: Apply gradient using r, g, b values to modify pixel data in image buffer
    (void)r;
    (void)g;
    (void)b;
    (void)image;
}

std::string ShareCardGenerator::generateCacheKey(
    const std::string& dog_id,
    Platform platform,
    CardStyle style) const {

    return dog_id + "_" + platformToString(platform) + "_" + cardStyleToString(style);
}

} // namespace wtl::content::social

/**
 * @file TikTokTemplate.cc
 * @brief Implementation of TikTok templates and TemplateManager
 * @see TikTokTemplate.h for documentation
 */

#include "TikTokTemplate.h"

// Standard library includes
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

// Drogon includes
#include <drogon/drogon.h>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::content::tiktok {

// ============================================================================
// TIKTOKTEMPLATE ENUM CONVERSIONS
// ============================================================================

std::string TikTokTemplate::typeToString(TikTokTemplateType type) {
    switch (type) {
        case TikTokTemplateType::URGENT_COUNTDOWN:  return "urgent_countdown";
        case TikTokTemplateType::LONGEST_WAITING:   return "longest_waiting";
        case TikTokTemplateType::OVERLOOKED_ANGEL:  return "overlooked_angel";
        case TikTokTemplateType::FOSTER_PLEA:       return "foster_plea";
        case TikTokTemplateType::HAPPY_UPDATE:      return "happy_update";
        case TikTokTemplateType::EDUCATIONAL:       return "educational";
        default:                                    return "unknown";
    }
}

TikTokTemplateType TikTokTemplate::stringToType(const std::string& str) {
    if (str == "urgent_countdown")  return TikTokTemplateType::URGENT_COUNTDOWN;
    if (str == "longest_waiting")   return TikTokTemplateType::LONGEST_WAITING;
    if (str == "overlooked_angel")  return TikTokTemplateType::OVERLOOKED_ANGEL;
    if (str == "foster_plea")       return TikTokTemplateType::FOSTER_PLEA;
    if (str == "happy_update")      return TikTokTemplateType::HAPPY_UPDATE;
    if (str == "educational")       return TikTokTemplateType::EDUCATIONAL;
    return TikTokTemplateType::EDUCATIONAL;  // Default
}

std::string TikTokTemplate::overlayStyleToString(OverlayStyle style) {
    switch (style) {
        case OverlayStyle::NONE:            return "none";
        case OverlayStyle::URGENT_RED:      return "urgent_red";
        case OverlayStyle::WAIT_TIME_GREEN: return "wait_time_green";
        case OverlayStyle::HEARTWARMING:    return "heartwarming";
        case OverlayStyle::INFORMATIVE:     return "informative";
        case OverlayStyle::FOSTER_APPEAL:   return "foster_appeal";
        default:                            return "none";
    }
}

OverlayStyle TikTokTemplate::stringToOverlayStyle(const std::string& str) {
    if (str == "urgent_red")      return OverlayStyle::URGENT_RED;
    if (str == "wait_time_green") return OverlayStyle::WAIT_TIME_GREEN;
    if (str == "heartwarming")    return OverlayStyle::HEARTWARMING;
    if (str == "informative")     return OverlayStyle::INFORMATIVE;
    if (str == "foster_appeal")   return OverlayStyle::FOSTER_APPEAL;
    return OverlayStyle::NONE;
}

// ============================================================================
// TIKTOKTEMPLATE JSON CONVERSION
// ============================================================================

Json::Value TikTokTemplate::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["name"] = name;
    json["type"] = typeToString(type);
    json["description"] = description;

    // Caption
    json["caption_template"] = caption_template;

    Json::Value required(Json::arrayValue);
    for (const auto& p : required_placeholders) {
        required.append(p);
    }
    json["required_placeholders"] = required;

    Json::Value optional(Json::arrayValue);
    for (const auto& p : optional_placeholders) {
        optional.append(p);
    }
    json["optional_placeholders"] = optional;

    // Hashtags
    Json::Value default_tags(Json::arrayValue);
    for (const auto& tag : default_hashtags) {
        default_tags.append(tag);
    }
    json["default_hashtags"] = default_tags;

    Json::Value opt_tags(Json::arrayValue);
    for (const auto& tag : optional_hashtags) {
        opt_tags.append(tag);
    }
    json["optional_hashtags"] = opt_tags;

    json["max_hashtags"] = max_hashtags;

    // Styling
    json["overlay_style"] = overlayStyleToString(overlay_style);

    if (suggested_music_id) json["suggested_music_id"] = *suggested_music_id;
    else json["suggested_music_id"] = Json::nullValue;

    if (suggested_music_name) json["suggested_music_name"] = *suggested_music_name;
    else json["suggested_music_name"] = Json::nullValue;

    json["include_countdown"] = include_countdown;
    json["include_wait_time"] = include_wait_time;

    if (text_overlay_template) json["text_overlay_template"] = *text_overlay_template;
    else json["text_overlay_template"] = Json::nullValue;

    // Timing
    Json::Value hours(Json::arrayValue);
    for (int hour : optimal_post_hours) {
        hours.append(hour);
    }
    json["optimal_post_hours"] = hours;

    json["auto_generate_enabled"] = auto_generate_enabled;
    json["priority"] = priority;

    return json;
}

TikTokTemplate TikTokTemplate::fromJson(const Json::Value& json) {
    TikTokTemplate tmpl;

    // Identity
    tmpl.id = json.get("id", "").asString();
    tmpl.name = json.get("name", "").asString();
    tmpl.type = stringToType(json.get("type", "educational").asString());
    tmpl.description = json.get("description", "").asString();

    // Caption
    tmpl.caption_template = json.get("caption_template", "").asString();

    if (json.isMember("required_placeholders") && json["required_placeholders"].isArray()) {
        for (const auto& p : json["required_placeholders"]) {
            tmpl.required_placeholders.push_back(p.asString());
        }
    }

    if (json.isMember("optional_placeholders") && json["optional_placeholders"].isArray()) {
        for (const auto& p : json["optional_placeholders"]) {
            tmpl.optional_placeholders.push_back(p.asString());
        }
    }

    // Hashtags
    if (json.isMember("default_hashtags") && json["default_hashtags"].isArray()) {
        for (const auto& tag : json["default_hashtags"]) {
            tmpl.default_hashtags.push_back(tag.asString());
        }
    }

    if (json.isMember("optional_hashtags") && json["optional_hashtags"].isArray()) {
        for (const auto& tag : json["optional_hashtags"]) {
            tmpl.optional_hashtags.push_back(tag.asString());
        }
    }

    tmpl.max_hashtags = json.get("max_hashtags", 10).asInt();

    // Styling
    tmpl.overlay_style = stringToOverlayStyle(json.get("overlay_style", "none").asString());

    if (json.isMember("suggested_music_id") && !json["suggested_music_id"].isNull()) {
        tmpl.suggested_music_id = json["suggested_music_id"].asString();
    }
    if (json.isMember("suggested_music_name") && !json["suggested_music_name"].isNull()) {
        tmpl.suggested_music_name = json["suggested_music_name"].asString();
    }

    tmpl.include_countdown = json.get("include_countdown", false).asBool();
    tmpl.include_wait_time = json.get("include_wait_time", false).asBool();

    if (json.isMember("text_overlay_template") && !json["text_overlay_template"].isNull()) {
        tmpl.text_overlay_template = json["text_overlay_template"].asString();
    }

    // Timing
    if (json.isMember("optimal_post_hours") && json["optimal_post_hours"].isArray()) {
        for (const auto& hour : json["optimal_post_hours"]) {
            tmpl.optimal_post_hours.push_back(hour.asInt());
        }
    }

    tmpl.auto_generate_enabled = json.get("auto_generate_enabled", false).asBool();
    tmpl.priority = json.get("priority", 0).asInt();

    return tmpl;
}

// ============================================================================
// TIKTOKTEMPLATE FORMATTING METHODS
// ============================================================================

std::string TikTokTemplate::formatCaption(
    const std::unordered_map<std::string, std::string>& placeholders) const {

    std::string result = caption_template;

    // Replace each placeholder
    for (const auto& [key, value] : placeholders) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    // Remove any unfilled optional placeholders (replace with empty string)
    std::regex optional_pattern(R"(\{[a-zA-Z_]+\})");
    result = std::regex_replace(result, optional_pattern, "");

    // Clean up any double spaces or trailing spaces
    std::regex double_space(R"(  +)");
    result = std::regex_replace(result, double_space, " ");

    // Trim
    auto start = result.find_first_not_of(" \t\n\r");
    auto end = result.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        result = result.substr(start, end - start + 1);
    }

    return result;
}

std::optional<std::string> TikTokTemplate::formatOverlayText(
    const std::unordered_map<std::string, std::string>& placeholders) const {

    if (!text_overlay_template) {
        return std::nullopt;
    }

    std::string result = *text_overlay_template;

    // Replace placeholders
    for (const auto& [key, value] : placeholders) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return result;
}

std::vector<std::string> TikTokTemplate::getHashtags(
    const std::vector<std::string>& additional) const {

    std::vector<std::string> result;

    // Add default hashtags first
    for (const auto& tag : default_hashtags) {
        if (result.size() >= static_cast<size_t>(max_hashtags)) break;

        // Normalize hashtag (ensure starts with #)
        std::string normalized = tag;
        if (!normalized.empty() && normalized[0] != '#') {
            normalized = "#" + normalized;
        }
        result.push_back(normalized);
    }

    // Add additional hashtags
    for (const auto& tag : additional) {
        if (result.size() >= static_cast<size_t>(max_hashtags)) break;

        std::string normalized = tag;
        if (!normalized.empty() && normalized[0] != '#') {
            normalized = "#" + normalized;
        }

        // Avoid duplicates
        bool exists = std::any_of(result.begin(), result.end(),
            [&normalized](const std::string& existing) {
                // Case-insensitive comparison
                std::string lower_norm = normalized;
                std::string lower_exist = existing;
                std::transform(lower_norm.begin(), lower_norm.end(), lower_norm.begin(), ::tolower);
                std::transform(lower_exist.begin(), lower_exist.end(), lower_exist.begin(), ::tolower);
                return lower_norm == lower_exist;
            });

        if (!exists) {
            result.push_back(normalized);
        }
    }

    // Add optional hashtags if space remaining
    for (const auto& tag : optional_hashtags) {
        if (result.size() >= static_cast<size_t>(max_hashtags)) break;

        std::string normalized = tag;
        if (!normalized.empty() && normalized[0] != '#') {
            normalized = "#" + normalized;
        }

        bool exists = std::any_of(result.begin(), result.end(),
            [&normalized](const std::string& existing) {
                std::string lower_norm = normalized;
                std::string lower_exist = existing;
                std::transform(lower_norm.begin(), lower_norm.end(), lower_norm.begin(), ::tolower);
                std::transform(lower_exist.begin(), lower_exist.end(), lower_exist.begin(), ::tolower);
                return lower_norm == lower_exist;
            });

        if (!exists) {
            result.push_back(normalized);
        }
    }

    return result;
}

std::vector<std::string> TikTokTemplate::validatePlaceholders(
    const std::unordered_map<std::string, std::string>& placeholders) const {

    std::vector<std::string> missing;

    for (const auto& required : required_placeholders) {
        auto it = placeholders.find(required);
        if (it == placeholders.end() || it->second.empty()) {
            missing.push_back(required);
        }
    }

    return missing;
}

// ============================================================================
// TEMPLATEMANAGER IMPLEMENTATION
// ============================================================================

TemplateManager& TemplateManager::getInstance() {
    static TemplateManager instance;
    return instance;
}

bool TemplateManager::initialize(const std::string& config_path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    config_path_ = config_path;

    // Load defaults first
    loadDefaults();

    // Try to load from config file to override defaults
    std::ifstream file(config_path);
    if (file.is_open()) {
        try {
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errors;

            if (Json::parseFromStream(builder, file, &root, &errors)) {
                int loaded = loadFromJson(root);
                LOG_DEBUG << "Loaded " << loaded << " TikTok templates from config";
            } else {
                WTL_CAPTURE_WARNING(
                    wtl::core::debug::ErrorCategory::CONFIGURATION,
                    "Failed to parse TikTok templates config: " + errors,
                    {{"path", config_path}});
            }
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::CONFIGURATION,
                "Exception loading TikTok templates: " + std::string(e.what()),
                {{"path", config_path}});
        }
    }

    initialized_ = true;
    return true;
}

bool TemplateManager::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();
        std::string path = config.getString("tiktok.templates_path", "config/tiktok_templates.json");
        return initialize(path);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize TemplateManager from config: " + std::string(e.what()),
            {});
        // Still load defaults
        loadDefaults();
        initialized_ = true;
        return true;
    }
}

void TemplateManager::loadDefaults() {
    // Clear existing data
    templates_.clear();
    type_index_.clear();

    // ========================================================================
    // URGENT_COUNTDOWN Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_urgent_countdown";
        tmpl.name = "Urgent Countdown";
        tmpl.type = TikTokTemplateType::URGENT_COUNTDOWN;
        tmpl.description = "For dogs with imminent euthanasia dates. Creates urgency.";

        tmpl.caption_template =
            "URGENT: {dog_name} has only {urgency} left!\n\n"
            "This sweet {breed} at {shelter_name} needs a hero NOW.\n\n"
            "{dog_name} has been waiting {wait_time} for someone to notice them.\n\n"
            "Can you be their second chance? Share to save a life!";

        tmpl.required_placeholders = {"dog_name", "urgency", "shelter_name"};
        tmpl.optional_placeholders = {"breed", "wait_time", "location", "age"};

        tmpl.default_hashtags = {
            "AdoptDontShop", "UrgentDog", "SaveALife", "ShelterDog",
            "RescueDog", "AdoptMe", "DogsOfTikTok"
        };
        tmpl.optional_hashtags = {
            "LastChance", "NeedsFoster", "ShareToSave", "Urgent"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::URGENT_RED;
        tmpl.include_countdown = true;
        tmpl.include_wait_time = true;
        tmpl.text_overlay_template = "ONLY {urgency} LEFT";

        tmpl.optimal_post_hours = {7, 12, 18, 21};  // Morning, lunch, evening, night
        tmpl.auto_generate_enabled = true;
        tmpl.priority = 100;  // Highest priority

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }

    // ========================================================================
    // LONGEST_WAITING Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_longest_waiting";
        tmpl.name = "Longest Waiting";
        tmpl.type = TikTokTemplateType::LONGEST_WAITING;
        tmpl.description = "For dogs who have been waiting the longest. Highlights their patience.";

        tmpl.caption_template =
            "{dog_name} has been waiting {wait_time} for a family.\n\n"
            "Every day, this {breed} watches other dogs leave while they stay behind.\n\n"
            "At {shelter_name}, {dog_name} keeps hoping someone will choose them.\n\n"
            "Will today finally be their day?";

        tmpl.required_placeholders = {"dog_name", "wait_time", "shelter_name"};
        tmpl.optional_placeholders = {"breed", "age", "location", "personality"};

        tmpl.default_hashtags = {
            "LongestWaiting", "WaitingForLove", "AdoptDontShop", "ShelterDog",
            "RescueDog", "AdoptMe", "DogsOfTikTok"
        };
        tmpl.optional_hashtags = {
            "OverlookedDog", "PatientPup", "ChooseMe", "GiveThemAChance"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::WAIT_TIME_GREEN;
        tmpl.include_countdown = false;
        tmpl.include_wait_time = true;
        tmpl.text_overlay_template = "Waiting: {wait_time}";

        tmpl.optimal_post_hours = {9, 17, 20};
        tmpl.auto_generate_enabled = true;
        tmpl.priority = 80;

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }

    // ========================================================================
    // OVERLOOKED_ANGEL Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_overlooked_angel";
        tmpl.name = "Overlooked Angel";
        tmpl.type = TikTokTemplateType::OVERLOOKED_ANGEL;
        tmpl.description = "For dogs often overlooked (seniors, black dogs, pit bulls, etc.)";

        tmpl.caption_template =
            "Meet {dog_name} - the angel nobody notices.\n\n"
            "As a {age} {breed}, {dog_name} is often passed by for younger, 'cuter' dogs.\n\n"
            "But those who stop to meet them discover the most loving soul.\n\n"
            "Sometimes the best things come in unexpected packages.";

        tmpl.required_placeholders = {"dog_name", "age", "breed"};
        tmpl.optional_placeholders = {"shelter_name", "wait_time", "personality", "special_traits"};

        tmpl.default_hashtags = {
            "SeniorDogs", "BlackDogsMatter", "OverlookedAngel", "AdoptDontShop",
            "ShelterDog", "RescueDog", "DogsOfTikTok"
        };
        tmpl.optional_hashtags = {
            "SeniorDogLove", "PitBullsAreFamily", "GentleGiant", "HiddenGem"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::HEARTWARMING;
        tmpl.include_countdown = false;
        tmpl.include_wait_time = true;

        tmpl.optimal_post_hours = {10, 14, 19};
        tmpl.auto_generate_enabled = true;
        tmpl.priority = 70;

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }

    // ========================================================================
    // FOSTER_PLEA Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_foster_plea";
        tmpl.name = "Foster Plea";
        tmpl.type = TikTokTemplateType::FOSTER_PLEA;
        tmpl.description = "Request for foster homes. Makes fostering approachable.";

        tmpl.caption_template =
            "Can you give {dog_name} a couch to crash on?\n\n"
            "Fostering isn't forever - it's just until they find their family.\n\n"
            "You provide: a safe space, love, and a few weeks.\n"
            "We provide: all supplies, vet care, and support.\n\n"
            "{dog_name} is 14x more likely to be adopted from a foster home.\n\n"
            "Be a hero - foster saves lives!";

        tmpl.required_placeholders = {"dog_name"};
        tmpl.optional_placeholders = {"shelter_name", "breed", "age", "location", "foster_needs"};

        tmpl.default_hashtags = {
            "FosterSavesLives", "FosterDog", "FosterToAdopt", "AdoptDontShop",
            "ShelterDog", "RescueDog", "DogsOfTikTok"
        };
        tmpl.optional_hashtags = {
            "FosterFamily", "TemporaryLove", "BeAHero", "FosterMe"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::FOSTER_APPEAL;
        tmpl.include_countdown = false;
        tmpl.include_wait_time = false;
        tmpl.text_overlay_template = "FOSTER NEEDED";

        tmpl.optimal_post_hours = {8, 12, 18};
        tmpl.auto_generate_enabled = true;
        tmpl.priority = 75;

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }

    // ========================================================================
    // HAPPY_UPDATE Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_happy_update";
        tmpl.name = "Happy Update";
        tmpl.type = TikTokTemplateType::HAPPY_UPDATE;
        tmpl.description = "Success stories and adoption updates. Celebrates wins!";

        tmpl.caption_template =
            "UPDATE on {dog_name}!\n\n"
            "After waiting {wait_time}, {dog_name} finally found their forever home!\n\n"
            "Thank you to everyone who shared, fostered, and supported.\n\n"
            "This is why we do what we do. Every share matters.";

        tmpl.required_placeholders = {"dog_name"};
        tmpl.optional_placeholders = {"wait_time", "shelter_name", "adopter_message", "update_details"};

        tmpl.default_hashtags = {
            "HappyTail", "Adopted", "RescueSuccess", "HappyEnding",
            "AdoptDontShop", "RescueDog", "DogsOfTikTok"
        };
        tmpl.optional_hashtags = {
            "ForeverHome", "AdoptedDog", "RescueStory", "HappyDog"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::HEARTWARMING;
        tmpl.include_countdown = false;
        tmpl.include_wait_time = false;
        tmpl.text_overlay_template = "ADOPTED!";

        tmpl.optimal_post_hours = {12, 19, 21};
        tmpl.auto_generate_enabled = false;  // Manual only
        tmpl.priority = 50;

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }

    // ========================================================================
    // EDUCATIONAL Template
    // ========================================================================
    {
        TikTokTemplate tmpl;
        tmpl.id = "default_educational";
        tmpl.name = "Educational";
        tmpl.type = TikTokTemplateType::EDUCATIONAL;
        tmpl.description = "Educational content about adoption, shelters, and pet care.";

        tmpl.caption_template =
            "Did you know?\n\n"
            "{educational_content}\n\n"
            "Learn more about how you can help shelter dogs find homes.\n\n"
            "Every action counts!";

        tmpl.required_placeholders = {"educational_content"};
        tmpl.optional_placeholders = {"stat", "tip", "shelter_name"};

        tmpl.default_hashtags = {
            "AdoptDontShop", "ShelterDogs", "PetEducation", "AnimalWelfare",
            "DogsOfTikTok", "RescueDog", "LearnOnTikTok"
        };
        tmpl.optional_hashtags = {
            "DogFacts", "ShelterLife", "HowToHelp", "DidYouKnow"
        };
        tmpl.max_hashtags = 10;

        tmpl.overlay_style = OverlayStyle::INFORMATIVE;
        tmpl.include_countdown = false;
        tmpl.include_wait_time = false;

        tmpl.optimal_post_hours = {11, 15, 20};
        tmpl.auto_generate_enabled = false;  // Manual only
        tmpl.priority = 30;

        templates_[tmpl.id] = tmpl;
        type_index_[tmpl.type].push_back(tmpl.id);
    }
}

bool TemplateManager::isInitialized() const {
    return initialized_;
}

int TemplateManager::loadFromJson(const Json::Value& json) {
    int count = 0;

    if (json.isMember("templates") && json["templates"].isArray()) {
        for (const auto& tmpl_json : json["templates"]) {
            try {
                TikTokTemplate tmpl = TikTokTemplate::fromJson(tmpl_json);

                if (!tmpl.id.empty()) {
                    // Remove from old type index if updating
                    auto it = templates_.find(tmpl.id);
                    if (it != templates_.end()) {
                        auto& old_index = type_index_[it->second.type];
                        old_index.erase(
                            std::remove(old_index.begin(), old_index.end(), tmpl.id),
                            old_index.end());
                    }

                    templates_[tmpl.id] = tmpl;
                    type_index_[tmpl.type].push_back(tmpl.id);
                    count++;
                }
            } catch (const std::exception& e) {
                WTL_CAPTURE_WARNING(
                    wtl::core::debug::ErrorCategory::CONFIGURATION,
                    "Failed to parse template: " + std::string(e.what()),
                    {});
            }
        }
    }

    return count;
}

std::optional<TikTokTemplate> TemplateManager::getTemplate(TikTokTemplateType type) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = type_index_.find(type);
    if (it == type_index_.end() || it->second.empty()) {
        return std::nullopt;
    }

    // Return first template of this type (highest priority if sorted)
    const std::string& id = it->second[0];
    auto tmpl_it = templates_.find(id);
    if (tmpl_it != templates_.end()) {
        return tmpl_it->second;
    }

    return std::nullopt;
}

std::optional<TikTokTemplate> TemplateManager::getTemplateById(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = templates_.find(id);
    if (it != templates_.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<TikTokTemplate> TemplateManager::getTemplatesByType(TikTokTemplateType type) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<TikTokTemplate> result;

    auto it = type_index_.find(type);
    if (it != type_index_.end()) {
        for (const auto& id : it->second) {
            auto tmpl_it = templates_.find(id);
            if (tmpl_it != templates_.end()) {
                result.push_back(tmpl_it->second);
            }
        }
    }

    return result;
}

std::vector<TikTokTemplate> TemplateManager::getAllTemplates() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<TikTokTemplate> result;
    result.reserve(templates_.size());

    for (const auto& [id, tmpl] : templates_) {
        result.push_back(tmpl);
    }

    // Sort by priority (highest first)
    std::sort(result.begin(), result.end(),
        [](const TikTokTemplate& a, const TikTokTemplate& b) {
            return a.priority > b.priority;
        });

    return result;
}

std::vector<TikTokTemplate> TemplateManager::getAutoGenerateTemplates() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<TikTokTemplate> result;

    for (const auto& [id, tmpl] : templates_) {
        if (tmpl.auto_generate_enabled) {
            result.push_back(tmpl);
        }
    }

    // Sort by priority
    std::sort(result.begin(), result.end(),
        [](const TikTokTemplate& a, const TikTokTemplate& b) {
            return a.priority > b.priority;
        });

    return result;
}

bool TemplateManager::registerTemplate(const TikTokTemplate& tmpl) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (templates_.find(tmpl.id) != templates_.end()) {
        return false;  // Already exists
    }

    templates_[tmpl.id] = tmpl;
    type_index_[tmpl.type].push_back(tmpl.id);

    return true;
}

bool TemplateManager::updateTemplate(const TikTokTemplate& tmpl) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = templates_.find(tmpl.id);
    if (it == templates_.end()) {
        return false;  // Not found
    }

    // Remove from old type index if type changed
    if (it->second.type != tmpl.type) {
        auto& old_index = type_index_[it->second.type];
        old_index.erase(
            std::remove(old_index.begin(), old_index.end(), tmpl.id),
            old_index.end());

        type_index_[tmpl.type].push_back(tmpl.id);
    }

    templates_[tmpl.id] = tmpl;

    return true;
}

bool TemplateManager::removeTemplate(const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = templates_.find(id);
    if (it == templates_.end()) {
        return false;
    }

    // Remove from type index
    auto& index = type_index_[it->second.type];
    index.erase(std::remove(index.begin(), index.end(), id), index.end());

    templates_.erase(it);

    return true;
}

bool TemplateManager::reload() {
    if (config_path_.empty()) {
        loadDefaults();
        return true;
    }

    return initialize(config_path_);
}

std::vector<int> TemplateManager::getOptimalPostHours(TikTokTemplateType type) const {
    auto tmpl = getTemplate(type);
    if (tmpl) {
        return tmpl->optimal_post_hours;
    }

    // Default optimal hours
    return {9, 12, 17, 20};
}

TikTokTemplateType TemplateManager::recommendTemplateType(
    int days_until_euthanasia,
    int wait_days,
    bool is_overlooked) const {

    // Critical urgency takes precedence
    if (days_until_euthanasia >= 0 && days_until_euthanasia <= 3) {
        return TikTokTemplateType::URGENT_COUNTDOWN;
    }

    // High urgency
    if (days_until_euthanasia >= 0 && days_until_euthanasia <= 7) {
        return TikTokTemplateType::URGENT_COUNTDOWN;
    }

    // Long wait time
    if (wait_days >= 180) {  // 6+ months
        return TikTokTemplateType::LONGEST_WAITING;
    }

    // Overlooked dogs
    if (is_overlooked) {
        return TikTokTemplateType::OVERLOOKED_ANGEL;
    }

    // Moderate wait time
    if (wait_days >= 90) {  // 3+ months
        return TikTokTemplateType::LONGEST_WAITING;
    }

    // Default to foster plea for remaining dogs
    return TikTokTemplateType::FOSTER_PLEA;
}

Json::Value TemplateManager::exportToJson() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    Json::Value root;
    Json::Value templates_array(Json::arrayValue);

    for (const auto& [id, tmpl] : templates_) {
        templates_array.append(tmpl.toJson());
    }

    root["templates"] = templates_array;

    return root;
}

} // namespace wtl::content::tiktok

/**
 * @file AIMediaModule.cc
 * @brief Implementation of the AIMediaModule
 * @see AIMediaModule.h for documentation
 */

#include "AIMediaModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>

namespace wtl::modules::intervention {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string contentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::DESCRIPTION: return "description";
        case ContentType::SHORT_BIO: return "short_bio";
        case ContentType::SOCIAL_CAPTION: return "social_caption";
        case ContentType::ADOPTION_APPEAL: return "adoption_appeal";
        case ContentType::VIDEO_SCRIPT: return "video_script";
        case ContentType::HASHTAGS: return "hashtags";
        case ContentType::ALT_TEXT: return "alt_text";
        default: return "unknown";
    }
}

ContentType stringToContentType(const std::string& str) {
    if (str == "description") return ContentType::DESCRIPTION;
    if (str == "short_bio") return ContentType::SHORT_BIO;
    if (str == "social_caption") return ContentType::SOCIAL_CAPTION;
    if (str == "adoption_appeal") return ContentType::ADOPTION_APPEAL;
    if (str == "video_script") return ContentType::VIDEO_SCRIPT;
    if (str == "hashtags") return ContentType::HASHTAGS;
    if (str == "alt_text") return ContentType::ALT_TEXT;
    return ContentType::DESCRIPTION;
}

std::string captionStyleToString(CaptionStyle style) {
    switch (style) {
        case CaptionStyle::EMOTIONAL: return "emotional";
        case CaptionStyle::FUNNY: return "funny";
        case CaptionStyle::INFORMATIVE: return "informative";
        case CaptionStyle::URGENT: return "urgent";
        case CaptionStyle::STORYTELLING: return "storytelling";
        default: return "emotional";
    }
}

CaptionStyle stringToCaptionStyle(const std::string& str) {
    if (str == "emotional") return CaptionStyle::EMOTIONAL;
    if (str == "funny") return CaptionStyle::FUNNY;
    if (str == "informative") return CaptionStyle::INFORMATIVE;
    if (str == "urgent") return CaptionStyle::URGENT;
    if (str == "storytelling") return CaptionStyle::STORYTELLING;
    return CaptionStyle::EMOTIONAL;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool AIMediaModule::initialize() {
    std::cout << "[AIMediaModule] Initializing..." << std::endl;

    // Load templates for fallback generation
    loadTemplates();

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto created_id = event_bus.subscribe(
        core::EventType::DOG_CREATED,
        [this](const core::Event& event) {
            handleDogCreated(event);
        },
        "ai_media_dog_created_handler"
    );
    subscription_ids_.push_back(created_id);

    auto urgent_id = event_bus.subscribe(
        core::EventType::DOG_BECAME_CRITICAL,
        [this](const core::Event& event) {
            handleDogBecameUrgent(event);
        },
        "ai_media_urgent_handler"
    );
    subscription_ids_.push_back(urgent_id);

    auto request_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "content_generation_request") {
                handleContentRequest(event);
            }
        },
        "ai_media_request_handler"
    );
    subscription_ids_.push_back(request_id);

    enabled_ = true;
    std::cout << "[AIMediaModule] Initialization complete." << std::endl;
    std::cout << "[AIMediaModule] Auto-generate descriptions: "
              << (auto_generate_descriptions_ ? "enabled" : "disabled") << std::endl;
    std::cout << "[AIMediaModule] Auto-generate urgent appeals: "
              << (auto_generate_urgent_appeals_ ? "enabled" : "disabled") << std::endl;

    return true;
}

void AIMediaModule::shutdown() {
    std::cout << "[AIMediaModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[AIMediaModule] Shutdown complete." << std::endl;
}

void AIMediaModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool AIMediaModule::isHealthy() const {
    return enabled_;
}

Json::Value AIMediaModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["auto_generate_descriptions"] = auto_generate_descriptions_;
    status["auto_generate_urgent_appeals"] = auto_generate_urgent_appeals_;

    Json::Value styles(Json::arrayValue);
    for (const auto& style : enabled_caption_styles_) {
        styles.append(captionStyleToString(style));
    }
    status["caption_styles"] = styles;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["content_queue_size"] = static_cast<int>(content_queue_.size());
    status["generated_content_count"] = static_cast<int>(generated_content_.size());

    return status;
}

Json::Value AIMediaModule::getMetrics() const {
    Json::Value metrics;
    metrics["descriptions_generated"] = static_cast<Json::Int64>(descriptions_generated_.load());
    metrics["captions_generated"] = static_cast<Json::Int64>(captions_generated_.load());
    metrics["appeals_generated"] = static_cast<Json::Int64>(appeals_generated_.load());
    metrics["scripts_generated"] = static_cast<Json::Int64>(scripts_generated_.load());
    metrics["content_approved"] = static_cast<Json::Int64>(content_approved_.load());
    metrics["total_views"] = static_cast<Json::Int64>(total_views_.load());
    metrics["total_engagements"] = static_cast<Json::Int64>(total_engagements_.load());

    // Calculate engagement rate
    int64_t views = total_views_.load();
    if (views > 0) {
        metrics["engagement_rate"] = static_cast<double>(total_engagements_.load()) / views * 100.0;
    } else {
        metrics["engagement_rate"] = 0.0;
    }

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AIMediaModule::configure(const Json::Value& config) {
    if (config.isMember("auto_generate_descriptions")) {
        auto_generate_descriptions_ = config["auto_generate_descriptions"].asBool();
    }
    if (config.isMember("auto_generate_urgent_appeals")) {
        auto_generate_urgent_appeals_ = config["auto_generate_urgent_appeals"].asBool();
    }
    if (config.isMember("ai_service_url")) {
        ai_service_url_ = config["ai_service_url"].asString();
    }
    if (config.isMember("ai_api_key")) {
        ai_api_key_ = config["ai_api_key"].asString();
    }
    if (config.isMember("social_caption_styles") && config["social_caption_styles"].isArray()) {
        enabled_caption_styles_.clear();
        for (const auto& style : config["social_caption_styles"]) {
            enabled_caption_styles_.push_back(stringToCaptionStyle(style.asString()));
        }
    }
}

Json::Value AIMediaModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["auto_generate_descriptions"] = true;
    config["auto_generate_urgent_appeals"] = true;
    config["ai_service_url"] = "";
    config["ai_api_key"] = "";

    Json::Value styles(Json::arrayValue);
    styles.append("emotional");
    styles.append("funny");
    styles.append("informative");
    config["social_caption_styles"] = styles;

    return config;
}

// ============================================================================
// CONTENT GENERATION API
// ============================================================================

GeneratedContent AIMediaModule::generateDescription(const DogProfile& profile) {
    GeneratedContent content;
    content.id = generateContentId();
    content.dog_id = profile.id;
    content.type = ContentType::DESCRIPTION;
    content.approved = false;
    content.engagement_score = 0;

    // Generate using template (fallback) or AI
    content.content = generateFromTemplate(profile, ContentType::DESCRIPTION);

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    content.generated_at = ss.str();

    // Store
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        generated_content_[content.id] = content;
        dog_content_map_[profile.id].push_back(content.id);
    }

    descriptions_generated_++;

    std::cout << "[AIMediaModule] Generated description for " << profile.name << std::endl;

    return content;
}

GeneratedContent AIMediaModule::generateShortBio(const DogProfile& profile, int max_length) {
    GeneratedContent content;
    content.id = generateContentId();
    content.dog_id = profile.id;
    content.type = ContentType::SHORT_BIO;
    content.approved = false;
    content.engagement_score = 0;

    // Generate short bio
    std::stringstream bio;
    bio << profile.name << " is a ";
    if (profile.age_years > 0) {
        bio << profile.age_years << "-year-old ";
    } else {
        bio << profile.age_months << "-month-old ";
    }
    bio << profile.gender << " " << profile.breed << " ";

    if (!profile.traits.empty()) {
        bio << "who is " << profile.traits[0];
        if (profile.traits.size() > 1) {
            bio << " and " << profile.traits[1];
        }
    }
    bio << ". Waiting " << profile.wait_time_days << " days for a home.";

    content.content = bio.str();

    // Truncate if needed
    if (content.content.length() > static_cast<size_t>(max_length)) {
        content.content = content.content.substr(0, max_length - 3) + "...";
    }

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    content.generated_at = ss.str();

    // Store
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        generated_content_[content.id] = content;
        dog_content_map_[profile.id].push_back(content.id);
    }

    return content;
}

GeneratedContent AIMediaModule::generateSocialCaption(const DogProfile& profile,
                                                        CaptionStyle style,
                                                        const std::string& platform) {
    GeneratedContent content;
    content.id = generateContentId();
    content.dog_id = profile.id;
    content.type = ContentType::SOCIAL_CAPTION;
    content.style = style;
    content.approved = false;
    content.engagement_score = 0;

    // Generate based on style
    std::stringstream caption;

    switch (style) {
        case CaptionStyle::EMOTIONAL:
            caption << "Meet " << profile.name << ", who has been waiting "
                    << profile.wait_time_days << " days for someone to see "
                    << (profile.gender == "male" ? "his" : "her") << " worth. ";
            if (profile.is_urgent) {
                caption << "Time is running out. ";
            }
            caption << "Every dog deserves love. Could you be "
                    << (profile.gender == "male" ? "his" : "her") << " person?";
            break;

        case CaptionStyle::FUNNY:
            caption << profile.name << " here! I've been practicing my cute face for "
                    << profile.wait_time_days << " days now. ";
            if (!profile.traits.empty()) {
                caption << "I'm told I'm very " << profile.traits[0] << ". ";
            }
            caption << "Paws-itively ready for adoption!";
            break;

        case CaptionStyle::INFORMATIVE:
            caption << "Adoption spotlight: " << profile.name << "\n";
            caption << "Breed: " << profile.breed << "\n";
            caption << "Age: " << profile.age_years << " years\n";
            caption << "Size: " << profile.size << "\n";
            caption << "At " << profile.shelter_name << " for " << profile.wait_time_days << " days\n";
            caption << "Contact the shelter to meet " << (profile.gender == "male" ? "him" : "her") << "!";
            break;

        case CaptionStyle::URGENT:
            caption << "URGENT: " << profile.name << " needs your help NOW!\n";
            if (profile.days_until_euthanasia > 0) {
                caption << "Only " << profile.days_until_euthanasia << " days left.\n";
            }
            caption << "This beautiful " << profile.breed << " has been waiting "
                    << profile.wait_time_days << " days.\n";
            caption << "Please share to help save " << (profile.gender == "male" ? "his" : "her") << " life!";
            break;

        case CaptionStyle::STORYTELLING:
            caption << "Day " << profile.wait_time_days << " at the shelter.\n\n";
            caption << profile.name << " wakes up each morning hoping today is the day "
                    << (profile.gender == "male" ? "he" : "she") << " finds a family.\n\n";
            caption << "A " << profile.breed << " with so much love to give, still waiting "
                    << "for someone to take " << (profile.gender == "male" ? "him" : "her")
                    << " home.";
            break;
    }

    content.content = formatForPlatform(caption.str(), platform);
    content.hashtags = generateHashtags(profile, 5);

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    content.generated_at = ss.str();

    // Store
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        generated_content_[content.id] = content;
        dog_content_map_[profile.id].push_back(content.id);
    }

    captions_generated_++;

    return content;
}

GeneratedContent AIMediaModule::generateAdoptionAppeal(const DogProfile& profile) {
    GeneratedContent content;
    content.id = generateContentId();
    content.dog_id = profile.id;
    content.type = ContentType::ADOPTION_APPEAL;
    content.approved = false;
    content.engagement_score = 0;

    std::stringstream appeal;
    appeal << "PLEASE SHARE - " << profile.name << " NEEDS YOUR HELP!\n\n";

    appeal << profile.name << " is a beautiful " << profile.age_years << "-year-old "
           << profile.breed << " who has been waiting at " << profile.shelter_name
           << " in " << profile.shelter_city << ", " << profile.shelter_state
           << " for " << profile.wait_time_days << " days.\n\n";

    if (profile.is_urgent) {
        appeal << "THIS IS URGENT - ";
        if (profile.days_until_euthanasia > 0) {
            appeal << profile.name << " is scheduled to be euthanized in just "
                   << profile.days_until_euthanasia << " days if no one steps forward.\n\n";
        } else {
            appeal << profile.name << " is at a kill shelter and running out of time.\n\n";
        }
    }

    if (!profile.traits.empty()) {
        appeal << "This sweet pup is known for being ";
        for (size_t i = 0; i < profile.traits.size(); ++i) {
            if (i > 0) appeal << (i == profile.traits.size() - 1 ? " and " : ", ");
            appeal << profile.traits[i];
        }
        appeal << ".\n\n";
    }

    appeal << "Good with dogs: " << (profile.good_with_dogs ? "Yes" : "Unknown") << "\n";
    appeal << "Good with cats: " << (profile.good_with_cats ? "Yes" : "Unknown") << "\n";
    appeal << "Good with kids: " << (profile.good_with_kids ? "Yes" : "Unknown") << "\n\n";

    appeal << "If you can adopt, foster, rescue, or share, please do. Every share helps!\n";
    appeal << "Contact " << profile.shelter_name << " to save " << profile.name << " today!";

    content.content = appeal.str();
    content.hashtags = generateHashtags(profile, 10);

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    content.generated_at = ss.str();

    // Store
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        generated_content_[content.id] = content;
        dog_content_map_[profile.id].push_back(content.id);
    }

    appeals_generated_++;

    std::cout << "[AIMediaModule] Generated adoption appeal for " << profile.name << std::endl;

    return content;
}

GeneratedContent AIMediaModule::generateVideoScript(const DogProfile& profile,
                                                      int duration_seconds) {
    GeneratedContent content;
    content.id = generateContentId();
    content.dog_id = profile.id;
    content.type = ContentType::VIDEO_SCRIPT;
    content.approved = false;
    content.engagement_score = 0;

    std::stringstream script;
    script << "[VIDEO SCRIPT - " << duration_seconds << " seconds]\n\n";

    script << "[0-5s] HOOK:\n";
    script << "Text overlay: \"Day " << profile.wait_time_days << " at the shelter\"\n";
    script << "Show " << profile.name << " looking at camera\n\n";

    script << "[5-15s] INTRODUCTION:\n";
    script << "Text: \"Meet " << profile.name << "\"\n";
    script << "\"A " << profile.age_years << "-year-old " << profile.breed << "\"\n";
    script << "Show " << profile.name << " playing or being cute\n\n";

    script << "[15-25s] PERSONALITY:\n";
    if (!profile.traits.empty()) {
        script << "Text: \"";
        for (size_t i = 0; i < std::min(profile.traits.size(), size_t(3)); ++i) {
            if (i > 0) script << " | ";
            script << profile.traits[i];
        }
        script << "\"\n";
    }
    script << "Show " << profile.name << " demonstrating personality\n\n";

    script << "[25-" << duration_seconds << "s] CALL TO ACTION:\n";
    if (profile.is_urgent) {
        script << "Text: \"TIME IS RUNNING OUT\"\n";
    }
    script << "Text: \"" << profile.shelter_name << "\"\n";
    script << "Text: \"" << profile.shelter_city << ", " << profile.shelter_state << "\"\n";
    script << "Final shot of " << profile.name << " with hopeful expression\n";

    content.content = script.str();
    content.hashtags = generateHashtags(profile, 8);

    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    content.generated_at = ss.str();

    // Store
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        generated_content_[content.id] = content;
        dog_content_map_[profile.id].push_back(content.id);
    }

    scripts_generated_++;

    return content;
}

std::vector<std::string> AIMediaModule::generateHashtags(const DogProfile& profile, int count) {
    std::vector<std::string> hashtags;

    // Common hashtags
    hashtags.push_back("#AdoptDontShop");
    hashtags.push_back("#RescueDog");
    hashtags.push_back("#ShelterDog");
    hashtags.push_back("#SaveALife");

    // Breed-specific
    std::string breed_tag = "#" + profile.breed;
    std::replace(breed_tag.begin(), breed_tag.end(), ' ', '_');
    hashtags.push_back(breed_tag);
    hashtags.push_back(breed_tag + "sOfInstagram");

    // Location
    hashtags.push_back("#" + profile.shelter_state + "Dogs");
    hashtags.push_back("#" + profile.shelter_city + "Rescue");

    // Urgency
    if (profile.is_urgent) {
        hashtags.push_back("#UrgentDogs");
        hashtags.push_back("#DeathRow");
        hashtags.push_back("#PleaseShare");
    }

    // General
    hashtags.push_back("#DogsOfInstagram");
    hashtags.push_back("#AdoptMe");

    // Limit to requested count
    if (hashtags.size() > static_cast<size_t>(count)) {
        hashtags.resize(count);
    }

    return hashtags;
}

std::string AIMediaModule::generateAltText(const DogProfile& profile,
                                            const std::string& photo_context) {
    std::stringstream alt;
    alt << "Photo of " << profile.name << ", a " << profile.color << " "
        << profile.breed << " dog";

    if (!photo_context.empty()) {
        alt << " " << photo_context;
    }

    alt << ". " << profile.name << " is available for adoption at "
        << profile.shelter_name << ".";

    return alt.str();
}

std::string AIMediaModule::queueContentRequest(const ContentRequest& request) {
    ContentRequest new_request = request;
    new_request.id = generateContentId();
    new_request.status = "pending";

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_request.created_at = ss.str();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        content_queue_.push(new_request);
    }

    return new_request.id;
}

std::optional<GeneratedContent> AIMediaModule::getGeneratedContent(const std::string& content_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = generated_content_.find(content_id);
    if (it != generated_content_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<GeneratedContent> AIMediaModule::getContentForDog(const std::string& dog_id) {
    std::vector<GeneratedContent> content;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = dog_content_map_.find(dog_id);
    if (it != dog_content_map_.end()) {
        for (const auto& id : it->second) {
            auto content_it = generated_content_.find(id);
            if (content_it != generated_content_.end()) {
                content.push_back(content_it->second);
            }
        }
    }

    return content;
}

bool AIMediaModule::approveContent(const std::string& content_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = generated_content_.find(content_id);
    if (it != generated_content_.end()) {
        it->second.approved = true;
        content_approved_++;
        return true;
    }
    return false;
}

void AIMediaModule::recordEngagement(const std::string& content_id,
                                      const std::string& engagement_type) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = generated_content_.find(content_id);
    if (it != generated_content_.end()) {
        it->second.engagement_score++;

        if (engagement_type == "view") {
            total_views_++;
        } else {
            total_engagements_++;
        }
    }
}

Json::Value AIMediaModule::getContentStats() {
    Json::Value stats;
    stats["descriptions_generated"] = static_cast<Json::Int64>(descriptions_generated_.load());
    stats["captions_generated"] = static_cast<Json::Int64>(captions_generated_.load());
    stats["appeals_generated"] = static_cast<Json::Int64>(appeals_generated_.load());
    stats["scripts_generated"] = static_cast<Json::Int64>(scripts_generated_.load());
    stats["content_approved"] = static_cast<Json::Int64>(content_approved_.load());
    stats["total_views"] = static_cast<Json::Int64>(total_views_.load());
    stats["total_engagements"] = static_cast<Json::Int64>(total_engagements_.load());

    return stats;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void AIMediaModule::handleDogCreated(const core::Event& event) {
    if (!auto_generate_descriptions_) return;

    DogProfile profile;
    profile.id = event.data.get("dog_id", "").asString();
    profile.name = event.data.get("name", "").asString();
    profile.breed = event.data.get("breed", "Mixed").asString();
    profile.age_years = event.data.get("age_years", 1).asInt();
    profile.gender = event.data.get("gender", "unknown").asString();

    if (!profile.id.empty() && !profile.name.empty()) {
        generateDescription(profile);
    }
}

void AIMediaModule::handleDogBecameUrgent(const core::Event& event) {
    if (!auto_generate_urgent_appeals_) return;

    DogProfile profile;
    profile.id = event.data.get("dog_id", "").asString();
    profile.name = event.data.get("name", "").asString();
    profile.breed = event.data.get("breed", "Mixed").asString();
    profile.age_years = event.data.get("age_years", 1).asInt();
    profile.gender = event.data.get("gender", "unknown").asString();
    profile.is_urgent = true;
    profile.days_until_euthanasia = event.data.get("days_until_euthanasia", 7).asInt();
    profile.shelter_name = event.data.get("shelter_name", "").asString();
    profile.shelter_city = event.data.get("shelter_city", "").asString();
    profile.shelter_state = event.data.get("shelter_state", "").asString();

    if (!profile.id.empty() && !profile.name.empty()) {
        generateAdoptionAppeal(profile);
    }
}

void AIMediaModule::handleContentRequest(const core::Event& event) {
    ContentRequest request;
    request.dog_id = event.data.get("dog_id", "").asString();
    request.type = stringToContentType(event.data.get("content_type", "description").asString());
    request.style = stringToCaptionStyle(event.data.get("style", "emotional").asString());
    request.platform = event.data.get("platform", "instagram").asString();

    if (!request.dog_id.empty()) {
        queueContentRequest(request);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

std::string AIMediaModule::generateContentId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "CNT-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

void AIMediaModule::loadTemplates() {
    // Description templates
    content_templates_[ContentType::DESCRIPTION] = {
        "{name} is a {age}-year-old {breed} who has been waiting {wait_days} days for a forever home. "
        "{pronoun_cap} is known for being {traits}. {name} would thrive in a home with {good_with}. "
        "Don't let {pronoun} wait any longer - {name} is ready to fill your life with love!",

        "Say hello to {name}! This wonderful {age}-year-old {breed} has been at {shelter} for {wait_days} days. "
        "{name} is {traits} and looking for a family to call {pronoun} own. "
        "Could you be the one to give {name} the second chance {pronoun} deserves?",

        "Meet {name}, a {traits} {breed} who has been patiently waiting {wait_days} days for someone special. "
        "At {age} years old, {pronoun} still has so much love to give. {name} is located at {shelter} "
        "and ready to meet you today!"
    };

    std::cout << "[AIMediaModule] Loaded " << content_templates_.size()
              << " content template categories" << std::endl;
}

std::string AIMediaModule::generateFromTemplate(const DogProfile& profile, ContentType type) {
    auto it = content_templates_.find(type);
    if (it == content_templates_.end() || it->second.empty()) {
        return "No template available for this content type.";
    }

    // Select random template
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(it->second.size()) - 1);

    std::string template_str = it->second[dis(gen)];
    return fillTemplate(template_str, profile);
}

std::string AIMediaModule::fillTemplate(const std::string& template_str, const DogProfile& profile) {
    std::string result = template_str;

    // Replace placeholders
    auto replace_all = [&result](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replace_all("{name}", profile.name);
    replace_all("{breed}", profile.breed);
    replace_all("{age}", std::to_string(profile.age_years));
    replace_all("{wait_days}", std::to_string(profile.wait_time_days));
    replace_all("{shelter}", profile.shelter_name);

    // Pronouns
    std::string pronoun = (profile.gender == "male") ? "him" : "her";
    std::string pronoun_cap = (profile.gender == "male") ? "He" : "She";
    replace_all("{pronoun}", pronoun);
    replace_all("{pronoun_cap}", pronoun_cap);

    // Traits
    std::string traits_str = profile.traits.empty() ? "wonderful" : profile.traits[0];
    if (profile.traits.size() > 1) {
        traits_str += " and " + profile.traits[1];
    }
    replace_all("{traits}", traits_str);

    // Good with
    std::vector<std::string> good_with;
    if (profile.good_with_dogs) good_with.push_back("other dogs");
    if (profile.good_with_cats) good_with.push_back("cats");
    if (profile.good_with_kids) good_with.push_back("children");
    std::string good_with_str = good_with.empty() ? "a loving family" :
        good_with[0] + (good_with.size() > 1 ? " and " + good_with[1] : "");
    replace_all("{good_with}", good_with_str);

    return result;
}

std::string AIMediaModule::formatForPlatform(const std::string& content,
                                              const std::string& platform) {
    std::string formatted = content;

    // Platform-specific formatting
    if (platform == "twitter") {
        // Truncate to 280 characters
        if (formatted.length() > 280) {
            formatted = formatted.substr(0, 277) + "...";
        }
    } else if (platform == "tiktok") {
        // Keep shorter for TikTok
        if (formatted.length() > 150) {
            formatted = formatted.substr(0, 147) + "...";
        }
    }

    return formatted;
}

} // namespace wtl::modules::intervention

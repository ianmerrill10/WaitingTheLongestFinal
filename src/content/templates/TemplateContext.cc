/**
 * @file TemplateContext.cc
 * @brief Implementation of TemplateContext and related structs
 * @see TemplateContext.h for documentation
 */

#include "content/templates/TemplateContext.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace wtl::content::templates {

// ============================================================================
// WAIT TIME COMPONENTS IMPLEMENTATION
// ============================================================================

std::string WaitTimeComponents::formatted() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << years << ":"
        << std::setw(2) << months << ":"
        << std::setw(2) << days << ":"
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds;
    return oss.str();
}

std::string WaitTimeComponents::humanReadable() const {
    std::vector<std::string> parts;

    if (years > 0) {
        parts.push_back(std::to_string(years) + (years == 1 ? " year" : " years"));
    }
    if (months > 0) {
        parts.push_back(std::to_string(months) + (months == 1 ? " month" : " months"));
    }
    if (days > 0 || parts.empty()) {
        parts.push_back(std::to_string(days) + (days == 1 ? " day" : " days"));
    }

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            if (i == parts.size() - 1) {
                result += " and ";
            } else {
                result += ", ";
            }
        }
        result += parts[i];
    }

    return result;
}

int64_t WaitTimeComponents::totalSeconds() const {
    int64_t total = seconds;
    total += static_cast<int64_t>(minutes) * 60;
    total += static_cast<int64_t>(hours) * 3600;
    total += static_cast<int64_t>(days) * 86400;
    total += static_cast<int64_t>(months) * 30 * 86400;  // Approximate
    total += static_cast<int64_t>(years) * 365 * 86400;  // Approximate
    return total;
}

Json::Value WaitTimeComponents::toJson() const {
    Json::Value json;
    json["years"] = years;
    json["months"] = months;
    json["days"] = days;
    json["hours"] = hours;
    json["minutes"] = minutes;
    json["seconds"] = seconds;
    json["formatted"] = formatted();
    json["human_readable"] = humanReadable();
    json["total_seconds"] = static_cast<Json::Int64>(totalSeconds());
    return json;
}

WaitTimeComponents WaitTimeComponents::fromJson(const Json::Value& json) {
    WaitTimeComponents result;
    result.years = json.get("years", 0).asInt();
    result.months = json.get("months", 0).asInt();
    result.days = json.get("days", 0).asInt();
    result.hours = json.get("hours", 0).asInt();
    result.minutes = json.get("minutes", 0).asInt();
    result.seconds = json.get("seconds", 0).asInt();
    return result;
}

WaitTimeComponents WaitTimeComponents::fromSeconds(int64_t total_seconds) {
    WaitTimeComponents result;

    // Calculate years (approximate: 365 days)
    result.years = static_cast<int>(total_seconds / (365LL * 24 * 60 * 60));
    total_seconds %= (365LL * 24 * 60 * 60);

    // Calculate months (approximate: 30 days)
    result.months = static_cast<int>(total_seconds / (30LL * 24 * 60 * 60));
    total_seconds %= (30LL * 24 * 60 * 60);

    // Calculate days
    result.days = static_cast<int>(total_seconds / (24 * 60 * 60));
    total_seconds %= (24 * 60 * 60);

    // Calculate hours
    result.hours = static_cast<int>(total_seconds / (60 * 60));
    total_seconds %= (60 * 60);

    // Calculate minutes
    result.minutes = static_cast<int>(total_seconds / 60);

    // Remaining seconds
    result.seconds = static_cast<int>(total_seconds % 60);

    return result;
}

WaitTimeComponents WaitTimeComponents::fromIntakeDate(std::chrono::system_clock::time_point intake_date) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - intake_date);
    return fromSeconds(duration.count());
}

// ============================================================================
// COUNTDOWN COMPONENTS IMPLEMENTATION
// ============================================================================

std::string CountdownComponents::formatted() const {
    if (is_expired) {
        return "EXPIRED";
    }

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << days << ":"
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds;
    return oss.str();
}

std::string CountdownComponents::humanReadable() const {
    if (is_expired) {
        return "TIME EXPIRED";
    }

    std::vector<std::string> parts;

    if (days > 0) {
        parts.push_back(std::to_string(days) + (days == 1 ? " day" : " days"));
    }
    if (hours > 0 || !parts.empty()) {
        parts.push_back(std::to_string(hours) + (hours == 1 ? " hour" : " hours"));
    }
    if (parts.empty() || days == 0) {
        parts.push_back(std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes"));
    }

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            if (i == parts.size() - 1) {
                result += " and ";
            } else {
                result += ", ";
            }
        }
        result += parts[i];
    }

    result += " remaining";
    return result;
}

int64_t CountdownComponents::totalSeconds() const {
    if (is_expired) return 0;

    int64_t total = seconds;
    total += static_cast<int64_t>(minutes) * 60;
    total += static_cast<int64_t>(hours) * 3600;
    total += static_cast<int64_t>(days) * 86400;
    return total;
}

bool CountdownComponents::isCritical() const {
    return !is_expired && totalSeconds() < (24 * 60 * 60);  // Less than 24 hours
}

bool CountdownComponents::isHighUrgency() const {
    return !is_expired && totalSeconds() < (72 * 60 * 60);  // Less than 72 hours
}

std::string CountdownComponents::urgencyLevel() const {
    if (is_expired) {
        return "expired";
    }

    int64_t total = totalSeconds();

    if (total < 24 * 60 * 60) {
        return "critical";  // Less than 24 hours
    } else if (total < 72 * 60 * 60) {
        return "high";      // Less than 72 hours
    } else if (total < 7 * 24 * 60 * 60) {
        return "medium";    // Less than 7 days
    } else {
        return "normal";
    }
}

Json::Value CountdownComponents::toJson() const {
    Json::Value json;
    json["days"] = days;
    json["hours"] = hours;
    json["minutes"] = minutes;
    json["seconds"] = seconds;
    json["is_expired"] = is_expired;
    json["is_critical"] = isCritical();
    json["is_high_urgency"] = isHighUrgency();
    json["urgency_level"] = urgencyLevel();
    json["formatted"] = formatted();
    json["human_readable"] = humanReadable();
    json["total_seconds"] = static_cast<Json::Int64>(totalSeconds());
    return json;
}

CountdownComponents CountdownComponents::fromJson(const Json::Value& json) {
    CountdownComponents result;
    result.days = json.get("days", 0).asInt();
    result.hours = json.get("hours", 0).asInt();
    result.minutes = json.get("minutes", 0).asInt();
    result.seconds = json.get("seconds", 0).asInt();
    result.is_expired = json.get("is_expired", false).asBool();
    return result;
}

CountdownComponents CountdownComponents::fromSeconds(int64_t total_seconds) {
    CountdownComponents result;

    if (total_seconds <= 0) {
        result.is_expired = true;
        return result;
    }

    result.days = static_cast<int>(total_seconds / (24 * 60 * 60));
    total_seconds %= (24 * 60 * 60);

    result.hours = static_cast<int>(total_seconds / (60 * 60));
    total_seconds %= (60 * 60);

    result.minutes = static_cast<int>(total_seconds / 60);
    result.seconds = static_cast<int>(total_seconds % 60);

    return result;
}

CountdownComponents CountdownComponents::fromEuthanasiaDate(std::chrono::system_clock::time_point euthanasia_date) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(euthanasia_date - now);
    return fromSeconds(duration.count());
}

// ============================================================================
// TEMPLATE CONTEXT IMPLEMENTATION
// ============================================================================

TemplateContext::TemplateContext(const Json::Value& json)
    : context_(json) {}

TemplateContext::TemplateContext(const TemplateContext& other)
    : context_(other.context_) {}

TemplateContext::TemplateContext(TemplateContext&& other) noexcept
    : context_(std::move(other.context_)) {}

TemplateContext& TemplateContext::operator=(const TemplateContext& other) {
    if (this != &other) {
        context_ = other.context_;
    }
    return *this;
}

TemplateContext& TemplateContext::operator=(TemplateContext&& other) noexcept {
    if (this != &other) {
        context_ = std::move(other.context_);
    }
    return *this;
}

// ============================================================================
// GENERIC SETTERS
// ============================================================================

TemplateContext& TemplateContext::set(const std::string& key, const std::string& value) {
    context_[key] = value;
    return *this;
}

TemplateContext& TemplateContext::set(const std::string& key, int value) {
    context_[key] = value;
    return *this;
}

TemplateContext& TemplateContext::set(const std::string& key, int64_t value) {
    context_[key] = static_cast<Json::Int64>(value);
    return *this;
}

TemplateContext& TemplateContext::set(const std::string& key, double value) {
    context_[key] = value;
    return *this;
}

TemplateContext& TemplateContext::set(const std::string& key, bool value) {
    context_[key] = value;
    return *this;
}

TemplateContext& TemplateContext::set(const std::string& key, const Json::Value& value) {
    context_[key] = value;
    return *this;
}

TemplateContext& TemplateContext::setArray(const std::string& key, const std::vector<std::string>& values) {
    Json::Value arr(Json::arrayValue);
    for (const auto& v : values) {
        arr.append(v);
    }
    context_[key] = arr;
    return *this;
}

TemplateContext& TemplateContext::setArray(const std::string& key, const Json::Value& values) {
    context_[key] = values;
    return *this;
}

TemplateContext& TemplateContext::setObject(const std::string& key,
                                            const std::unordered_map<std::string, std::string>& values) {
    Json::Value obj;
    for (const auto& [k, v] : values) {
        obj[k] = v;
    }
    context_[key] = obj;
    return *this;
}

TemplateContext& TemplateContext::remove(const std::string& key) {
    context_.removeMember(key);
    return *this;
}

TemplateContext& TemplateContext::clear() {
    context_.clear();
    return *this;
}

// ============================================================================
// DOMAIN-SPECIFIC SETTERS
// ============================================================================

TemplateContext& TemplateContext::setDog(const Json::Value& dog_json) {
    // Store the full dog object
    context_["dog"] = dog_json;

    // Also set flattened convenience variables
    if (dog_json.isMember("id")) {
        context_["dog_id"] = dog_json["id"];
    }
    if (dog_json.isMember("name")) {
        context_["dog_name"] = dog_json["name"];
    }
    if (dog_json.isMember("breed_primary")) {
        context_["dog_breed"] = dog_json["breed_primary"];
    }
    if (dog_json.isMember("breed_secondary") && !dog_json["breed_secondary"].isNull()) {
        context_["dog_breed_secondary"] = dog_json["breed_secondary"];
        context_["dog_breed_full"] = dog_json["breed_primary"].asString() +
                                      "/" + dog_json["breed_secondary"].asString();
    } else {
        context_["dog_breed_full"] = dog_json.get("breed_primary", "Unknown").asString();
    }
    if (dog_json.isMember("age_months")) {
        int age_months = dog_json["age_months"].asInt();
        context_["dog_age_months"] = age_months;
        context_["dog_age_years"] = age_months / 12;

        // Human readable age
        if (age_months < 12) {
            context_["dog_age"] = std::to_string(age_months) + " months";
        } else {
            int years = age_months / 12;
            int remaining_months = age_months % 12;
            if (remaining_months > 0) {
                context_["dog_age"] = std::to_string(years) + " years, " +
                                      std::to_string(remaining_months) + " months";
            } else {
                context_["dog_age"] = std::to_string(years) +
                                      (years == 1 ? " year" : " years");
            }
        }
    }
    if (dog_json.isMember("size")) {
        context_["dog_size"] = dog_json["size"];
    }
    if (dog_json.isMember("gender")) {
        context_["dog_gender"] = dog_json["gender"];
        // Set pronouns
        std::string gender = dog_json["gender"].asString();
        if (gender == "male") {
            context_["dog_pronoun"] = "he";
            context_["dog_pronoun_possessive"] = "his";
            context_["dog_pronoun_object"] = "him";
        } else if (gender == "female") {
            context_["dog_pronoun"] = "she";
            context_["dog_pronoun_possessive"] = "her";
            context_["dog_pronoun_object"] = "her";
        } else {
            context_["dog_pronoun"] = "they";
            context_["dog_pronoun_possessive"] = "their";
            context_["dog_pronoun_object"] = "them";
        }
    }
    if (dog_json.isMember("photo_urls") && dog_json["photo_urls"].isArray() &&
        !dog_json["photo_urls"].empty()) {
        context_["dog_photo_url"] = dog_json["photo_urls"][0];
        context_["dog_photo_urls"] = dog_json["photo_urls"];
    }
    if (dog_json.isMember("description")) {
        context_["dog_description"] = dog_json["description"];
    }
    if (dog_json.isMember("status")) {
        context_["dog_status"] = dog_json["status"];
    }
    if (dog_json.isMember("urgency_level")) {
        context_["dog_urgency_level"] = dog_json["urgency_level"];
    }
    if (dog_json.isMember("tags") && dog_json["tags"].isArray()) {
        context_["dog_tags"] = dog_json["tags"];
    }

    return *this;
}

TemplateContext& TemplateContext::setDogBasic(const std::string& id, const std::string& name,
                                               const std::string& breed, int age_months) {
    context_["dog_id"] = id;
    context_["dog_name"] = name;
    context_["dog_breed"] = breed;
    context_["dog_age_months"] = age_months;

    // Human readable age
    if (age_months < 12) {
        context_["dog_age"] = std::to_string(age_months) + " months";
    } else {
        int years = age_months / 12;
        context_["dog_age"] = std::to_string(years) + (years == 1 ? " year" : " years");
    }

    return *this;
}

TemplateContext& TemplateContext::setShelter(const Json::Value& shelter_json) {
    // Store the full shelter object
    context_["shelter"] = shelter_json;

    // Set flattened convenience variables
    if (shelter_json.isMember("id")) {
        context_["shelter_id"] = shelter_json["id"];
    }
    if (shelter_json.isMember("name")) {
        context_["shelter_name"] = shelter_json["name"];
    }
    if (shelter_json.isMember("city")) {
        context_["shelter_city"] = shelter_json["city"];
    }
    if (shelter_json.isMember("state_code")) {
        context_["shelter_state"] = shelter_json["state_code"];
    }
    if (shelter_json.isMember("phone")) {
        context_["shelter_phone"] = shelter_json["phone"];
    }
    if (shelter_json.isMember("email")) {
        context_["shelter_email"] = shelter_json["email"];
    }
    if (shelter_json.isMember("website")) {
        context_["shelter_website"] = shelter_json["website"];
    }
    if (shelter_json.isMember("is_kill_shelter")) {
        context_["shelter_is_kill_shelter"] = shelter_json["is_kill_shelter"];
    }
    if (shelter_json.isMember("address")) {
        context_["shelter_address"] = shelter_json["address"];
    }
    if (shelter_json.isMember("zip_code")) {
        context_["shelter_zip"] = shelter_json["zip_code"];
    }

    // Build full location string
    std::string location;
    if (shelter_json.isMember("city")) {
        location = shelter_json["city"].asString();
    }
    if (shelter_json.isMember("state_code")) {
        if (!location.empty()) location += ", ";
        location += shelter_json["state_code"].asString();
    }
    context_["shelter_location"] = location;

    return *this;
}

TemplateContext& TemplateContext::setShelterBasic(const std::string& id, const std::string& name,
                                                   const std::string& city, const std::string& state_code) {
    context_["shelter_id"] = id;
    context_["shelter_name"] = name;
    context_["shelter_city"] = city;
    context_["shelter_state"] = state_code;
    context_["shelter_location"] = city + ", " + state_code;

    return *this;
}

TemplateContext& TemplateContext::setWaitTime(const WaitTimeComponents& wait_time) {
    // Store as object
    context_["wait_time"] = wait_time.toJson();

    // Flatten for easy access
    context_["wait_time_years"] = wait_time.years;
    context_["wait_time_months"] = wait_time.months;
    context_["wait_time_days"] = wait_time.days;
    context_["wait_time_hours"] = wait_time.hours;
    context_["wait_time_minutes"] = wait_time.minutes;
    context_["wait_time_seconds"] = wait_time.seconds;
    context_["wait_time_formatted"] = wait_time.formatted();
    context_["wait_time_human"] = wait_time.humanReadable();
    context_["wait_time_total_seconds"] = static_cast<Json::Int64>(wait_time.totalSeconds());

    return *this;
}

TemplateContext& TemplateContext::setCountdown(const CountdownComponents& countdown) {
    // Store as object
    context_["countdown"] = countdown.toJson();

    // Flatten for easy access
    context_["countdown_days"] = countdown.days;
    context_["countdown_hours"] = countdown.hours;
    context_["countdown_minutes"] = countdown.minutes;
    context_["countdown_seconds"] = countdown.seconds;
    context_["countdown_formatted"] = countdown.formatted();
    context_["countdown_human"] = countdown.humanReadable();
    context_["countdown_is_expired"] = countdown.is_expired;
    context_["countdown_is_critical"] = countdown.isCritical();
    context_["countdown_is_high_urgency"] = countdown.isHighUrgency();
    context_["countdown_urgency_level"] = countdown.urgencyLevel();
    context_["countdown_total_seconds"] = static_cast<Json::Int64>(countdown.totalSeconds());

    return *this;
}

TemplateContext& TemplateContext::setUrgency(const std::string& level) {
    context_["urgency_level"] = level;
    context_["urgency_is_critical"] = (level == "critical" || level == "expired");
    context_["urgency_is_high"] = (level == "high" || level == "critical" || level == "expired");
    context_["urgency_is_medium"] = (level == "medium" || level == "high" ||
                                     level == "critical" || level == "expired");

    // Urgency display colors
    if (level == "critical" || level == "expired") {
        context_["urgency_color"] = "#FF0000";
        context_["urgency_emoji"] = "!!!!!!!!";
        context_["urgency_label"] = "CRITICAL";
    } else if (level == "high") {
        context_["urgency_color"] = "#FF6600";
        context_["urgency_emoji"] = "!!!!!!";
        context_["urgency_label"] = "HIGH URGENCY";
    } else if (level == "medium") {
        context_["urgency_color"] = "#FFCC00";
        context_["urgency_emoji"] = "!!!";
        context_["urgency_label"] = "URGENT";
    } else {
        context_["urgency_color"] = "#00CC00";
        context_["urgency_emoji"] = "";
        context_["urgency_label"] = "Available";
    }

    return *this;
}

TemplateContext& TemplateContext::setUser(const Json::Value& user_json) {
    context_["user"] = user_json;

    if (user_json.isMember("id")) {
        context_["user_id"] = user_json["id"];
    }
    if (user_json.isMember("display_name")) {
        context_["user_name"] = user_json["display_name"];
    }
    if (user_json.isMember("email")) {
        // Partial email for privacy
        std::string email = user_json["email"].asString();
        size_t at_pos = email.find('@');
        if (at_pos != std::string::npos && at_pos > 2) {
            context_["user_email_partial"] = email.substr(0, 2) + "***" + email.substr(at_pos);
        } else {
            context_["user_email_partial"] = "***@***";
        }
    }

    return *this;
}

TemplateContext& TemplateContext::setFosterHome(const Json::Value& foster_json) {
    context_["foster"] = foster_json;

    if (foster_json.isMember("id")) {
        context_["foster_id"] = foster_json["id"];
    }
    if (foster_json.isMember("max_dogs")) {
        context_["foster_capacity"] = foster_json["max_dogs"];
    }
    if (foster_json.isMember("current_dogs")) {
        context_["foster_current_dogs"] = foster_json["current_dogs"];
    }

    return *this;
}

TemplateContext& TemplateContext::setHashtags(const std::vector<std::string>& hashtags) {
    Json::Value arr(Json::arrayValue);
    std::string formatted;

    for (const auto& tag : hashtags) {
        arr.append(tag);
        if (!formatted.empty()) {
            formatted += " ";
        }
        formatted += "#" + tag;
    }

    context_["hashtags"] = arr;
    context_["hashtags_formatted"] = formatted;
    context_["hashtags_count"] = static_cast<int>(hashtags.size());

    return *this;
}

TemplateContext& TemplateContext::setPlatform(const std::string& platform) {
    context_["platform"] = platform;

    // Set platform-specific character limits
    if (platform == "twitter") {
        context_["platform_char_limit"] = 280;
    } else if (platform == "instagram") {
        context_["platform_char_limit"] = 2200;
    } else if (platform == "facebook") {
        context_["platform_char_limit"] = 63206;
    } else if (platform == "tiktok") {
        context_["platform_char_limit"] = 2200;
    } else {
        context_["platform_char_limit"] = 1000;
    }

    return *this;
}

TemplateContext& TemplateContext::setEmailMeta(const std::string& subject, const std::string& preheader) {
    context_["email_subject"] = subject;
    context_["email_preheader"] = preheader;

    return *this;
}

TemplateContext& TemplateContext::setSiteBranding(const std::string& site_name, const std::string& site_url) {
    context_["site_name"] = site_name;
    context_["site_url"] = site_url;

    return *this;
}

TemplateContext& TemplateContext::setCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::localtime(&time_t_now);

    std::ostringstream date_oss, time_oss, datetime_oss;
    date_oss << std::put_time(tm, "%Y-%m-%d");
    time_oss << std::put_time(tm, "%H:%M:%S");
    datetime_oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");

    context_["current_date"] = date_oss.str();
    context_["current_time"] = time_oss.str();
    context_["current_datetime"] = datetime_oss.str();
    context_["current_year"] = 1900 + tm->tm_year;
    context_["current_month"] = 1 + tm->tm_mon;
    context_["current_day"] = tm->tm_mday;

    return *this;
}

TemplateContext& TemplateContext::setDogList(const Json::Value& dogs) {
    context_["dogs"] = dogs;
    context_["dogs_count"] = dogs.isArray() ? dogs.size() : 0;

    return *this;
}

// ============================================================================
// ACCESSORS
// ============================================================================

Json::Value TemplateContext::get(const std::string& key) const {
    return context_.get(key, Json::Value());
}

bool TemplateContext::has(const std::string& key) const {
    return context_.isMember(key);
}

std::string TemplateContext::getString(const std::string& key, const std::string& default_value) const {
    return context_.get(key, default_value).asString();
}

int TemplateContext::getInt(const std::string& key, int default_value) const {
    return context_.get(key, default_value).asInt();
}

bool TemplateContext::getBool(const std::string& key, bool default_value) const {
    return context_.get(key, default_value).asBool();
}

Json::Value TemplateContext::toJson() const {
    return context_;
}

std::vector<std::string> TemplateContext::getKeys() const {
    return context_.getMemberNames();
}

// ============================================================================
// MERGING
// ============================================================================

TemplateContext& TemplateContext::merge(const TemplateContext& other, bool overwrite) {
    return merge(other.context_, overwrite);
}

TemplateContext& TemplateContext::merge(const Json::Value& json, bool overwrite) {
    if (!json.isObject()) {
        return *this;
    }

    for (const auto& key : json.getMemberNames()) {
        if (overwrite || !context_.isMember(key)) {
            context_[key] = json[key];
        }
    }

    return *this;
}

// ============================================================================
// STATIC FACTORY METHODS
// ============================================================================

TemplateContext TemplateContext::forUrgentAlert(const Json::Value& dog_json,
                                                 const Json::Value& shelter_json,
                                                 const CountdownComponents& countdown) {
    TemplateContext ctx;
    ctx.setDog(dog_json)
       .setShelter(shelter_json)
       .setCountdown(countdown)
       .setUrgency(countdown.urgencyLevel())
       .setCurrentDateTime()
       .setSiteBranding("WaitingTheLongest.com", "https://waitingthelongest.com");

    // Add standard urgent hashtags
    ctx.setHashtags({"SaveThisLife", "AdoptDontShop", "UrgentDogs", "ShelterDogs",
                     dog_json.get("breed_primary", "Dogs").asString() + "OfTikTok"});

    return ctx;
}

TemplateContext TemplateContext::forLongestWaiting(const Json::Value& dog_json,
                                                    const Json::Value& shelter_json,
                                                    const WaitTimeComponents& wait_time) {
    TemplateContext ctx;
    ctx.setDog(dog_json)
       .setShelter(shelter_json)
       .setWaitTime(wait_time)
       .setCurrentDateTime()
       .setSiteBranding("WaitingTheLongest.com", "https://waitingthelongest.com");

    // Add standard long-wait hashtags
    ctx.setHashtags({"LongestWaiting", "AdoptDontShop", "ShelterDogs", "ForeverHome",
                     dog_json.get("breed_primary", "Dogs").asString() + "NeedLove"});

    return ctx;
}

TemplateContext TemplateContext::forSuccessStory(const Json::Value& dog_json,
                                                  const std::string& adopter_name,
                                                  int days_in_shelter) {
    TemplateContext ctx;
    ctx.setDog(dog_json)
       .set("adopter_name", adopter_name)
       .set("days_in_shelter", days_in_shelter)
       .setCurrentDateTime()
       .setSiteBranding("WaitingTheLongest.com", "https://waitingthelongest.com");

    // Add success story hashtags
    ctx.setHashtags({"AdoptionSuccess", "HappyTails", "AdoptDontShop", "ForeverHome",
                     "RescueDog", "AdoptedPet"});

    return ctx;
}

} // namespace wtl::content::templates

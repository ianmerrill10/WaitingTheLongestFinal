/**
 * @file TemplateHelpers.cc
 * @brief Implementation of TemplateHelpers
 * @see TemplateHelpers.h for documentation
 */

#include "content/templates/TemplateHelpers.h"
#include "content/templates/TemplateEngine.h"
#include "content/templates/TemplateContext.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace wtl::content::templates {

// ============================================================================
// REGISTRATION
// ============================================================================

void TemplateHelpers::registerAll(TemplateEngine& engine) {
    // Wait time helpers
    engine.registerHelper("formatWaitTime", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatWaitTime(args[0]);
    });

    engine.registerHelper("formatWaitTimeLED", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatWaitTimeLED(args[0]);
    });

    engine.registerHelper("waitTimePrimary", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : waitTimePrimary(args[0]);
    });

    // Countdown helpers
    engine.registerHelper("formatCountdown", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatCountdown(args[0]);
    });

    engine.registerHelper("formatCountdownLED", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatCountdownLED(args[0]);
    });

    engine.registerHelper("countdownUrgent", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : countdownUrgent(args[0]);
    });

    // Urgency helpers
    engine.registerHelper("formatUrgency", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatUrgency(args[0].asString());
    });

    engine.registerHelper("urgencyEmoji", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : urgencyEmoji(args[0].asString());
    });

    engine.registerHelper("urgencyColor", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : urgencyColor(args[0].asString());
    });

    engine.registerHelper("isUrgent", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "false" : isUrgent(args[0].asString());
    });

    // Text formatting helpers
    engine.registerHelper("pluralize", [](const Json::Value& args, const Json::Value&) {
        if (args.size() < 3) return std::string("");
        return pluralize(args[0].asInt(), args[1].asString(), args[2].asString());
    });

    engine.registerHelper("truncate", [](const Json::Value& args, const Json::Value&) {
        if (args.size() < 2) return args.empty() ? "" : args[0].asString();
        std::string suffix = args.size() > 2 ? args[2].asString() : "...";
        return truncate(args[0].asString(), args[1].asInt(), suffix);
    });

    engine.registerHelper("uppercase", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : uppercase(args[0].asString());
    });

    engine.registerHelper("lowercase", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : lowercase(args[0].asString());
    });

    engine.registerHelper("capitalize", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : capitalize(args[0].asString());
    });

    engine.registerHelper("titleCase", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : titleCase(args[0].asString());
    });

    engine.registerHelper("stripHtml", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : stripHtml(args[0].asString());
    });

    engine.registerHelper("escapeHtml", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : escapeHtml(args[0].asString());
    });

    engine.registerHelper("nl2br", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : nl2br(args[0].asString());
    });

    // Date/time helpers
    engine.registerHelper("date", [](const Json::Value& args, const Json::Value&) {
        if (args.empty()) return std::string("");
        std::string format = args.size() > 1 ? args[1].asString() : "%B %d, %Y";
        return formatDate(args[0].asString(), format);
    });

    engine.registerHelper("timeAgo", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : timeAgo(args[0].asString());
    });

    engine.registerHelper("now", [](const Json::Value& args, const Json::Value&) {
        std::string format = args.empty() ? "%Y-%m-%d %H:%M:%S" : args[0].asString();
        return now(format);
    });

    // Number helpers
    engine.registerHelper("formatNumber", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatNumber(args[0].asInt64());
    });

    engine.registerHelper("ordinal", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : ordinal(args[0].asInt());
    });

    // Dog-specific helpers
    engine.registerHelper("ageCategory", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : ageCategory(args[0].asInt());
    });

    engine.registerHelper("formatAge", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatAge(args[0].asInt());
    });

    engine.registerHelper("pronoun", [](const Json::Value& args, const Json::Value&) {
        if (args.empty()) return std::string("they");
        std::string type = args.size() > 1 ? args[1].asString() : "subject";
        return pronoun(args[0].asString(), type);
    });

    engine.registerHelper("formatSize", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : formatSize(args[0].asString());
    });

    // URL helpers
    engine.registerHelper("urlEncode", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : urlEncode(args[0].asString());
    });

    engine.registerHelper("dogUrl", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : dogUrl(args[0].asString());
    });

    engine.registerHelper("shelterUrl", [](const Json::Value& args, const Json::Value&) {
        return args.empty() ? "" : shelterUrl(args[0].asString());
    });
}

// ============================================================================
// WAIT TIME HELPERS
// ============================================================================

std::string TemplateHelpers::formatWaitTime(const Json::Value& components) {
    int years = components.get("years", 0).asInt();
    int months = components.get("months", 0).asInt();
    int days = components.get("days", 0).asInt();

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

std::string TemplateHelpers::formatWaitTimeLED(const Json::Value& components) {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << components.get("years", 0).asInt() << ":"
        << std::setw(2) << components.get("months", 0).asInt() << ":"
        << std::setw(2) << components.get("days", 0).asInt() << ":"
        << std::setw(2) << components.get("hours", 0).asInt() << ":"
        << std::setw(2) << components.get("minutes", 0).asInt() << ":"
        << std::setw(2) << components.get("seconds", 0).asInt();
    return oss.str();
}

std::string TemplateHelpers::waitTimePrimary(const Json::Value& components) {
    int years = components.get("years", 0).asInt();
    int months = components.get("months", 0).asInt();
    int days = components.get("days", 0).asInt();

    if (years > 0) {
        return std::to_string(years) + (years == 1 ? " year" : " years");
    }
    if (months > 0) {
        return std::to_string(months) + (months == 1 ? " month" : " months");
    }
    return std::to_string(days) + (days == 1 ? " day" : " days");
}

// ============================================================================
// COUNTDOWN HELPERS
// ============================================================================

std::string TemplateHelpers::formatCountdown(const Json::Value& components) {
    bool is_expired = components.get("is_expired", false).asBool();
    if (is_expired) {
        return "TIME EXPIRED";
    }

    int days = components.get("days", 0).asInt();
    int hours = components.get("hours", 0).asInt();
    int minutes = components.get("minutes", 0).asInt();

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

std::string TemplateHelpers::formatCountdownLED(const Json::Value& components) {
    bool is_expired = components.get("is_expired", false).asBool();
    if (is_expired) {
        return "EXPIRED";
    }

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << components.get("days", 0).asInt() << ":"
        << std::setw(2) << components.get("hours", 0).asInt() << ":"
        << std::setw(2) << components.get("minutes", 0).asInt() << ":"
        << std::setw(2) << components.get("seconds", 0).asInt();
    return oss.str();
}

std::string TemplateHelpers::countdownUrgent(const Json::Value& components) {
    bool is_expired = components.get("is_expired", false).asBool();
    if (is_expired) {
        return "TIME HAS RUN OUT!";
    }

    int days = components.get("days", 0).asInt();
    int hours = components.get("hours", 0).asInt();

    if (days == 0 && hours < 12) {
        return "LESS THAN 12 HOURS LEFT!";
    } else if (days == 0) {
        return "LESS THAN 24 HOURS!";
    } else if (days == 1) {
        return "ONLY 1 DAY LEFT!";
    } else if (days <= 3) {
        return "ONLY " + std::to_string(days) + " DAYS LEFT!";
    } else {
        return std::to_string(days) + " DAYS REMAINING";
    }
}

// ============================================================================
// URGENCY HELPERS
// ============================================================================

std::string TemplateHelpers::formatUrgency(const std::string& level) {
    if (level == "critical" || level == "expired") {
        return "CRITICAL";
    } else if (level == "high") {
        return "HIGH URGENCY";
    } else if (level == "medium") {
        return "URGENT";
    } else {
        return "Available";
    }
}

std::string TemplateHelpers::urgencyEmoji(const std::string& level) {
    if (level == "critical" || level == "expired") {
        return "!!!!!!!!";
    } else if (level == "high") {
        return "!!!!!!";
    } else if (level == "medium") {
        return "!!!";
    } else {
        return "";
    }
}

std::string TemplateHelpers::urgencyColor(const std::string& level) {
    if (level == "critical" || level == "expired") {
        return "#FF0000";
    } else if (level == "high") {
        return "#FF6600";
    } else if (level == "medium") {
        return "#FFCC00";
    } else {
        return "#00CC00";
    }
}

std::string TemplateHelpers::isUrgent(const std::string& level) {
    if (level == "critical" || level == "expired" || level == "high") {
        return "true";
    }
    return "false";
}

// ============================================================================
// TEXT FORMATTING HELPERS
// ============================================================================

std::string TemplateHelpers::pluralize(int count, const std::string& singular,
                                        const std::string& plural) {
    return std::to_string(count) + " " + (count == 1 ? singular : plural);
}

std::string TemplateHelpers::truncate(const std::string& text, int max_length,
                                      const std::string& suffix) {
    if (static_cast<int>(text.length()) <= max_length) {
        return text;
    }

    int truncate_at = max_length - static_cast<int>(suffix.length());
    if (truncate_at <= 0) {
        return suffix;
    }

    // Try to break at word boundary
    size_t last_space = text.rfind(' ', truncate_at);
    if (last_space != std::string::npos && last_space > static_cast<size_t>(truncate_at / 2)) {
        truncate_at = static_cast<int>(last_space);
    }

    return text.substr(0, truncate_at) + suffix;
}

std::string TemplateHelpers::uppercase(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string TemplateHelpers::lowercase(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string TemplateHelpers::capitalize(const std::string& text) {
    if (text.empty()) return text;
    std::string result = text;
    result[0] = std::toupper(result[0]);
    return result;
}

std::string TemplateHelpers::titleCase(const std::string& text) {
    std::string result = text;
    bool capitalize_next = true;

    for (size_t i = 0; i < result.length(); ++i) {
        if (std::isspace(result[i])) {
            capitalize_next = true;
        } else if (capitalize_next) {
            result[i] = std::toupper(result[i]);
            capitalize_next = false;
        } else {
            result[i] = std::tolower(result[i]);
        }
    }

    return result;
}

std::string TemplateHelpers::stripHtml(const std::string& text) {
    std::regex html_tags("<[^>]*>");
    return std::regex_replace(text, html_tags, "");
}

std::string TemplateHelpers::escapeHtml(const std::string& text) {
    std::string result;
    result.reserve(text.length() * 1.1);

    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }

    return result;
}

std::string TemplateHelpers::nl2br(const std::string& text) {
    std::string result;
    result.reserve(text.length() * 1.2);

    for (char c : text) {
        if (c == '\n') {
            result += "<br>\n";
        } else {
            result += c;
        }
    }

    return result;
}

// ============================================================================
// DATE/TIME HELPERS
// ============================================================================

std::string TemplateHelpers::formatDate(const std::string& timestamp, const std::string& format) {
    int64_t ts = parseTimestamp(timestamp);
    if (ts == 0) return timestamp;

    std::time_t time = static_cast<std::time_t>(ts);
    std::tm* tm = std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(tm, format.c_str());
    return oss.str();
}

std::string TemplateHelpers::timeAgo(const std::string& timestamp) {
    int64_t ts = parseTimestamp(timestamp);
    if (ts == 0) return timestamp;

    auto now = std::chrono::system_clock::now();
    auto then = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(ts));
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - then).count();

    if (diff < 60) {
        return "just now";
    } else if (diff < 3600) {
        int minutes = static_cast<int>(diff / 60);
        return std::to_string(minutes) + (minutes == 1 ? " minute ago" : " minutes ago");
    } else if (diff < 86400) {
        int hours = static_cast<int>(diff / 3600);
        return std::to_string(hours) + (hours == 1 ? " hour ago" : " hours ago");
    } else if (diff < 2592000) {
        int days = static_cast<int>(diff / 86400);
        return std::to_string(days) + (days == 1 ? " day ago" : " days ago");
    } else if (diff < 31536000) {
        int months = static_cast<int>(diff / 2592000);
        return std::to_string(months) + (months == 1 ? " month ago" : " months ago");
    } else {
        int years = static_cast<int>(diff / 31536000);
        return std::to_string(years) + (years == 1 ? " year ago" : " years ago");
    }
}

std::string TemplateHelpers::now(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(tm, format.c_str());
    return oss.str();
}

// ============================================================================
// NUMBER FORMATTING HELPERS
// ============================================================================

std::string TemplateHelpers::formatNumber(int64_t number) {
    std::string num_str = std::to_string(std::abs(number));
    std::string result;

    int count = 0;
    for (auto it = num_str.rbegin(); it != num_str.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            result = ',' + result;
        }
        result = *it + result;
        ++count;
    }

    if (number < 0) {
        result = '-' + result;
    }

    return result;
}

std::string TemplateHelpers::ordinal(int number) {
    int abs_num = std::abs(number);
    std::string suffix;

    if (abs_num % 100 >= 11 && abs_num % 100 <= 13) {
        suffix = "th";
    } else {
        switch (abs_num % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: suffix = "th"; break;
        }
    }

    return std::to_string(number) + suffix;
}

// ============================================================================
// DOG-SPECIFIC HELPERS
// ============================================================================

std::string TemplateHelpers::ageCategory(int age_months) {
    if (age_months < 12) {
        return "puppy";
    } else if (age_months < 36) {
        return "young adult";
    } else if (age_months < 84) {
        return "adult";
    } else {
        return "senior";
    }
}

std::string TemplateHelpers::formatAge(int age_months) {
    if (age_months < 12) {
        return std::to_string(age_months) + (age_months == 1 ? " month" : " months");
    }

    int years = age_months / 12;
    int months = age_months % 12;

    if (months == 0) {
        return std::to_string(years) + (years == 1 ? " year" : " years");
    }

    return std::to_string(years) + (years == 1 ? " year" : " years") + ", " +
           std::to_string(months) + (months == 1 ? " month" : " months");
}

std::string TemplateHelpers::pronoun(const std::string& gender, const std::string& type) {
    std::string g = lowercase(gender);
    std::string t = lowercase(type);

    if (g == "male") {
        if (t == "object") return "him";
        if (t == "possessive") return "his";
        return "he";
    } else if (g == "female") {
        if (t == "object") return "her";
        if (t == "possessive") return "her";
        return "she";
    } else {
        if (t == "object") return "them";
        if (t == "possessive") return "their";
        return "they";
    }
}

std::string TemplateHelpers::formatSize(const std::string& size) {
    std::string s = lowercase(size);

    if (s == "small") return "Small";
    if (s == "medium") return "Medium";
    if (s == "large") return "Large";
    if (s == "xlarge" || s == "x-large" || s == "extra-large") return "Extra Large";

    return capitalize(size);
}

// ============================================================================
// HASHTAG HELPERS
// ============================================================================

std::string TemplateHelpers::formatHashtags(const std::vector<std::string>& tags) {
    std::string result;
    for (const auto& tag : tags) {
        if (!result.empty()) {
            result += " ";
        }
        result += "#" + tag;
    }
    return result;
}

std::vector<std::string> TemplateHelpers::generateHashtags(const std::string& breed,
                                                            const std::string& state,
                                                            const std::string& urgency) {
    std::vector<std::string> tags;

    // Always add base tags
    tags.push_back("AdoptDontShop");
    tags.push_back("ShelterDogs");

    // Urgency-based tags
    if (urgency == "critical" || urgency == "expired") {
        tags.push_back("SaveThisLife");
        tags.push_back("UrgentDogs");
        tags.push_back("EmergencyAdopt");
    } else if (urgency == "high") {
        tags.push_back("UrgentDogs");
        tags.push_back("NeedsHomeNow");
    }

    // Breed tag (clean up for hashtag)
    if (!breed.empty()) {
        std::string breed_tag = breed;
        // Remove spaces and special characters
        breed_tag.erase(std::remove_if(breed_tag.begin(), breed_tag.end(),
            [](char c) { return !std::isalnum(c); }), breed_tag.end());
        if (!breed_tag.empty()) {
            tags.push_back(breed_tag);
            tags.push_back(breed_tag + "sOfTikTok");
        }
    }

    // State tag
    if (!state.empty()) {
        tags.push_back(state + "Dogs");
        tags.push_back("Adopt" + state);
    }

    // General rescue tags
    tags.push_back("RescueDog");
    tags.push_back("ForeverHome");

    return tags;
}

// ============================================================================
// URL HELPERS
// ============================================================================

std::string TemplateHelpers::urlEncode(const std::string& text) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(c));
        }
    }

    return encoded.str();
}

std::string TemplateHelpers::dogUrl(const std::string& dog_id) {
    return "https://waitingthelongest.com/dog/" + dog_id;
}

std::string TemplateHelpers::shelterUrl(const std::string& shelter_id) {
    return "https://waitingthelongest.com/shelter/" + shelter_id;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

int64_t TemplateHelpers::parseTimestamp(const std::string& timestamp) {
    // Try to parse as Unix timestamp
    try {
        return std::stoll(timestamp);
    } catch (const std::exception& e) {
        LOG_WARN << "TemplateHelpers: " << e.what();
    } catch (...) {
        LOG_WARN << "TemplateHelpers: unknown error";
    }

    // Try to parse as ISO 8601 date string
    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Try different formats
    const char* formats[] = {
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d",
        "%m/%d/%Y %H:%M:%S",
        "%m/%d/%Y"
    };

    for (const char* fmt : formats) {
        ss.clear();
        ss.str(timestamp);
        if (ss >> std::get_time(&tm, fmt)) {
            return std::mktime(&tm);
        }
    }

    return 0;
}

} // namespace wtl::content::templates

/**
 * @file DataMapper.cc
 * @brief Implementation of external data to internal model mapping
 * @see DataMapper.h for documentation
 */

#include "aggregators/DataMapper.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

#include "core/models/Dog.h"
#include "core/models/Shelter.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::aggregators {

// ============================================================================
// SINGLETON
// ============================================================================

DataMapper& DataMapper::getInstance() {
    static DataMapper instance;
    return instance;
}

DataMapper::DataMapper() {
    initializeDefaultMappings();
}

// ============================================================================
// DOG MAPPING
// ============================================================================

MappingResult<wtl::core::models::Dog> DataMapper::mapToDog(
    const Json::Value& data,
    const std::string& source) {

    if (source == "rescuegroups") {
        return mapRescueGroupsDog(data);
    } else {
        return mapDirectFeedDog(data);
    }
}

MappingResult<wtl::core::models::Dog> DataMapper::mapRescueGroupsDog(const Json::Value& data) {
    MappingResult<wtl::core::models::Dog> result;
    wtl::core::models::Dog dog;

    try {
        // RescueGroups uses "animalID" or "id"
        dog.external_id = getString(data, "animalID", getString(data, "id"));
        if (!dog.external_id.has_value() || dog.external_id->empty()) {
            result.error = "Missing required field: animalID/id";
            return result;
        }

        dog.source = "rescuegroups";
        dog.name = getString(data, "animalName", getString(data, "name", "Unknown"));

        // Organization
        dog.shelter_id = getString(data, "animalOrgID", getString(data, "orgID"));

        // Breeds - RescueGroups uses different field names
        dog.breed_primary = normalizeBreed(
            getString(data, "animalBreed",
                getString(data, "animalPrimaryBreed", "Mixed")));

        std::string secondary = getString(data, "animalSecondaryBreed");
        if (!secondary.empty() && secondary != "None") {
            dog.breed_secondary = normalizeBreed(secondary);
        }

        // Size, age, gender
        dog.size = normalizeSize(getString(data, "animalGeneralSizePotential",
            getString(data, "animalSizeCurrent", "medium")));
        dog.age_category = normalizeAge(getString(data, "animalGeneralAge", "adult"));

        // Try to get specific age
        std::string birth_date = getString(data, "animalBirthdate");
        if (!birth_date.empty()) {
            // Calculate age from birth date
            auto birth = parseTimestamp(birth_date);
            auto now = std::chrono::system_clock::now();
            auto diff = now - birth;
            auto days = std::chrono::duration_cast<std::chrono::hours>(diff).count() / 24;
            dog.age_months = static_cast<int>(days / 30);
        } else {
            dog.age_months = parseAgeToMonths(dog.age_category);
        }

        dog.gender = normalizeGender(getString(data, "animalSex", "unknown"));

        // Colors
        dog.color_primary = normalizeColor(getString(data, "animalColorPrimary",
            getString(data, "animalColor", "")));
        std::string secondary_color = getString(data, "animalColorSecondary");
        if (!secondary_color.empty()) {
            dog.color_secondary = normalizeColor(secondary_color);
        }

        // Status
        dog.status = normalizeStatus(getString(data, "animalStatus", "adoptable"));
        dog.is_available = isAvailableStatus(dog.status);

        // Description
        dog.description = getString(data, "animalDescription",
            getString(data, "animalDescriptionPlain", ""));

        // Photos - RescueGroups has different photo structures
        if (data.isMember("animalPictures") && data["animalPictures"].isArray()) {
            for (const auto& pic : data["animalPictures"]) {
                std::string url = getString(pic, "urlSecureFull",
                    getString(pic, "urlFull", getString(pic, "url")));
                if (!url.empty()) {
                    dog.photo_urls.push_back(url);
                }
            }
        }

        // Also check for single photo fields
        for (int i = 1; i <= 4; i++) {
            std::string key = "animalPictureUrl" + std::to_string(i);
            std::string url = getString(data, key);
            if (!url.empty()) {
                dog.photo_urls.push_back(url);
            }
        }

        // Tags
        if (getBool(data, "animalAltered")) dog.tags.push_back("spayed_neutered");
        if (getBool(data, "animalHousetrained")) dog.tags.push_back("house_trained");
        if (getBool(data, "animalSpecialneeds")) dog.tags.push_back("special_needs");
        if (getBool(data, "animalShotsCurrent")) dog.tags.push_back("shots_current");
        if (getBool(data, "animalOKWithKids")) dog.tags.push_back("good_with_children");
        if (getBool(data, "animalOKWithDogs")) dog.tags.push_back("good_with_dogs");
        if (getBool(data, "animalOKWithCats")) dog.tags.push_back("good_with_cats");

        // Dates
        std::string intake = getString(data, "animalIntakeDate",
            getString(data, "animalCreatedDate"));
        if (!intake.empty()) {
            dog.intake_date = parseTimestamp(intake);
        } else {
            dog.intake_date = std::chrono::system_clock::now();
        }

        dog.created_at = std::chrono::system_clock::now();
        dog.updated_at = std::chrono::system_clock::now();

        // Urgency
        dog.urgency_level = "normal";
        dog.is_on_euthanasia_list = false;

        // Check for urgency indicators
        std::string urgency = normalizeString(getString(data, "animalUrgent"));
        if (urgency == "yes" || urgency == "true" || urgency == "1") {
            dog.urgency_level = "high";
        }

        result.success = true;
        result.data = std::move(dog);

    } catch (const std::exception& e) {
        result.error = std::string("Mapping error: ") + e.what();
    }

    return result;
}

MappingResult<wtl::core::models::Dog> DataMapper::mapDirectFeedDog(const Json::Value& data) {
    MappingResult<wtl::core::models::Dog> result;
    wtl::core::models::Dog dog;

    try {
        // Direct feeds should use our field names
        dog.external_id = getString(data, "external_id", getString(data, "id"));
        if (!dog.external_id.has_value() || dog.external_id->empty()) {
            result.error = "Missing required field: external_id or id";
            return result;
        }

        dog.source = getString(data, "source", "shelter_direct");
        dog.name = getString(data, "name", "Unknown");
        dog.shelter_id = getString(data, "shelter_id");

        dog.breed_primary = normalizeBreed(getString(data, "breed_primary",
            getString(data, "breed", "Mixed")));
        std::string secondary = getString(data, "breed_secondary");
        if (!secondary.empty()) {
            dog.breed_secondary = normalizeBreed(secondary);
        }

        dog.size = normalizeSize(getString(data, "size", "medium"));
        dog.age_category = normalizeAge(getString(data, "age_category",
            getString(data, "age", "adult")));
        dog.age_months = getInt(data, "age_months", parseAgeToMonths(dog.age_category));
        dog.gender = normalizeGender(getString(data, "gender", "unknown"));

        dog.color_primary = normalizeColor(getString(data, "color_primary",
            getString(data, "color", "")));
        std::string secondary_color = getString(data, "color_secondary");
        if (!secondary_color.empty()) {
            dog.color_secondary = normalizeColor(secondary_color);
        }

        if (data.isMember("weight_lbs") || data.isMember("weight")) {
            double weight = data.get("weight_lbs", data.get("weight", 0.0)).asDouble();
            if (weight > 0) {
                dog.weight_lbs = weight;
            }
        }

        dog.status = normalizeStatus(getString(data, "status", "adoptable"));
        dog.is_available = data.isMember("is_available")
            ? getBool(data, "is_available")
            : isAvailableStatus(dog.status);

        dog.description = getString(data, "description", "");
        dog.photo_urls = getStringArray(data, "photo_urls");
        if (dog.photo_urls.empty()) {
            dog.photo_urls = getStringArray(data, "photos");
        }

        std::string video = getString(data, "video_url");
        if (!video.empty()) {
            dog.video_url = video;
        }

        dog.tags = getStringArray(data, "tags");

        // Dates
        std::string intake = getString(data, "intake_date");
        dog.intake_date = !intake.empty() ? parseTimestamp(intake) : std::chrono::system_clock::now();

        std::string euthanasia = getString(data, "euthanasia_date");
        if (!euthanasia.empty()) {
            dog.euthanasia_date = parseTimestamp(euthanasia);
        }

        dog.created_at = std::chrono::system_clock::now();
        dog.updated_at = std::chrono::system_clock::now();

        dog.urgency_level = normalizeString(getString(data, "urgency_level", "normal"));
        dog.is_on_euthanasia_list = getBool(data, "is_on_euthanasia_list");

        result.success = true;
        result.data = std::move(dog);

    } catch (const std::exception& e) {
        result.error = std::string("Mapping error: ") + e.what();
    }

    return result;
}

// ============================================================================
// SHELTER MAPPING
// ============================================================================

MappingResult<wtl::core::models::Shelter> DataMapper::mapToShelter(
    const Json::Value& data,
    const std::string& source) {

    if (source == "rescuegroups") {
        return mapRescueGroupsShelter(data);
    }

    // Generic mapping for direct feeds
    MappingResult<wtl::core::models::Shelter> result;
    wtl::core::models::Shelter shelter;

    try {
        shelter.external_id = getString(data, "external_id", getString(data, "id"));
        shelter.source = source;
        shelter.name = getString(data, "name");
        shelter.city = getString(data, "city");
        shelter.state_code = getString(data, "state_code", getString(data, "state"));
        shelter.zip_code = getString(data, "zip_code", getString(data, "zip"));
        shelter.phone = getString(data, "phone");
        shelter.email = getString(data, "email");
        shelter.website = getString(data, "website");
        shelter.is_kill_shelter = getBool(data, "is_kill_shelter");

        result.success = true;
        result.data = std::move(shelter);
    } catch (const std::exception& e) {
        result.error = std::string("Mapping error: ") + e.what();
    }

    return result;
}

MappingResult<wtl::core::models::Shelter> DataMapper::mapRescueGroupsShelter(const Json::Value& data) {
    MappingResult<wtl::core::models::Shelter> result;
    wtl::core::models::Shelter shelter;

    try {
        shelter.external_id = getString(data, "orgID", getString(data, "id"));
        shelter.source = "rescuegroups";
        shelter.name = getString(data, "orgName", getString(data, "name"));
        shelter.city = getString(data, "orgCity");
        shelter.state_code = getString(data, "orgState");
        shelter.zip_code = getString(data, "orgPostalcode");
        shelter.phone = getString(data, "orgPhone");
        shelter.email = getString(data, "orgEmail");
        shelter.website = getString(data, "orgWebsiteUrl");

        shelter.is_kill_shelter = false;

        result.success = true;
        result.data = std::move(shelter);
    } catch (const std::exception& e) {
        result.error = std::string("Mapping error: ") + e.what();
    }

    return result;
}

// ============================================================================
// BREED NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeBreed(const std::string& breed_name) {
    std::string normalized = normalizeString(breed_name);

    // Check direct mapping
    auto it = breed_mappings_.find(normalized);
    if (it != breed_mappings_.end()) {
        return it->second.normalized_name;
    }

    // Try partial matches
    for (const auto& [key, mapping] : breed_mappings_) {
        if (normalized.find(key) != std::string::npos ||
            key.find(normalized) != std::string::npos) {
            return mapping.normalized_name;
        }
    }

    // Return as-is with title case
    if (!breed_name.empty()) {
        std::string result = breed_name;
        result[0] = std::toupper(result[0]);
        return result;
    }

    return "Mixed Breed";
}

bool DataMapper::isKnownBreed(const std::string& breed_name) const {
    std::string normalized = normalizeString(breed_name);
    return breed_mappings_.find(normalized) != breed_mappings_.end();
}

std::string DataMapper::getBreedCategory(const std::string& breed_name) const {
    std::string normalized = normalizeString(breed_name);
    auto it = breed_mappings_.find(normalized);
    if (it != breed_mappings_.end()) {
        return it->second.category;
    }
    return "unknown";
}

void DataMapper::addBreedMapping(const std::string& source_name,
                                  const std::string& normalized_name,
                                  const std::string& category) {
    BreedMapping mapping;
    mapping.source_name = source_name;
    mapping.normalized_name = normalized_name;
    mapping.category = category;
    breed_mappings_[normalizeString(source_name)] = mapping;
}

// ============================================================================
// SIZE NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeSize(const std::string& size) {
    std::string normalized = normalizeString(size);

    auto it = size_mappings_.find(normalized);
    if (it != size_mappings_.end()) {
        return it->second;
    }

    // Default mappings
    if (normalized.find("small") != std::string::npos ||
        normalized.find("tiny") != std::string::npos) {
        return "small";
    }
    if (normalized.find("large") != std::string::npos ||
        normalized.find("big") != std::string::npos) {
        return "large";
    }
    if (normalized.find("xlarge") != std::string::npos ||
        normalized.find("extra large") != std::string::npos ||
        normalized.find("giant") != std::string::npos) {
        return "xlarge";
    }

    return "medium";
}

std::string DataMapper::getSizeFromWeight(double weight_lbs) const {
    for (const auto& cat : size_categories_) {
        if (weight_lbs >= cat.min_weight_lbs && weight_lbs < cat.max_weight_lbs) {
            return cat.name;
        }
    }
    return "medium";
}

// ============================================================================
// AGE NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeAge(const std::string& age) {
    std::string normalized = normalizeString(age);

    auto it = age_mappings_.find(normalized);
    if (it != age_mappings_.end()) {
        return it->second;
    }

    if (normalized.find("puppy") != std::string::npos ||
        normalized.find("baby") != std::string::npos) {
        return "puppy";
    }
    if (normalized.find("young") != std::string::npos ||
        normalized.find("adolescent") != std::string::npos) {
        return "young";
    }
    if (normalized.find("senior") != std::string::npos ||
        normalized.find("old") != std::string::npos) {
        return "senior";
    }

    return "adult";
}

std::string DataMapper::getAgeFromMonths(int months) const {
    for (const auto& cat : age_categories_) {
        if (months >= cat.min_months && months < cat.max_months) {
            return cat.name;
        }
    }
    return "adult";
}

int DataMapper::parseAgeToMonths(const std::string& age_str) const {
    std::string lower = normalizeString(age_str);

    // Check for category names
    if (lower == "puppy" || lower == "baby") return 6;
    if (lower == "young" || lower == "adolescent") return 18;
    if (lower == "adult") return 48;
    if (lower == "senior" || lower == "old") return 96;

    // Try to parse "X years" or "X months"
    std::regex year_pattern(R"((\d+)\s*(?:year|yr)s?)");
    std::regex month_pattern(R"((\d+)\s*(?:month|mo)s?)");
    std::smatch match;

    if (std::regex_search(lower, match, year_pattern)) {
        int years = std::stoi(match[1]);
        return years * 12;
    }
    if (std::regex_search(lower, match, month_pattern)) {
        return std::stoi(match[1]);
    }

    // Default to adult age
    return 48;
}

// ============================================================================
// STATUS NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeStatus(const std::string& status) {
    std::string normalized = normalizeString(status);

    auto it = status_mappings_.find(normalized);
    if (it != status_mappings_.end()) {
        return it->second;
    }

    if (normalized.find("adopt") != std::string::npos) return "adoptable";
    if (normalized.find("pending") != std::string::npos) return "pending";
    if (normalized.find("hold") != std::string::npos) return "on_hold";
    if (normalized.find("foster") != std::string::npos) return "in_foster";

    return "adoptable";
}

bool DataMapper::isAvailableStatus(const std::string& status) const {
    return status == "adoptable" || status == "available";
}

// ============================================================================
// GENDER NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeGender(const std::string& gender) {
    std::string normalized = normalizeString(gender);

    auto it = gender_mappings_.find(normalized);
    if (it != gender_mappings_.end()) {
        return it->second;
    }

    if (normalized.find("male") != std::string::npos &&
        normalized.find("female") == std::string::npos) {
        return "male";
    }
    if (normalized.find("female") != std::string::npos) {
        return "female";
    }

    return "unknown";
}

// ============================================================================
// COLOR NORMALIZATION
// ============================================================================

std::string DataMapper::normalizeColor(const std::string& color) {
    std::string normalized = normalizeString(color);

    auto it = color_mappings_.find(normalized);
    if (it != color_mappings_.end()) {
        return it->second;
    }

    // Return title case
    if (!color.empty()) {
        std::string result = color;
        result[0] = std::toupper(result[0]);
        return result;
    }

    return "";
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string DataMapper::getString(const Json::Value& json,
                                   const std::string& key,
                                   const std::string& default_val) {
    if (json.isMember(key) && json[key].isString()) {
        return json[key].asString();
    }
    if (json.isMember(key) && !json[key].isNull()) {
        return json[key].asString();
    }
    return default_val;
}

std::string DataMapper::getNestedString(const Json::Value& json,
                                         const std::string& path,
                                         const std::string& default_val) {
    std::istringstream iss(path);
    std::string part;
    const Json::Value* current = &json;

    while (std::getline(iss, part, '.')) {
        if (!current->isMember(part)) {
            return default_val;
        }
        current = &(*current)[part];
    }

    if (current->isString()) {
        return current->asString();
    }
    return default_val;
}

int DataMapper::getInt(const Json::Value& json,
                        const std::string& key,
                        int default_val) {
    if (json.isMember(key)) {
        if (json[key].isInt()) {
            return json[key].asInt();
        }
        if (json[key].isString()) {
            try {
                return std::stoi(json[key].asString());
            } catch (const std::exception& e) {
                LOG_WARN << "DataMapper: " << e.what();
            } catch (...) {
                LOG_WARN << "DataMapper: unknown error";
            }
        }
    }
    return default_val;
}

bool DataMapper::getBool(const Json::Value& json,
                          const std::string& key,
                          bool default_val) {
    if (json.isMember(key)) {
        if (json[key].isBool()) {
            return json[key].asBool();
        }
        if (json[key].isString()) {
            std::string val = normalizeString(json[key].asString());
            return val == "true" || val == "yes" || val == "1";
        }
        if (json[key].isInt()) {
            return json[key].asInt() != 0;
        }
    }
    return default_val;
}

std::vector<std::string> DataMapper::getStringArray(const Json::Value& json,
                                                      const std::string& key) {
    std::vector<std::string> result;
    if (json.isMember(key) && json[key].isArray()) {
        for (const auto& item : json[key]) {
            if (item.isString()) {
                result.push_back(item.asString());
            }
        }
    }
    return result;
}

std::chrono::system_clock::time_point DataMapper::parseTimestamp(const std::string& timestamp) {
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Try ISO 8601 format
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail()) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // Try date only
    ss.clear();
    ss.str(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (!ss.fail()) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // Return now if parse fails
    return std::chrono::system_clock::now();
}

void DataMapper::loadMappings(const Json::Value& config) {
    if (config.isMember("breeds")) {
        for (const auto& breed : config["breeds"]) {
            addBreedMapping(
                getString(breed, "source"),
                getString(breed, "normalized"),
                getString(breed, "category")
            );
        }
    }

    if (config.isMember("sizes")) {
        for (const auto& key : config["sizes"].getMemberNames()) {
            size_mappings_[normalizeString(key)] = config["sizes"][key].asString();
        }
    }

    if (config.isMember("ages")) {
        for (const auto& key : config["ages"].getMemberNames()) {
            age_mappings_[normalizeString(key)] = config["ages"][key].asString();
        }
    }

    if (config.isMember("statuses")) {
        for (const auto& key : config["statuses"].getMemberNames()) {
            status_mappings_[normalizeString(key)] = config["statuses"][key].asString();
        }
    }
}

std::string DataMapper::normalizeString(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            result += std::tolower(static_cast<unsigned char>(c));
        } else if (c == ' ' && !result.empty() && result.back() != '_') {
            result += '_';
        }
    }

    // Trim trailing underscore
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }

    return result;
}

// ============================================================================
// INITIALIZE DEFAULT MAPPINGS
// ============================================================================

void DataMapper::initializeDefaultMappings() {
    // Common breed mappings
    addBreedMapping("lab", "Labrador Retriever", "sporting");
    addBreedMapping("labrador", "Labrador Retriever", "sporting");
    addBreedMapping("labrador retriever", "Labrador Retriever", "sporting");
    addBreedMapping("pit", "Pit Bull Terrier", "terrier");
    addBreedMapping("pit bull", "Pit Bull Terrier", "terrier");
    addBreedMapping("pitbull", "Pit Bull Terrier", "terrier");
    addBreedMapping("american pit bull terrier", "Pit Bull Terrier", "terrier");
    addBreedMapping("german shepherd", "German Shepherd Dog", "herding");
    addBreedMapping("german shepherd dog", "German Shepherd Dog", "herding");
    addBreedMapping("gsd", "German Shepherd Dog", "herding");
    addBreedMapping("golden", "Golden Retriever", "sporting");
    addBreedMapping("golden retriever", "Golden Retriever", "sporting");
    addBreedMapping("beagle", "Beagle", "hound");
    addBreedMapping("boxer", "Boxer", "working");
    addBreedMapping("chihuahua", "Chihuahua", "toy");
    addBreedMapping("dachshund", "Dachshund", "hound");
    addBreedMapping("husky", "Siberian Husky", "working");
    addBreedMapping("siberian husky", "Siberian Husky", "working");
    addBreedMapping("poodle", "Poodle", "non-sporting");
    addBreedMapping("mixed", "Mixed Breed", "mixed");
    addBreedMapping("mixed breed", "Mixed Breed", "mixed");
    addBreedMapping("mutt", "Mixed Breed", "mixed");

    // Size mappings
    size_mappings_["s"] = "small";
    size_mappings_["sm"] = "small";
    size_mappings_["small"] = "small";
    size_mappings_["m"] = "medium";
    size_mappings_["med"] = "medium";
    size_mappings_["medium"] = "medium";
    size_mappings_["l"] = "large";
    size_mappings_["lg"] = "large";
    size_mappings_["large"] = "large";
    size_mappings_["xl"] = "xlarge";
    size_mappings_["xlarge"] = "xlarge";
    size_mappings_["extra_large"] = "xlarge";
    size_mappings_["giant"] = "xlarge";

    // Size categories by weight
    size_categories_ = {
        {"small", 0.0, 25.0},
        {"medium", 25.0, 50.0},
        {"large", 50.0, 90.0},
        {"xlarge", 90.0, 500.0}
    };

    // Age mappings
    age_mappings_["baby"] = "puppy";
    age_mappings_["puppy"] = "puppy";
    age_mappings_["young"] = "young";
    age_mappings_["adolescent"] = "young";
    age_mappings_["adult"] = "adult";
    age_mappings_["senior"] = "senior";
    age_mappings_["old"] = "senior";

    // Age categories by months
    age_categories_ = {
        {"puppy", 0, 12},
        {"young", 12, 36},
        {"adult", 36, 84},
        {"senior", 84, 999}
    };

    // Status mappings
    status_mappings_["adoptable"] = "adoptable";
    status_mappings_["available"] = "adoptable";
    status_mappings_["pending"] = "pending";
    status_mappings_["adoption_pending"] = "pending";
    status_mappings_["adopted"] = "adopted";
    status_mappings_["on_hold"] = "on_hold";
    status_mappings_["hold"] = "on_hold";
    status_mappings_["foster"] = "in_foster";
    status_mappings_["in_foster"] = "in_foster";

    // Gender mappings
    gender_mappings_["m"] = "male";
    gender_mappings_["male"] = "male";
    gender_mappings_["f"] = "female";
    gender_mappings_["female"] = "female";
    gender_mappings_["unknown"] = "unknown";

    // Color mappings
    color_mappings_["blk"] = "Black";
    color_mappings_["black"] = "Black";
    color_mappings_["wht"] = "White";
    color_mappings_["white"] = "White";
    color_mappings_["brn"] = "Brown";
    color_mappings_["brown"] = "Brown";
    color_mappings_["tan"] = "Tan";
    color_mappings_["brindle"] = "Brindle";
    color_mappings_["merle"] = "Merle";
}

} // namespace wtl::aggregators

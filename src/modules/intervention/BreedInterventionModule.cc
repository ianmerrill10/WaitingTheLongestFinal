/**
 * @file BreedInterventionModule.cc
 * @brief Implementation of the BreedInterventionModule
 * @see BreedInterventionModule.h for documentation
 */

#include "BreedInterventionModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace wtl::modules::intervention {

// ============================================================================
// LIFECYCLE
// ============================================================================

bool BreedInterventionModule::initialize() {
    std::cout << "[BreedInterventionModule] Initializing..." << std::endl;

    // Load data
    loadBreedProfiles();
    loadBSLData();
    loadResources();

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto created_id = event_bus.subscribe(
        core::EventType::DOG_CREATED,
        [this](const core::Event& event) {
            handleDogCreated(event);
        },
        "breed_intervention_created_handler"
    );
    subscription_ids_.push_back(created_id);

    auto adopted_id = event_bus.subscribe(
        core::EventType::DOG_ADOPTED,
        [this](const core::Event& event) {
            handleDogAdopted(event);
        },
        "breed_intervention_adopted_handler"
    );
    subscription_ids_.push_back(adopted_id);

    auto location_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "adopter_location_search") {
                handleAdopterLocationSearch(event);
            }
        },
        "breed_intervention_location_handler"
    );
    subscription_ids_.push_back(location_id);

    enabled_ = true;
    std::cout << "[BreedInterventionModule] Initialization complete." << std::endl;
    std::cout << "[BreedInterventionModule] Loaded " << breed_profiles_.size()
              << " breed profiles" << std::endl;
    std::cout << "[BreedInterventionModule] Loaded " << bsl_locations_.size()
              << " BSL locations" << std::endl;
    std::cout << "[BreedInterventionModule] BSL tracking: "
              << (bsl_tracking_enabled_ ? "enabled" : "disabled") << std::endl;

    return true;
}

void BreedInterventionModule::shutdown() {
    std::cout << "[BreedInterventionModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[BreedInterventionModule] Shutdown complete." << std::endl;
}

void BreedInterventionModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool BreedInterventionModule::isHealthy() const {
    return enabled_;
}

Json::Value BreedInterventionModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["bsl_tracking_enabled"] = bsl_tracking_enabled_;

    Json::Value breeds(Json::arrayValue);
    for (const auto& breed : featured_breeds_) {
        breeds.append(breed);
    }
    status["featured_breeds"] = breeds;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["breed_profiles_count"] = static_cast<int>(breed_profiles_.size());
    status["bsl_locations_count"] = static_cast<int>(bsl_locations_.size());
    status["resources_count"] = static_cast<int>(pet_friendly_resources_.size());
    status["bsl_breeds_count"] = static_cast<int>(bsl_breeds_.size());

    return status;
}

Json::Value BreedInterventionModule::getMetrics() const {
    Json::Value metrics;
    metrics["bsl_alerts_sent"] = static_cast<Json::Int64>(bsl_alerts_sent_.load());
    metrics["resource_lookups"] = static_cast<Json::Int64>(resource_lookups_.load());
    metrics["education_views"] = static_cast<Json::Int64>(education_views_.load());
    metrics["restricted_breed_adoptions"] = static_cast<Json::Int64>(restricted_breed_adoptions_.load());

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void BreedInterventionModule::configure(const Json::Value& config) {
    if (config.isMember("bsl_tracking_enabled")) {
        bsl_tracking_enabled_ = config["bsl_tracking_enabled"].asBool();
    }
    if (config.isMember("featured_breeds") && config["featured_breeds"].isArray()) {
        featured_breeds_.clear();
        for (const auto& breed : config["featured_breeds"]) {
            featured_breeds_.push_back(breed.asString());
        }
    }
}

Json::Value BreedInterventionModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["bsl_tracking_enabled"] = true;

    Json::Value breeds(Json::arrayValue);
    breeds.append("pit_bull");
    breeds.append("rottweiler");
    breeds.append("german_shepherd");
    breeds.append("doberman");
    breeds.append("akita");
    config["featured_breeds"] = breeds;

    return config;
}

// ============================================================================
// BREED PROFILES
// ============================================================================

std::optional<BreedProfile> BreedInterventionModule::getBreedProfile(const std::string& breed_code) {
    std::string normalized = normalizeBreedCode(breed_code);

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = breed_profiles_.find(normalized);
    if (it != breed_profiles_.end()) {
        return it->second;
    }

    // Try common names
    for (const auto& [code, profile] : breed_profiles_) {
        for (const auto& name : profile.common_names) {
            if (normalizeBreedCode(name) == normalized) {
                return profile;
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> BreedInterventionModule::getBSLBreeds() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return std::vector<std::string>(bsl_breeds_.begin(), bsl_breeds_.end());
}

std::vector<std::string> BreedInterventionModule::getRestrictedBreeds() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return std::vector<std::string>(restricted_breeds_.begin(), restricted_breeds_.end());
}

std::vector<std::string> BreedInterventionModule::getOverlookedBreeds() {
    std::vector<std::string> overlooked;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [code, profile] : breed_profiles_) {
        // Breeds with higher than average wait times
        if (profile.avg_shelter_wait_days > 45.0) {
            overlooked.push_back(code);
        }
    }

    return overlooked;
}

// ============================================================================
// BSL TRACKING
// ============================================================================

bool BreedInterventionModule::isBreedBannedInLocation(const std::string& breed_code,
                                                        const std::string& city,
                                                        const std::string& state) {
    std::string normalized = normalizeBreedCode(breed_code);
    std::string lower_city = city;
    std::transform(lower_city.begin(), lower_city.end(), lower_city.begin(), ::tolower);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& loc : bsl_locations_) {
        // Check if location matches
        std::string loc_city = loc.city;
        std::transform(loc_city.begin(), loc_city.end(), loc_city.begin(), ::tolower);

        if ((loc_city == lower_city || loc.city.empty()) &&
            (loc.state == state || loc.state.empty())) {

            // Check if breed is banned
            for (const auto& banned : loc.banned_breeds) {
                if (breedMatchesCategory(normalized, banned)) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::optional<BSLLocation> BreedInterventionModule::getBreedRestriction(
    const std::string& breed_code,
    const std::string& city,
    const std::string& state) {

    std::string normalized = normalizeBreedCode(breed_code);
    std::string lower_city = city;
    std::transform(lower_city.begin(), lower_city.end(), lower_city.begin(), ::tolower);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& loc : bsl_locations_) {
        std::string loc_city = loc.city;
        std::transform(loc_city.begin(), loc_city.end(), loc_city.begin(), ::tolower);

        if ((loc_city == lower_city || loc.city.empty()) &&
            (loc.state == state || loc.state.empty())) {

            // Check banned breeds
            for (const auto& banned : loc.banned_breeds) {
                if (breedMatchesCategory(normalized, banned)) {
                    return loc;
                }
            }

            // Check restricted breeds
            for (const auto& restricted : loc.restricted_breeds) {
                if (breedMatchesCategory(normalized, restricted)) {
                    return loc;
                }
            }
        }
    }

    return std::nullopt;
}

std::vector<BSLLocation> BreedInterventionModule::getBSLLocationsForBreed(
    const std::string& breed_code) {

    std::vector<BSLLocation> locations;
    std::string normalized = normalizeBreedCode(breed_code);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& loc : bsl_locations_) {
        bool found = false;

        for (const auto& banned : loc.banned_breeds) {
            if (breedMatchesCategory(normalized, banned)) {
                found = true;
                break;
            }
        }

        if (!found) {
            for (const auto& restricted : loc.restricted_breeds) {
                if (breedMatchesCategory(normalized, restricted)) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            locations.push_back(loc);
        }
    }

    return locations;
}

std::vector<BSLLocation> BreedInterventionModule::getBSLLocationsInState(const std::string& state) {
    std::vector<BSLLocation> locations;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& loc : bsl_locations_) {
        if (loc.state == state) {
            locations.push_back(loc);
        }
    }

    return locations;
}

// ============================================================================
// PET-FRIENDLY RESOURCES
// ============================================================================

std::vector<PetFriendlyResource> BreedInterventionModule::findPetFriendlyHousing(
    const std::string& breed_code,
    const std::string& city,
    const std::string& state) {

    resource_lookups_++;
    return getResourcesForBreed(breed_code, "housing");
}

std::vector<PetFriendlyResource> BreedInterventionModule::findBreedFriendlyInsurance(
    const std::string& breed_code,
    const std::string& state) {

    resource_lookups_++;
    return getResourcesForBreed(breed_code, "insurance");
}

std::vector<PetFriendlyResource> BreedInterventionModule::getResourcesForBreed(
    const std::string& breed_code,
    const std::string& resource_type) {

    std::vector<PetFriendlyResource> matching;
    std::string normalized = normalizeBreedCode(breed_code);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& resource : pet_friendly_resources_) {
        // Filter by type if specified
        if (!resource_type.empty() && resource.type != resource_type) {
            continue;
        }

        // Check if resource accepts this breed
        if (resource.accepted_breeds.empty()) {
            // Empty means accepts all breeds
            matching.push_back(resource);
        } else {
            for (const auto& accepted : resource.accepted_breeds) {
                if (breedMatchesCategory(normalized, accepted) || accepted == "all") {
                    matching.push_back(resource);
                    break;
                }
            }
        }
    }

    return matching;
}

// ============================================================================
// EDUCATION & ADVOCACY
// ============================================================================

std::vector<BreedAdvocacyResource> BreedInterventionModule::getEducationalResources(
    const std::string& breed_code) {

    education_views_++;
    std::vector<BreedAdvocacyResource> resources;
    std::string normalized = normalizeBreedCode(breed_code);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& resource : advocacy_resources_) {
        if (resource.type == "article" || resource.type == "video" ||
            resource.type == "infographic" || resource.type == "study") {

            for (const auto& breed : resource.relevant_breeds) {
                if (breed == normalized || breed == "all") {
                    resources.push_back(resource);
                    break;
                }
            }
        }
    }

    return resources;
}

std::vector<BreedAdvocacyResource> BreedInterventionModule::getAdvocacyOrganizations(
    const std::string& breed_code) {

    std::vector<BreedAdvocacyResource> orgs;
    std::string normalized = normalizeBreedCode(breed_code);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& resource : advocacy_resources_) {
        if (resource.type == "organization") {
            for (const auto& breed : resource.relevant_breeds) {
                if (breed == normalized || breed == "all") {
                    orgs.push_back(resource);
                    break;
                }
            }
        }
    }

    return orgs;
}

Json::Value BreedInterventionModule::generateMythBustingContent(const std::string& breed_code) {
    Json::Value content;
    content["breed_code"] = breed_code;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = breed_profiles_.find(normalizeBreedCode(breed_code));
    if (it == breed_profiles_.end()) {
        content["error"] = "Breed not found";
        return content;
    }

    const auto& profile = it->second;
    content["breed_name"] = profile.breed_name;

    // Common myths and facts
    Json::Value myths(Json::arrayValue);

    if (breed_code == "pit_bull" || breedMatchesCategory(breed_code, "pit_bull")) {
        Json::Value myth1;
        myth1["myth"] = "Pit bulls have locking jaws";
        myth1["fact"] = "Pit bulls have the same jaw structure as all other dogs. There is no 'locking mechanism.'";
        myths.append(myth1);

        Json::Value myth2;
        myth2["myth"] = "Pit bulls are inherently aggressive";
        myth2["fact"] = "The American Temperament Test Society found pit bulls pass at 87.4%, higher than many popular breeds.";
        myths.append(myth2);

        Json::Value myth3;
        myth3["myth"] = "Pit bulls can't be trusted around other pets or children";
        myth3["fact"] = "Like all dogs, behavior depends on training, socialization, and individual personality.";
        myths.append(myth3);
    }

    content["myths_busted"] = myths;
    content["positive_traits"] = Json::Value(Json::arrayValue);
    for (const auto& trait : profile.positive_traits) {
        content["positive_traits"].append(trait);
    }

    return content;
}

// ============================================================================
// STATISTICS & REPORTING
// ============================================================================

Json::Value BreedInterventionModule::getBreedStats(const std::string& breed_code) {
    Json::Value stats;
    stats["breed_code"] = breed_code;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = breed_profiles_.find(normalizeBreedCode(breed_code));
    if (it != breed_profiles_.end()) {
        stats["breed_name"] = it->second.breed_name;
        stats["avg_shelter_wait_days"] = it->second.avg_shelter_wait_days;
        stats["adoption_rate_percent"] = it->second.adoption_rate_percent;
        stats["return_rate_percent"] = it->second.return_rate_percent;
        stats["subject_to_bsl"] = it->second.subject_to_bsl;
        stats["commonly_restricted"] = it->second.commonly_restricted;

        // Count BSL locations
        int bsl_count = 0;
        for (const auto& loc : bsl_locations_) {
            for (const auto& banned : loc.banned_breeds) {
                if (breedMatchesCategory(breed_code, banned)) {
                    bsl_count++;
                    break;
                }
            }
        }
        stats["bsl_location_count"] = bsl_count;
    }

    return stats;
}

Json::Value BreedInterventionModule::getBreedComparison() {
    Json::Value comparison(Json::arrayValue);

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [code, profile] : breed_profiles_) {
        Json::Value breed;
        breed["code"] = code;
        breed["name"] = profile.breed_name;
        breed["avg_wait_days"] = profile.avg_shelter_wait_days;
        breed["adoption_rate"] = profile.adoption_rate_percent;
        breed["return_rate"] = profile.return_rate_percent;
        breed["bsl_affected"] = profile.subject_to_bsl;
        comparison.append(breed);
    }

    return comparison;
}

void BreedInterventionModule::alertAdopterAboutBSL(const std::string& adopter_id,
                                                     const std::string& breed_code,
                                                     const std::string& location) {
    if (!bsl_tracking_enabled_) return;

    bsl_alerts_sent_++;

    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "bsl_alert";
    event.data["adopter_id"] = adopter_id;
    event.data["breed_code"] = breed_code;
    event.data["location"] = location;
    event.data["message"] = "Please verify local breed regulations before adopting.";
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[BreedInterventionModule] BSL alert sent to adopter " << adopter_id
              << " for " << breed_code << " in " << location << std::endl;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void BreedInterventionModule::handleDogCreated(const core::Event& event) {
    std::string breed = event.data.get("breed", "").asString();
    if (breed.empty()) return;

    std::string normalized = normalizeBreedCode(breed);

    // Check if this is a BSL breed and log
    if (bsl_breeds_.count(normalized) > 0 || restricted_breeds_.count(normalized) > 0) {
        std::cout << "[BreedInterventionModule] BSL-affected breed entered system: "
                  << breed << std::endl;
    }
}

void BreedInterventionModule::handleDogAdopted(const core::Event& event) {
    std::string breed = event.data.get("breed", "").asString();
    if (breed.empty()) return;

    std::string normalized = normalizeBreedCode(breed);

    // Track restricted breed adoptions
    if (bsl_breeds_.count(normalized) > 0 || restricted_breeds_.count(normalized) > 0) {
        restricted_breed_adoptions_++;
        std::cout << "[BreedInterventionModule] BSL-affected breed adopted: " << breed << std::endl;
    }
}

void BreedInterventionModule::handleAdopterLocationSearch(const core::Event& event) {
    if (!bsl_tracking_enabled_) return;

    std::string adopter_id = event.data.get("adopter_id", "").asString();
    std::string breed = event.data.get("breed", "").asString();
    std::string city = event.data.get("city", "").asString();
    std::string state = event.data.get("state", "").asString();

    if (adopter_id.empty() || breed.empty()) return;

    // Check for BSL
    auto restriction = getBreedRestriction(breed, city, state);
    if (restriction.has_value()) {
        alertAdopterAboutBSL(adopter_id, breed, city + ", " + state);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void BreedInterventionModule::loadBreedProfiles() {
    // Load default breed profiles
    std::vector<BreedProfile> profiles = {
        {"pit_bull", "Pit Bull", {"pit bull terrier", "american pit bull", "pitbull", "pittie"},
         "Loyal, affectionate, and eager to please",
         {"loyal", "affectionate", "athletic", "eager to please", "good with people"},
         {"BSL in many areas", "housing restrictions", "insurance discrimination", "breed bias"},
         65.0, 35.0, 8.0, true, true, {}},

        {"rottweiler", "Rottweiler", {"rottie"},
         "Confident, calm, and courageous guardian",
         {"confident", "calm", "courageous", "loyal", "intelligent"},
         {"BSL in some areas", "insurance restrictions", "size misconceptions"},
         45.0, 50.0, 6.0, true, true, {}},

        {"german_shepherd", "German Shepherd", {"gsd", "german shepherd dog"},
         "Intelligent, versatile, and devoted working dog",
         {"intelligent", "versatile", "loyal", "trainable", "protective"},
         {"insurance restrictions in some areas", "health concerns"},
         25.0, 70.0, 5.0, false, true, {}},

        {"doberman", "Doberman Pinscher", {"dobie", "dobermann"},
         "Alert, fearless, and loyal companion",
         {"alert", "loyal", "intelligent", "athletic", "devoted"},
         {"BSL in some areas", "insurance restrictions"},
         35.0, 55.0, 5.0, true, true, {}},

        {"black_dog", "Black Dogs", {"black lab", "black mixed"},
         "Often overlooked due to Black Dog Syndrome",
         {"varies by individual"},
         {"Black Dog Syndrome - harder to photograph, perceived as less friendly"},
         55.0, 40.0, 7.0, false, false, {}},

        {"senior_dog", "Senior Dogs", {},
         "Mature dogs with established personalities",
         {"calm", "trained", "known personality", "lower energy", "grateful"},
         {"perceived health issues", "shorter remaining lifespan", "overlooked for puppies"},
         70.0, 30.0, 3.0, false, false, {}}
    };

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& profile : profiles) {
        breed_profiles_[profile.breed_code] = profile;

        if (profile.subject_to_bsl) {
            bsl_breeds_.insert(profile.breed_code);
        }
        if (profile.commonly_restricted) {
            restricted_breeds_.insert(profile.breed_code);
        }
    }
}

void BreedInterventionModule::loadBSLData() {
    // Load sample BSL data
    std::vector<BSLLocation> locations = {
        {"denver_co", "Denver", "", "CO", "US",
         {"pit_bull"}, {}, "ban", "Denver Pit Bull Ban",
         "1989-05-09", "Lifted in 2021 with restrictions", true},

        {"miami_dade_fl", "", "Miami-Dade", "FL", "US",
         {"pit_bull"}, {}, "ban", "Miami-Dade County Pit Bull Ban",
         "1989-09-05", "", true},

        {"prince_georges_md", "", "Prince George's", "MD", "US",
         {"pit_bull", "rottweiler", "doberman"}, {},
         "ban", "Prince George's County Dangerous Dog Law",
         "1996-01-01", "", true},

        {"aurora_co", "Aurora", "", "CO", "US",
         {"pit_bull"}, {}, "ban", "Aurora Pit Bull Ban",
         "2005-06-20", "", true}
    };

    std::lock_guard<std::mutex> lock(data_mutex_);
    bsl_locations_ = locations;
}

void BreedInterventionModule::loadResources() {
    // Load pet-friendly resources
    pet_friendly_resources_ = {
        {"res_001", "State Farm", "insurance",
         "One of the few major insurers that doesn't discriminate by breed",
         {}, {"national"}, "https://statefarm.com", "", true, "2024-01-01"},

        {"res_002", "USAA", "insurance",
         "No breed restrictions for pet liability coverage",
         {}, {"national"}, "https://usaa.com", "", true, "2024-01-01"},

        {"res_003", "Apartments.com Pit Bull Friendly", "housing",
         "Search filter for pit bull friendly apartments",
         {"pit_bull", "all"}, {"national"},
         "https://apartments.com", "", true, "2024-01-01"},

        {"res_004", "My Pit Bull is Family", "housing",
         "Database of pit bull friendly housing",
         {"pit_bull"}, {"national"},
         "https://mypitbullisfamily.org", "", true, "2024-01-01"}
    };

    // Load advocacy resources
    advocacy_resources_ = {
        {"adv_001", "ASPCA Position on Pit Bulls", "article",
         "https://aspca.org/about-us/aspca-policy-and-position-statements/position-statement-pit-bulls",
         {"pit_bull"}, "Official ASPCA position against BSL", {"bsl", "advocacy"}},

        {"adv_002", "Animal Farm Foundation", "organization",
         "https://animalfarmfoundation.org",
         {"pit_bull", "all"}, "Advocacy for equal treatment of all dogs", {"advocacy", "education"}},

        {"adv_003", "Best Friends Animal Society BSL Toolkit", "article",
         "https://bestfriends.org/resources/breed-discriminatory-legislation-toolkit",
         {"all"}, "Resources for fighting BSL", {"bsl", "toolkit", "advocacy"}}
    };
}

std::string BreedInterventionModule::normalizeBreedCode(const std::string& breed) {
    std::string normalized = breed;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

    // Replace spaces with underscores
    std::replace(normalized.begin(), normalized.end(), ' ', '_');

    // Common normalizations
    if (normalized == "pitbull" || normalized == "pittie" ||
        normalized == "american_pit_bull_terrier" || normalized == "apbt") {
        return "pit_bull";
    }
    if (normalized == "rottie") {
        return "rottweiler";
    }
    if (normalized == "gsd" || normalized == "german_shepherd_dog") {
        return "german_shepherd";
    }
    if (normalized == "dobie" || normalized == "dobermann") {
        return "doberman";
    }

    return normalized;
}

bool BreedInterventionModule::breedMatchesCategory(const std::string& breed_code,
                                                     const std::string& category) {
    std::string normalized_breed = normalizeBreedCode(breed_code);
    std::string normalized_category = normalizeBreedCode(category);

    if (normalized_breed == normalized_category) {
        return true;
    }

    // Pit bull variations
    if (normalized_category == "pit_bull") {
        if (normalized_breed.find("pit") != std::string::npos ||
            normalized_breed.find("staff") != std::string::npos ||
            normalized_breed.find("bully") != std::string::npos) {
            return true;
        }
    }

    return false;
}

} // namespace wtl::modules::intervention

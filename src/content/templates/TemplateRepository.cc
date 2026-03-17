/**
 * @file TemplateRepository.cc
 * @brief Implementation of TemplateRepository
 * @see TemplateRepository.h for documentation
 */

#include "content/templates/TemplateRepository.h"
#include "content/templates/TemplateEngine.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace wtl::content::templates {

using namespace ::wtl::core::debug;

// ============================================================================
// TEMPLATE METADATA IMPLEMENTATION
// ============================================================================

Json::Value TemplateMetadata::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["category"] = category;
    json["type"] = type;
    json["description"] = description;
    json["version"] = version;
    json["is_active"] = is_active;
    json["is_default"] = is_default;
    json["overlay_style"] = overlay_style;
    json["character_limit"] = character_limit;
    json["created_by"] = created_by;
    json["updated_by"] = updated_by;

    Json::Value urgency_arr(Json::arrayValue);
    for (const auto& u : use_for_urgency) {
        urgency_arr.append(u);
    }
    json["use_for_urgency"] = urgency_arr;

    Json::Value platforms_arr(Json::arrayValue);
    for (const auto& p : use_for_platforms) {
        platforms_arr.append(p);
    }
    json["use_for_platforms"] = platforms_arr;

    Json::Value hashtags_arr(Json::arrayValue);
    for (const auto& h : hashtags) {
        hashtags_arr.append(h);
    }
    json["hashtags"] = hashtags_arr;

    // Format timestamps
    auto to_iso = [](const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    };

    json["created_at"] = to_iso(created_at);
    json["updated_at"] = to_iso(updated_at);

    return json;
}

TemplateMetadata TemplateMetadata::fromJson(const Json::Value& json) {
    TemplateMetadata meta;
    meta.id = json.get("id", "").asString();
    meta.name = json.get("name", "").asString();
    meta.category = json.get("category", "").asString();
    meta.type = json.get("type", "").asString();
    meta.description = json.get("description", "").asString();
    meta.version = json.get("version", 1).asInt();
    meta.is_active = json.get("is_active", true).asBool();
    meta.is_default = json.get("is_default", false).asBool();
    meta.overlay_style = json.get("overlay_style", "").asString();
    meta.character_limit = json.get("character_limit", 0).asInt();
    meta.created_by = json.get("created_by", "").asString();
    meta.updated_by = json.get("updated_by", "").asString();

    if (json.isMember("use_for_urgency") && json["use_for_urgency"].isArray()) {
        for (const auto& u : json["use_for_urgency"]) {
            meta.use_for_urgency.push_back(u.asString());
        }
    }

    if (json.isMember("use_for_platforms") && json["use_for_platforms"].isArray()) {
        for (const auto& p : json["use_for_platforms"]) {
            meta.use_for_platforms.push_back(p.asString());
        }
    }

    if (json.isMember("hashtags") && json["hashtags"].isArray()) {
        for (const auto& h : json["hashtags"]) {
            meta.hashtags.push_back(h.asString());
        }
    }

    // Parse timestamps (default to now if not provided)
    auto now = std::chrono::system_clock::now();
    meta.created_at = now;
    meta.updated_at = now;

    return meta;
}

// ============================================================================
// TEMPLATE IMPLEMENTATION
// ============================================================================

bool Template::isValid() const {
    return !metadata.name.empty() && !metadata.category.empty() && !content.empty();
}

Json::Value Template::toJson() const {
    Json::Value json = metadata.toJson();
    json["content"] = content;
    json["variables"] = variables;
    json["config"] = config;

    // Also include flattened fields for template loading
    if (json.isMember("hashtags")) {
        json["hashtags"] = metadata.toJson()["hashtags"];
    }

    return json;
}

Template Template::fromJson(const Json::Value& json) {
    Template tpl;
    tpl.metadata = TemplateMetadata::fromJson(json);

    // Support multiple content field names
    if (json.isMember("content")) {
        tpl.content = json["content"].asString();
    } else if (json.isMember("caption")) {
        tpl.content = json["caption"].asString();
    } else if (json.isMember("template")) {
        tpl.content = json["template"].asString();
    } else if (json.isMember("body")) {
        tpl.content = json["body"].asString();
    }

    tpl.variables = json.get("variables", Json::Value(Json::objectValue));
    tpl.config = json.get("config", Json::Value(Json::objectValue));

    return tpl;
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

TemplateRepository& TemplateRepository::getInstance() {
    static TemplateRepository instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void TemplateRepository::initialize(const std::string& templates_dir, bool use_database) {
    std::unique_lock lock(mutex_);

    templates_dir_ = templates_dir;
    use_database_ = use_database;

    // Clear existing cache
    cache_.clear();
    id_to_key_.clear();
    category_index_.clear();

    initialized_ = true;

    // Load templates
    lock.unlock();
    reloadAll();
}

void TemplateRepository::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();
        std::string templates_dir = config.getString("templates.directory", "content_templates");
        bool use_database = config.getBool("templates.use_database", true);
        initialize(templates_dir, use_database);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to initialize TemplateRepository from config: " + std::string(e.what()),
                         {});
        initialize();
    }
}

// ============================================================================
// TEMPLATE RETRIEVAL
// ============================================================================

std::optional<Template> TemplateRepository::getTemplate(const std::string& category, const std::string& name) {
    std::string key = getCacheKey(category, name);

    {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            cache_hits_++;
            return it->second;
        }
    }

    cache_misses_++;

    // Try to load from file
    std::string file_path = templates_dir_ + "/" + category + "/" + name + ".json";
    auto tpl = loadTemplateFile(file_path);

    if (tpl) {
        // Cache it
        std::unique_lock lock(mutex_);
        cache_[key] = *tpl;
        if (!tpl->metadata.id.empty()) {
            id_to_key_[tpl->metadata.id] = key;
        }
        category_index_[category].push_back(key);
        return tpl;
    }

    return std::nullopt;
}

std::optional<Template> TemplateRepository::getTemplateById(const std::string& id) {
    std::shared_lock lock(mutex_);

    auto it = id_to_key_.find(id);
    if (it != id_to_key_.end()) {
        auto tpl_it = cache_.find(it->second);
        if (tpl_it != cache_.end()) {
            cache_hits_++;
            return tpl_it->second;
        }
    }

    cache_misses_++;
    return std::nullopt;
}

std::vector<Template> TemplateRepository::getTemplatesByCategory(const std::string& category) {
    std::vector<Template> result;
    std::shared_lock lock(mutex_);

    auto it = category_index_.find(category);
    if (it != category_index_.end()) {
        for (const auto& key : it->second) {
            auto tpl_it = cache_.find(key);
            if (tpl_it != cache_.end()) {
                result.push_back(tpl_it->second);
            }
        }
    }

    return result;
}

std::vector<Template> TemplateRepository::getTemplatesForUrgency(const std::string& urgency_level) {
    std::vector<Template> result;
    std::shared_lock lock(mutex_);

    for (const auto& [key, tpl] : cache_) {
        const auto& urgencies = tpl.metadata.use_for_urgency;
        if (std::find(urgencies.begin(), urgencies.end(), urgency_level) != urgencies.end()) {
            result.push_back(tpl);
        }
    }

    return result;
}

std::optional<Template> TemplateRepository::getDefaultTemplate(const std::string& type) {
    std::shared_lock lock(mutex_);

    for (const auto& [key, tpl] : cache_) {
        if (tpl.metadata.type == type && tpl.metadata.is_default) {
            return tpl;
        }
    }

    // If no default, return first matching type
    for (const auto& [key, tpl] : cache_) {
        if (tpl.metadata.type == type) {
            return tpl;
        }
    }

    return std::nullopt;
}

std::vector<std::string> TemplateRepository::getCategories() {
    std::vector<std::string> categories;
    std::shared_lock lock(mutex_);

    for (const auto& [category, _] : category_index_) {
        categories.push_back(category);
    }

    return categories;
}

std::vector<std::string> TemplateRepository::getTemplateNames(const std::string& category) {
    std::vector<std::string> names;
    std::shared_lock lock(mutex_);

    auto it = category_index_.find(category);
    if (it != category_index_.end()) {
        for (const auto& key : it->second) {
            auto tpl_it = cache_.find(key);
            if (tpl_it != cache_.end()) {
                names.push_back(tpl_it->second.metadata.name);
            }
        }
    }

    return names;
}

// ============================================================================
// TEMPLATE MANAGEMENT
// ============================================================================

Template TemplateRepository::saveTemplate(const Template& tpl, bool to_database) {
    Template saved = tpl;

    // Generate ID if not present
    if (saved.metadata.id.empty()) {
        // Simple UUID-like ID generation
        std::ostringstream oss;
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        oss << std::hex << now << "-" << std::rand();
        saved.metadata.id = oss.str();
    }

    // Update timestamps
    auto now = std::chrono::system_clock::now();
    if (saved.metadata.created_at == std::chrono::system_clock::time_point{}) {
        saved.metadata.created_at = now;
    }
    saved.metadata.updated_at = now;

    // Save to file
    saveTemplateToFile(saved);

    // Save to database if requested
    if (to_database && use_database_) {
        try {
            saved = saveTemplateToDatabase(saved);
        } catch (const std::exception& e) {
            WTL_CAPTURE_WARNING(ErrorCategory::DATABASE,
                               "Failed to save template to database: " + std::string(e.what()),
                               {{"template_name", saved.metadata.name}});
        }
    }

    // Update cache
    std::string key = getCacheKey(saved.metadata.category, saved.metadata.name);
    {
        std::unique_lock lock(mutex_);
        cache_[key] = saved;
        id_to_key_[saved.metadata.id] = key;
        auto& cat_index = category_index_[saved.metadata.category];
        if (std::find(cat_index.begin(), cat_index.end(), key) == cat_index.end()) {
            cat_index.push_back(key);
        }
    }

    return saved;
}

std::optional<Template> TemplateRepository::updateTemplate(const std::string& id, const Template& tpl) {
    std::unique_lock lock(mutex_);

    auto it = id_to_key_.find(id);
    if (it == id_to_key_.end()) {
        return std::nullopt;
    }

    Template updated = tpl;
    updated.metadata.id = id;
    updated.metadata.updated_at = std::chrono::system_clock::now();
    updated.metadata.version = cache_[it->second].metadata.version + 1;

    // Update cache
    cache_[it->second] = updated;

    lock.unlock();

    // Save to storage
    saveTemplateToFile(updated);

    if (use_database_) {
        try {
            saveTemplateToDatabase(updated);
        } catch (const std::exception& e) {
            WTL_CAPTURE_WARNING(ErrorCategory::DATABASE,
                               "Failed to update template in database: " + std::string(e.what()),
                               {{"template_id", id}});
        }
    }

    return updated;
}

bool TemplateRepository::deleteTemplate(const std::string& id) {
    std::unique_lock lock(mutex_);

    auto it = id_to_key_.find(id);
    if (it == id_to_key_.end()) {
        return false;
    }

    std::string key = it->second;
    auto tpl_it = cache_.find(key);
    if (tpl_it != cache_.end()) {
        // Remove from category index
        std::string category = tpl_it->second.metadata.category;
        auto& cat_index = category_index_[category];
        cat_index.erase(std::remove(cat_index.begin(), cat_index.end(), key), cat_index.end());

        // Remove from cache
        cache_.erase(tpl_it);
    }

    id_to_key_.erase(it);

    // Delete file
    std::string file_path = templates_dir_ + "/" + key + ".json";
    try {
        std::filesystem::remove(file_path);
    } catch (const std::exception& e) {
        LOG_WARN << "TemplateRepository: " << e.what();
    } catch (...) {
        LOG_WARN << "TemplateRepository: unknown error";
    }

    return true;
}

std::optional<Template> TemplateRepository::createVersion(const std::string& id, const std::string& new_content) {
    auto existing = getTemplateById(id);
    if (!existing) {
        return std::nullopt;
    }

    Template new_version = *existing;
    new_version.content = new_content;
    new_version.metadata.version++;
    new_version.metadata.updated_at = std::chrono::system_clock::now();

    return updateTemplate(id, new_version);
}

std::vector<Template> TemplateRepository::getVersionHistory(const std::string& category, const std::string& name) {
    // For file-based storage, we only have the current version
    // Database storage would have version history
    std::vector<Template> history;

    auto current = getTemplate(category, name);
    if (current) {
        history.push_back(*current);
    }

    return history;
}

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

int TemplateRepository::reloadAll() {
    int count = 0;

    // Clear cache
    {
        std::unique_lock lock(mutex_);
        cache_.clear();
        id_to_key_.clear();
        category_index_.clear();
    }

    // Load from files
    count += loadFromFiles();

    // Load from database (may override file versions)
    if (use_database_) {
        count += loadFromDatabase();
    }

    load_count_ = count;
    return count;
}

int TemplateRepository::reloadCategory(const std::string& category) {
    int count = 0;

    // Remove category from cache
    {
        std::unique_lock lock(mutex_);
        auto it = category_index_.find(category);
        if (it != category_index_.end()) {
            for (const auto& key : it->second) {
                auto tpl_it = cache_.find(key);
                if (tpl_it != cache_.end()) {
                    if (!tpl_it->second.metadata.id.empty()) {
                        id_to_key_.erase(tpl_it->second.metadata.id);
                    }
                    cache_.erase(tpl_it);
                }
            }
            category_index_.erase(it);
        }
    }

    // Reload from file
    std::string category_dir = templates_dir_ + "/" + category;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(category_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto tpl = loadTemplateFile(entry.path().string());
                if (tpl) {
                    std::string key = getCacheKey(category, tpl->metadata.name);
                    std::unique_lock lock(mutex_);
                    cache_[key] = *tpl;
                    if (!tpl->metadata.id.empty()) {
                        id_to_key_[tpl->metadata.id] = key;
                    }
                    category_index_[category].push_back(key);
                    count++;
                }
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(ErrorCategory::FILE_IO,
                           "Error reloading category: " + std::string(e.what()),
                           {{"category", category}});
    }

    return count;
}

void TemplateRepository::clearCache() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    id_to_key_.clear();
    category_index_.clear();
}

int TemplateRepository::preloadAll() {
    return reloadAll();
}

// ============================================================================
// VALIDATION
// ============================================================================

TemplateValidationResult TemplateRepository::validateTemplate(const std::string& content) {
    TemplateValidationResult result;

    try {
        auto& engine = TemplateEngine::getInstance();
        auto [is_valid, error] = engine.validate(content);
        result.is_valid = is_valid;
        result.error_message = error;

        if (is_valid) {
            result.required_variables = engine.getRequiredVariables(content);
            result.estimated_render_length = static_cast<int>(content.length());
        }

        // Add warnings for common issues
        if (content.length() > 2200) {
            result.warnings.push_back("Content exceeds typical social media character limit (2200)");
        }
        if (content.find("{{") == std::string::npos) {
            result.warnings.push_back("Template contains no variables - consider adding dynamic content");
        }

    } catch (const std::exception& e) {
        result.is_valid = false;
        result.error_message = e.what();
    }

    return result;
}

TemplateValidationResult TemplateRepository::validateTemplate(const Template& tpl) {
    auto result = validateTemplate(tpl.content);

    // Additional validation for complete template
    if (tpl.metadata.name.empty()) {
        result.is_valid = false;
        result.error_message = "Template name is required";
    }
    if (tpl.metadata.category.empty()) {
        result.is_valid = false;
        result.error_message = "Template category is required";
    }

    // Check character limit if specified
    if (tpl.metadata.character_limit > 0 &&
        result.estimated_render_length > tpl.metadata.character_limit) {
        result.warnings.push_back("Template may exceed character limit of " +
                                  std::to_string(tpl.metadata.character_limit));
    }

    return result;
}

bool TemplateRepository::isNameAvailable(const std::string& category, const std::string& name) {
    std::string key = getCacheKey(category, name);
    std::shared_lock lock(mutex_);
    return cache_.find(key) == cache_.end();
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value TemplateRepository::getStats() const {
    Json::Value stats;
    stats["total_templates"] = static_cast<Json::UInt>(getCount());
    stats["load_count"] = static_cast<Json::UInt64>(load_count_.load());
    stats["cache_hits"] = static_cast<Json::UInt64>(cache_hits_.load());
    stats["cache_misses"] = static_cast<Json::UInt64>(cache_misses_.load());

    Json::Value by_category;
    {
        std::shared_lock lock(mutex_);
        for (const auto& [category, keys] : category_index_) {
            by_category[category] = static_cast<Json::UInt>(keys.size());
        }
    }
    stats["by_category"] = by_category;

    if (cache_hits_ + cache_misses_ > 0) {
        double hit_rate = static_cast<double>(cache_hits_) /
                          static_cast<double>(cache_hits_ + cache_misses_) * 100.0;
        stats["cache_hit_rate_percent"] = hit_rate;
    }

    return stats;
}

size_t TemplateRepository::getCount() const {
    std::shared_lock lock(mutex_);
    return cache_.size();
}

size_t TemplateRepository::getCountByCategory(const std::string& category) const {
    std::shared_lock lock(mutex_);
    auto it = category_index_.find(category);
    return it != category_index_.end() ? it->second.size() : 0;
}

// ============================================================================
// UTILITY
// ============================================================================

std::string TemplateRepository::categoryToString(TemplateCategory category) {
    switch (category) {
        case TemplateCategory::TIKTOK: return "tiktok";
        case TemplateCategory::INSTAGRAM: return "instagram";
        case TemplateCategory::FACEBOOK: return "facebook";
        case TemplateCategory::TWITTER: return "twitter";
        case TemplateCategory::EMAIL: return "email";
        case TemplateCategory::SMS: return "sms";
        case TemplateCategory::PUSH: return "push";
        case TemplateCategory::SOCIAL: return "social";
        case TemplateCategory::CUSTOM: return "custom";
        default: return "unknown";
    }
}

TemplateCategory TemplateRepository::stringToCategory(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "tiktok") return TemplateCategory::TIKTOK;
    if (s == "instagram") return TemplateCategory::INSTAGRAM;
    if (s == "facebook") return TemplateCategory::FACEBOOK;
    if (s == "twitter") return TemplateCategory::TWITTER;
    if (s == "email") return TemplateCategory::EMAIL;
    if (s == "sms") return TemplateCategory::SMS;
    if (s == "push") return TemplateCategory::PUSH;
    if (s == "social") return TemplateCategory::SOCIAL;

    return TemplateCategory::CUSTOM;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

int TemplateRepository::loadFromFiles() {
    int count = 0;

    try {
        for (const auto& category_entry : std::filesystem::directory_iterator(templates_dir_)) {
            if (category_entry.is_directory()) {
                std::string category = category_entry.path().filename().string();

                for (const auto& entry : std::filesystem::directory_iterator(category_entry.path())) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".json" || ext == ".html" || ext == ".txt") {
                            auto tpl = loadTemplateFile(entry.path().string());
                            if (tpl) {
                                // Set category and name from path if not in file
                                if (tpl->metadata.category.empty()) {
                                    tpl->metadata.category = category;
                                }
                                if (tpl->metadata.name.empty()) {
                                    tpl->metadata.name = entry.path().stem().string();
                                }

                                std::string key = getCacheKey(tpl->metadata.category, tpl->metadata.name);
                                std::unique_lock lock(mutex_);
                                cache_[key] = *tpl;
                                if (!tpl->metadata.id.empty()) {
                                    id_to_key_[tpl->metadata.id] = key;
                                }
                                category_index_[tpl->metadata.category].push_back(key);
                                count++;
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(ErrorCategory::FILE_IO,
                           "Error loading templates from files: " + std::string(e.what()),
                           {{"directory", templates_dir_}});
    }

    return count;
}

int TemplateRepository::loadFromDatabase() {
    // Database loading would be implemented here
    // For now, return 0 as we primarily use file-based templates
    return 0;
}

std::optional<Template> TemplateRepository::loadTemplateFile(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::string ext = std::filesystem::path(file_path).extension().string();
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        Template tpl;

        if (ext == ".json") {
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::istringstream iss(content);
            std::string errors;

            if (Json::parseFromStream(builder, iss, &root, &errors)) {
                tpl = Template::fromJson(root);
            } else {
                WTL_CAPTURE_WARNING(ErrorCategory::VALIDATION,
                                   "Failed to parse template JSON: " + errors,
                                   {{"file", file_path}});
                return std::nullopt;
            }
        } else {
            // For non-JSON files, use content directly
            tpl.content = content;
        }

        // Set name from filename if not present
        if (tpl.metadata.name.empty()) {
            tpl.metadata.name = std::filesystem::path(file_path).stem().string();
        }

        return tpl;

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(ErrorCategory::FILE_IO,
                           "Error loading template file: " + std::string(e.what()),
                           {{"file", file_path}});
        return std::nullopt;
    }
}

bool TemplateRepository::saveTemplateToFile(const Template& tpl) {
    try {
        std::string dir_path = templates_dir_ + "/" + tpl.metadata.category;
        std::filesystem::create_directories(dir_path);

        std::string file_path = dir_path + "/" + tpl.metadata.name + ".json";
        std::ofstream file(file_path);

        if (!file.is_open()) {
            return false;
        }

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(tpl.toJson(), &file);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::FILE_IO,
                         "Error saving template file: " + std::string(e.what()),
                         {{"template_name", tpl.metadata.name}});
        return false;
    }
}

Template TemplateRepository::saveTemplateToDatabase(const Template& tpl) {
    // Database save would be implemented here
    // For now, just return the template as-is
    return tpl;
}

std::string TemplateRepository::getCacheKey(const std::string& category, const std::string& name) const {
    return category + "/" + name;
}

} // namespace wtl::content::templates

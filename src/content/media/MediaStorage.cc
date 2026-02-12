/**
 * @file MediaStorage.cc
 * @brief Implementation of media file storage management
 * @see MediaStorage.h for documentation
 */

#include "content/media/MediaStorage.h"

// Standard library includes
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::media {

namespace fs = std::filesystem;

// ============================================================================
// MEDIA RECORD IMPLEMENTATION
// ============================================================================

Json::Value MediaRecord::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["dog_id"] = dog_id;
    json["type"] = MediaStorage::mediaTypeToString(type);
    json["filename"] = filename;
    json["storage_path"] = storage_path;
    json["content_type"] = content_type;
    json["file_size"] = static_cast<Json::Int64>(file_size);
    json["width"] = width;
    json["height"] = height;
    json["duration"] = duration;
    json["backend"] = (backend == StorageBackend::S3) ? "s3" : "local";
    json["url"] = url;

    // Convert timestamps
    auto created_time = std::chrono::system_clock::to_time_t(created_at);
    std::ostringstream created_ss;
    created_ss << std::put_time(std::gmtime(&created_time), "%Y-%m-%dT%H:%M:%SZ");
    json["created_at"] = created_ss.str();

    if (expires_at != std::chrono::system_clock::time_point{}) {
        auto expires_time = std::chrono::system_clock::to_time_t(expires_at);
        std::ostringstream expires_ss;
        expires_ss << std::put_time(std::gmtime(&expires_time), "%Y-%m-%dT%H:%M:%SZ");
        json["expires_at"] = expires_ss.str();
    }

    json["metadata"] = metadata;
    return json;
}

MediaRecord MediaRecord::fromJson(const Json::Value& json) {
    MediaRecord record;
    record.id = json.get("id", "").asString();
    record.dog_id = json.get("dog_id", "").asString();
    record.type = MediaStorage::stringToMediaType(json.get("type", "image").asString());
    record.filename = json.get("filename", "").asString();
    record.storage_path = json.get("storage_path", "").asString();
    record.content_type = json.get("content_type", "").asString();
    record.file_size = json.get("file_size", 0).asInt64();
    record.width = json.get("width", 0).asInt();
    record.height = json.get("height", 0).asInt();
    record.duration = json.get("duration", 0.0).asDouble();
    record.backend = (json.get("backend", "local").asString() == "s3") ?
                     StorageBackend::S3 : StorageBackend::LOCAL;
    record.url = json.get("url", "").asString();
    if (json.isMember("metadata")) {
        record.metadata = json["metadata"];
    }
    return record;
}

MediaRecord MediaRecord::fromDbRow(const std::vector<std::string>& row) {
    MediaRecord record;
    if (row.size() >= 12) {
        record.id = row[0];
        record.dog_id = row[1];
        record.type = MediaStorage::stringToMediaType(row[2]);
        record.filename = row[3];
        record.storage_path = row[4];
        record.content_type = row[5];
        record.file_size = std::stoll(row[6]);
        record.width = std::stoi(row[7]);
        record.height = std::stoi(row[8]);
        record.duration = std::stod(row[9]);
        record.backend = (row[10] == "s3") ? StorageBackend::S3 : StorageBackend::LOCAL;
        record.url = row[11];
    }
    return record;
}

bool MediaRecord::isExpired() const {
    if (expires_at == std::chrono::system_clock::time_point{}) {
        return false;
    }
    return std::chrono::system_clock::now() > expires_at;
}

// ============================================================================
// STORAGE RESULT IMPLEMENTATION
// ============================================================================

Json::Value StorageResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["media_id"] = media_id;
    json["url"] = url;
    json["storage_path"] = storage_path;
    json["error_message"] = error_message;
    json["file_size"] = static_cast<Json::Int64>(file_size);
    return json;
}

// ============================================================================
// STORAGE CONFIG IMPLEMENTATION
// ============================================================================

Json::Value StorageConfig::toJson() const {
    Json::Value json;
    json["default_backend"] = (default_backend == StorageBackend::S3) ? "s3" : "local";
    json["local_base_path"] = local_base_path;
    json["local_url_prefix"] = local_url_prefix;
    json["s3_bucket"] = s3_bucket;
    json["s3_region"] = s3_region;
    json["s3_endpoint"] = s3_endpoint;
    json["s3_url_prefix"] = s3_url_prefix;
    json["max_file_size"] = static_cast<Json::Int64>(max_file_size);
    json["retention_days"] = retention_days;
    json["auto_generate_thumbnails"] = auto_generate_thumbnails;
    return json;
}

StorageConfig StorageConfig::fromJson(const Json::Value& json) {
    StorageConfig config;
    std::string backend = json.get("default_backend", "local").asString();
    config.default_backend = (backend == "s3") ? StorageBackend::S3 : StorageBackend::LOCAL;
    config.local_base_path = json.get("local_base_path", "media").asString();
    config.local_url_prefix = json.get("local_url_prefix", "/media").asString();
    config.s3_bucket = json.get("s3_bucket", "").asString();
    config.s3_region = json.get("s3_region", "us-east-1").asString();
    config.s3_access_key = json.get("s3_access_key", "").asString();
    config.s3_secret_key = json.get("s3_secret_key", "").asString();
    config.s3_endpoint = json.get("s3_endpoint", "").asString();
    config.s3_url_prefix = json.get("s3_url_prefix", "").asString();
    config.max_file_size = json.get("max_file_size", 100 * 1024 * 1024).asInt64();
    config.retention_days = json.get("retention_days", 365).asInt();
    config.auto_generate_thumbnails = json.get("auto_generate_thumbnails", true).asBool();
    return config;
}

// ============================================================================
// MEDIA STORAGE IMPLEMENTATION
// ============================================================================

MediaStorage& MediaStorage::getInstance() {
    static MediaStorage instance;
    return instance;
}

bool MediaStorage::initialize(const StorageConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = config;

    // Create local storage directory if using local backend
    if (config_.default_backend == StorageBackend::LOCAL ||
        config_.default_backend == StorageBackend::AUTO) {
        try {
            if (!fs::exists(config_.local_base_path)) {
                fs::create_directories(config_.local_base_path);
            }

            // Create subdirectories for each media type
            fs::create_directories(config_.local_base_path + "/images");
            fs::create_directories(config_.local_base_path + "/videos");
            fs::create_directories(config_.local_base_path + "/thumbnails");
            fs::create_directories(config_.local_base_path + "/share_cards");
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::FILE_IO,
                "Failed to create storage directories: " + std::string(e.what()),
                {{"path", config_.local_base_path}}
            );
            return false;
        }
    }

    initialized_ = true;
    return true;
}

bool MediaStorage::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        StorageConfig storage_config;
        std::string backend = config.getString("media.storage.backend", "local");
        storage_config.default_backend = (backend == "s3") ?
                                         StorageBackend::S3 : StorageBackend::LOCAL;

        storage_config.local_base_path = config.getString("media.storage.local_path", "media");
        storage_config.local_url_prefix = config.getString("media.storage.local_url_prefix", "/media");

        storage_config.s3_bucket = config.getString("media.storage.s3_bucket", "");
        storage_config.s3_region = config.getString("media.storage.s3_region", "us-east-1");
        storage_config.s3_access_key = config.getString("media.storage.s3_access_key", "");
        storage_config.s3_secret_key = config.getString("media.storage.s3_secret_key", "");
        storage_config.s3_endpoint = config.getString("media.storage.s3_endpoint", "");

        storage_config.max_file_size = config.getInt("media.storage.max_file_size",
                                                     100 * 1024 * 1024);
        storage_config.retention_days = config.getInt("media.storage.retention_days", 365);

        return initialize(storage_config);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize MediaStorage from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

bool MediaStorage::isInitialized() const {
    return initialized_.load();
}

StorageResult MediaStorage::saveMedia(
    const std::string& file_path,
    MediaType type,
    const std::string& dog_id,
    const Json::Value& metadata
) {
    StorageResult result;

    if (!fs::exists(file_path)) {
        result.error_message = "Source file not found: " + file_path;
        return result;
    }

    // Check file size
    int64_t file_size = fs::file_size(file_path);
    if (file_size > config_.max_file_size) {
        result.error_message = "File exceeds maximum size limit";
        return result;
    }

    // Generate media ID and storage path
    std::string media_id = generateMediaId();
    std::string filename = fs::path(file_path).filename().string();
    std::string storage_path = generateStoragePath(dog_id, type, filename);
    std::string content_type = getContentType(filename);

    // Determine backend
    StorageBackend backend = config_.default_backend;
    if (backend == StorageBackend::AUTO) {
        // Use S3 for large files if configured, otherwise local
        backend = (!config_.s3_bucket.empty() && file_size > 10 * 1024 * 1024) ?
                  StorageBackend::S3 : StorageBackend::LOCAL;
    }

    // Save file
    bool save_success = false;
    if (backend == StorageBackend::S3) {
        save_success = saveToS3(file_path, storage_path, content_type);
    } else {
        save_success = saveToLocal(file_path, storage_path);
    }

    if (!save_success) {
        result.error_message = "Failed to save file to storage";
        return result;
    }

    // Create media record
    MediaRecord record;
    record.id = media_id;
    record.dog_id = dog_id;
    record.type = type;
    record.filename = filename;
    record.storage_path = storage_path;
    record.content_type = content_type;
    record.file_size = file_size;
    record.backend = backend;
    record.created_at = std::chrono::system_clock::now();
    record.metadata = metadata;

    // Generate URL
    if (backend == StorageBackend::S3) {
        record.url = config_.s3_url_prefix + "/" + storage_path;
    } else {
        record.url = config_.local_url_prefix + "/" + storage_path;
    }

    // Save record to database
    if (!saveRecord(record)) {
        // Cleanup file on record save failure
        if (backend == StorageBackend::S3) {
            deleteFromS3(storage_path);
        } else {
            deleteFromLocal(storage_path);
        }
        result.error_message = "Failed to save media record";
        return result;
    }

    result.success = true;
    result.media_id = media_id;
    result.url = record.url;
    result.storage_path = storage_path;
    result.file_size = file_size;

    total_saved_++;
    total_bytes_saved_ += file_size;

    return result;
}

StorageResult MediaStorage::saveMediaData(
    const void* data,
    size_t data_size,
    const std::string& filename,
    MediaType type,
    const std::string& dog_id,
    const Json::Value& metadata
) {
    StorageResult result;

    if (static_cast<int64_t>(data_size) > config_.max_file_size) {
        result.error_message = "Data exceeds maximum size limit";
        return result;
    }

    // Write to temporary file
    std::string temp_path = config_.local_base_path + "/temp_" + generateMediaId() +
                            "_" + filename;

    try {
        std::ofstream file(temp_path, std::ios::binary);
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(data_size));
        file.close();
    } catch (const std::exception& e) {
        result.error_message = "Failed to write temporary file: " + std::string(e.what());
        return result;
    }

    // Use saveMedia for actual storage
    result = saveMedia(temp_path, type, dog_id, metadata);

    // Clean up temp file
    try {
        fs::remove(temp_path);
    } catch (...) {
        // Ignore cleanup errors
    }

    return result;
}

StorageResult MediaStorage::saveAndMove(
    const std::string& file_path,
    MediaType type,
    const std::string& dog_id,
    bool move_file
) {
    StorageResult result = saveMedia(file_path, type, dog_id);

    if (result.success && move_file) {
        try {
            fs::remove(file_path);
        } catch (...) {
            // Ignore errors when removing source
        }
    }

    return result;
}

std::string MediaStorage::getMediaUrl(const std::string& media_id) {
    auto record = getMediaRecord(media_id);
    return record ? record->url : "";
}

std::optional<MediaRecord> MediaStorage::getMediaRecord(const std::string& media_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        pqxx::result result = txn.exec_params(
            "SELECT id, dog_id, type, filename, storage_path, content_type, "
            "file_size, width, height, duration, backend, url, metadata, "
            "created_at, expires_at "
            "FROM media_files WHERE id = $1",
            media_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        MediaRecord record;
        const auto& row = result[0];
        record.id = row["id"].as<std::string>();
        record.dog_id = row["dog_id"].as<std::string>();
        record.type = stringToMediaType(row["type"].as<std::string>());
        record.filename = row["filename"].as<std::string>();
        record.storage_path = row["storage_path"].as<std::string>();
        record.content_type = row["content_type"].as<std::string>();
        record.file_size = row["file_size"].as<int64_t>();
        record.width = row["width"].as<int>(0);
        record.height = row["height"].as<int>(0);
        record.duration = row["duration"].as<double>(0.0);
        record.backend = (row["backend"].as<std::string>() == "s3") ?
                         StorageBackend::S3 : StorageBackend::LOCAL;
        record.url = row["url"].as<std::string>();

        return record;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get media record: " + std::string(e.what()),
            {{"media_id", media_id}}
        );
        return std::nullopt;
    }
}

std::vector<MediaRecord> MediaStorage::getMediaForDog(
    const std::string& dog_id,
    std::optional<MediaType> type
) {
    std::vector<MediaRecord> records;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string query = "SELECT id, dog_id, type, filename, storage_path, "
                            "content_type, file_size, width, height, duration, "
                            "backend, url FROM media_files WHERE dog_id = $1";

        pqxx::result result;
        if (type.has_value()) {
            query += " AND type = $2 ORDER BY created_at DESC";
            result = txn.exec_params(query, dog_id, mediaTypeToString(*type));
        } else {
            query += " ORDER BY created_at DESC";
            result = txn.exec_params(query, dog_id);
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            MediaRecord record;
            record.id = row["id"].as<std::string>();
            record.dog_id = row["dog_id"].as<std::string>();
            record.type = stringToMediaType(row["type"].as<std::string>());
            record.filename = row["filename"].as<std::string>();
            record.storage_path = row["storage_path"].as<std::string>();
            record.content_type = row["content_type"].as<std::string>();
            record.file_size = row["file_size"].as<int64_t>();
            record.width = row["width"].as<int>(0);
            record.height = row["height"].as<int>(0);
            record.duration = row["duration"].as<double>(0.0);
            record.backend = (row["backend"].as<std::string>() == "s3") ?
                             StorageBackend::S3 : StorageBackend::LOCAL;
            record.url = row["url"].as<std::string>();
            records.push_back(record);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get media for dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return records;
}

std::vector<MediaRecord> MediaStorage::getMediaByType(
    MediaType type,
    int limit,
    int offset
) {
    std::vector<MediaRecord> records;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        pqxx::result result = txn.exec_params(
            "SELECT id, dog_id, type, filename, storage_path, content_type, "
            "file_size, width, height, duration, backend, url "
            "FROM media_files WHERE type = $1 "
            "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
            mediaTypeToString(type), limit, offset
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            MediaRecord record;
            record.id = row["id"].as<std::string>();
            record.dog_id = row["dog_id"].as<std::string>();
            record.type = type;
            record.filename = row["filename"].as<std::string>();
            record.storage_path = row["storage_path"].as<std::string>();
            record.content_type = row["content_type"].as<std::string>();
            record.file_size = row["file_size"].as<int64_t>();
            record.backend = (row["backend"].as<std::string>() == "s3") ?
                             StorageBackend::S3 : StorageBackend::LOCAL;
            record.url = row["url"].as<std::string>();
            records.push_back(record);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get media by type: " + std::string(e.what()),
            {}
        );
    }

    return records;
}

bool MediaStorage::downloadMedia(
    const std::string& media_id,
    const std::string& output_path
) {
    auto record = getMediaRecord(media_id);
    if (!record) {
        return false;
    }

    if (record->backend == StorageBackend::LOCAL) {
        std::string source = config_.local_base_path + "/" + record->storage_path;
        try {
            fs::copy_file(source, output_path, fs::copy_options::overwrite_existing);
            return true;
        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::FILE_IO,
                "Failed to download media: " + std::string(e.what()),
                {{"media_id", media_id}}
            );
            return false;
        }
    }

    // S3 download not implemented - would use AWS SDK
    return false;
}

bool MediaStorage::deleteMedia(const std::string& media_id) {
    auto record = getMediaRecord(media_id);
    if (!record) {
        return false;
    }

    // Delete from storage
    bool deleted = false;
    if (record->backend == StorageBackend::S3) {
        deleted = deleteFromS3(record->storage_path);
    } else {
        deleted = deleteFromLocal(record->storage_path);
    }

    if (!deleted) {
        return false;
    }

    // Delete database record
    if (!deleteRecord(media_id)) {
        return false;
    }

    total_deleted_++;
    return true;
}

int MediaStorage::deleteMediaForDog(const std::string& dog_id) {
    auto records = getMediaForDog(dog_id);
    int deleted = 0;

    for (const auto& record : records) {
        if (deleteMedia(record.id)) {
            deleted++;
        }
    }

    return deleted;
}

int MediaStorage::deleteExpiredMedia() {
    int deleted = 0;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        pqxx::result result = txn.exec(
            "SELECT id FROM media_files "
            "WHERE expires_at IS NOT NULL AND expires_at < NOW()"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            if (deleteMedia(row["id"].as<std::string>())) {
                deleted++;
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to delete expired media: " + std::string(e.what()),
            {}
        );
    }

    return deleted;
}

int MediaStorage::cleanupOldMedia(int days_old) {
    int days = days_old > 0 ? days_old : config_.retention_days;
    int deleted = 0;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        pqxx::result result = txn.exec_params(
            "SELECT id FROM media_files "
            "WHERE created_at < NOW() - INTERVAL '$1 days'",
            days
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            if (deleteMedia(row["id"].as<std::string>())) {
                deleted++;
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to cleanup old media: " + std::string(e.what()),
            {}
        );
    }

    return deleted;
}

int MediaStorage::cleanupOrphanedFiles() {
    // This would scan filesystem and compare with database
    // Implementation depends on storage backend
    return 0;
}

Json::Value MediaStorage::getStorageStats() {
    Json::Value stats;

    stats["total_saved"] = static_cast<Json::UInt64>(total_saved_.load());
    stats["total_deleted"] = static_cast<Json::UInt64>(total_deleted_.load());
    stats["total_bytes_saved"] = static_cast<Json::UInt64>(total_bytes_saved_.load());

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        pqxx::result count_result = txn.exec(
            "SELECT COUNT(*) as count, SUM(file_size) as total_size FROM media_files"
        );

        if (!count_result.empty()) {
            stats["total_files"] = count_result[0]["count"].as<int>();
            stats["total_storage_bytes"] = count_result[0]["total_size"].as<int64_t>(0);
        }

        pqxx::result type_result = txn.exec(
            "SELECT type, COUNT(*) as count FROM media_files GROUP BY type"
        );

        Json::Value by_type;
        for (const auto& row : type_result) {
            by_type[row["type"].as<std::string>()] = row["count"].as<int>();
        }
        stats["by_type"] = by_type;

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get storage stats: " + std::string(e.what()),
            {}
        );
    }

    return stats;
}

std::string MediaStorage::generateSignedUrl(
    const std::string& media_id,
    int expiration_seconds
) {
    auto record = getMediaRecord(media_id);
    if (!record) {
        return "";
    }

    if (record->backend == StorageBackend::LOCAL) {
        // Local storage doesn't support signed URLs, return regular URL
        return record->url;
    }

    // S3 signed URL generation would use AWS SDK
    // For now, return regular URL
    return record->url;
}

bool MediaStorage::mediaExists(const std::string& media_id) {
    return getMediaRecord(media_id).has_value();
}

std::string MediaStorage::getContentType(const std::string& filename) {
    std::string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::map<std::string, std::string> content_types = {
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        {".mov", "video/quicktime"},
        {".avi", "video/x-msvideo"},
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".m4a", "audio/mp4"}
    };

    auto it = content_types.find(ext);
    return it != content_types.end() ? it->second : "application/octet-stream";
}

std::string MediaStorage::mediaTypeToString(MediaType type) {
    switch (type) {
        case MediaType::IMAGE:      return "image";
        case MediaType::VIDEO:      return "video";
        case MediaType::THUMBNAIL:  return "thumbnail";
        case MediaType::SHARE_CARD: return "share_card";
        case MediaType::OVERLAY:    return "overlay";
        case MediaType::AUDIO:      return "audio";
        default:                    return "image";
    }
}

MediaType MediaStorage::stringToMediaType(const std::string& str) {
    if (str == "image")      return MediaType::IMAGE;
    if (str == "video")      return MediaType::VIDEO;
    if (str == "thumbnail")  return MediaType::THUMBNAIL;
    if (str == "share_card") return MediaType::SHARE_CARD;
    if (str == "overlay")    return MediaType::OVERLAY;
    if (str == "audio")      return MediaType::AUDIO;
    return MediaType::IMAGE;
}

std::string MediaStorage::generateMediaId() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (dist(gen) & 0xFFFFFFFF)
       << "-"
       << std::setw(4) << (dist(gen) & 0xFFFF)
       << "-"
       << std::setw(4) << (dist(gen) & 0xFFFF)
       << "-"
       << std::setw(4) << (dist(gen) & 0xFFFF)
       << "-"
       << std::setw(12) << (dist(gen) & 0xFFFFFFFFFFFF);

    return ss.str();
}

std::string MediaStorage::generateStoragePath(
    const std::string& dog_id,
    MediaType type,
    const std::string& filename
) {
    std::string type_dir = mediaTypeToString(type) + "s";  // images, videos, etc.

    // Generate date-based path for better organization
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time);

    std::ostringstream path;
    path << type_dir << "/"
         << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << "/"
         << std::setw(2) << (tm.tm_mon + 1) << "/"
         << dog_id << "_" << generateMediaId().substr(0, 8) << "_" << filename;

    return path.str();
}

bool MediaStorage::saveToLocal(
    const std::string& source_path,
    const std::string& storage_path
) {
    std::string full_path = config_.local_base_path + "/" + storage_path;

    try {
        // Create parent directories
        fs::create_directories(fs::path(full_path).parent_path());

        // Copy file
        fs::copy_file(source_path, full_path, fs::copy_options::overwrite_existing);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::FILE_IO,
            "Failed to save to local storage: " + std::string(e.what()),
            {{"source", source_path}, {"target", full_path}}
        );
        return false;
    }
}

bool MediaStorage::saveToS3(
    const std::string& source_path,
    const std::string& storage_path,
    const std::string& content_type
) {
    // S3 upload not implemented - would use AWS SDK
    // For now, fall back to local storage
    return saveToLocal(source_path, storage_path);
}

bool MediaStorage::deleteFromLocal(const std::string& storage_path) {
    std::string full_path = config_.local_base_path + "/" + storage_path;

    try {
        if (fs::exists(full_path)) {
            fs::remove(full_path);
        }
        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::FILE_IO,
            "Failed to delete from local storage: " + std::string(e.what()),
            {{"path", full_path}}
        );
        return false;
    }
}

bool MediaStorage::deleteFromS3(const std::string& storage_path) {
    // S3 delete not implemented - would use AWS SDK
    return deleteFromLocal(storage_path);
}

bool MediaStorage::saveRecord(const MediaRecord& record) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "INSERT INTO media_files "
            "(id, dog_id, type, filename, storage_path, content_type, "
            "file_size, width, height, duration, backend, url, metadata) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)",
            record.id,
            record.dog_id,
            mediaTypeToString(record.type),
            record.filename,
            record.storage_path,
            record.content_type,
            record.file_size,
            record.width,
            record.height,
            record.duration,
            (record.backend == StorageBackend::S3) ? "s3" : "local",
            record.url,
            record.metadata.toStyledString()
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to save media record: " + std::string(e.what()),
            {{"media_id", record.id}}
        );
        return false;
    }
}

bool MediaStorage::deleteRecord(const std::string& media_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM media_files WHERE id = $1",
            media_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to delete media record: " + std::string(e.what()),
            {{"media_id", media_id}}
        );
        return false;
    }
}

} // namespace wtl::content::media

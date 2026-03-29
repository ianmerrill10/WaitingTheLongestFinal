/**
 * @file MediaStorage.h
 * @brief Media file storage management with local and S3 support
 *
 * PURPOSE:
 * Handles storage and retrieval of media files (images, videos, thumbnails).
 * Supports both local filesystem storage and AWS S3 cloud storage.
 *
 * USAGE:
 * auto& storage = MediaStorage::getInstance();
 * auto result = storage.saveMedia(data, MediaType::VIDEO, dog_id);
 * std::string url = storage.getMediaUrl(media_id);
 *
 * DEPENDENCIES:
 * - filesystem (local storage)
 * - AWS SDK (S3 storage, optional)
 * - ErrorCapture, SelfHealing (error handling)
 *
 * MODIFICATION GUIDE:
 * - Add new storage backends by implementing StorageBackend interface
 * - Extend MediaType for new media categories
 * - Add new metadata fields to MediaRecord
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::content::media {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum MediaType
 * @brief Types of media files
 */
enum class MediaType {
    IMAGE = 0,
    VIDEO,
    THUMBNAIL,
    SHARE_CARD,
    OVERLAY,
    AUDIO
};

/**
 * @enum StorageBackend
 * @brief Storage backend types
 */
enum class StorageBackend {
    LOCAL = 0,
    S3,
    AUTO  // Auto-select based on configuration
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MediaRecord
 * @brief Record of a stored media file
 */
struct MediaRecord {
    std::string id;                    ///< Unique media ID
    std::string dog_id;                ///< Associated dog ID
    MediaType type;                    ///< Media type
    std::string filename;              ///< Original filename
    std::string storage_path;          ///< Path/key in storage
    std::string content_type;          ///< MIME content type
    int64_t file_size = 0;             ///< File size in bytes
    int width = 0;                     ///< Width in pixels (for images/videos)
    int height = 0;                    ///< Height in pixels
    double duration = 0.0;             ///< Duration in seconds (for videos/audio)
    StorageBackend backend;            ///< Storage backend used
    std::string url;                   ///< Public URL to access
    std::chrono::system_clock::time_point created_at;  ///< Creation timestamp
    std::chrono::system_clock::time_point expires_at;  ///< Expiration timestamp (optional)
    Json::Value metadata;              ///< Additional metadata

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static MediaRecord fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     */
    static MediaRecord fromDbRow(const std::vector<std::string>& row);

    /**
     * @brief Check if media has expired
     */
    bool isExpired() const;
};

/**
 * @struct StorageResult
 * @brief Result of a storage operation
 */
struct StorageResult {
    bool success = false;              ///< Whether operation succeeded
    std::string media_id;              ///< Generated media ID
    std::string url;                   ///< Public URL
    std::string storage_path;          ///< Internal storage path
    std::string error_message;         ///< Error message if failed
    int64_t file_size = 0;             ///< Stored file size

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct StorageConfig
 * @brief Configuration for storage system
 */
struct StorageConfig {
    StorageBackend default_backend = StorageBackend::LOCAL;

    // Local storage config
    std::string local_base_path = "media";
    std::string local_url_prefix = "/media";

    // S3 storage config
    std::string s3_bucket;
    std::string s3_region = "us-east-1";
    std::string s3_access_key;
    std::string s3_secret_key;
    std::string s3_endpoint;  // For S3-compatible services
    std::string s3_url_prefix;

    // General config
    int64_t max_file_size = 100 * 1024 * 1024;  // 100MB default
    int retention_days = 365;  // Keep files for 1 year by default
    bool auto_generate_thumbnails = true;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static StorageConfig fromJson(const Json::Value& json);
};

// ============================================================================
// MEDIA STORAGE CLASS
// ============================================================================

/**
 * @class MediaStorage
 * @brief Singleton class for media file storage management
 *
 * Handles saving, retrieving, and managing media files with support
 * for local filesystem and AWS S3 storage backends.
 */
class MediaStorage {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the MediaStorage singleton
     */
    static MediaStorage& getInstance();

    // Delete copy/move constructors
    MediaStorage(const MediaStorage&) = delete;
    MediaStorage& operator=(const MediaStorage&) = delete;
    MediaStorage(MediaStorage&&) = delete;
    MediaStorage& operator=(MediaStorage&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize with configuration
     * @param config Storage configuration
     * @return True if initialization successful
     */
    bool initialize(const StorageConfig& config);

    /**
     * @brief Initialize from application config
     * @return True if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if storage is initialized
     * @return True if initialized
     */
    bool isInitialized() const;

    // =========================================================================
    // SAVE OPERATIONS
    // =========================================================================

    /**
     * @brief Save media from file path
     * @param file_path Path to source file
     * @param type Media type
     * @param dog_id Associated dog ID
     * @param metadata Additional metadata
     * @return StorageResult
     */
    StorageResult saveMedia(
        const std::string& file_path,
        MediaType type,
        const std::string& dog_id,
        const Json::Value& metadata = Json::Value()
    );

    /**
     * @brief Save media from binary data
     * @param data Binary data
     * @param data_size Size of data
     * @param filename Original filename
     * @param type Media type
     * @param dog_id Associated dog ID
     * @param metadata Additional metadata
     * @return StorageResult
     */
    StorageResult saveMediaData(
        const void* data,
        size_t data_size,
        const std::string& filename,
        MediaType type,
        const std::string& dog_id,
        const Json::Value& metadata = Json::Value()
    );

    /**
     * @brief Save media and move/copy source file
     * @param file_path Path to source file
     * @param type Media type
     * @param dog_id Associated dog ID
     * @param move_file Move instead of copy
     * @return StorageResult
     */
    StorageResult saveAndMove(
        const std::string& file_path,
        MediaType type,
        const std::string& dog_id,
        bool move_file = true
    );

    // =========================================================================
    // RETRIEVE OPERATIONS
    // =========================================================================

    /**
     * @brief Get media URL by ID
     * @param media_id Media ID
     * @return URL or empty string if not found
     */
    std::string getMediaUrl(const std::string& media_id);

    /**
     * @brief Get media record by ID
     * @param media_id Media ID
     * @return MediaRecord if found
     */
    std::optional<MediaRecord> getMediaRecord(const std::string& media_id);

    /**
     * @brief Get all media for a dog
     * @param dog_id Dog ID
     * @param type Optional type filter
     * @return List of media records
     */
    std::vector<MediaRecord> getMediaForDog(
        const std::string& dog_id,
        std::optional<MediaType> type = std::nullopt
    );

    /**
     * @brief Get media by type
     * @param type Media type
     * @param limit Maximum records to return
     * @param offset Offset for pagination
     * @return List of media records
     */
    std::vector<MediaRecord> getMediaByType(
        MediaType type,
        int limit = 100,
        int offset = 0
    );

    /**
     * @brief Download media to local path
     * @param media_id Media ID
     * @param output_path Local output path
     * @return True if successful
     */
    bool downloadMedia(
        const std::string& media_id,
        const std::string& output_path
    );

    // =========================================================================
    // DELETE OPERATIONS
    // =========================================================================

    /**
     * @brief Delete media by ID
     * @param media_id Media ID
     * @return True if successful
     */
    bool deleteMedia(const std::string& media_id);

    /**
     * @brief Delete all media for a dog
     * @param dog_id Dog ID
     * @return Number of files deleted
     */
    int deleteMediaForDog(const std::string& dog_id);

    /**
     * @brief Delete expired media
     * @return Number of files deleted
     */
    int deleteExpiredMedia();

    // =========================================================================
    // CLEANUP OPERATIONS
    // =========================================================================

    /**
     * @brief Clean up old media files
     * @param days_old Delete files older than this many days
     * @return Number of files deleted
     */
    int cleanupOldMedia(int days_old = 0);

    /**
     * @brief Clean up orphaned files (not in database)
     * @return Number of files deleted
     */
    int cleanupOrphanedFiles();

    /**
     * @brief Get storage usage statistics
     * @return JSON with storage stats
     */
    Json::Value getStorageStats();

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Generate signed URL with expiration (for S3)
     * @param media_id Media ID
     * @param expiration_seconds URL expiration time
     * @return Signed URL
     */
    std::string generateSignedUrl(
        const std::string& media_id,
        int expiration_seconds = 3600
    );

    /**
     * @brief Check if media exists
     * @param media_id Media ID
     * @return True if exists
     */
    bool mediaExists(const std::string& media_id);

    /**
     * @brief Get content type for file
     * @param filename Filename with extension
     * @return MIME content type
     */
    static std::string getContentType(const std::string& filename);

    /**
     * @brief Convert MediaType to string
     * @param type Media type
     * @return String representation
     */
    static std::string mediaTypeToString(MediaType type);

    /**
     * @brief Convert string to MediaType
     * @param str String representation
     * @return Media type
     */
    static MediaType stringToMediaType(const std::string& str);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    MediaStorage() = default;
    ~MediaStorage() = default;

    /**
     * @brief Generate unique media ID
     * @return Unique ID
     */
    std::string generateMediaId();

    /**
     * @brief Generate storage path
     * @param dog_id Dog ID
     * @param type Media type
     * @param filename Filename
     * @return Storage path
     */
    std::string generateStoragePath(
        const std::string& dog_id,
        MediaType type,
        const std::string& filename
    );

    /**
     * @brief Save to local filesystem
     * @param source_path Source file path
     * @param storage_path Target storage path
     * @return True if successful
     */
    bool saveToLocal(
        const std::string& source_path,
        const std::string& storage_path
    );

    /**
     * @brief Save to S3
     * @param source_path Source file path
     * @param storage_path S3 key
     * @param content_type MIME type
     * @return True if successful
     */
    bool saveToS3(
        const std::string& source_path,
        const std::string& storage_path,
        const std::string& content_type
    );

    /**
     * @brief Delete from local filesystem
     * @param storage_path Storage path
     * @return True if successful
     */
    bool deleteFromLocal(const std::string& storage_path);

    /**
     * @brief Delete from S3
     * @param storage_path S3 key
     * @return True if successful
     */
    bool deleteFromS3(const std::string& storage_path);

    /**
     * @brief Save media record to database
     * @param record Media record
     * @return True if successful
     */
    bool saveRecord(const MediaRecord& record);

    /**
     * @brief Delete media record from database
     * @param media_id Media ID
     * @return True if successful
     */
    bool deleteRecord(const std::string& media_id);

    // Configuration
    StorageConfig config_;

    // State
    std::atomic<bool> initialized_{false};

    // Statistics
    std::atomic<uint64_t> total_saved_{0};
    std::atomic<uint64_t> total_deleted_{0};
    std::atomic<uint64_t> total_bytes_saved_{0};

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::media

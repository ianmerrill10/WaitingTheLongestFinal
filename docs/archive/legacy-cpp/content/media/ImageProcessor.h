/**
 * @file ImageProcessor.h
 * @brief Image processing for enhancement, overlays, and share cards
 *
 * PURPOSE:
 * Provides image processing capabilities for enhancing dog photos, adding
 * LED counter overlays, urgency badges, and generating social share cards.
 * Uses FFmpeg/ImageMagick for image manipulation.
 *
 * USAGE:
 * auto& processor = ImageProcessor::getInstance();
 * auto result = processor.enhance(dog_photo);
 * auto share_card = processor.generateShareCard(dog);
 *
 * DEPENDENCIES:
 * - FFmpegWrapper (for image operations)
 * - OverlayConfig (for overlay styling)
 * - ErrorCapture, SelfHealing (for error handling)
 *
 * MODIFICATION GUIDE:
 * - Add new enhancement effects to enhance()
 * - Add new badge types to addUrgencyBadge()
 * - Extend share card templates in generateShareCard()
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/media/OverlayConfig.h"

namespace wtl::content::media {

// Forward declarations
class FFmpegWrapper;

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct EnhanceOptions
 * @brief Options for image enhancement
 */
struct EnhanceOptions {
    bool auto_levels = true;           ///< Auto-adjust levels
    bool auto_contrast = true;         ///< Auto-adjust contrast
    bool auto_saturation = false;      ///< Auto-adjust saturation
    float brightness_adjust = 0.0f;    ///< Brightness adjustment (-1.0 to 1.0)
    float contrast_adjust = 0.0f;      ///< Contrast adjustment (-1.0 to 1.0)
    float saturation_adjust = 0.0f;    ///< Saturation adjustment (-1.0 to 1.0)
    float sharpness = 0.0f;            ///< Sharpness (0.0 to 1.0)
    bool denoise = false;              ///< Apply noise reduction
    bool remove_red_eye = false;       ///< Attempt red-eye removal

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static EnhanceOptions fromJson(const Json::Value& json);

    /**
     * @brief Get auto-enhance options
     */
    static EnhanceOptions autoEnhance();

    /**
     * @brief Get subtle enhancement options
     */
    static EnhanceOptions subtle();
};

/**
 * @struct OptimizeOptions
 * @brief Options for image optimization
 */
struct OptimizeOptions {
    int quality = 85;                  ///< JPEG quality (1-100)
    bool strip_metadata = true;        ///< Remove EXIF and other metadata
    bool progressive = true;           ///< Create progressive JPEG
    int max_file_size_kb = 0;          ///< Max file size in KB (0 = no limit)

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static OptimizeOptions fromJson(const Json::Value& json);

    /**
     * @brief Get web-optimized options
     */
    static OptimizeOptions forWeb();

    /**
     * @brief Get high-quality options
     */
    static OptimizeOptions highQuality();
};

/**
 * @struct ShareCardOptions
 * @brief Options for generating social share cards
 */
struct ShareCardOptions {
    int width = 1200;                  ///< Card width
    int height = 630;                  ///< Card height (Facebook recommended)
    std::string template_style = "default";  ///< Template style
    bool show_wait_time = true;        ///< Show wait time counter
    bool show_urgency = true;          ///< Show urgency badge
    bool show_shelter_logo = false;    ///< Show shelter logo
    bool show_qr_code = false;         ///< Show QR code link
    Color background_color = Color::white();  ///< Background color
    std::string font_family = "Arial"; ///< Font family

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static ShareCardOptions fromJson(const Json::Value& json);

    /**
     * @brief Get Facebook-optimized options
     */
    static ShareCardOptions forFacebook();

    /**
     * @brief Get Twitter-optimized options
     */
    static ShareCardOptions forTwitter();

    /**
     * @brief Get Instagram-optimized options
     */
    static ShareCardOptions forInstagram();
};

/**
 * @struct ImageResult
 * @brief Result of an image processing operation
 */
struct ImageResult {
    bool success = false;              ///< Whether operation succeeded
    std::string output_path;           ///< Path to output image
    std::string error_message;         ///< Error message if failed
    int width = 0;                     ///< Output width
    int height = 0;                    ///< Output height
    int64_t file_size = 0;             ///< Output file size in bytes

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct DogInfo
 * @brief Dog information for share card generation
 */
struct DogInfo {
    std::string id;                    ///< Dog ID
    std::string name;                  ///< Dog name
    std::string breed;                 ///< Primary breed
    std::string age;                   ///< Age description
    std::string wait_time;             ///< Formatted wait time
    std::string urgency_level;         ///< Urgency level
    std::string shelter_name;          ///< Shelter name
    std::string photo_url;             ///< Main photo URL/path
    std::string profile_url;           ///< URL to dog profile
    std::optional<std::string> countdown; ///< Countdown time if applicable

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static DogInfo fromJson(const Json::Value& json);
};

// ============================================================================
// IMAGE PROCESSOR CLASS
// ============================================================================

/**
 * @class ImageProcessor
 * @brief Singleton class for image processing operations
 *
 * Provides methods for enhancing photos, adding overlays, resizing,
 * optimizing, and generating social share cards.
 */
class ImageProcessor {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the ImageProcessor singleton
     */
    static ImageProcessor& getInstance();

    // Delete copy/move constructors
    ImageProcessor(const ImageProcessor&) = delete;
    ImageProcessor& operator=(const ImageProcessor&) = delete;
    ImageProcessor(ImageProcessor&&) = delete;
    ImageProcessor& operator=(ImageProcessor&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the image processor
     * @param assets_dir Directory containing overlay assets
     * @param temp_dir Temporary directory for intermediate files
     * @return True if initialization successful
     */
    bool initialize(
        const std::string& assets_dir = "static/images",
        const std::string& temp_dir = "/tmp/wtl_media"
    );

    /**
     * @brief Initialize from configuration
     * @return True if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if processor is initialized
     * @return True if initialized
     */
    bool isInitialized() const;

    // =========================================================================
    // ENHANCEMENT
    // =========================================================================

    /**
     * @brief Enhance photo quality
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param options Enhancement options
     * @return ImageResult
     */
    ImageResult enhance(
        const std::string& input_path,
        const std::string& output_path,
        const EnhanceOptions& options = EnhanceOptions::autoEnhance()
    );

    /**
     * @brief Auto-enhance photo with default settings
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @return ImageResult
     */
    ImageResult autoEnhance(
        const std::string& input_path,
        const std::string& output_path
    );

    // =========================================================================
    // OVERLAYS
    // =========================================================================

    /**
     * @brief Add LED wait time counter overlay
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param wait_time Wait time string (e.g., "02:03:15:08:30:00")
     * @param config LED counter configuration
     * @return ImageResult
     */
    ImageResult addWaitTimeOverlay(
        const std::string& input_path,
        const std::string& output_path,
        const std::string& wait_time,
        const LEDCounterConfig& config = LEDCounterConfig::defaultWaitTime()
    );

    /**
     * @brief Add urgency badge overlay
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param urgency_level Urgency level (normal, medium, high, critical)
     * @param config Badge configuration
     * @return ImageResult
     */
    ImageResult addUrgencyBadge(
        const std::string& input_path,
        const std::string& output_path,
        const std::string& urgency_level,
        const UrgencyBadgeConfig& config = UrgencyBadgeConfig()
    );

    /**
     * @brief Add text overlay to image
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param config Text overlay configuration
     * @return ImageResult
     */
    ImageResult addTextOverlay(
        const std::string& input_path,
        const std::string& output_path,
        const TextOverlayConfig& config
    );

    /**
     * @brief Add multiple overlays to image
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param overlay_config Complete overlay configuration
     * @param wait_time Wait time for LED counter
     * @param countdown Optional countdown time
     * @return ImageResult
     */
    ImageResult addOverlays(
        const std::string& input_path,
        const std::string& output_path,
        const OverlayConfig& overlay_config,
        const std::string& wait_time,
        const std::optional<std::string>& countdown = std::nullopt
    );

    // =========================================================================
    // SHARE CARD GENERATION
    // =========================================================================

    /**
     * @brief Generate social media share card
     * @param dog Dog information
     * @param output_path Path for output image
     * @param options Share card options
     * @return ImageResult
     */
    ImageResult generateShareCard(
        const DogInfo& dog,
        const std::string& output_path,
        const ShareCardOptions& options = ShareCardOptions::forFacebook()
    );

    /**
     * @brief Generate share card from dog photo
     * @param photo_path Path to dog photo
     * @param dog Dog information
     * @param output_path Path for output image
     * @param options Share card options
     * @return ImageResult
     */
    ImageResult generateShareCardFromPhoto(
        const std::string& photo_path,
        const DogInfo& dog,
        const std::string& output_path,
        const ShareCardOptions& options = ShareCardOptions::forFacebook()
    );

    // =========================================================================
    // RESIZING
    // =========================================================================

    /**
     * @brief Resize image
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param dimensions Target dimensions
     * @param maintain_aspect Maintain aspect ratio
     * @return ImageResult
     */
    ImageResult resize(
        const std::string& input_path,
        const std::string& output_path,
        const Dimensions& dimensions,
        bool maintain_aspect = true
    );

    /**
     * @brief Resize image to width (height calculated to maintain aspect)
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param width Target width
     * @return ImageResult
     */
    ImageResult resizeToWidth(
        const std::string& input_path,
        const std::string& output_path,
        int width
    );

    /**
     * @brief Resize image to height (width calculated to maintain aspect)
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param height Target height
     * @return ImageResult
     */
    ImageResult resizeToHeight(
        const std::string& input_path,
        const std::string& output_path,
        int height
    );

    /**
     * @brief Crop and resize to exact dimensions (center crop)
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param dimensions Target dimensions
     * @return ImageResult
     */
    ImageResult cropAndResize(
        const std::string& input_path,
        const std::string& output_path,
        const Dimensions& dimensions
    );

    // =========================================================================
    // OPTIMIZATION
    // =========================================================================

    /**
     * @brief Optimize image for web delivery
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param options Optimization options
     * @return ImageResult
     */
    ImageResult optimize(
        const std::string& input_path,
        const std::string& output_path,
        const OptimizeOptions& options = OptimizeOptions::forWeb()
    );

    /**
     * @brief Convert image format
     * @param input_path Path to input image
     * @param output_path Path for output image
     * @param format Target format (jpg, png, webp)
     * @param quality Quality for lossy formats
     * @return ImageResult
     */
    ImageResult convert(
        const std::string& input_path,
        const std::string& output_path,
        const std::string& format,
        int quality = 85
    );

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Get image dimensions
     * @param path Path to image
     * @return Dimensions struct
     */
    Dimensions getImageDimensions(const std::string& path);

    /**
     * @brief Check if file is a valid image
     * @param path Path to file
     * @return True if valid image
     */
    bool isValidImage(const std::string& path);

    /**
     * @brief Generate temporary file path
     * @param extension File extension
     * @return Temporary file path
     */
    std::string getTempPath(const std::string& extension);

    /**
     * @brief Clean up temporary files
     */
    void cleanupTempFiles();

    /**
     * @brief Get processing statistics
     * @return JSON with statistics
     */
    Json::Value getStats() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    ImageProcessor() = default;
    ~ImageProcessor();

    /**
     * @brief Create LED counter overlay image
     * @param wait_time Wait time string
     * @param config Counter configuration
     * @param output_path Output path for overlay
     * @return True if successful
     */
    bool createLEDCounterOverlay(
        const std::string& wait_time,
        const LEDCounterConfig& config,
        const std::string& output_path
    );

    /**
     * @brief Create urgency badge overlay image
     * @param urgency_level Urgency level
     * @param config Badge configuration
     * @param output_path Output path for overlay
     * @return True if successful
     */
    bool createUrgencyBadgeOverlay(
        const std::string& urgency_level,
        const UrgencyBadgeConfig& config,
        const std::string& output_path
    );

    /**
     * @brief Create text overlay image
     * @param config Text configuration
     * @param output_path Output path for overlay
     * @return True if successful
     */
    bool createTextOverlay(
        const TextOverlayConfig& config,
        const std::string& output_path
    );

    /**
     * @brief Build FFmpeg enhancement filter
     * @param options Enhancement options
     * @return Filter string
     */
    std::string buildEnhanceFilter(const EnhanceOptions& options);

    /**
     * @brief Get path to urgency badge asset
     * @param urgency_level Urgency level
     * @return Path to badge asset
     */
    std::string getUrgencyBadgePath(const std::string& urgency_level);

    // Configuration
    std::string assets_dir_ = "static/images";
    std::string temp_dir_ = "/tmp/wtl_media";
    std::string overlays_dir_ = "static/images/overlays";

    // State
    std::atomic<bool> initialized_{false};

    // Temp file tracking
    std::vector<std::string> temp_files_;

    // Statistics
    std::atomic<uint64_t> images_processed_{0};
    std::atomic<uint64_t> share_cards_generated_{0};
    std::atomic<uint64_t> processing_errors_{0};

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::media

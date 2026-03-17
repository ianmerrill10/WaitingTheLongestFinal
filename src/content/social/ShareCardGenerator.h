/**
 * @file ShareCardGenerator.h
 * @brief Generator for shareable social media cards with dog information
 *
 * PURPOSE:
 * The ShareCardGenerator creates visually appealing share cards for social media
 * that include:
 * - Dog photo with optimized sizing for each platform
 * - Wait time overlay showing days waiting
 * - Urgency badges for long-wait dogs
 * - QR codes linking to the dog's profile
 * - Shelter branding and contact information
 * - Call-to-action text
 *
 * USAGE:
 * auto& generator = ShareCardGenerator::getInstance();
 * auto card = generator.generateCard(dog_id, Platform::INSTAGRAM);
 * // card.image_data contains the PNG bytes
 * // card.image_url contains the URL after upload
 *
 * DEPENDENCIES:
 * - Platform.h (Platform enum)
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - ImageMagick/GraphicsMagick (image processing)
 *
 * MODIFICATION GUIDE:
 * - Add new card styles by implementing new template functions
 * - Adjust sizing for new platforms in getPlatformDimensions
 * - Add new badge types in the UrgencyBadge enum
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/social/Platform.h"

namespace wtl::content::social {

// ============================================================================
// CARD TYPES AND BADGES
// ============================================================================

/**
 * @enum CardStyle
 * @brief Different styles of share cards
 */
enum class CardStyle {
    STANDARD,             ///< Standard adoption card
    URGENT,               ///< Urgent/long-wait appeal
    MILESTONE,            ///< Waiting milestone (1 year, etc.)
    SUCCESS_STORY,        ///< Adoption success story
    EVENT,                ///< Adoption event promotion
    COMPARISON,           ///< Before/after or comparison
    CAROUSEL_ITEM,        ///< Single item in a carousel
    STORY                 ///< Vertical story format
};

/**
 * @enum UrgencyBadge
 * @brief Types of urgency badges to display
 */
enum class UrgencyBadge {
    NONE,                 ///< No badge
    NEW_ARRIVAL,          ///< New to shelter (< 7 days)
    LONG_WAIT,            ///< Long wait (> 6 months)
    URGENT,               ///< Very long wait (> 1 year)
    CRITICAL,             ///< Critical (> 2 years)
    SENIOR,               ///< Senior dog
    SPECIAL_NEEDS,        ///< Special needs dog
    BONDED_PAIR           ///< Bonded pair
};

/**
 * @brief Convert CardStyle to string
 */
std::string cardStyleToString(CardStyle style);

/**
 * @brief Convert UrgencyBadge to string
 */
std::string urgencyBadgeToString(UrgencyBadge badge);

// ============================================================================
// CARD CONFIGURATION
// ============================================================================

/**
 * @struct CardDimensions
 * @brief Dimensions for a share card
 */
struct CardDimensions {
    int width;                            ///< Width in pixels
    int height;                           ///< Height in pixels
    float aspect_ratio;                   ///< Aspect ratio (width/height)
    std::string format;                   ///< Image format (png, jpg)
    int quality;                          ///< JPEG quality (1-100)

    /**
     * @brief Get dimensions for a platform
     */
    static CardDimensions forPlatform(Platform platform, CardStyle style = CardStyle::STANDARD);
};

/**
 * @struct CardColors
 * @brief Color scheme for a share card
 */
struct CardColors {
    std::string primary;                  ///< Primary brand color (hex)
    std::string secondary;                ///< Secondary color (hex)
    std::string accent;                   ///< Accent color (hex)
    std::string text_light;               ///< Light text color (hex)
    std::string text_dark;                ///< Dark text color (hex)
    std::string overlay;                  ///< Overlay background (hex with alpha)
    std::string urgent_badge;             ///< Urgent badge color (hex)
    std::string success_badge;            ///< Success badge color (hex)

    /**
     * @brief Get default color scheme
     */
    static CardColors defaultScheme();

    /**
     * @brief Create from JSON
     */
    static CardColors fromJson(const Json::Value& json);
};

/**
 * @struct CardFonts
 * @brief Font configuration for a share card
 */
struct CardFonts {
    std::string heading_family;           ///< Heading font family
    std::string body_family;              ///< Body font family
    int heading_size;                     ///< Heading font size
    int subheading_size;                  ///< Subheading font size
    int body_size;                        ///< Body font size
    int badge_size;                       ///< Badge font size
    int cta_size;                         ///< Call-to-action font size

    /**
     * @brief Get default fonts
     */
    static CardFonts defaultFonts();
};

/**
 * @struct CardConfig
 * @brief Complete configuration for card generation
 */
struct CardConfig {
    CardDimensions dimensions;
    CardColors colors;
    CardFonts fonts;

    bool show_wait_time{true};            ///< Show days waiting overlay
    bool show_urgency_badge{true};        ///< Show urgency badge
    bool show_qr_code{false};             ///< Include QR code
    bool show_shelter_logo{true};         ///< Include shelter logo
    bool show_cta{true};                  ///< Show call-to-action text
    bool show_website{true};              ///< Show website URL

    std::string logo_path;                ///< Path to shelter logo
    std::string cta_text;                 ///< Call-to-action text
    std::string website_url;              ///< Website URL to display

    /**
     * @brief Create default config for platform
     */
    static CardConfig defaultForPlatform(Platform platform);

    /**
     * @brief Create from JSON
     */
    static CardConfig fromJson(const Json::Value& json);

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

// ============================================================================
// GENERATED CARD
// ============================================================================

/**
 * @struct GeneratedCard
 * @brief Result of card generation
 */
struct GeneratedCard {
    bool success;                         ///< Whether generation succeeded
    std::string error_message;            ///< Error if failed

    // Image data
    std::vector<uint8_t> image_data;      ///< Raw image bytes
    std::string image_format;             ///< Format (png, jpg)
    int width;                            ///< Actual width
    int height;                           ///< Actual height
    size_t file_size;                     ///< Size in bytes

    // Metadata
    std::string dog_id;                   ///< Dog ID
    std::string dog_name;                 ///< Dog name
    int days_waiting;                     ///< Days waiting
    UrgencyBadge badge;                   ///< Applied badge
    CardStyle style;                      ///< Card style used
    Platform platform;                    ///< Target platform

    // URLs (after upload)
    std::string image_url;                ///< CDN URL after upload
    std::string qr_code_url;              ///< QR code destination URL

    // Generation info
    std::chrono::system_clock::time_point generated_at;
    std::chrono::milliseconds generation_time;

    /**
     * @brief Convert to JSON (without image data)
     */
    Json::Value toJson() const;
};

// ============================================================================
// DOG CARD DATA
// ============================================================================

/**
 * @struct DogCardData
 * @brief Data about a dog for card generation
 */
struct DogCardData {
    std::string id;
    std::string name;
    std::string breed;
    std::string age_display;              ///< e.g., "2 years old"
    std::string sex;                      ///< "Male" or "Female"
    std::string size;                     ///< "Small", "Medium", "Large"
    std::string primary_photo_url;
    std::vector<std::string> additional_photos;
    int days_waiting;
    bool is_senior;
    bool is_special_needs;
    bool is_bonded_pair;
    std::string bonded_pair_name;         ///< Name of bonded partner
    std::string short_bio;                ///< Brief description
    std::string shelter_name;
    std::string profile_url;              ///< Full URL to dog's profile

    /**
     * @brief Determine appropriate urgency badge
     */
    UrgencyBadge determineUrgencyBadge() const;

    /**
     * @brief Load from database
     */
    static std::optional<DogCardData> loadFromDatabase(const std::string& dog_id);

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

// ============================================================================
// SHARE CARD GENERATOR
// ============================================================================

/**
 * @class ShareCardGenerator
 * @brief Singleton service for generating social media share cards
 */
class ShareCardGenerator {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static ShareCardGenerator& getInstance();

    // Prevent copying
    ShareCardGenerator(const ShareCardGenerator&) = delete;
    ShareCardGenerator& operator=(const ShareCardGenerator&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the generator
     * @param assets_path Path to assets directory (fonts, logos, etc.)
     */
    void initialize(const std::string& assets_path);

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // CARD GENERATION
    // ========================================================================

    /**
     * @brief Generate a share card for a dog
     * @param dog_id Dog ID
     * @param platform Target platform
     * @param style Card style
     * @param config Optional custom configuration
     * @return GeneratedCard Result with image data
     */
    GeneratedCard generateCard(
        const std::string& dog_id,
        Platform platform,
        CardStyle style = CardStyle::STANDARD,
        const std::optional<CardConfig>& config = std::nullopt);

    /**
     * @brief Generate a share card from dog data
     * @param dog_data Dog card data
     * @param platform Target platform
     * @param style Card style
     * @param config Optional custom configuration
     * @return GeneratedCard Result with image data
     */
    GeneratedCard generateCard(
        const DogCardData& dog_data,
        Platform platform,
        CardStyle style = CardStyle::STANDARD,
        const std::optional<CardConfig>& config = std::nullopt);

    /**
     * @brief Generate cards for multiple platforms
     * @param dog_id Dog ID
     * @param platforms Target platforms
     * @param style Card style
     * @return std::map<Platform, GeneratedCard> Cards per platform
     */
    std::map<Platform, GeneratedCard> generateMultiPlatformCards(
        const std::string& dog_id,
        const std::vector<Platform>& platforms,
        CardStyle style = CardStyle::STANDARD);

    /**
     * @brief Generate a carousel of cards for one dog
     * @param dog_id Dog ID
     * @param platform Target platform
     * @param max_cards Maximum number of carousel cards
     * @return std::vector<GeneratedCard> Carousel cards
     */
    std::vector<GeneratedCard> generateCarousel(
        const std::string& dog_id,
        Platform platform,
        int max_cards = 5);

    /**
     * @brief Generate a milestone card
     * @param dog_id Dog ID
     * @param milestone_type Type of milestone (e.g., "1_year")
     * @param platform Target platform
     * @return GeneratedCard Milestone card
     */
    GeneratedCard generateMilestoneCard(
        const std::string& dog_id,
        const std::string& milestone_type,
        Platform platform);

    /**
     * @brief Generate a success story card
     * @param dog_id Dog ID (now adopted)
     * @param before_photo_url Photo from shelter
     * @param after_photo_url Photo in new home
     * @param platform Target platform
     * @return GeneratedCard Success story card
     */
    GeneratedCard generateSuccessStoryCard(
        const std::string& dog_id,
        const std::string& before_photo_url,
        const std::string& after_photo_url,
        Platform platform);

    // ========================================================================
    // QR CODE GENERATION
    // ========================================================================

    /**
     * @brief Generate QR code for dog profile
     * @param profile_url URL to encode
     * @param size Size in pixels
     * @return std::vector<uint8_t> QR code PNG data
     */
    std::vector<uint8_t> generateQRCode(
        const std::string& profile_url,
        int size = 200);

    // ========================================================================
    // UPLOAD
    // ========================================================================

    /**
     * @brief Upload generated card to CDN
     * @param card Card to upload
     * @return std::string CDN URL or empty on failure
     */
    std::string uploadCard(GeneratedCard& card);

    /**
     * @brief Upload multiple cards
     * @param cards Cards to upload
     * @return int Number successfully uploaded
     */
    int uploadCards(std::vector<GeneratedCard>& cards);

    // ========================================================================
    // CACHE MANAGEMENT
    // ========================================================================

    /**
     * @brief Get cached card if available
     * @param dog_id Dog ID
     * @param platform Platform
     * @param style Card style
     * @return std::optional<GeneratedCard> Cached card if available
     */
    std::optional<GeneratedCard> getCachedCard(
        const std::string& dog_id,
        Platform platform,
        CardStyle style) const;

    /**
     * @brief Invalidate cache for a dog
     * @param dog_id Dog ID
     */
    void invalidateCache(const std::string& dog_id);

    /**
     * @brief Clear entire cache
     */
    void clearCache();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set default color scheme
     * @param colors Color scheme
     */
    void setDefaultColors(const CardColors& colors);

    /**
     * @brief Set default fonts
     * @param fonts Font configuration
     */
    void setDefaultFonts(const CardFonts& fonts);

    /**
     * @brief Set shelter branding
     * @param logo_path Path to logo file
     * @param website_url Shelter website
     * @param cta_text Default call-to-action
     */
    void setShelterBranding(
        const std::string& logo_path,
        const std::string& website_url,
        const std::string& cta_text);

private:
    ShareCardGenerator() = default;
    ~ShareCardGenerator() = default;

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Render the standard card layout
     */
    std::vector<uint8_t> renderStandardCard(
        const DogCardData& dog,
        const CardConfig& config);

    /**
     * @brief Render the urgent card layout
     */
    std::vector<uint8_t> renderUrgentCard(
        const DogCardData& dog,
        const CardConfig& config);

    /**
     * @brief Render the milestone card layout
     */
    std::vector<uint8_t> renderMilestoneCard(
        const DogCardData& dog,
        const std::string& milestone_type,
        const CardConfig& config);

    /**
     * @brief Render the success story card layout
     */
    std::vector<uint8_t> renderSuccessStoryCard(
        const DogCardData& dog,
        const std::string& before_photo,
        const std::string& after_photo,
        const CardConfig& config);

    /**
     * @brief Render the story (vertical) card layout
     */
    std::vector<uint8_t> renderStoryCard(
        const DogCardData& dog,
        const CardConfig& config);

    /**
     * @brief Download image from URL
     */
    std::vector<uint8_t> downloadImage(const std::string& url);

    /**
     * @brief Resize image to fit dimensions
     */
    std::vector<uint8_t> resizeImage(
        const std::vector<uint8_t>& image_data,
        int width,
        int height,
        bool crop = true);

    /**
     * @brief Apply text overlay
     */
    void applyTextOverlay(
        std::vector<uint8_t>& image,
        const std::string& text,
        int x,
        int y,
        const std::string& font_family,
        int font_size,
        const std::string& color);

    /**
     * @brief Apply wait time badge
     */
    void applyWaitTimeBadge(
        std::vector<uint8_t>& image,
        int days_waiting,
        const CardConfig& config);

    /**
     * @brief Apply urgency badge
     */
    void applyUrgencyBadge(
        std::vector<uint8_t>& image,
        UrgencyBadge badge,
        const CardConfig& config);

    /**
     * @brief Apply QR code overlay
     */
    void applyQRCode(
        std::vector<uint8_t>& image,
        const std::string& url,
        int x,
        int y,
        int size);

    /**
     * @brief Apply shelter logo
     */
    void applyShelterLogo(
        std::vector<uint8_t>& image,
        const CardConfig& config);

    /**
     * @brief Apply gradient overlay
     */
    void applyGradientOverlay(
        std::vector<uint8_t>& image,
        const std::string& color,
        float opacity,
        bool from_bottom = true);

    /**
     * @brief Generate cache key
     */
    std::string generateCacheKey(
        const std::string& dog_id,
        Platform platform,
        CardStyle style) const;

    // ========================================================================
    // STATE
    // ========================================================================

    bool initialized_{false};
    std::string assets_path_;

    // Default configuration
    CardColors default_colors_;
    CardFonts default_fonts_;
    std::string default_logo_path_;
    std::string default_website_;
    std::string default_cta_;

    // Cache
    mutable std::map<std::string, GeneratedCard> card_cache_;
    mutable std::mutex cache_mutex_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::social

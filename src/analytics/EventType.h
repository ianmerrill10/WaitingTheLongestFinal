/**
 * @file EventType.h
 * @brief Analytics event type enumeration for tracking user and system events
 *
 * PURPOSE:
 * Defines all possible analytics event types for tracking user behavior,
 * dog interactions, social media engagement, and system events on
 * WaitingTheLongest.com. This enumeration is the foundation for all
 * analytics data collection.
 *
 * USAGE:
 * AnalyticsEventType type = AnalyticsEventType::DOG_VIEW;
 * std::string str = analyticsEventTypeToString(type);
 * auto parsed = analyticsEventTypeFromString("DOG_VIEW");
 *
 * DEPENDENCIES:
 * - None (self-contained header)
 *
 * MODIFICATION GUIDE:
 * - To add new event types, add to the enum and update both conversion functions
 * - Keep event types in logical groups (views, interactions, applications, social, system)
 * - Update getEventTypeCategory() when adding new types
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace wtl::analytics {

/**
 * @enum AnalyticsEventType
 * @brief All trackable event types in the platform
 *
 * Events are grouped by category:
 * - Page/View events: User navigation and content viewing
 * - Interaction events: User actions on dogs/shelters
 * - Application events: Foster and adoption workflows
 * - Social events: Social media integration and sharing
 * - Notification events: Push notifications and emails
 * - User events: Authentication and account actions
 * - System events: Internal platform events
 */
enum class AnalyticsEventType {
    // =========================================================================
    // PAGE VIEW EVENTS
    // =========================================================================
    PAGE_VIEW,              // Generic page view
    DOG_VIEW,               // User viewed a dog's profile
    SHELTER_VIEW,           // User viewed a shelter's page
    STATE_VIEW,             // User viewed a state's page
    HOME_VIEW,              // User viewed homepage
    SEARCH_PAGE_VIEW,       // User viewed search results page

    // =========================================================================
    // SEARCH EVENTS
    // =========================================================================
    SEARCH,                 // User performed a search
    FILTER_APPLIED,         // User applied search filters
    SEARCH_REFINED,         // User refined existing search
    SEARCH_CLEARED,         // User cleared search filters
    AUTOCOMPLETE_USED,      // User used autocomplete suggestion

    // =========================================================================
    // DOG INTERACTION EVENTS
    // =========================================================================
    DOG_FAVORITED,          // User added dog to favorites
    DOG_UNFAVORITED,        // User removed dog from favorites
    DOG_SHARED,             // User shared dog via share button
    DOG_PHOTO_VIEWED,       // User viewed additional dog photos
    DOG_VIDEO_PLAYED,       // User played dog video
    DOG_CONTACT_CLICKED,    // User clicked shelter contact info
    DOG_DIRECTIONS_CLICKED, // User clicked for directions to shelter

    // =========================================================================
    // FOSTER APPLICATION EVENTS
    // =========================================================================
    FOSTER_APPLICATION,         // User submitted foster application
    FOSTER_APPLICATION_STARTED, // User began foster application
    FOSTER_APPLICATION_VIEWED,  // User viewed application status
    FOSTER_APPROVED,            // Foster application was approved
    FOSTER_REJECTED,            // Foster application was rejected
    FOSTER_STARTED,             // Foster placement began
    FOSTER_ENDED,               // Foster placement ended
    FOSTER_EXTENDED,            // Foster placement was extended

    // =========================================================================
    // ADOPTION EVENTS
    // =========================================================================
    ADOPTION_APPLICATION,       // User submitted adoption application
    ADOPTION_APPLICATION_STARTED, // User began adoption application
    ADOPTION_INQUIRY,           // User inquired about adoption
    ADOPTION_COMPLETED,         // Dog was adopted
    ADOPTION_FAILED,            // Adoption fell through

    // =========================================================================
    // SOCIAL MEDIA EVENTS
    // =========================================================================
    TIKTOK_VIEW,            // View from TikTok
    TIKTOK_LIKE,            // Like on TikTok
    TIKTOK_SHARE,           // Share on TikTok
    TIKTOK_COMMENT,         // Comment on TikTok
    TIKTOK_FOLLOW,          // Follow from TikTok
    INSTAGRAM_VIEW,         // View from Instagram
    INSTAGRAM_LIKE,         // Like on Instagram
    INSTAGRAM_SHARE,        // Share on Instagram
    INSTAGRAM_COMMENT,      // Comment on Instagram
    INSTAGRAM_FOLLOW,       // Follow from Instagram
    FACEBOOK_VIEW,          // View from Facebook
    FACEBOOK_LIKE,          // Like on Facebook
    FACEBOOK_SHARE,         // Share on Facebook
    FACEBOOK_COMMENT,       // Comment on Facebook
    TWITTER_VIEW,           // View from Twitter/X
    TWITTER_LIKE,           // Like on Twitter/X
    TWITTER_SHARE,          // Retweet/share on Twitter/X
    SOCIAL_ENGAGEMENT,      // Generic social engagement
    SOCIAL_REFERRAL,        // User came from social media

    // =========================================================================
    // NOTIFICATION EVENTS
    // =========================================================================
    NOTIFICATION_SENT,      // Notification was sent to user
    NOTIFICATION_DELIVERED, // Notification was delivered
    NOTIFICATION_OPENED,    // User opened notification
    NOTIFICATION_CLICKED,   // User clicked action in notification
    NOTIFICATION_DISMISSED, // User dismissed notification
    EMAIL_SENT,             // Email was sent
    EMAIL_OPENED,           // Email was opened
    EMAIL_CLICKED,          // Link in email was clicked
    PUSH_SENT,              // Push notification sent
    PUSH_OPENED,            // Push notification opened

    // =========================================================================
    // USER EVENTS
    // =========================================================================
    USER_REGISTERED,        // New user registered
    USER_LOGIN,             // User logged in
    USER_LOGOUT,            // User logged out
    USER_PROFILE_UPDATED,   // User updated their profile
    USER_PREFERENCES_CHANGED, // User changed preferences
    USER_DELETED,           // User deleted account
    USER_VERIFIED,          // User verified email

    // =========================================================================
    // URGENCY/RESCUE EVENTS
    // =========================================================================
    DOG_BECAME_CRITICAL,    // Dog entered critical urgency
    DOG_RESCUED,            // Dog was rescued/pulled
    DOG_TRANSPORTED,        // Dog was transported
    URGENT_ALERT_SENT,      // Urgent alert was broadcast
    URGENT_ALERT_VIEWED,    // User viewed urgent alert

    // =========================================================================
    // SYSTEM EVENTS
    // =========================================================================
    API_REQUEST,            // API request made
    ERROR_OCCURRED,         // Error occurred
    PERFORMANCE_METRIC,     // Performance measurement
    DATA_IMPORT,            // Data imported from external source
    DATA_EXPORT             // Data exported
};

/**
 * @brief Convert AnalyticsEventType to string representation
 * @param type The event type to convert
 * @return String representation of the event type
 */
inline std::string analyticsEventTypeToString(AnalyticsEventType type) {
    switch (type) {
        // Page views
        case AnalyticsEventType::PAGE_VIEW: return "PAGE_VIEW";
        case AnalyticsEventType::DOG_VIEW: return "DOG_VIEW";
        case AnalyticsEventType::SHELTER_VIEW: return "SHELTER_VIEW";
        case AnalyticsEventType::STATE_VIEW: return "STATE_VIEW";
        case AnalyticsEventType::HOME_VIEW: return "HOME_VIEW";
        case AnalyticsEventType::SEARCH_PAGE_VIEW: return "SEARCH_PAGE_VIEW";

        // Search
        case AnalyticsEventType::SEARCH: return "SEARCH";
        case AnalyticsEventType::FILTER_APPLIED: return "FILTER_APPLIED";
        case AnalyticsEventType::SEARCH_REFINED: return "SEARCH_REFINED";
        case AnalyticsEventType::SEARCH_CLEARED: return "SEARCH_CLEARED";
        case AnalyticsEventType::AUTOCOMPLETE_USED: return "AUTOCOMPLETE_USED";

        // Dog interactions
        case AnalyticsEventType::DOG_FAVORITED: return "DOG_FAVORITED";
        case AnalyticsEventType::DOG_UNFAVORITED: return "DOG_UNFAVORITED";
        case AnalyticsEventType::DOG_SHARED: return "DOG_SHARED";
        case AnalyticsEventType::DOG_PHOTO_VIEWED: return "DOG_PHOTO_VIEWED";
        case AnalyticsEventType::DOG_VIDEO_PLAYED: return "DOG_VIDEO_PLAYED";
        case AnalyticsEventType::DOG_CONTACT_CLICKED: return "DOG_CONTACT_CLICKED";
        case AnalyticsEventType::DOG_DIRECTIONS_CLICKED: return "DOG_DIRECTIONS_CLICKED";

        // Foster
        case AnalyticsEventType::FOSTER_APPLICATION: return "FOSTER_APPLICATION";
        case AnalyticsEventType::FOSTER_APPLICATION_STARTED: return "FOSTER_APPLICATION_STARTED";
        case AnalyticsEventType::FOSTER_APPLICATION_VIEWED: return "FOSTER_APPLICATION_VIEWED";
        case AnalyticsEventType::FOSTER_APPROVED: return "FOSTER_APPROVED";
        case AnalyticsEventType::FOSTER_REJECTED: return "FOSTER_REJECTED";
        case AnalyticsEventType::FOSTER_STARTED: return "FOSTER_STARTED";
        case AnalyticsEventType::FOSTER_ENDED: return "FOSTER_ENDED";
        case AnalyticsEventType::FOSTER_EXTENDED: return "FOSTER_EXTENDED";

        // Adoption
        case AnalyticsEventType::ADOPTION_APPLICATION: return "ADOPTION_APPLICATION";
        case AnalyticsEventType::ADOPTION_APPLICATION_STARTED: return "ADOPTION_APPLICATION_STARTED";
        case AnalyticsEventType::ADOPTION_INQUIRY: return "ADOPTION_INQUIRY";
        case AnalyticsEventType::ADOPTION_COMPLETED: return "ADOPTION_COMPLETED";
        case AnalyticsEventType::ADOPTION_FAILED: return "ADOPTION_FAILED";

        // Social - TikTok
        case AnalyticsEventType::TIKTOK_VIEW: return "TIKTOK_VIEW";
        case AnalyticsEventType::TIKTOK_LIKE: return "TIKTOK_LIKE";
        case AnalyticsEventType::TIKTOK_SHARE: return "TIKTOK_SHARE";
        case AnalyticsEventType::TIKTOK_COMMENT: return "TIKTOK_COMMENT";
        case AnalyticsEventType::TIKTOK_FOLLOW: return "TIKTOK_FOLLOW";

        // Social - Instagram
        case AnalyticsEventType::INSTAGRAM_VIEW: return "INSTAGRAM_VIEW";
        case AnalyticsEventType::INSTAGRAM_LIKE: return "INSTAGRAM_LIKE";
        case AnalyticsEventType::INSTAGRAM_SHARE: return "INSTAGRAM_SHARE";
        case AnalyticsEventType::INSTAGRAM_COMMENT: return "INSTAGRAM_COMMENT";
        case AnalyticsEventType::INSTAGRAM_FOLLOW: return "INSTAGRAM_FOLLOW";

        // Social - Facebook
        case AnalyticsEventType::FACEBOOK_VIEW: return "FACEBOOK_VIEW";
        case AnalyticsEventType::FACEBOOK_LIKE: return "FACEBOOK_LIKE";
        case AnalyticsEventType::FACEBOOK_SHARE: return "FACEBOOK_SHARE";
        case AnalyticsEventType::FACEBOOK_COMMENT: return "FACEBOOK_COMMENT";

        // Social - Twitter
        case AnalyticsEventType::TWITTER_VIEW: return "TWITTER_VIEW";
        case AnalyticsEventType::TWITTER_LIKE: return "TWITTER_LIKE";
        case AnalyticsEventType::TWITTER_SHARE: return "TWITTER_SHARE";

        // Social - Generic
        case AnalyticsEventType::SOCIAL_ENGAGEMENT: return "SOCIAL_ENGAGEMENT";
        case AnalyticsEventType::SOCIAL_REFERRAL: return "SOCIAL_REFERRAL";

        // Notifications
        case AnalyticsEventType::NOTIFICATION_SENT: return "NOTIFICATION_SENT";
        case AnalyticsEventType::NOTIFICATION_DELIVERED: return "NOTIFICATION_DELIVERED";
        case AnalyticsEventType::NOTIFICATION_OPENED: return "NOTIFICATION_OPENED";
        case AnalyticsEventType::NOTIFICATION_CLICKED: return "NOTIFICATION_CLICKED";
        case AnalyticsEventType::NOTIFICATION_DISMISSED: return "NOTIFICATION_DISMISSED";
        case AnalyticsEventType::EMAIL_SENT: return "EMAIL_SENT";
        case AnalyticsEventType::EMAIL_OPENED: return "EMAIL_OPENED";
        case AnalyticsEventType::EMAIL_CLICKED: return "EMAIL_CLICKED";
        case AnalyticsEventType::PUSH_SENT: return "PUSH_SENT";
        case AnalyticsEventType::PUSH_OPENED: return "PUSH_OPENED";

        // User
        case AnalyticsEventType::USER_REGISTERED: return "USER_REGISTERED";
        case AnalyticsEventType::USER_LOGIN: return "USER_LOGIN";
        case AnalyticsEventType::USER_LOGOUT: return "USER_LOGOUT";
        case AnalyticsEventType::USER_PROFILE_UPDATED: return "USER_PROFILE_UPDATED";
        case AnalyticsEventType::USER_PREFERENCES_CHANGED: return "USER_PREFERENCES_CHANGED";
        case AnalyticsEventType::USER_DELETED: return "USER_DELETED";
        case AnalyticsEventType::USER_VERIFIED: return "USER_VERIFIED";

        // Urgency
        case AnalyticsEventType::DOG_BECAME_CRITICAL: return "DOG_BECAME_CRITICAL";
        case AnalyticsEventType::DOG_RESCUED: return "DOG_RESCUED";
        case AnalyticsEventType::DOG_TRANSPORTED: return "DOG_TRANSPORTED";
        case AnalyticsEventType::URGENT_ALERT_SENT: return "URGENT_ALERT_SENT";
        case AnalyticsEventType::URGENT_ALERT_VIEWED: return "URGENT_ALERT_VIEWED";

        // System
        case AnalyticsEventType::API_REQUEST: return "API_REQUEST";
        case AnalyticsEventType::ERROR_OCCURRED: return "ERROR_OCCURRED";
        case AnalyticsEventType::PERFORMANCE_METRIC: return "PERFORMANCE_METRIC";
        case AnalyticsEventType::DATA_IMPORT: return "DATA_IMPORT";
        case AnalyticsEventType::DATA_EXPORT: return "DATA_EXPORT";

        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse string to AnalyticsEventType
 * @param str The string to parse
 * @return The parsed event type or nullopt if invalid
 */
inline std::optional<AnalyticsEventType> analyticsEventTypeFromString(const std::string& str) {
    // Page views
    if (str == "PAGE_VIEW") return AnalyticsEventType::PAGE_VIEW;
    if (str == "DOG_VIEW") return AnalyticsEventType::DOG_VIEW;
    if (str == "SHELTER_VIEW") return AnalyticsEventType::SHELTER_VIEW;
    if (str == "STATE_VIEW") return AnalyticsEventType::STATE_VIEW;
    if (str == "HOME_VIEW") return AnalyticsEventType::HOME_VIEW;
    if (str == "SEARCH_PAGE_VIEW") return AnalyticsEventType::SEARCH_PAGE_VIEW;

    // Search
    if (str == "SEARCH") return AnalyticsEventType::SEARCH;
    if (str == "FILTER_APPLIED") return AnalyticsEventType::FILTER_APPLIED;
    if (str == "SEARCH_REFINED") return AnalyticsEventType::SEARCH_REFINED;
    if (str == "SEARCH_CLEARED") return AnalyticsEventType::SEARCH_CLEARED;
    if (str == "AUTOCOMPLETE_USED") return AnalyticsEventType::AUTOCOMPLETE_USED;

    // Dog interactions
    if (str == "DOG_FAVORITED") return AnalyticsEventType::DOG_FAVORITED;
    if (str == "DOG_UNFAVORITED") return AnalyticsEventType::DOG_UNFAVORITED;
    if (str == "DOG_SHARED") return AnalyticsEventType::DOG_SHARED;
    if (str == "DOG_PHOTO_VIEWED") return AnalyticsEventType::DOG_PHOTO_VIEWED;
    if (str == "DOG_VIDEO_PLAYED") return AnalyticsEventType::DOG_VIDEO_PLAYED;
    if (str == "DOG_CONTACT_CLICKED") return AnalyticsEventType::DOG_CONTACT_CLICKED;
    if (str == "DOG_DIRECTIONS_CLICKED") return AnalyticsEventType::DOG_DIRECTIONS_CLICKED;

    // Foster
    if (str == "FOSTER_APPLICATION") return AnalyticsEventType::FOSTER_APPLICATION;
    if (str == "FOSTER_APPLICATION_STARTED") return AnalyticsEventType::FOSTER_APPLICATION_STARTED;
    if (str == "FOSTER_APPLICATION_VIEWED") return AnalyticsEventType::FOSTER_APPLICATION_VIEWED;
    if (str == "FOSTER_APPROVED") return AnalyticsEventType::FOSTER_APPROVED;
    if (str == "FOSTER_REJECTED") return AnalyticsEventType::FOSTER_REJECTED;
    if (str == "FOSTER_STARTED") return AnalyticsEventType::FOSTER_STARTED;
    if (str == "FOSTER_ENDED") return AnalyticsEventType::FOSTER_ENDED;
    if (str == "FOSTER_EXTENDED") return AnalyticsEventType::FOSTER_EXTENDED;

    // Adoption
    if (str == "ADOPTION_APPLICATION") return AnalyticsEventType::ADOPTION_APPLICATION;
    if (str == "ADOPTION_APPLICATION_STARTED") return AnalyticsEventType::ADOPTION_APPLICATION_STARTED;
    if (str == "ADOPTION_INQUIRY") return AnalyticsEventType::ADOPTION_INQUIRY;
    if (str == "ADOPTION_COMPLETED") return AnalyticsEventType::ADOPTION_COMPLETED;
    if (str == "ADOPTION_FAILED") return AnalyticsEventType::ADOPTION_FAILED;

    // Social - TikTok
    if (str == "TIKTOK_VIEW") return AnalyticsEventType::TIKTOK_VIEW;
    if (str == "TIKTOK_LIKE") return AnalyticsEventType::TIKTOK_LIKE;
    if (str == "TIKTOK_SHARE") return AnalyticsEventType::TIKTOK_SHARE;
    if (str == "TIKTOK_COMMENT") return AnalyticsEventType::TIKTOK_COMMENT;
    if (str == "TIKTOK_FOLLOW") return AnalyticsEventType::TIKTOK_FOLLOW;

    // Social - Instagram
    if (str == "INSTAGRAM_VIEW") return AnalyticsEventType::INSTAGRAM_VIEW;
    if (str == "INSTAGRAM_LIKE") return AnalyticsEventType::INSTAGRAM_LIKE;
    if (str == "INSTAGRAM_SHARE") return AnalyticsEventType::INSTAGRAM_SHARE;
    if (str == "INSTAGRAM_COMMENT") return AnalyticsEventType::INSTAGRAM_COMMENT;
    if (str == "INSTAGRAM_FOLLOW") return AnalyticsEventType::INSTAGRAM_FOLLOW;

    // Social - Facebook
    if (str == "FACEBOOK_VIEW") return AnalyticsEventType::FACEBOOK_VIEW;
    if (str == "FACEBOOK_LIKE") return AnalyticsEventType::FACEBOOK_LIKE;
    if (str == "FACEBOOK_SHARE") return AnalyticsEventType::FACEBOOK_SHARE;
    if (str == "FACEBOOK_COMMENT") return AnalyticsEventType::FACEBOOK_COMMENT;

    // Social - Twitter
    if (str == "TWITTER_VIEW") return AnalyticsEventType::TWITTER_VIEW;
    if (str == "TWITTER_LIKE") return AnalyticsEventType::TWITTER_LIKE;
    if (str == "TWITTER_SHARE") return AnalyticsEventType::TWITTER_SHARE;

    // Social - Generic
    if (str == "SOCIAL_ENGAGEMENT") return AnalyticsEventType::SOCIAL_ENGAGEMENT;
    if (str == "SOCIAL_REFERRAL") return AnalyticsEventType::SOCIAL_REFERRAL;

    // Notifications
    if (str == "NOTIFICATION_SENT") return AnalyticsEventType::NOTIFICATION_SENT;
    if (str == "NOTIFICATION_DELIVERED") return AnalyticsEventType::NOTIFICATION_DELIVERED;
    if (str == "NOTIFICATION_OPENED") return AnalyticsEventType::NOTIFICATION_OPENED;
    if (str == "NOTIFICATION_CLICKED") return AnalyticsEventType::NOTIFICATION_CLICKED;
    if (str == "NOTIFICATION_DISMISSED") return AnalyticsEventType::NOTIFICATION_DISMISSED;
    if (str == "EMAIL_SENT") return AnalyticsEventType::EMAIL_SENT;
    if (str == "EMAIL_OPENED") return AnalyticsEventType::EMAIL_OPENED;
    if (str == "EMAIL_CLICKED") return AnalyticsEventType::EMAIL_CLICKED;
    if (str == "PUSH_SENT") return AnalyticsEventType::PUSH_SENT;
    if (str == "PUSH_OPENED") return AnalyticsEventType::PUSH_OPENED;

    // User
    if (str == "USER_REGISTERED") return AnalyticsEventType::USER_REGISTERED;
    if (str == "USER_LOGIN") return AnalyticsEventType::USER_LOGIN;
    if (str == "USER_LOGOUT") return AnalyticsEventType::USER_LOGOUT;
    if (str == "USER_PROFILE_UPDATED") return AnalyticsEventType::USER_PROFILE_UPDATED;
    if (str == "USER_PREFERENCES_CHANGED") return AnalyticsEventType::USER_PREFERENCES_CHANGED;
    if (str == "USER_DELETED") return AnalyticsEventType::USER_DELETED;
    if (str == "USER_VERIFIED") return AnalyticsEventType::USER_VERIFIED;

    // Urgency
    if (str == "DOG_BECAME_CRITICAL") return AnalyticsEventType::DOG_BECAME_CRITICAL;
    if (str == "DOG_RESCUED") return AnalyticsEventType::DOG_RESCUED;
    if (str == "DOG_TRANSPORTED") return AnalyticsEventType::DOG_TRANSPORTED;
    if (str == "URGENT_ALERT_SENT") return AnalyticsEventType::URGENT_ALERT_SENT;
    if (str == "URGENT_ALERT_VIEWED") return AnalyticsEventType::URGENT_ALERT_VIEWED;

    // System
    if (str == "API_REQUEST") return AnalyticsEventType::API_REQUEST;
    if (str == "ERROR_OCCURRED") return AnalyticsEventType::ERROR_OCCURRED;
    if (str == "PERFORMANCE_METRIC") return AnalyticsEventType::PERFORMANCE_METRIC;
    if (str == "DATA_IMPORT") return AnalyticsEventType::DATA_IMPORT;
    if (str == "DATA_EXPORT") return AnalyticsEventType::DATA_EXPORT;

    return std::nullopt;
}

/**
 * @brief Get the category of an event type
 * @param type The event type
 * @return Category string (view, search, interaction, foster, adoption, social, notification, user, urgency, system)
 */
inline std::string getEventTypeCategory(AnalyticsEventType type) {
    switch (type) {
        case AnalyticsEventType::PAGE_VIEW:
        case AnalyticsEventType::DOG_VIEW:
        case AnalyticsEventType::SHELTER_VIEW:
        case AnalyticsEventType::STATE_VIEW:
        case AnalyticsEventType::HOME_VIEW:
        case AnalyticsEventType::SEARCH_PAGE_VIEW:
            return "view";

        case AnalyticsEventType::SEARCH:
        case AnalyticsEventType::FILTER_APPLIED:
        case AnalyticsEventType::SEARCH_REFINED:
        case AnalyticsEventType::SEARCH_CLEARED:
        case AnalyticsEventType::AUTOCOMPLETE_USED:
            return "search";

        case AnalyticsEventType::DOG_FAVORITED:
        case AnalyticsEventType::DOG_UNFAVORITED:
        case AnalyticsEventType::DOG_SHARED:
        case AnalyticsEventType::DOG_PHOTO_VIEWED:
        case AnalyticsEventType::DOG_VIDEO_PLAYED:
        case AnalyticsEventType::DOG_CONTACT_CLICKED:
        case AnalyticsEventType::DOG_DIRECTIONS_CLICKED:
            return "interaction";

        case AnalyticsEventType::FOSTER_APPLICATION:
        case AnalyticsEventType::FOSTER_APPLICATION_STARTED:
        case AnalyticsEventType::FOSTER_APPLICATION_VIEWED:
        case AnalyticsEventType::FOSTER_APPROVED:
        case AnalyticsEventType::FOSTER_REJECTED:
        case AnalyticsEventType::FOSTER_STARTED:
        case AnalyticsEventType::FOSTER_ENDED:
        case AnalyticsEventType::FOSTER_EXTENDED:
            return "foster";

        case AnalyticsEventType::ADOPTION_APPLICATION:
        case AnalyticsEventType::ADOPTION_APPLICATION_STARTED:
        case AnalyticsEventType::ADOPTION_INQUIRY:
        case AnalyticsEventType::ADOPTION_COMPLETED:
        case AnalyticsEventType::ADOPTION_FAILED:
            return "adoption";

        case AnalyticsEventType::TIKTOK_VIEW:
        case AnalyticsEventType::TIKTOK_LIKE:
        case AnalyticsEventType::TIKTOK_SHARE:
        case AnalyticsEventType::TIKTOK_COMMENT:
        case AnalyticsEventType::TIKTOK_FOLLOW:
        case AnalyticsEventType::INSTAGRAM_VIEW:
        case AnalyticsEventType::INSTAGRAM_LIKE:
        case AnalyticsEventType::INSTAGRAM_SHARE:
        case AnalyticsEventType::INSTAGRAM_COMMENT:
        case AnalyticsEventType::INSTAGRAM_FOLLOW:
        case AnalyticsEventType::FACEBOOK_VIEW:
        case AnalyticsEventType::FACEBOOK_LIKE:
        case AnalyticsEventType::FACEBOOK_SHARE:
        case AnalyticsEventType::FACEBOOK_COMMENT:
        case AnalyticsEventType::TWITTER_VIEW:
        case AnalyticsEventType::TWITTER_LIKE:
        case AnalyticsEventType::TWITTER_SHARE:
        case AnalyticsEventType::SOCIAL_ENGAGEMENT:
        case AnalyticsEventType::SOCIAL_REFERRAL:
            return "social";

        case AnalyticsEventType::NOTIFICATION_SENT:
        case AnalyticsEventType::NOTIFICATION_DELIVERED:
        case AnalyticsEventType::NOTIFICATION_OPENED:
        case AnalyticsEventType::NOTIFICATION_CLICKED:
        case AnalyticsEventType::NOTIFICATION_DISMISSED:
        case AnalyticsEventType::EMAIL_SENT:
        case AnalyticsEventType::EMAIL_OPENED:
        case AnalyticsEventType::EMAIL_CLICKED:
        case AnalyticsEventType::PUSH_SENT:
        case AnalyticsEventType::PUSH_OPENED:
            return "notification";

        case AnalyticsEventType::USER_REGISTERED:
        case AnalyticsEventType::USER_LOGIN:
        case AnalyticsEventType::USER_LOGOUT:
        case AnalyticsEventType::USER_PROFILE_UPDATED:
        case AnalyticsEventType::USER_PREFERENCES_CHANGED:
        case AnalyticsEventType::USER_DELETED:
        case AnalyticsEventType::USER_VERIFIED:
            return "user";

        case AnalyticsEventType::DOG_BECAME_CRITICAL:
        case AnalyticsEventType::DOG_RESCUED:
        case AnalyticsEventType::DOG_TRANSPORTED:
        case AnalyticsEventType::URGENT_ALERT_SENT:
        case AnalyticsEventType::URGENT_ALERT_VIEWED:
            return "urgency";

        case AnalyticsEventType::API_REQUEST:
        case AnalyticsEventType::ERROR_OCCURRED:
        case AnalyticsEventType::PERFORMANCE_METRIC:
        case AnalyticsEventType::DATA_IMPORT:
        case AnalyticsEventType::DATA_EXPORT:
            return "system";

        default:
            return "unknown";
    }
}

/**
 * @brief Check if event type represents a high-value conversion event
 * @param type The event type to check
 * @return True if this is a conversion event (adoption, foster, etc.)
 */
inline bool isConversionEvent(AnalyticsEventType type) {
    switch (type) {
        case AnalyticsEventType::FOSTER_APPLICATION:
        case AnalyticsEventType::FOSTER_APPROVED:
        case AnalyticsEventType::FOSTER_STARTED:
        case AnalyticsEventType::ADOPTION_APPLICATION:
        case AnalyticsEventType::ADOPTION_COMPLETED:
        case AnalyticsEventType::DOG_RESCUED:
        case AnalyticsEventType::USER_REGISTERED:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if event type is a social media event
 * @param type The event type to check
 * @return True if this is a social media event
 */
inline bool isSocialEvent(AnalyticsEventType type) {
    return getEventTypeCategory(type) == "social";
}

/**
 * @brief Get platform name for social event types
 * @param type The event type
 * @return Platform name or empty string if not a social event
 */
inline std::string getSocialPlatform(AnalyticsEventType type) {
    switch (type) {
        case AnalyticsEventType::TIKTOK_VIEW:
        case AnalyticsEventType::TIKTOK_LIKE:
        case AnalyticsEventType::TIKTOK_SHARE:
        case AnalyticsEventType::TIKTOK_COMMENT:
        case AnalyticsEventType::TIKTOK_FOLLOW:
            return "tiktok";

        case AnalyticsEventType::INSTAGRAM_VIEW:
        case AnalyticsEventType::INSTAGRAM_LIKE:
        case AnalyticsEventType::INSTAGRAM_SHARE:
        case AnalyticsEventType::INSTAGRAM_COMMENT:
        case AnalyticsEventType::INSTAGRAM_FOLLOW:
            return "instagram";

        case AnalyticsEventType::FACEBOOK_VIEW:
        case AnalyticsEventType::FACEBOOK_LIKE:
        case AnalyticsEventType::FACEBOOK_SHARE:
        case AnalyticsEventType::FACEBOOK_COMMENT:
            return "facebook";

        case AnalyticsEventType::TWITTER_VIEW:
        case AnalyticsEventType::TWITTER_LIKE:
        case AnalyticsEventType::TWITTER_SHARE:
            return "twitter";

        default:
            return "";
    }
}

/**
 * @brief Get all event types as a list
 * @return Vector of all event types
 */
inline std::vector<AnalyticsEventType> getAllEventTypes() {
    return {
        AnalyticsEventType::PAGE_VIEW,
        AnalyticsEventType::DOG_VIEW,
        AnalyticsEventType::SHELTER_VIEW,
        AnalyticsEventType::STATE_VIEW,
        AnalyticsEventType::HOME_VIEW,
        AnalyticsEventType::SEARCH_PAGE_VIEW,
        AnalyticsEventType::SEARCH,
        AnalyticsEventType::FILTER_APPLIED,
        AnalyticsEventType::SEARCH_REFINED,
        AnalyticsEventType::SEARCH_CLEARED,
        AnalyticsEventType::AUTOCOMPLETE_USED,
        AnalyticsEventType::DOG_FAVORITED,
        AnalyticsEventType::DOG_UNFAVORITED,
        AnalyticsEventType::DOG_SHARED,
        AnalyticsEventType::DOG_PHOTO_VIEWED,
        AnalyticsEventType::DOG_VIDEO_PLAYED,
        AnalyticsEventType::DOG_CONTACT_CLICKED,
        AnalyticsEventType::DOG_DIRECTIONS_CLICKED,
        AnalyticsEventType::FOSTER_APPLICATION,
        AnalyticsEventType::FOSTER_APPLICATION_STARTED,
        AnalyticsEventType::FOSTER_APPLICATION_VIEWED,
        AnalyticsEventType::FOSTER_APPROVED,
        AnalyticsEventType::FOSTER_REJECTED,
        AnalyticsEventType::FOSTER_STARTED,
        AnalyticsEventType::FOSTER_ENDED,
        AnalyticsEventType::FOSTER_EXTENDED,
        AnalyticsEventType::ADOPTION_APPLICATION,
        AnalyticsEventType::ADOPTION_APPLICATION_STARTED,
        AnalyticsEventType::ADOPTION_INQUIRY,
        AnalyticsEventType::ADOPTION_COMPLETED,
        AnalyticsEventType::ADOPTION_FAILED,
        AnalyticsEventType::TIKTOK_VIEW,
        AnalyticsEventType::TIKTOK_LIKE,
        AnalyticsEventType::TIKTOK_SHARE,
        AnalyticsEventType::TIKTOK_COMMENT,
        AnalyticsEventType::TIKTOK_FOLLOW,
        AnalyticsEventType::INSTAGRAM_VIEW,
        AnalyticsEventType::INSTAGRAM_LIKE,
        AnalyticsEventType::INSTAGRAM_SHARE,
        AnalyticsEventType::INSTAGRAM_COMMENT,
        AnalyticsEventType::INSTAGRAM_FOLLOW,
        AnalyticsEventType::FACEBOOK_VIEW,
        AnalyticsEventType::FACEBOOK_LIKE,
        AnalyticsEventType::FACEBOOK_SHARE,
        AnalyticsEventType::FACEBOOK_COMMENT,
        AnalyticsEventType::TWITTER_VIEW,
        AnalyticsEventType::TWITTER_LIKE,
        AnalyticsEventType::TWITTER_SHARE,
        AnalyticsEventType::SOCIAL_ENGAGEMENT,
        AnalyticsEventType::SOCIAL_REFERRAL,
        AnalyticsEventType::NOTIFICATION_SENT,
        AnalyticsEventType::NOTIFICATION_DELIVERED,
        AnalyticsEventType::NOTIFICATION_OPENED,
        AnalyticsEventType::NOTIFICATION_CLICKED,
        AnalyticsEventType::NOTIFICATION_DISMISSED,
        AnalyticsEventType::EMAIL_SENT,
        AnalyticsEventType::EMAIL_OPENED,
        AnalyticsEventType::EMAIL_CLICKED,
        AnalyticsEventType::PUSH_SENT,
        AnalyticsEventType::PUSH_OPENED,
        AnalyticsEventType::USER_REGISTERED,
        AnalyticsEventType::USER_LOGIN,
        AnalyticsEventType::USER_LOGOUT,
        AnalyticsEventType::USER_PROFILE_UPDATED,
        AnalyticsEventType::USER_PREFERENCES_CHANGED,
        AnalyticsEventType::USER_DELETED,
        AnalyticsEventType::USER_VERIFIED,
        AnalyticsEventType::DOG_BECAME_CRITICAL,
        AnalyticsEventType::DOG_RESCUED,
        AnalyticsEventType::DOG_TRANSPORTED,
        AnalyticsEventType::URGENT_ALERT_SENT,
        AnalyticsEventType::URGENT_ALERT_VIEWED,
        AnalyticsEventType::API_REQUEST,
        AnalyticsEventType::ERROR_OCCURRED,
        AnalyticsEventType::PERFORMANCE_METRIC,
        AnalyticsEventType::DATA_IMPORT,
        AnalyticsEventType::DATA_EXPORT
    };
}

} // namespace wtl::analytics

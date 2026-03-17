/**
 * @file EmailChannel.h
 * @brief SendGrid email notification channel
 *
 * PURPOSE:
 * Handles sending email notifications via SendGrid API. Supports both
 * transactional emails (notifications) and template-based emails.
 * Templates are used for different notification types to ensure consistent
 * branding and optimal email deliverability.
 *
 * USAGE:
 * auto& email = EmailChannel::getInstance();
 * email.sendEmail(email_address, subject, body, template_id);
 * email.sendNotificationEmail(user, notification);
 *
 * DEPENDENCIES:
 * - libcurl (HTTP requests to SendGrid)
 * - Config (SendGrid API key)
 * - ConnectionPool (user email lookup)
 * - ErrorCapture (error logging)
 *
 * CONFIGURATION:
 * Requires the following in config.json:
 * {
 *   "notifications": {
 *     "sendgrid": {
 *       "api_key": "your-sendgrid-api-key",
 *       "from_email": "alerts@waitingthelongest.com",
 *       "from_name": "WaitingTheLongest",
 *       "templates": {
 *         "critical_alert": "d-xxxxx",
 *         "high_urgency": "d-xxxxx",
 *         ...
 *       }
 *     }
 *   }
 * }
 *
 * MODIFICATION GUIDE:
 * - Add new templates for new notification types
 * - Update getTemplateId() when adding types
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "notifications/Notification.h"

namespace wtl::notifications::channels {

/**
 * @struct EmailResult
 * @brief Result of an email send attempt
 */
struct EmailResult {
    bool success;
    std::string message_id;       ///< SendGrid message ID
    std::string error_code;
    std::string error_message;
    bool should_retry;
    bool email_invalid;           ///< Email address is invalid/bounced

    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        if (!message_id.empty()) json["message_id"] = message_id;
        if (!error_code.empty()) json["error_code"] = error_code;
        if (!error_message.empty()) json["error_message"] = error_message;
        json["should_retry"] = should_retry;
        json["email_invalid"] = email_invalid;
        return json;
    }
};

/**
 * @struct EmailRecipient
 * @brief Email recipient information
 */
struct EmailRecipient {
    std::string email;
    std::string name;
    std::unordered_map<std::string, std::string> substitutions; ///< Template variables
};

/**
 * @struct EmailMessage
 * @brief Complete email message structure
 */
struct EmailMessage {
    std::vector<EmailRecipient> to;
    std::string subject;
    std::string html_content;
    std::string text_content;
    std::optional<std::string> template_id;
    Json::Value template_data;
    std::optional<std::string> reply_to;
    std::string category;           ///< For SendGrid analytics
    std::optional<std::string> unsubscribe_group_id;
    bool track_opens = true;
    bool track_clicks = true;

    /**
     * @brief Create from notification
     */
    static EmailMessage fromNotification(const Notification& notif, const EmailRecipient& recipient);
};

/**
 * @class EmailChannel
 * @brief Singleton service for sending emails via SendGrid
 *
 * Thread-safe singleton that manages SendGrid integration.
 * Supports templated emails, batch sending, and analytics tracking.
 */
class EmailChannel {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to EmailChannel instance
     */
    static EmailChannel& getInstance();

    // Prevent copying
    EmailChannel(const EmailChannel&) = delete;
    EmailChannel& operator=(const EmailChannel&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the email channel
     *
     * @param api_key SendGrid API key
     * @param from_email Sender email address
     * @param from_name Sender display name
     * @return true if initialization successful
     */
    bool initialize(const std::string& api_key,
                   const std::string& from_email,
                   const std::string& from_name);

    /**
     * @brief Initialize from config.json
     * @return true if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Set template ID for notification type
     */
    void setTemplateId(NotificationType type, const std::string& template_id);

    // =========================================================================
    // SEND EMAILS
    // =========================================================================

    /**
     * @brief Send a simple email
     *
     * @param to_email Recipient email address
     * @param subject Email subject
     * @param body Email body (HTML)
     * @param template_id Optional SendGrid template ID
     * @return EmailResult Result of the send attempt
     */
    EmailResult sendEmail(const std::string& to_email,
                          const std::string& subject,
                          const std::string& body,
                          const std::string& template_id = "");

    /**
     * @brief Send email with template data
     *
     * @param to_email Recipient email address
     * @param template_id SendGrid template ID
     * @param template_data Dynamic template data
     * @return EmailResult Result of the send attempt
     */
    EmailResult sendTemplateEmail(const std::string& to_email,
                                  const std::string& template_id,
                                  const Json::Value& template_data);

    /**
     * @brief Send notification email to a user
     *
     * @param to_email Recipient email
     * @param to_name Recipient name
     * @param notification The notification
     * @return EmailResult Result of the send attempt
     */
    EmailResult sendNotificationEmail(const std::string& to_email,
                                      const std::string& to_name,
                                      const Notification& notification);

    /**
     * @brief Send email to a user by ID
     *
     * @param user_id User UUID
     * @param notification The notification
     * @return EmailResult Result of the send attempt
     */
    EmailResult sendToUser(const std::string& user_id, const Notification& notification);

    /**
     * @brief Send bulk emails
     *
     * @param messages Vector of email messages
     * @return std::vector<EmailResult> Results for each email
     */
    std::vector<EmailResult> sendBulk(const std::vector<EmailMessage>& messages);

    /**
     * @brief Send personalized emails to multiple recipients
     *
     * Uses SendGrid's personalization feature for efficient batch sending.
     *
     * @param recipients Vector of recipients with personalization data
     * @param template_id SendGrid template ID
     * @param common_data Data shared across all recipients
     * @return std::vector<EmailResult> Results for each recipient
     */
    std::vector<EmailResult> sendPersonalized(
        const std::vector<EmailRecipient>& recipients,
        const std::string& template_id,
        const Json::Value& common_data = Json::Value());

    // =========================================================================
    // TEMPLATES
    // =========================================================================

    /**
     * @brief Get template ID for notification type
     *
     * @param type Notification type
     * @return std::string Template ID or empty if none configured
     */
    std::string getTemplateId(NotificationType type) const;

    /**
     * @brief Build template data for notification
     *
     * @param notification The notification
     * @param recipient_name Recipient's name
     * @return Json::Value Template variable data
     */
    Json::Value buildTemplateData(const Notification& notification,
                                  const std::string& recipient_name) const;

    // =========================================================================
    // EMAIL VALIDATION
    // =========================================================================

    /**
     * @brief Validate email address format
     *
     * @param email Email address to validate
     * @return true if email format is valid
     */
    static bool isValidEmail(const std::string& email);

    /**
     * @brief Check if email is in bounce/suppression list
     *
     * @param email Email to check
     * @return true if email should not be sent to
     */
    bool isEmailSuppressed(const std::string& email);

    /**
     * @brief Add email to suppression list
     *
     * @param email Email to suppress
     * @param reason Reason for suppression
     */
    void suppressEmail(const std::string& email, const std::string& reason);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get channel statistics
     * @return Json::Value Statistics JSON
     */
    Json::Value getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    EmailChannel() = default;
    ~EmailChannel() = default;

    /**
     * @brief Build SendGrid API request body
     */
    Json::Value buildSendGridRequest(const EmailMessage& message);

    /**
     * @brief Send HTTP request to SendGrid
     */
    EmailResult sendSendGridRequest(const Json::Value& request);

    /**
     * @brief Parse SendGrid response
     */
    EmailResult parseSendGridResponse(const std::string& response, long http_code,
                                      const std::string& message_id_header);

    /**
     * @brief Get user email by ID
     */
    std::pair<std::string, std::string> getUserEmail(const std::string& user_id);

    /**
     * @brief Build plain text version from HTML
     */
    std::string htmlToPlainText(const std::string& html) const;

    /**
     * @brief Generate email-safe HTML content for notification
     */
    std::string generateNotificationHtml(const Notification& notification) const;

    // Configuration
    std::string api_key_;
    std::string from_email_;
    std::string from_name_;
    std::unordered_map<NotificationType, std::string> template_ids_;
    bool initialized_ = false;

    // SendGrid API endpoint
    static constexpr const char* SENDGRID_API_URL = "https://api.sendgrid.com/v3/mail/send";

    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_sent_ = 0;
    uint64_t total_success_ = 0;
    uint64_t total_failed_ = 0;
    uint64_t total_bounced_ = 0;
};

} // namespace wtl::notifications::channels

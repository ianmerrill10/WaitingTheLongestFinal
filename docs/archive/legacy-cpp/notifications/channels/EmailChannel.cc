/**
 * @file EmailChannel.cc
 * @brief Implementation of SendGrid email notification channel
 * @see EmailChannel.h for documentation
 */

#include "notifications/channels/EmailChannel.h"

// Standard library includes
#include <regex>
#include <sstream>

// Third-party includes
#include <curl/curl.h>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::notifications::channels {

// ============================================================================
// CURL CALLBACK
// ============================================================================

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, std::string* userp) {
    size_t total_size = size * nitems;
    std::string header(buffer, total_size);

    // Look for X-Message-Id header
    if (header.find("X-Message-Id:") == 0 || header.find("x-message-id:") == 0) {
        size_t pos = header.find(':');
        if (pos != std::string::npos) {
            std::string value = header.substr(pos + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            *userp = value;
        }
    }

    return total_size;
}

// ============================================================================
// EMAIL MESSAGE
// ============================================================================

EmailMessage EmailMessage::fromNotification(const Notification& notif, const EmailRecipient& recipient) {
    EmailMessage msg;
    msg.to.push_back(recipient);
    msg.subject = notif.title;
    msg.category = notificationTypeToString(notif.type);
    msg.track_opens = true;
    msg.track_clicks = true;

    // Template data
    msg.template_data["notification_title"] = notif.title;
    msg.template_data["notification_body"] = notif.body;
    msg.template_data["notification_type"] = notificationTypeToString(notif.type);
    msg.template_data["notification_color"] = getNotificationColor(notif.type);
    msg.template_data["recipient_name"] = recipient.name;

    if (notif.image_url.has_value()) {
        msg.template_data["image_url"] = notif.image_url.value();
    }
    if (notif.action_url.has_value()) {
        msg.template_data["action_url"] = notif.action_url.value();
    }
    if (notif.dog_id.has_value()) {
        msg.template_data["dog_id"] = notif.dog_id.value();
    }

    return msg;
}

// ============================================================================
// SINGLETON
// ============================================================================

EmailChannel& EmailChannel::getInstance() {
    static EmailChannel instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool EmailChannel::initialize(const std::string& api_key,
                             const std::string& from_email,
                             const std::string& from_name) {
    api_key_ = api_key;
    from_email_ = from_email;
    from_name_ = from_name;

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    initialized_ = true;

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::EXTERNAL_API,
        "EmailChannel initialized successfully",
        {{"from_email", from_email_}}
    );

    return true;
}

bool EmailChannel::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string api_key = config.getString("notifications.sendgrid.api_key", "");
        std::string from_email = config.getString("notifications.sendgrid.from_email", "alerts@waitingthelongest.com");
        std::string from_name = config.getString("notifications.sendgrid.from_name", "WaitingTheLongest");

        if (api_key.empty()) {
            WTL_CAPTURE_WARNING(
                wtl::core::debug::ErrorCategory::CONFIGURATION,
                "SendGrid API key not configured - email notifications disabled",
                {}
            );
            return false;
        }

        bool result = initialize(api_key, from_email, from_name);

        // Load template IDs
        std::string critical_template = config.getString("notifications.sendgrid.templates.critical_alert", "");
        if (!critical_template.empty()) {
            setTemplateId(NotificationType::CRITICAL_ALERT, critical_template);
        }

        std::string urgency_template = config.getString("notifications.sendgrid.templates.high_urgency", "");
        if (!urgency_template.empty()) {
            setTemplateId(NotificationType::HIGH_URGENCY, urgency_template);
        }

        std::string match_template = config.getString("notifications.sendgrid.templates.perfect_match", "");
        if (!match_template.empty()) {
            setTemplateId(NotificationType::PERFECT_MATCH, match_template);
            setTemplateId(NotificationType::GOOD_MATCH, match_template);
        }

        std::string foster_template = config.getString("notifications.sendgrid.templates.foster_needed", "");
        if (!foster_template.empty()) {
            setTemplateId(NotificationType::FOSTER_NEEDED_URGENT, foster_template);
        }

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize EmailChannel from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

void EmailChannel::setTemplateId(NotificationType type, const std::string& template_id) {
    template_ids_[type] = template_id;
}

// ============================================================================
// SEND EMAILS
// ============================================================================

EmailResult EmailChannel::sendEmail(const std::string& to_email,
                                    const std::string& subject,
                                    const std::string& body,
                                    const std::string& template_id) {
    EmailResult result;
    result.success = false;
    result.should_retry = false;
    result.email_invalid = false;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "EmailChannel not initialized";
        return result;
    }

    if (!isValidEmail(to_email)) {
        result.error_code = "INVALID_EMAIL";
        result.error_message = "Invalid email address format";
        result.email_invalid = true;
        return result;
    }

    if (isEmailSuppressed(to_email)) {
        result.error_code = "EMAIL_SUPPRESSED";
        result.error_message = "Email address is in suppression list";
        result.email_invalid = true;
        return result;
    }

    EmailMessage msg;
    EmailRecipient recipient;
    recipient.email = to_email;
    msg.to.push_back(recipient);
    msg.subject = subject;
    msg.html_content = body;
    msg.text_content = htmlToPlainText(body);

    if (!template_id.empty()) {
        msg.template_id = template_id;
    }

    Json::Value request = buildSendGridRequest(msg);
    result = sendSendGridRequest(request);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
        } else {
            total_failed_++;
            if (result.email_invalid) {
                total_bounced_++;
            }
        }
    }

    return result;
}

EmailResult EmailChannel::sendTemplateEmail(const std::string& to_email,
                                            const std::string& template_id,
                                            const Json::Value& template_data) {
    EmailResult result;
    result.success = false;
    result.should_retry = false;
    result.email_invalid = false;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "EmailChannel not initialized";
        return result;
    }

    if (!isValidEmail(to_email)) {
        result.error_code = "INVALID_EMAIL";
        result.error_message = "Invalid email address format";
        result.email_invalid = true;
        return result;
    }

    EmailMessage msg;
    EmailRecipient recipient;
    recipient.email = to_email;
    msg.to.push_back(recipient);
    msg.template_id = template_id;
    msg.template_data = template_data;

    Json::Value request = buildSendGridRequest(msg);
    result = sendSendGridRequest(request);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
        } else {
            total_failed_++;
        }
    }

    return result;
}

EmailResult EmailChannel::sendNotificationEmail(const std::string& to_email,
                                                const std::string& to_name,
                                                const Notification& notification) {
    // Get template for notification type
    std::string template_id = getTemplateId(notification.type);

    if (template_id.empty()) {
        // No template, send with generated HTML
        std::string html = generateNotificationHtml(notification);
        return sendEmail(to_email, notification.title, html);
    }

    // Build template data
    Json::Value template_data = buildTemplateData(notification, to_name);

    return sendTemplateEmail(to_email, template_id, template_data);
}

EmailResult EmailChannel::sendToUser(const std::string& user_id, const Notification& notification) {
    auto [email, name] = getUserEmail(user_id);

    if (email.empty()) {
        EmailResult result;
        result.success = false;
        result.error_code = "USER_NOT_FOUND";
        result.error_message = "User not found or has no email";
        return result;
    }

    return sendNotificationEmail(email, name, notification);
}

std::vector<EmailResult> EmailChannel::sendBulk(const std::vector<EmailMessage>& messages) {
    std::vector<EmailResult> results;
    results.reserve(messages.size());

    for (const auto& msg : messages) {
        Json::Value request = buildSendGridRequest(msg);
        results.push_back(sendSendGridRequest(request));
    }

    return results;
}

std::vector<EmailResult> EmailChannel::sendPersonalized(
    const std::vector<EmailRecipient>& recipients,
    const std::string& template_id,
    const Json::Value& common_data) {

    std::vector<EmailResult> results;

    if (!initialized_ || recipients.empty()) {
        return results;
    }

    // SendGrid allows up to 1000 personalizations per request
    const size_t BATCH_SIZE = 1000;

    for (size_t i = 0; i < recipients.size(); i += BATCH_SIZE) {
        size_t end = std::min(i + BATCH_SIZE, recipients.size());

        Json::Value request;
        Json::Value personalizations(Json::arrayValue);

        for (size_t j = i; j < end; j++) {
            Json::Value personalization;

            Json::Value to_array(Json::arrayValue);
            Json::Value to_obj;
            to_obj["email"] = recipients[j].email;
            if (!recipients[j].name.empty()) {
                to_obj["name"] = recipients[j].name;
            }
            to_array.append(to_obj);
            personalization["to"] = to_array;

            // Merge common data with substitutions
            Json::Value dynamic_data = common_data;
            for (const auto& [key, value] : recipients[j].substitutions) {
                dynamic_data[key] = value;
            }
            personalization["dynamic_template_data"] = dynamic_data;

            personalizations.append(personalization);
        }

        request["personalizations"] = personalizations;

        Json::Value from;
        from["email"] = from_email_;
        from["name"] = from_name_;
        request["from"] = from;

        request["template_id"] = template_id;

        EmailResult batch_result = sendSendGridRequest(request);

        // Create individual results for each recipient in batch
        for (size_t j = i; j < end; j++) {
            results.push_back(batch_result);
        }
    }

    return results;
}

// ============================================================================
// TEMPLATES
// ============================================================================

std::string EmailChannel::getTemplateId(NotificationType type) const {
    auto it = template_ids_.find(type);
    if (it != template_ids_.end()) {
        return it->second;
    }
    return "";
}

Json::Value EmailChannel::buildTemplateData(const Notification& notification,
                                            const std::string& recipient_name) const {
    Json::Value data;

    // Common fields
    data["recipient_name"] = recipient_name.empty() ? "Friend" : recipient_name;
    data["notification_title"] = notification.title;
    data["notification_body"] = notification.body;
    data["notification_type"] = notificationTypeToString(notification.type);
    data["notification_type_display"] = getNotificationTypeTitle(notification.type);
    data["urgency_color"] = getNotificationColor(notification.type);
    data["icon"] = getNotificationIcon(notification.type);

    // Optional fields
    if (notification.image_url.has_value()) {
        data["image_url"] = notification.image_url.value();
        data["has_image"] = true;
    } else {
        data["has_image"] = false;
    }

    if (notification.action_url.has_value()) {
        data["action_url"] = notification.action_url.value();
        data["has_action"] = true;
    } else {
        data["has_action"] = false;
    }

    // Type-specific fields
    if (notification.type == NotificationType::CRITICAL_ALERT ||
        notification.type == NotificationType::HIGH_URGENCY) {
        data["is_urgent"] = true;
        data["urgent_message"] = "Time is running out - please help!";
    } else {
        data["is_urgent"] = false;
    }

    if (notification.type == NotificationType::SUCCESS_STORY) {
        data["is_success"] = true;
    }

    // Merge any custom data
    if (!notification.data.isNull()) {
        for (const auto& key : notification.data.getMemberNames()) {
            data[key] = notification.data[key];
        }
    }

    return data;
}

// ============================================================================
// EMAIL VALIDATION
// ============================================================================

bool EmailChannel::isValidEmail(const std::string& email) {
    // Basic email regex validation
    static const std::regex email_regex(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"
    );

    return std::regex_match(email, email_regex);
}

bool EmailChannel::isEmailSuppressed(const std::string& email) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT 1 FROM email_suppressions WHERE email = $1 AND suppressed_until > NOW()",
            email
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return !result.empty();

    } catch (const std::exception& e) {
        // If we can't check, assume not suppressed
        return false;
    }
}

void EmailChannel::suppressEmail(const std::string& email, const std::string& reason) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Suppress for 30 days
        txn.exec_params(
            R"(
            INSERT INTO email_suppressions (email, reason, suppressed_until, created_at)
            VALUES ($1, $2, NOW() + INTERVAL '30 days', NOW())
            ON CONFLICT (email) DO UPDATE SET
                reason = $2,
                suppressed_until = NOW() + INTERVAL '30 days'
            )",
            email, reason
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to suppress email: " + std::string(e.what()),
            {{"email", email}}
        );
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value EmailChannel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_sent"] = static_cast<Json::UInt64>(total_sent_);
    stats["total_success"] = static_cast<Json::UInt64>(total_success_);
    stats["total_failed"] = static_cast<Json::UInt64>(total_failed_);
    stats["total_bounced"] = static_cast<Json::UInt64>(total_bounced_);
    stats["success_rate"] = total_sent_ > 0 ?
        static_cast<double>(total_success_) / total_sent_ : 0.0;
    stats["initialized"] = initialized_;

    return stats;
}

void EmailChannel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_sent_ = 0;
    total_success_ = 0;
    total_failed_ = 0;
    total_bounced_ = 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Json::Value EmailChannel::buildSendGridRequest(const EmailMessage& message) {
    Json::Value request;

    // Personalizations (recipients)
    Json::Value personalizations(Json::arrayValue);
    Json::Value personalization;

    Json::Value to_array(Json::arrayValue);
    for (const auto& recipient : message.to) {
        Json::Value to_obj;
        to_obj["email"] = recipient.email;
        if (!recipient.name.empty()) {
            to_obj["name"] = recipient.name;
        }
        to_array.append(to_obj);
    }
    personalization["to"] = to_array;

    // Template data for dynamic templates
    if (message.template_id.has_value() && !message.template_data.isNull()) {
        personalization["dynamic_template_data"] = message.template_data;
    }

    personalizations.append(personalization);
    request["personalizations"] = personalizations;

    // From
    Json::Value from;
    from["email"] = from_email_;
    from["name"] = from_name_;
    request["from"] = from;

    // Reply-to
    if (message.reply_to.has_value()) {
        Json::Value reply_to;
        reply_to["email"] = message.reply_to.value();
        request["reply_to"] = reply_to;
    }

    // Subject and content (not needed if using template)
    if (!message.template_id.has_value()) {
        request["subject"] = message.subject;

        Json::Value content(Json::arrayValue);

        if (!message.text_content.empty()) {
            Json::Value text;
            text["type"] = "text/plain";
            text["value"] = message.text_content;
            content.append(text);
        }

        if (!message.html_content.empty()) {
            Json::Value html;
            html["type"] = "text/html";
            html["value"] = message.html_content;
            content.append(html);
        }

        request["content"] = content;
    } else {
        request["template_id"] = message.template_id.value();
    }

    // Category for analytics
    if (!message.category.empty()) {
        Json::Value categories(Json::arrayValue);
        categories.append(message.category);
        request["categories"] = categories;
    }

    // Tracking settings
    Json::Value tracking;
    Json::Value click_tracking;
    click_tracking["enable"] = message.track_clicks;
    tracking["click_tracking"] = click_tracking;

    Json::Value open_tracking;
    open_tracking["enable"] = message.track_opens;
    tracking["open_tracking"] = open_tracking;

    request["tracking_settings"] = tracking;

    return request;
}

EmailResult EmailChannel::sendSendGridRequest(const Json::Value& request) {
    EmailResult result;
    result.success = false;
    result.should_retry = false;
    result.email_invalid = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error_code = "CURL_INIT_FAILED";
        result.error_message = "Failed to initialize curl";
        return result;
    }

    std::string response;
    std::string message_id;

    Json::StreamWriterBuilder writer;
    std::string body = Json::writeString(writer, request);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, SENDGRID_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &message_id);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error_code = "CURL_ERROR";
        result.error_message = curl_easy_strerror(res);
        result.should_retry = true;
        return result;
    }

    return parseSendGridResponse(response, http_code, message_id);
}

EmailResult EmailChannel::parseSendGridResponse(const std::string& response, long http_code,
                                                const std::string& message_id_header) {
    EmailResult result;
    result.success = false;
    result.should_retry = false;
    result.email_invalid = false;

    // 202 Accepted is success for SendGrid
    if (http_code == 202 || http_code == 200) {
        result.success = true;
        result.message_id = message_id_header;
        return result;
    }

    // Parse error response
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(response, json)) {
        if (json.isMember("errors") && json["errors"].isArray() && !json["errors"].empty()) {
            auto& error = json["errors"][0];
            result.error_message = error.get("message", "Unknown error").asString();
            result.error_code = error.get("field", "UNKNOWN").asString();
        }
    }

    // Check for specific error types
    if (http_code == 400) {
        // Bad request - possibly invalid email
        if (result.error_message.find("email") != std::string::npos ||
            result.error_message.find("invalid") != std::string::npos) {
            result.email_invalid = true;
        }
    } else if (http_code == 401 || http_code == 403) {
        result.error_code = "AUTH_ERROR";
        result.error_message = "SendGrid authentication failed";
    } else if (http_code == 429) {
        result.error_code = "RATE_LIMITED";
        result.error_message = "SendGrid rate limit exceeded";
        result.should_retry = true;
    } else if (http_code >= 500) {
        result.error_code = "SERVER_ERROR";
        result.should_retry = true;
    }

    return result;
}

std::pair<std::string, std::string> EmailChannel::getUserEmail(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT email, name FROM users WHERE id = $1 AND email_verified = true",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return {
                result[0]["email"].as<std::string>(),
                result[0]["name"].is_null() ? "" : result[0]["name"].as<std::string>()
            };
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user email: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return {"", ""};
}

std::string EmailChannel::htmlToPlainText(const std::string& html) const {
    std::string text = html;

    // Remove HTML tags
    static const std::regex tag_regex("<[^>]*>");
    text = std::regex_replace(text, tag_regex, "");

    // Convert entities
    static const std::vector<std::pair<std::string, std::string>> entities = {
        {"&nbsp;", " "},
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&quot;", "\""},
        {"&#39;", "'"},
        {"&apos;", "'"}
    };

    for (const auto& [entity, replacement] : entities) {
        size_t pos = 0;
        while ((pos = text.find(entity, pos)) != std::string::npos) {
            text.replace(pos, entity.length(), replacement);
            pos += replacement.length();
        }
    }

    // Collapse whitespace
    static const std::regex whitespace_regex("\\s+");
    text = std::regex_replace(text, whitespace_regex, " ");

    return text;
}

std::string EmailChannel::generateNotificationHtml(const Notification& notification) const {
    std::ostringstream html;

    std::string color = getNotificationColor(notification.type);
    bool is_urgent = notification.type == NotificationType::CRITICAL_ALERT ||
                     notification.type == NotificationType::HIGH_URGENCY;

    html << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << notification.title << R"(</title>
</head>
<body style="margin: 0; padding: 0; font-family: Arial, sans-serif; background-color: #f4f4f4;">
    <table width="100%" cellpadding="0" cellspacing="0" style="background-color: #f4f4f4; padding: 20px;">
        <tr>
            <td align="center">
                <table width="600" cellpadding="0" cellspacing="0" style="background-color: #ffffff; border-radius: 8px; overflow: hidden;">
                    <!-- Header -->
                    <tr>
                        <td style="background-color: )" << color << R"(; padding: 20px; text-align: center;">
                            <h1 style="color: #ffffff; margin: 0; font-size: 24px;">)" << notification.title << R"(</h1>
                        </td>
                    </tr>
)";

    // Urgent banner
    if (is_urgent) {
        html << R"(
                    <tr>
                        <td style="background-color: #ff0000; padding: 10px; text-align: center;">
                            <strong style="color: #ffffff; font-size: 16px;">URGENT - TIME SENSITIVE</strong>
                        </td>
                    </tr>
)";
    }

    // Body
    html << R"(
                    <tr>
                        <td style="padding: 30px;">
                            <p style="font-size: 16px; line-height: 1.6; color: #333333;">)" << notification.body << R"(</p>
)";

    // Image
    if (notification.image_url.has_value()) {
        html << R"(
                            <img src=")" << notification.image_url.value() << R"(" alt="Dog photo" style="max-width: 100%; height: auto; border-radius: 8px; margin: 20px 0;">
)";
    }

    // Action button
    if (notification.action_url.has_value()) {
        html << R"(
                            <p style="text-align: center; margin-top: 30px;">
                                <a href=")" << notification.action_url.value() << R"(" style="display: inline-block; background-color: )" << color << R"(; color: #ffffff; padding: 15px 30px; text-decoration: none; border-radius: 5px; font-weight: bold;">View Details</a>
                            </p>
)";
    }

    html << R"(
                        </td>
                    </tr>
                    <!-- Footer -->
                    <tr>
                        <td style="background-color: #f8f8f8; padding: 20px; text-align: center; font-size: 12px; color: #666666;">
                            <p>You received this email because you're subscribed to WaitingTheLongest alerts.</p>
                            <p><a href="https://waitingthelongest.com/preferences" style="color: #666666;">Manage your notification preferences</a></p>
                            <p style="margin-top: 10px;">&copy; WaitingTheLongest.com - Helping Dogs Find Forever Homes</p>
                        </td>
                    </tr>
                </table>
            </td>
        </tr>
    </table>
</body>
</html>)";

    return html.str();
}

} // namespace wtl::notifications::channels

/**
 * @file ContentGenerationWorker.cc
 * @brief Implementation of ContentGenerationWorker
 */

#include "ContentGenerationWorker.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace wtl::workers {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

ContentGenerationWorker::ContentGenerationWorker()
    : BaseWorker("content_generation_worker",
                 "Generates daily roundups and social media content",
                 std::chrono::seconds(3600),  // Every hour
                 WorkerPriority::LOW)
{
}

ContentGenerationResult ContentGenerationWorker::getLastResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_;
}

ContentGenerationResult ContentGenerationWorker::generateNow() {
    ContentGenerationResult result;

    try {
        // Check if it's time for daily roundup (6 AM)
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_now = std::gmtime(&time_t_now);

        if (tm_now->tm_hour == 6) {
            generateDailyUrgentRoundup();
            result.roundups_generated++;
        }

        // Generate social posts for critical dogs
        generateTikTokPosts();

        // Process scheduled blog posts
        processScheduledBlogPosts();

    } catch (const std::exception& e) {
        result.errors++;
        logError("Content generation failed: " + std::string(e.what()), {});
    }

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        last_result_ = result;
    }

    return result;
}

WorkerResult ContentGenerationWorker::doExecute() {
    ContentGenerationResult result = generateNow();

    int total = result.roundups_generated + result.social_posts_created +
                result.blog_posts_scheduled;

    WorkerResult wr = WorkerResult::Success(
        "Generated " + std::to_string(total) + " content items", total);
    wr.metadata = result.toJson();

    return wr;
}

void ContentGenerationWorker::generateDailyUrgentRoundup() {
    logInfo("Generating daily urgent dogs roundup...");

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get critical and high urgency dogs
        auto dogs = txn.exec(R"(
            SELECT d.id, d.name, d.breed_primary, s.name as shelter_name,
                   s.city, s.state_code, d.euthanasia_date,
                   EXTRACT(EPOCH FROM (d.euthanasia_date - NOW())) / 3600 AS hours_remaining
            FROM dogs d
            JOIN shelters s ON d.shelter_id = s.id
            WHERE d.is_available = true
            AND d.urgency_level IN ('critical', 'high')
            ORDER BY d.euthanasia_date ASC NULLS LAST
            LIMIT 50
        )");

        std::ostringstream content;
        content << "# Daily Urgent Dogs Roundup - ";

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        content << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%d") << "\n\n";

        content << "## Dogs Needing Immediate Help\n\n";

        int critical_count = 0;
        int high_count = 0;

        for (const auto& row : dogs) {
            double hours = row["hours_remaining"].is_null() ? 999.0 : row["hours_remaining"].as<double>();

            if (hours <= 24) {
                critical_count++;
                content << "### CRITICAL: " << row["name"].as<std::string>() << "\n";
            } else {
                high_count++;
                content << "### " << row["name"].as<std::string>() << "\n";
            }

            content << "- Breed: " << row["breed_primary"].as<std::string>() << "\n";
            content << "- Location: " << row["shelter_name"].as<std::string>()
                    << ", " << row["city"].as<std::string>()
                    << ", " << row["state_code"].as<std::string>() << "\n";

            if (!row["euthanasia_date"].is_null()) {
                content << "- Time Remaining: " << static_cast<int>(hours) << " hours\n";
            }

            content << "\n";
        }

        content << "\n---\n";
        content << "Total Critical: " << critical_count << "\n";
        content << "Total High Urgency: " << high_count << "\n";

        // Store the roundup
        txn.exec_params(
            R"(INSERT INTO content_posts (
                post_type, title, content, status, scheduled_at, created_at
            ) VALUES ('roundup', $1, $2, 'published', NOW(), NOW()))",
            "Daily Urgent Dogs Roundup",
            content.str()
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        logInfo("Daily roundup generated with " + std::to_string(critical_count) +
                " critical and " + std::to_string(high_count) + " high urgency dogs");

    } catch (const std::exception& e) {
        logError("Failed to generate daily roundup: " + std::string(e.what()), {});
    }
}

void ContentGenerationWorker::generateTikTokPosts() {
    // Check for dogs that need social media posts
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find critical dogs without recent social posts
        auto dogs = txn.exec(R"(
            SELECT d.id, d.name, d.breed_primary, d.photo_urls,
                   s.name as shelter_name, s.city, s.state_code
            FROM dogs d
            JOIN shelters s ON d.shelter_id = s.id
            LEFT JOIN social_posts sp ON d.id = sp.dog_id
                AND sp.created_at > NOW() - INTERVAL '24 hours'
            WHERE d.is_available = true
            AND d.urgency_level = 'critical'
            AND sp.id IS NULL
            LIMIT 5
        )");

        for (const auto& row : dogs) {
            Json::Value dog;
            dog["id"] = row["id"].as<std::string>();
            dog["name"] = row["name"].as<std::string>();
            dog["breed"] = row["breed_primary"].as<std::string>();
            dog["shelter"] = row["shelter_name"].as<std::string>();
            dog["location"] = row["city"].as<std::string>() + ", " +
                             row["state_code"].as<std::string>();

            std::string content = generateSocialPostContent(dog);

            txn.exec_params(
                R"(INSERT INTO social_posts (
                    dog_id, platform, content, status, created_at
                ) VALUES ($1, 'tiktok', $2, 'pending', NOW()))",
                dog["id"].asString(),
                content
            );
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Failed to generate TikTok posts: " + std::string(e.what()));
    }
}

void ContentGenerationWorker::processScheduledBlogPosts() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find and publish scheduled posts
        auto result = txn.exec(R"(
            UPDATE content_posts
            SET status = 'published', published_at = NOW()
            WHERE status = 'scheduled'
            AND scheduled_at <= NOW()
            RETURNING id
        )");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.size() > 0) {
            logInfo("Published " + std::to_string(result.size()) + " scheduled blog posts");
        }

    } catch (const std::exception& e) {
        logWarning("Failed to process scheduled blog posts: " + std::string(e.what()));
    }
}

std::string ContentGenerationWorker::generateUrgentDogsContent() {
    // Placeholder for more complex content generation
    return "";
}

std::string ContentGenerationWorker::generateSocialPostContent(const Json::Value& dog) {
    std::ostringstream content;

    content << "URGENT: " << dog["name"].asString() << " needs a home NOW!\n\n";
    content << "Breed: " << dog["breed"].asString() << "\n";
    content << "Location: " << dog["location"].asString() << "\n";
    content << "Shelter: " << dog["shelter"].asString() << "\n\n";
    content << "This sweet pup is running out of time. "
            << "Can you help spread the word?\n\n";
    content << "#SaveALife #AdoptDontShop #UrgentDog #RescueDog";

    return content.str();
}

} // namespace wtl::workers

/**
 * @file ContentGenerationWorker.h
 * @brief Worker for generating scheduled content
 *
 * PURPOSE:
 * Generates scheduled content including daily urgent roundups,
 * auto-generated social media posts, and blog post scheduling.
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <json/json.h>
#include "BaseWorker.h"

namespace wtl::workers {

/**
 * @struct ContentGenerationResult
 */
struct ContentGenerationResult {
    int roundups_generated{0};
    int social_posts_created{0};
    int blog_posts_scheduled{0};
    int errors{0};

    Json::Value toJson() const {
        Json::Value json;
        json["roundups_generated"] = roundups_generated;
        json["social_posts_created"] = social_posts_created;
        json["blog_posts_scheduled"] = blog_posts_scheduled;
        json["errors"] = errors;
        return json;
    }
};

/**
 * @class ContentGenerationWorker
 * @brief Worker for generating daily content
 */
class ContentGenerationWorker : public BaseWorker {
public:
    /**
     * @brief Constructor - runs hourly by default
     */
    ContentGenerationWorker();

    ~ContentGenerationWorker() override = default;

    ContentGenerationResult getLastResult() const;
    ContentGenerationResult generateNow();

    void generateDailyUrgentRoundup();
    void generateTikTokPosts();
    void processScheduledBlogPosts();

protected:
    WorkerResult doExecute() override;

private:
    std::string generateUrgentDogsContent();
    std::string generateSocialPostContent(const Json::Value& dog);

    mutable std::mutex result_mutex_;
    ContentGenerationResult last_result_;
};

} // namespace wtl::workers

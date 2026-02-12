/**
 * @file BlogEngine.cc
 * @brief Implementation of BlogEngine
 * @see BlogEngine.h for documentation
 */

#include "content/blog/BlogEngine.h"

// Standard library includes
#include <algorithm>

// Project includes
#include "content/blog/ContentGenerator.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/services/FosterService.h"
#include "core/EventBus.h"

namespace wtl::content::blog {

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

BlogEngine& BlogEngine::getInstance() {
    static BlogEngine instance;
    return instance;
}

BlogEngine::BlogEngine()
    : running_(false)
{
    schedule_config_ = ScheduleConfig();
    stats_ = EngineStats();
}

BlogEngine::~BlogEngine() {
    shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void BlogEngine::initialize(const ScheduleConfig& schedule_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return;  // Already running
    }

    schedule_config_ = schedule_config;
    running_ = true;

    // Start background scheduler thread
    scheduler_thread_ = std::thread(&BlogEngine::schedulerLoop, this);

    // Subscribe to relevant events
    auto& event_bus = wtl::core::EventBus::getInstance();

    event_bus.subscribe(wtl::core::EventType::DOG_BECAME_CRITICAL,
        [this](const wtl::core::Event& event) {
            if (event.data.isMember("dog_id")) {
                onDogUrgencyChanged(event.data["dog_id"].asString(), "critical");
            }
        });

    event_bus.subscribe(wtl::core::EventType::FOSTER_PLACEMENT_ENDED,
        [this](const wtl::core::Event& event) {
            if (event.data.isMember("adoption_id") &&
                event.data.isMember("outcome") &&
                (event.data["outcome"].asString() == "adopted_by_foster" ||
                 event.data["outcome"].asString() == "adopted_other")) {
                onAdoptionCompleted(event.data["adoption_id"].asString());
            }
        });

    event_bus.subscribe(wtl::core::EventType::DOG_CREATED,
        [this](const wtl::core::Event& event) {
            if (event.data.isMember("dog_id")) {
                onNewDogAdded(event.data["dog_id"].asString());
            }
        });
}

void BlogEngine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }

    cv_.notify_all();

    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

bool BlogEngine::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

// ============================================================================
// URGENT ROUNDUP
// ============================================================================

GenerationResult BlogEngine::generateUrgentRoundup(bool auto_publish) {
    return generateUrgentRoundupForDate(std::chrono::system_clock::now(), auto_publish);
}

GenerationResult BlogEngine::generateUrgentRoundupForDate(
    const std::chrono::system_clock::time_point& date,
    bool auto_publish
) {
    GenerationResult result;

    try {
        // Get all urgent dogs
        auto urgent_dogs = getUrgentDogs();

        if (urgent_dogs.empty()) {
            result.success = false;
            result.error = "No urgent dogs found";
            return result;
        }

        // Generate the roundup post
        auto& generator = MarkdownGenerator::getInstance();
        BlogPost post = generator.generateUrgentRoundup(urgent_dogs, date);

        // Set status based on auto_publish
        if (auto_publish) {
            post.status = PostStatus::PUBLISHED;
            post.published_at = std::chrono::system_clock::now();
        } else {
            post.status = PostStatus::DRAFT;
        }

        // Save the post
        std::string post_id = savePost(post);

        result.success = true;
        result.post_id = post_id;
        result.post = post;
        result.post.id = post_id;

        // Update stats
        updateStats(BlogCategory::URGENT);
        stats_.last_urgent_roundup = std::chrono::system_clock::now();

        // Emit event
        wtl::core::Event event;
        event.type = wtl::core::EventType::CONTENT_GENERATED;
        event.data["post_id"] = post_id;
        event.data["category"] = "urgent";
        event.data["dog_count"] = static_cast<int>(urgent_dogs.size());
        wtl::core::EventBus::getInstance().publish(event);

    } catch (const std::exception& e) {
        result.success = false;
        result.error = "Failed to generate urgent roundup: " + std::string(e.what());

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            result.error,
            {}
        );
    }

    return result;
}

// ============================================================================
// DOG FEATURES
// ============================================================================

GenerationResult BlogEngine::generateDogFeature(
    const wtl::core::models::Dog& dog,
    bool auto_publish
) {
    GenerationResult result;

    try {
        // Get shelter info
        std::optional<wtl::core::models::Shelter> shelter;
        if (!dog.shelter_id.empty()) {
            auto& shelter_service = wtl::core::services::ShelterService::getInstance();
            shelter = shelter_service.findById(dog.shelter_id);
        }

        // Generate the feature post
        auto& generator = MarkdownGenerator::getInstance();
        BlogPost post = generator.generateDogFeature(dog, shelter);

        // Set status
        if (auto_publish) {
            post.status = PostStatus::PUBLISHED;
            post.published_at = std::chrono::system_clock::now();
        } else {
            post.status = PostStatus::DRAFT;
        }

        // Save the post
        std::string post_id = savePost(post);

        result.success = true;
        result.post_id = post_id;
        result.post = post;
        result.post.id = post_id;

        // Update stats
        if (dog.urgency_level == "critical" || dog.urgency_level == "high") {
            updateStats(BlogCategory::URGENT);
        } else {
            updateStats(BlogCategory::LONGEST_WAITING);
        }
        stats_.last_feature = std::chrono::system_clock::now();

        // Emit event
        wtl::core::Event event;
        event.type = wtl::core::EventType::CONTENT_GENERATED;
        event.data["post_id"] = post_id;
        event.data["category"] = "dog_feature";
        event.data["dog_id"] = dog.id;
        wtl::core::EventBus::getInstance().publish(event);

    } catch (const std::exception& e) {
        result.success = false;
        result.error = "Failed to generate dog feature: " + std::string(e.what());

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            result.error,
            {{"dog_id", dog.id}}
        );
    }

    return result;
}

GenerationResult BlogEngine::generateDogFeatureById(
    const std::string& dog_id,
    bool auto_publish
) {
    GenerationResult result;

    auto& dog_service = wtl::core::services::DogService::getInstance();
    auto dog = dog_service.findById(dog_id);

    if (!dog) {
        result.success = false;
        result.error = "Dog not found: " + dog_id;
        return result;
    }

    return generateDogFeature(*dog, auto_publish);
}

std::vector<GenerationResult> BlogEngine::generateLongestWaitingFeatures(int count) {
    std::vector<GenerationResult> results;

    auto dogs = getLongestWaitingWithoutFeatures(count);

    for (const auto& dog : dogs) {
        results.push_back(generateDogFeature(dog, false));
    }

    return results;
}

std::vector<GenerationResult> BlogEngine::generateOverlookedDogFeatures(int count) {
    std::vector<GenerationResult> results;

    auto dogs = getOverlookedDogsWithoutFeatures(count);

    for (const auto& dog : dogs) {
        auto result = generateDogFeature(dog, false);
        if (result.success) {
            // Update category to OVERLOOKED
            result.post.category = BlogCategory::OVERLOOKED;
            // Update in database
            try {
                auto& pool = wtl::core::db::ConnectionPool::getInstance();
                auto conn = pool.acquire();
                pqxx::work txn(*conn);
                txn.exec_params(
                    "UPDATE blog_posts SET category = $1 WHERE id = $2",
                    blogCategoryToString(BlogCategory::OVERLOOKED),
                    result.post_id
                );
                txn.commit();
                pool.release(conn);
            } catch (...) {
                // Log but don't fail
            }
        }
        results.push_back(result);
    }

    return results;
}

// ============================================================================
// SUCCESS STORIES
// ============================================================================

GenerationResult BlogEngine::generateSuccessStory(
    const wtl::core::models::FosterPlacement& adoption
) {
    return generateSuccessStoryById(adoption.id);
}

GenerationResult BlogEngine::generateSuccessStoryById(
    const std::string& adoption_id,
    const std::string& adopter_story,
    const std::vector<std::string>& after_photos
) {
    GenerationResult result;

    try {
        // Get adoption record
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM foster_placements WHERE id = $1",
            adoption_id
        );

        if (res.empty()) {
            result.success = false;
            result.error = "Adoption not found: " + adoption_id;
            pool.release(conn);
            return result;
        }

        auto adoption = wtl::core::models::FosterPlacement::fromDbRow(res[0]);
        txn.commit();
        pool.release(conn);

        // Get dog
        auto& dog_service = wtl::core::services::DogService::getInstance();
        auto dog = dog_service.findById(adoption.dog_id);

        if (!dog) {
            result.success = false;
            result.error = "Dog not found for adoption";
            return result;
        }

        // Generate success story
        auto& generator = MarkdownGenerator::getInstance();
        BlogPost post = generator.generateSuccessStory(*dog, adoption, adopter_story, after_photos);

        post.status = PostStatus::DRAFT;

        // Save
        std::string post_id = savePost(post);

        result.success = true;
        result.post_id = post_id;
        result.post = post;
        result.post.id = post_id;

        // Update stats
        updateStats(BlogCategory::SUCCESS_STORY);
        stats_.last_success_story = std::chrono::system_clock::now();

        // Emit event
        wtl::core::Event event;
        event.type = wtl::core::EventType::CONTENT_GENERATED;
        event.data["post_id"] = post_id;
        event.data["category"] = "success_story";
        event.data["adoption_id"] = adoption_id;
        wtl::core::EventBus::getInstance().publish(event);

    } catch (const std::exception& e) {
        result.success = false;
        result.error = "Failed to generate success story: " + std::string(e.what());

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            result.error,
            {{"adoption_id", adoption_id}}
        );
    }

    return result;
}

std::vector<GenerationResult> BlogEngine::generateRecentSuccessStories(
    int days_back,
    int max_count
) {
    std::vector<GenerationResult> results;

    auto adoptions = getRecentAdoptionsWithoutStories(days_back, max_count);

    for (const auto& adoption : adoptions) {
        results.push_back(generateSuccessStoryById(adoption.id));
    }

    return results;
}

// ============================================================================
// EDUCATIONAL CONTENT
// ============================================================================

GenerationResult BlogEngine::generateEducationalPost(
    const std::string& topic,
    const Json::Value& custom_data
) {
    GenerationResult result;

    try {
        auto& generator = MarkdownGenerator::getInstance();
        BlogPost post = generator.generateEducationalPost(topic, custom_data);

        post.status = PostStatus::DRAFT;

        std::string post_id = savePost(post);

        result.success = true;
        result.post_id = post_id;
        result.post = post;
        result.post.id = post_id;

        updateStats(BlogCategory::EDUCATIONAL);

    } catch (const std::exception& e) {
        result.success = false;
        result.error = "Failed to generate educational post: " + std::string(e.what());

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            result.error,
            {{"topic", topic}}
        );
    }

    return result;
}

// ============================================================================
// SCHEDULING
// ============================================================================

int BlogEngine::schedulePosts() {
    std::lock_guard<std::mutex> lock(mutex_);

    int scheduled_count = 0;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Get draft posts ordered by priority (urgent first, then by category priority)
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'draft' "
            "ORDER BY "
            "CASE category "
            "  WHEN 'urgent' THEN 1 "
            "  WHEN 'longest_waiting' THEN 2 "
            "  WHEN 'overlooked' THEN 3 "
            "  WHEN 'new_arrivals' THEN 4 "
            "  WHEN 'success_story' THEN 5 "
            "  WHEN 'educational' THEN 6 "
            "END, "
            "created_at ASC "
            "LIMIT $1",
            schedule_config_.max_scheduled_posts
        );

        auto next_slot = getNextPublishSlot();

        for (const auto& row : res) {
            std::string post_id = row["id"].as<std::string>();

            // Schedule the post
            auto scheduled_time = next_slot;

            txn.exec_params(
                "UPDATE blog_posts SET status = 'scheduled', scheduled_at = $1, updated_at = NOW() WHERE id = $2",
                MarkdownGenerator::getInstance().formatDate(scheduled_time, "%Y-%m-%dT%H:%M:%SZ"),
                post_id
            );

            // Move to next slot
            next_slot += std::chrono::hours(schedule_config_.min_hours_between_posts);
            scheduled_count++;
        }

        txn.commit();
        pool.release(conn);

        stats_.posts_scheduled = scheduled_count;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to schedule posts: " + std::string(e.what()),
            {}
        );
    }

    return scheduled_count;
}

std::vector<ScheduleInfo> BlogEngine::getScheduledPosts() {
    std::vector<ScheduleInfo> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec(
            "SELECT id, title, category, scheduled_at, is_auto_generated "
            "FROM blog_posts WHERE status = 'scheduled' "
            "ORDER BY scheduled_at ASC"
        );

        for (const auto& row : res) {
            ScheduleInfo info;
            info.post_id = row["id"].as<std::string>();
            info.title = row["title"].as<std::string>();
            info.category = blogCategoryFromString(row["category"].as<std::string>());

            // Parse scheduled_at
            std::string scheduled_str = row["scheduled_at"].as<std::string>();
            std::tm tm = {};
            std::istringstream ss(scheduled_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            info.scheduled_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            info.is_auto_generated = row["is_auto_generated"].as<bool>(false);

            posts.push_back(info);
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get scheduled posts: " + std::string(e.what()),
            {}
        );
    }

    return posts;
}

bool BlogEngine::reschedulePost(
    const std::string& post_id,
    const std::chrono::system_clock::time_point& new_time
) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto time_str = MarkdownGenerator::getInstance().formatDate(new_time, "%Y-%m-%dT%H:%M:%SZ");

        txn.exec_params(
            "UPDATE blog_posts SET scheduled_at = $1, updated_at = NOW() WHERE id = $2 AND status = 'scheduled'",
            time_str, post_id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to reschedule post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

bool BlogEngine::cancelScheduledPost(const std::string& post_id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE blog_posts SET status = 'draft', scheduled_at = NULL, updated_at = NOW() "
            "WHERE id = $1 AND status = 'scheduled'",
            post_id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to cancel scheduled post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

// ============================================================================
// PUBLISHING
// ============================================================================

bool BlogEngine::publishPost(const std::string& post_id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto now_str = MarkdownGenerator::getInstance().formatDate(
            std::chrono::system_clock::now(), "%Y-%m-%dT%H:%M:%SZ");

        txn.exec_params(
            "UPDATE blog_posts SET status = 'published', published_at = $1, updated_at = $1 WHERE id = $2",
            now_str, post_id
        );

        txn.commit();
        pool.release(conn);

        stats_.posts_published_today++;

        // Emit event
        wtl::core::Event event;
        event.type = wtl::core::EventType::CONTENT_PUBLISHED;
        event.data["post_id"] = post_id;
        wtl::core::EventBus::getInstance().publish(event);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to publish post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

bool BlogEngine::unpublishPost(const std::string& post_id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE blog_posts SET status = 'draft', updated_at = NOW() WHERE id = $1",
            post_id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to unpublish post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

int BlogEngine::processScheduledPosts() {
    int published = 0;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto now_str = MarkdownGenerator::getInstance().formatDate(
            std::chrono::system_clock::now(), "%Y-%m-%dT%H:%M:%SZ");

        // Get posts that are due
        auto res = txn.exec_params(
            "SELECT id FROM blog_posts WHERE status = 'scheduled' AND scheduled_at <= $1",
            now_str
        );

        txn.commit();
        pool.release(conn);

        for (const auto& row : res) {
            std::string post_id = row["id"].as<std::string>();
            if (publishPost(post_id)) {
                published++;
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to process scheduled posts: " + std::string(e.what()),
            {}
        );
    }

    return published;
}

// ============================================================================
// STATISTICS
// ============================================================================

BlogEngine::EngineStats BlogEngine::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void BlogEngine::setScheduleConfig(const ScheduleConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    schedule_config_ = config;
}

ScheduleConfig BlogEngine::getScheduleConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return schedule_config_;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void BlogEngine::onDogUrgencyChanged(const std::string& dog_id, const std::string& new_urgency) {
    // For critical dogs, consider generating a feature immediately
    if (new_urgency == "critical" && !dogHasRecentFeature(dog_id, 7)) {
        generateDogFeatureById(dog_id, false);
    }
}

void BlogEngine::onAdoptionCompleted(const std::string& adoption_id) {
    // Queue success story generation
    if (schedule_config_.auto_publish_success_stories && !adoptionHasStory(adoption_id)) {
        generateSuccessStoryById(adoption_id);
    }
}

void BlogEngine::onNewDogAdded(const std::string& dog_id) {
    // Could potentially generate new arrivals content
    // For now, just log
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void BlogEngine::schedulerLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for 15 minutes or until shutdown
        if (cv_.wait_for(lock, std::chrono::minutes(15), [this] { return !running_; })) {
            break;  // Shutdown requested
        }

        lock.unlock();

        // Process scheduled posts
        processScheduledPosts();

        // Check if we need to generate daily urgent roundup
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        if (schedule_config_.auto_generate_urgent_roundup &&
            tm.tm_hour == schedule_config_.urgent_roundup_hour) {

            // Check if we haven't generated one today
            auto today_start = now - std::chrono::hours(tm.tm_hour) -
                std::chrono::minutes(tm.tm_min) - std::chrono::seconds(tm.tm_sec);

            if (stats_.last_urgent_roundup < today_start) {
                generateUrgentRoundup(true);  // Auto-publish
            }
        }

        // Auto-schedule draft posts
        schedulePosts();
    }
}

std::vector<wtl::core::models::Dog> BlogEngine::getUrgentDogs() {
    std::vector<wtl::core::models::Dog> dogs;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec(
            "SELECT * FROM dogs WHERE is_available = true "
            "AND (urgency_level = 'critical' OR urgency_level = 'high') "
            "ORDER BY urgency_level DESC, euthanasia_date ASC NULLS LAST "
            "LIMIT 20"
        );

        for (const auto& row : res) {
            dogs.push_back(wtl::core::models::Dog::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get urgent dogs: " + std::string(e.what()),
            {}
        );
    }

    return dogs;
}

std::vector<wtl::core::models::Dog> BlogEngine::getLongestWaitingWithoutFeatures(int count) {
    std::vector<wtl::core::models::Dog> dogs;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec_params(
            "SELECT d.* FROM dogs d "
            "LEFT JOIN blog_posts bp ON d.id = bp.dog_id "
            "AND bp.created_at > NOW() - INTERVAL '30 days' "
            "WHERE d.is_available = true AND bp.id IS NULL "
            "ORDER BY d.intake_date ASC "
            "LIMIT $1",
            count
        );

        for (const auto& row : res) {
            dogs.push_back(wtl::core::models::Dog::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get longest waiting dogs: " + std::string(e.what()),
            {}
        );
    }

    return dogs;
}

std::vector<wtl::core::models::Dog> BlogEngine::getOverlookedDogsWithoutFeatures(int count) {
    std::vector<wtl::core::models::Dog> dogs;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Overlooked: seniors, large/xlarge, black dogs
        auto res = txn.exec_params(
            "SELECT d.* FROM dogs d "
            "LEFT JOIN blog_posts bp ON d.id = bp.dog_id "
            "AND bp.created_at > NOW() - INTERVAL '30 days' "
            "WHERE d.is_available = true AND bp.id IS NULL "
            "AND (d.age_category = 'senior' OR d.size IN ('large', 'xlarge') "
            "OR d.color_primary ILIKE '%black%') "
            "ORDER BY d.intake_date ASC "
            "LIMIT $1",
            count
        );

        for (const auto& row : res) {
            dogs.push_back(wtl::core::models::Dog::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get overlooked dogs: " + std::string(e.what()),
            {}
        );
    }

    return dogs;
}

std::vector<wtl::core::models::FosterPlacement> BlogEngine::getRecentAdoptionsWithoutStories(
    int days_back, int max_count
) {
    std::vector<wtl::core::models::FosterPlacement> adoptions;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec_params(
            "SELECT fp.* FROM foster_placements fp "
            "LEFT JOIN blog_posts bp ON fp.id = bp.adoption_id "
            "WHERE fp.outcome IN ('adopted_by_foster', 'adopted_other') "
            "AND fp.actual_end_date > NOW() - INTERVAL '$1 days' "
            "AND bp.id IS NULL "
            "ORDER BY fp.actual_end_date DESC "
            "LIMIT $2",
            days_back, max_count
        );

        for (const auto& row : res) {
            adoptions.push_back(wtl::core::models::FosterPlacement::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get recent adoptions: " + std::string(e.what()),
            {}
        );
    }

    return adoptions;
}

bool BlogEngine::dogHasRecentFeature(const std::string& dog_id, int days) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec_params(
            "SELECT COUNT(*) FROM blog_posts WHERE dog_id = $1 "
            "AND created_at > NOW() - INTERVAL '$2 days'",
            dog_id, days
        );

        int count = res[0][0].as<int>();

        txn.commit();
        pool.release(conn);

        return count > 0;

    } catch (...) {
        return false;
    }
}

bool BlogEngine::adoptionHasStory(const std::string& adoption_id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        auto res = txn.exec_params(
            "SELECT COUNT(*) FROM blog_posts WHERE adoption_id = $1",
            adoption_id
        );

        int count = res[0][0].as<int>();

        txn.commit();
        pool.release(conn);

        return count > 0;

    } catch (...) {
        return false;
    }
}

std::chrono::system_clock::time_point BlogEngine::getNextPublishSlot() {
    auto now = std::chrono::system_clock::now();

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Get latest scheduled time
        auto res = txn.exec(
            "SELECT MAX(scheduled_at) FROM blog_posts WHERE status = 'scheduled'"
        );

        txn.commit();
        pool.release(conn);

        if (!res.empty() && !res[0][0].is_null()) {
            std::string latest_str = res[0][0].as<std::string>();
            std::tm tm = {};
            std::istringstream ss(latest_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            auto latest = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            // Next slot is min_hours after latest
            return latest + std::chrono::hours(schedule_config_.min_hours_between_posts);
        }

    } catch (...) {
        // Fall through to default
    }

    // Default: next hour
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_hour += 1;

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string BlogEngine::savePost(const BlogPost& post) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Generate UUID for new post
        auto id_res = txn.exec("SELECT gen_random_uuid()::text");
        std::string post_id = id_res[0][0].as<std::string>();

        // Insert post
        txn.exec_params(
            "INSERT INTO blog_posts (id, title, slug, content, excerpt, category, tags, "
            "featured_image, gallery_images, dog_id, adoption_id, shelter_id, "
            "status, scheduled_at, published_at, meta_description, og_title, og_description, og_image, "
            "author_id, author_name, is_auto_generated, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, NOW(), NOW())",
            post_id,
            post.title,
            post.slug,
            post.content,
            post.excerpt,
            blogCategoryToString(post.category),
            BlogPost::formatPostgresArray(post.tags),
            post.featured_image.value_or(""),
            BlogPost::formatPostgresArray(post.gallery_images),
            post.dog_id.value_or(""),
            post.adoption_id.value_or(""),
            post.shelter_id.value_or(""),
            postStatusToString(post.status),
            post.scheduled_at ? MarkdownGenerator::getInstance().formatDate(*post.scheduled_at, "%Y-%m-%dT%H:%M:%SZ") : "",
            post.published_at ? MarkdownGenerator::getInstance().formatDate(*post.published_at, "%Y-%m-%dT%H:%M:%SZ") : "",
            post.meta_description,
            post.og_title.value_or(""),
            post.og_description.value_or(""),
            post.og_image.value_or(""),
            post.author_id.value_or(""),
            post.author_name,
            post.is_auto_generated
        );

        txn.commit();
        pool.release(conn);

        stats_.total_posts_generated++;

        return post_id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to save blog post: " + std::string(e.what()),
            {{"title", post.title}}
        );
        throw;
    }
}

void BlogEngine::updateStats(BlogCategory category) {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (category) {
        case BlogCategory::URGENT:
            stats_.urgent_roundups_this_week++;
            break;
        case BlogCategory::LONGEST_WAITING:
        case BlogCategory::OVERLOOKED:
        case BlogCategory::NEW_ARRIVALS:
            stats_.dog_features_this_week++;
            break;
        case BlogCategory::SUCCESS_STORY:
            stats_.success_stories_this_week++;
            break;
        default:
            break;
    }
}

} // namespace wtl::content::blog

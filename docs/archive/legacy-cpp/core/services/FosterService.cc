/**
 * @file FosterService.cc
 * @brief Implementation of FosterService
 * @see FosterService.h for documentation
 */

#include "core/services/FosterService.h"

// Additional includes for implementation
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/EventBus.h"

#include <random>
#include <sstream>
#include <iomanip>

namespace wtl::core::services {

using ::wtl::core::debug::ErrorCategory;

// ============================================================================
// SINGLETON
// ============================================================================

FosterService& FosterService::getInstance() {
    static FosterService instance;
    return instance;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string FosterService::generateUuid() {
    // Generate a random UUID v4
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";  // Version 4
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);  // Variant
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);

    return ss.str();
}

std::chrono::system_clock::time_point FosterService::now() {
    return std::chrono::system_clock::now();
}

// ============================================================================
// FOSTER HOME CRUD
// ============================================================================

std::optional<models::FosterHome> FosterService::findById(const std::string& id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_homes WHERE id = $1",
            id
        );

        db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return models::FosterHome::fromDbRow(result[0]);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find foster home: " + std::string(e.what()),
            {{"foster_home_id", id}, {"operation", "findById"}}
        );
        throw;
    }
}

std::optional<models::FosterHome> FosterService::findByUserId(const std::string& user_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_homes WHERE user_id = $1",
            user_id
        );

        db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return models::FosterHome::fromDbRow(result[0]);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find foster home by user: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "findByUserId"}}
        );
        throw;
    }
}

std::vector<models::FosterHome> FosterService::findAll(int limit, int offset) {
    std::vector<models::FosterHome> homes;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_homes ORDER BY created_at DESC LIMIT $1 OFFSET $2",
            limit, offset
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            homes.push_back(models::FosterHome::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find all foster homes: " + std::string(e.what()),
            {{"limit", std::to_string(limit)}, {"offset", std::to_string(offset)}}
        );
        throw;
    }

    return homes;
}

std::vector<models::FosterHome> FosterService::findActive() {
    std::vector<models::FosterHome> homes;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT * FROM foster_homes WHERE is_active = TRUE ORDER BY created_at DESC"
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            homes.push_back(models::FosterHome::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find active foster homes: " + std::string(e.what()),
            {{"operation", "findActive"}}
        );
        throw;
    }

    return homes;
}

std::vector<models::FosterHome> FosterService::findByZipCode(const std::string& zip, int radius_miles) {
    std::vector<models::FosterHome> homes;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // For now, simple ZIP code prefix matching
        // In production, this would use PostGIS for proper distance calculations
        std::string zip_prefix = zip.substr(0, 3);

        auto result = txn.exec_params(
            "SELECT * FROM foster_homes "
            "WHERE is_active = TRUE AND zip_code LIKE $1 "
            "AND max_transport_miles >= $2 "
            "ORDER BY fosters_completed DESC",
            zip_prefix + "%", radius_miles
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            homes.push_back(models::FosterHome::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find foster homes by ZIP: " + std::string(e.what()),
            {{"zip", zip}, {"radius", std::to_string(radius_miles)}}
        );
        throw;
    }

    return homes;
}

models::FosterHome FosterService::save(const models::FosterHome& home) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        models::FosterHome new_home = home;
        new_home.id = generateUuid();
        new_home.created_at = now();
        new_home.updated_at = now();

        // Convert vectors to PostgreSQL array format
        auto toArrayLiteral = [](const std::vector<std::string>& vec) -> std::string {
            if (vec.empty()) return "{}";
            std::string result = "{";
            for (size_t i = 0; i < vec.size(); i++) {
                if (i > 0) result += ",";
                result += "\"" + vec[i] + "\"";
            }
            result += "}";
            return result;
        };

        txn.exec_params(
            "INSERT INTO foster_homes (id, user_id, max_dogs, current_dogs, "
            "has_yard, has_other_dogs, has_cats, has_children, children_ages, "
            "ok_with_puppies, ok_with_seniors, ok_with_medical, ok_with_behavioral, "
            "preferred_sizes, preferred_breeds, zip_code, max_transport_miles, "
            "latitude, longitude, is_active, is_verified, background_check_date, "
            "fosters_completed, fosters_converted_to_adoption, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, "
            "$16, $17, $18, $19, $20, $21, $22, $23, $24, $25, $26)",
            new_home.id,
            new_home.user_id,
            new_home.max_dogs,
            new_home.current_dogs,
            new_home.has_yard,
            new_home.has_other_dogs,
            new_home.has_cats,
            new_home.has_children,
            toArrayLiteral(new_home.children_ages),
            new_home.ok_with_puppies,
            new_home.ok_with_seniors,
            new_home.ok_with_medical,
            new_home.ok_with_behavioral,
            toArrayLiteral(new_home.preferred_sizes),
            toArrayLiteral(new_home.preferred_breeds),
            new_home.zip_code,
            new_home.max_transport_miles,
            new_home.latitude.has_value() ? std::to_string(new_home.latitude.value()) : nullptr,
            new_home.longitude.has_value() ? std::to_string(new_home.longitude.value()) : nullptr,
            new_home.is_active,
            new_home.is_verified,
            nullptr,  // background_check_date initially null
            new_home.fosters_completed,
            new_home.fosters_converted_to_adoption,
            std::chrono::system_clock::to_time_t(new_home.created_at),
            std::chrono::system_clock::to_time_t(new_home.updated_at)
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return new_home;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save foster home: " + std::string(e.what()),
            {{"user_id", home.user_id}, {"operation", "save"}}
        );
        throw;
    }
}

models::FosterHome FosterService::update(const models::FosterHome& home) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        models::FosterHome updated_home = home;
        updated_home.updated_at = now();

        auto toArrayLiteral = [](const std::vector<std::string>& vec) -> std::string {
            if (vec.empty()) return "{}";
            std::string result = "{";
            for (size_t i = 0; i < vec.size(); i++) {
                if (i > 0) result += ",";
                result += "\"" + vec[i] + "\"";
            }
            result += "}";
            return result;
        };

        txn.exec_params(
            "UPDATE foster_homes SET "
            "max_dogs = $2, current_dogs = $3, has_yard = $4, has_other_dogs = $5, "
            "has_cats = $6, has_children = $7, children_ages = $8, ok_with_puppies = $9, "
            "ok_with_seniors = $10, ok_with_medical = $11, ok_with_behavioral = $12, "
            "preferred_sizes = $13, preferred_breeds = $14, zip_code = $15, "
            "max_transport_miles = $16, latitude = $17, longitude = $18, is_active = $19, "
            "is_verified = $20, fosters_completed = $21, fosters_converted_to_adoption = $22, "
            "updated_at = NOW() "
            "WHERE id = $1",
            updated_home.id,
            updated_home.max_dogs,
            updated_home.current_dogs,
            updated_home.has_yard,
            updated_home.has_other_dogs,
            updated_home.has_cats,
            updated_home.has_children,
            toArrayLiteral(updated_home.children_ages),
            updated_home.ok_with_puppies,
            updated_home.ok_with_seniors,
            updated_home.ok_with_medical,
            updated_home.ok_with_behavioral,
            toArrayLiteral(updated_home.preferred_sizes),
            toArrayLiteral(updated_home.preferred_breeds),
            updated_home.zip_code,
            updated_home.max_transport_miles,
            updated_home.latitude.has_value() ? std::to_string(updated_home.latitude.value()) : nullptr,
            updated_home.longitude.has_value() ? std::to_string(updated_home.longitude.value()) : nullptr,
            updated_home.is_active,
            updated_home.is_verified,
            updated_home.fosters_completed,
            updated_home.fosters_converted_to_adoption
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return updated_home;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to update foster home: " + std::string(e.what()),
            {{"foster_home_id", home.id}, {"operation", "update"}}
        );
        throw;
    }
}

bool FosterService::deleteById(const std::string& id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "DELETE FROM foster_homes WHERE id = $1",
            id
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to delete foster home: " + std::string(e.what()),
            {{"foster_home_id", id}, {"operation", "deleteById"}}
        );
        throw;
    }
}

// ============================================================================
// FOSTER HOME OPERATIONS
// ============================================================================

bool FosterService::registerAsFoster(const std::string& user_id, const models::FosterHome& home) {
    // Check if user already has a foster home
    auto existing = findByUserId(user_id);
    if (existing.has_value()) {
        return false;  // Already registered
    }

    models::FosterHome new_home = home;
    new_home.user_id = user_id;
    save(new_home);

    return true;
}

bool FosterService::deactivate(const std::string& home_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_homes SET is_active = FALSE, updated_at = NOW() WHERE id = $1",
            home_id
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to deactivate foster home: " + std::string(e.what()),
            {{"foster_home_id", home_id}}
        );
        return false;
    }
}

bool FosterService::reactivate(const std::string& home_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_homes SET is_active = TRUE, updated_at = NOW() WHERE id = $1",
            home_id
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to reactivate foster home: " + std::string(e.what()),
            {{"foster_home_id", home_id}}
        );
        return false;
    }
}

bool FosterService::updateCapacity(const std::string& home_id, int current_dogs) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_homes SET current_dogs = $2, updated_at = NOW() WHERE id = $1",
            home_id, current_dogs
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to update foster capacity: " + std::string(e.what()),
            {{"foster_home_id", home_id}, {"current_dogs", std::to_string(current_dogs)}}
        );
        return false;
    }
}

// ============================================================================
// PLACEMENT OPERATIONS
// ============================================================================

models::FosterPlacement FosterService::createPlacement(const std::string& foster_home_id,
                                                       const std::string& dog_id,
                                                       const std::string& shelter_id) {
    models::FosterPlacement placement;
    placement.id = generateUuid();
    placement.foster_home_id = foster_home_id;
    placement.dog_id = dog_id;
    placement.shelter_id = shelter_id;
    placement.start_date = now();
    placement.status = "active";
    placement.outcome = "ongoing";
    placement.created_at = now();
    placement.updated_at = now();

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "INSERT INTO foster_placements (id, foster_home_id, dog_id, shelter_id, "
            "start_date, status, outcome, supplies_provided, vet_visits, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, NOW(), $5, $6, $7, $8, NOW(), NOW())",
            placement.id,
            placement.foster_home_id,
            placement.dog_id,
            placement.shelter_id,
            placement.status,
            placement.outcome,
            placement.supplies_provided,
            placement.vet_visits
        );

        // Update foster home current dogs count
        txn.exec_params(
            "UPDATE foster_homes SET current_dogs = current_dogs + 1, updated_at = NOW() "
            "WHERE id = $1",
            foster_home_id
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        // Emit event
        emitEvent(EventType::FOSTER_PLACEMENT_STARTED, placement.id, "foster_placement", placement.toJson(), "FosterService");

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to create foster placement: " + std::string(e.what()),
            {{"foster_home_id", foster_home_id}, {"dog_id", dog_id}}
        );
        throw;
    }

    return placement;
}

models::FosterPlacement FosterService::endPlacement(const std::string& placement_id,
                                                    const std::string& outcome,
                                                    const std::string& notes) {
    auto placement_opt = findPlacementById(placement_id);
    if (!placement_opt.has_value()) {
        throw std::runtime_error("Placement not found: " + placement_id);
    }

    models::FosterPlacement placement = placement_opt.value();
    placement.status = "completed";
    placement.outcome = outcome;
    placement.outcome_notes = notes.empty() ? std::nullopt : std::optional<std::string>(notes);
    placement.actual_end_date = now();
    placement.updated_at = now();

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_placements SET status = $2, outcome = $3, outcome_notes = $4, "
            "actual_end_date = NOW(), updated_at = NOW() WHERE id = $1",
            placement_id,
            placement.status,
            placement.outcome,
            notes.empty() ? nullptr : notes.c_str()
        );

        // Update foster home current dogs count
        txn.exec_params(
            "UPDATE foster_homes SET current_dogs = GREATEST(0, current_dogs - 1), "
            "fosters_completed = fosters_completed + 1, "
            "fosters_converted_to_adoption = fosters_converted_to_adoption + $2, "
            "updated_at = NOW() WHERE id = $1",
            placement.foster_home_id,
            outcome == "adopted_by_foster" ? 1 : 0
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        // Emit event
        emitEvent(EventType::FOSTER_PLACEMENT_ENDED, placement_id, "foster_placement", placement.toJson(), "FosterService");

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to end foster placement: " + std::string(e.what()),
            {{"placement_id", placement_id}, {"outcome", outcome}}
        );
        throw;
    }

    return placement;
}

std::vector<models::FosterPlacement> FosterService::getActivePlacements() {
    std::vector<models::FosterPlacement> placements;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT * FROM foster_placements WHERE status = 'active' ORDER BY start_date DESC"
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            placements.push_back(models::FosterPlacement::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get active placements: " + std::string(e.what()),
            {{"operation", "getActivePlacements"}}
        );
        throw;
    }

    return placements;
}

std::vector<models::FosterPlacement> FosterService::getPlacementsByHome(const std::string& home_id) {
    std::vector<models::FosterPlacement> placements;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_placements WHERE foster_home_id = $1 ORDER BY start_date DESC",
            home_id
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            placements.push_back(models::FosterPlacement::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get placements by home: " + std::string(e.what()),
            {{"foster_home_id", home_id}}
        );
        throw;
    }

    return placements;
}

std::vector<models::FosterPlacement> FosterService::getPlacementsByDog(const std::string& dog_id) {
    std::vector<models::FosterPlacement> placements;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_placements WHERE dog_id = $1 ORDER BY start_date DESC",
            dog_id
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            placements.push_back(models::FosterPlacement::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get placements by dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        throw;
    }

    return placements;
}

std::optional<models::FosterPlacement> FosterService::findPlacementById(const std::string& placement_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_placements WHERE id = $1",
            placement_id
        );

        db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return models::FosterPlacement::fromDbRow(result[0]);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find placement: " + std::string(e.what()),
            {{"placement_id", placement_id}}
        );
        throw;
    }
}

// ============================================================================
// APPLICATION OPERATIONS
// ============================================================================

models::FosterApplication FosterService::submitApplication(const std::string& foster_home_id,
                                                           const std::string& dog_id,
                                                           const std::string& message) {
    models::FosterApplication app;
    app.id = generateUuid();
    app.foster_home_id = foster_home_id;
    app.dog_id = dog_id;
    app.status = "pending";
    app.message = message;
    app.created_at = now();
    app.updated_at = now();

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "INSERT INTO foster_applications (id, foster_home_id, dog_id, status, message, "
            "created_at, updated_at) VALUES ($1, $2, $3, $4, $5, NOW(), NOW())",
            app.id,
            app.foster_home_id,
            app.dog_id,
            app.status,
            app.message
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        // Emit event
        emitEvent(EventType::FOSTER_APPLICATION_SUBMITTED, app.id, "foster_application", app.toJson(), "FosterService");

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to submit foster application: " + std::string(e.what()),
            {{"foster_home_id", foster_home_id}, {"dog_id", dog_id}}
        );
        throw;
    }

    return app;
}

models::FosterApplication FosterService::approveApplication(const std::string& app_id,
                                                            const std::string& response) {
    auto app_opt = findApplicationById(app_id);
    if (!app_opt.has_value()) {
        throw std::runtime_error("Application not found: " + app_id);
    }

    models::FosterApplication app = app_opt.value();
    app.status = "approved";
    app.response = response;
    app.updated_at = now();

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_applications SET status = $2, response = $3, updated_at = NOW() "
            "WHERE id = $1",
            app_id,
            app.status,
            response
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        // Emit event
        emitEvent(EventType::FOSTER_APPROVED, app_id, "foster_application", app.toJson(), "FosterService");

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to approve foster application: " + std::string(e.what()),
            {{"app_id", app_id}}
        );
        throw;
    }

    return app;
}

models::FosterApplication FosterService::rejectApplication(const std::string& app_id,
                                                           const std::string& response) {
    auto app_opt = findApplicationById(app_id);
    if (!app_opt.has_value()) {
        throw std::runtime_error("Application not found: " + app_id);
    }

    models::FosterApplication app = app_opt.value();
    app.status = "rejected";
    app.response = response;
    app.updated_at = now();

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE foster_applications SET status = $2, response = $3, updated_at = NOW() "
            "WHERE id = $1",
            app_id,
            app.status,
            response
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to reject foster application: " + std::string(e.what()),
            {{"app_id", app_id}}
        );
        throw;
    }

    return app;
}

bool FosterService::withdrawApplication(const std::string& app_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE foster_applications SET status = 'withdrawn', updated_at = NOW() "
            "WHERE id = $1 AND status = 'pending'",
            app_id
        );

        txn.commit();
        db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to withdraw foster application: " + std::string(e.what()),
            {{"app_id", app_id}}
        );
        return false;
    }
}

std::vector<models::FosterApplication> FosterService::getPendingApplications() {
    std::vector<models::FosterApplication> applications;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT * FROM foster_applications WHERE status = 'pending' ORDER BY created_at ASC"
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            applications.push_back(models::FosterApplication::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get pending applications: " + std::string(e.what()),
            {{"operation", "getPendingApplications"}}
        );
        throw;
    }

    return applications;
}

std::vector<models::FosterApplication> FosterService::getApplicationsByHome(const std::string& home_id) {
    std::vector<models::FosterApplication> applications;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_applications WHERE foster_home_id = $1 ORDER BY created_at DESC",
            home_id
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            applications.push_back(models::FosterApplication::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get applications by home: " + std::string(e.what()),
            {{"foster_home_id", home_id}}
        );
        throw;
    }

    return applications;
}

std::vector<models::FosterApplication> FosterService::getApplicationsByDog(const std::string& dog_id) {
    std::vector<models::FosterApplication> applications;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_applications WHERE dog_id = $1 ORDER BY created_at DESC",
            dog_id
        );

        db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            applications.push_back(models::FosterApplication::fromDbRow(row));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get applications by dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        throw;
    }

    return applications;
}

std::optional<models::FosterApplication> FosterService::findApplicationById(const std::string& app_id) {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM foster_applications WHERE id = $1",
            app_id
        );

        db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return models::FosterApplication::fromDbRow(result[0]);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to find application: " + std::string(e.what()),
            {{"app_id", app_id}}
        );
        throw;
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

int FosterService::countActiveFosters() {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT COUNT(*) FROM foster_homes WHERE is_active = TRUE"
        );

        db::ConnectionPool::getInstance().release(conn);

        return result[0][0].as<int>();
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to count active fosters: " + std::string(e.what()),
            {{"operation", "countActiveFosters"}}
        );
        return 0;
    }
}

int FosterService::countActivePlacements() {
    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT COUNT(*) FROM foster_placements WHERE status = 'active'"
        );

        db::ConnectionPool::getInstance().release(conn);

        return result[0][0].as<int>();
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to count active placements: " + std::string(e.what()),
            {{"operation", "countActivePlacements"}}
        );
        return 0;
    }
}

Json::Value FosterService::getStatistics() {
    Json::Value stats;

    try {
        auto conn = db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Count active foster homes
        auto foster_count = txn.exec(
            "SELECT COUNT(*) FROM foster_homes WHERE is_active = TRUE"
        );
        stats["active_foster_homes"] = foster_count[0][0].as<int>();

        // Count total foster homes
        auto total_foster = txn.exec(
            "SELECT COUNT(*) FROM foster_homes"
        );
        stats["total_foster_homes"] = total_foster[0][0].as<int>();

        // Count active placements
        auto active_placements = txn.exec(
            "SELECT COUNT(*) FROM foster_placements WHERE status = 'active'"
        );
        stats["active_placements"] = active_placements[0][0].as<int>();

        // Count total placements
        auto total_placements = txn.exec(
            "SELECT COUNT(*) FROM foster_placements"
        );
        stats["total_placements"] = total_placements[0][0].as<int>();

        // Count foster fails (adopted by foster)
        auto foster_fails = txn.exec(
            "SELECT COUNT(*) FROM foster_placements WHERE outcome = 'adopted_by_foster'"
        );
        stats["adopted_by_foster"] = foster_fails[0][0].as<int>();

        // Count pending applications
        auto pending_apps = txn.exec(
            "SELECT COUNT(*) FROM foster_applications WHERE status = 'pending'"
        );
        stats["pending_applications"] = pending_apps[0][0].as<int>();

        // Average placement duration
        auto avg_duration = txn.exec(
            "SELECT AVG(EXTRACT(DAY FROM actual_end_date - start_date)) "
            "FROM foster_placements WHERE actual_end_date IS NOT NULL"
        );
        if (!avg_duration[0][0].is_null()) {
            stats["avg_placement_days"] = avg_duration[0][0].as<double>();
        } else {
            stats["avg_placement_days"] = 0.0;
        }

        // Available capacity
        auto capacity = txn.exec(
            "SELECT SUM(max_dogs - current_dogs) FROM foster_homes WHERE is_active = TRUE"
        );
        if (!capacity[0][0].is_null()) {
            stats["available_capacity"] = capacity[0][0].as<int>();
        } else {
            stats["available_capacity"] = 0;
        }

        db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get foster statistics: " + std::string(e.what()),
            {{"operation", "getStatistics"}}
        );
        stats["error"] = e.what();
    }

    return stats;
}

} // namespace wtl::core::services

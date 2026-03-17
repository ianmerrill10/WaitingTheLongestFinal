/**
 * @file SponsorshipService.cc
 * @brief Implementation of the Sponsor-a-Fee service
 * @see SponsorshipService.h
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/SponsorshipService.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/ApiResponse.h"

#include <pqxx/pqxx>
#include <iostream>

using namespace ::wtl::core::utils;
using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

namespace wtl::core::services {

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

Json::Value Sponsor::toJson() const {
    Json::Value json;
    json["id"] = id;
    if (!user_id.empty()) json["user_id"] = user_id;
    json["name"] = name;
    json["email"] = email;
    json["is_anonymous"] = is_anonymous;
    json["display_name"] = display_name;
    json["total_sponsored"] = total_sponsored;
    json["dogs_sponsored"] = dogs_sponsored;
    json["created_at"] = created_at;
    return json;
}

Json::Value Sponsorship::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["sponsor_id"] = sponsor_id;
    json["dog_id"] = dog_id;
    json["amount"] = amount;
    json["covers_full_fee"] = covers_full_fee;
    json["payment_status"] = payment_status;
    json["payment_id"] = payment_id;
    json["status"] = status;
    if (!used_at.empty()) json["used_at"] = used_at;
    if (!adopter_id.empty()) json["adopter_id"] = adopter_id;
    json["message"] = message;
    json["created_at"] = created_at;
    return json;
}

Json::Value CorporateSponsor::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["company_name"] = company_name;
    json["logo_url"] = logo_url;
    json["website"] = website;
    json["contact_email"] = contact_email;
    json["monthly_budget"] = monthly_budget;
    json["dogs_per_month"] = dogs_per_month;
    Json::Value statesArr(Json::arrayValue);
    for (const auto& s : states) statesArr.append(s);
    json["states"] = statesArr;
    json["total_sponsored"] = total_sponsored;
    json["total_dogs"] = total_dogs;
    json["is_active"] = is_active;
    json["created_at"] = created_at;
    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

SponsorshipService& SponsorshipService::getInstance() {
    static SponsorshipService instance;
    return instance;
}

// ============================================================================
// SPONSOR MANAGEMENT
// ============================================================================

std::optional<Sponsor> SponsorshipService::createSponsor(
    const std::string& name,
    const std::string& email,
    bool isAnonymous,
    const std::string& userId)
{
    return SelfHealing::getInstance().executeOrThrow<std::optional<Sponsor>>(
        "create_sponsor",
        [&]() -> std::optional<Sponsor> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);

                std::string displayName = isAnonymous ? "Anonymous" : name;
                if (!isAnonymous && name.length() > 1) {
                    displayName = name.substr(0, name.find(' ') + 2) + ".";
                }

                std::string sql;
                pqxx::result result;

                if (userId.empty()) {
                    sql = "INSERT INTO sponsors (name, email, is_anonymous, display_name) "
                          "VALUES ($1, $2, $3, $4) RETURNING *";
                    result = txn.exec_params(sql, name, email, isAnonymous, displayName);
                } else {
                    sql = "INSERT INTO sponsors (user_id, name, email, is_anonymous, display_name) "
                          "VALUES ($1, $2, $3, $4, $5) RETURNING *";
                    result = txn.exec_params(sql, userId, name, email, isAnonymous, displayName);
                }

                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                Sponsor sponsor;
                sponsor.id = result[0]["id"].as<std::string>();
                sponsor.user_id = result[0]["user_id"].is_null() ? "" : result[0]["user_id"].as<std::string>();
                sponsor.name = result[0]["name"].as<std::string>();
                sponsor.email = result[0]["email"].as<std::string>();
                sponsor.is_anonymous = result[0]["is_anonymous"].as<bool>();
                sponsor.display_name = result[0]["display_name"].as<std::string>();
                sponsor.total_sponsored = result[0]["total_sponsored"].as<double>();
                sponsor.dogs_sponsored = result[0]["dogs_sponsored"].as<int>();
                sponsor.created_at = result[0]["created_at"].as<std::string>();
                return sponsor;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to create sponsor: " + std::string(e.what()),
                    {{"name", name}, {"email", email}}
                );
                throw;
            }
        }
    );
}

std::optional<Sponsor> SponsorshipService::findSponsorById(const std::string& id) {
    return SelfHealing::getInstance().executeOrThrow<std::optional<Sponsor>>(
        "find_sponsor_by_id",
        [&]() -> std::optional<Sponsor> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT * FROM sponsors WHERE id = $1", id
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                Sponsor sponsor;
                sponsor.id = result[0]["id"].as<std::string>();
                sponsor.user_id = result[0]["user_id"].is_null() ? "" : result[0]["user_id"].as<std::string>();
                sponsor.name = result[0]["name"].as<std::string>();
                sponsor.email = result[0]["email"].as<std::string>();
                sponsor.is_anonymous = result[0]["is_anonymous"].as<bool>();
                sponsor.display_name = result[0]["display_name"].as<std::string>();
                sponsor.total_sponsored = result[0]["total_sponsored"].as<double>();
                sponsor.dogs_sponsored = result[0]["dogs_sponsored"].as<int>();
                sponsor.created_at = result[0]["created_at"].as<std::string>();
                return sponsor;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find sponsor: " + std::string(e.what()),
                    {{"sponsor_id", id}}
                );
                throw;
            }
        }
    );
}

std::optional<Sponsor> SponsorshipService::findSponsorByUserId(const std::string& userId) {
    return SelfHealing::getInstance().executeOrThrow<std::optional<Sponsor>>(
        "find_sponsor_by_user",
        [&]() -> std::optional<Sponsor> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT * FROM sponsors WHERE user_id = $1", userId
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                Sponsor sponsor;
                sponsor.id = result[0]["id"].as<std::string>();
                sponsor.user_id = result[0]["user_id"].as<std::string>();
                sponsor.name = result[0]["name"].as<std::string>();
                sponsor.email = result[0]["email"].as<std::string>();
                sponsor.is_anonymous = result[0]["is_anonymous"].as<bool>();
                sponsor.display_name = result[0]["display_name"].as<std::string>();
                sponsor.total_sponsored = result[0]["total_sponsored"].as<double>();
                sponsor.dogs_sponsored = result[0]["dogs_sponsored"].as<int>();
                sponsor.created_at = result[0]["created_at"].as<std::string>();
                return sponsor;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find sponsor by user: " + std::string(e.what()),
                    {{"user_id", userId}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// SPONSORSHIP TRANSACTIONS
// ============================================================================

std::optional<Sponsorship> SponsorshipService::sponsorDog(
    const std::string& sponsorId,
    const std::string& dogId,
    double amount,
    const std::string& message,
    const std::string& paymentId)
{
    return SelfHealing::getInstance().executeOrThrow<std::optional<Sponsorship>>(
        "sponsor_dog",
        [&]() -> std::optional<Sponsorship> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);

                // Get dog's adoption fee to determine if this covers full fee
                auto dogResult = txn.exec_params(
                    "SELECT adoption_fee FROM dogs WHERE id = $1", dogId
                );

                bool coversFullFee = false;
                if (!dogResult.empty() && !dogResult[0]["adoption_fee"].is_null()) {
                    double fee = dogResult[0]["adoption_fee"].as<double>();
                    coversFullFee = (amount >= fee);
                }

                // Create sponsorship
                auto result = txn.exec_params(
                    "INSERT INTO sponsorships (sponsor_id, dog_id, amount, covers_full_fee, "
                    "payment_status, payment_id, message) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING *",
                    sponsorId, dogId, amount, coversFullFee,
                    paymentId.empty() ? "pending" : "completed",
                    paymentId.empty() ? nullptr : paymentId.c_str(),
                    message.empty() ? nullptr : message.c_str()
                );

                // Update sponsor totals
                txn.exec_params(
                    "UPDATE sponsors SET total_sponsored = total_sponsored + $1, "
                    "dogs_sponsored = dogs_sponsored + 1 WHERE id = $2",
                    amount, sponsorId
                );

                // Mark dog as fee-sponsored if full fee covered
                if (coversFullFee) {
                    txn.exec_params(
                        "UPDATE dogs SET fee_sponsored = TRUE, sponsor_id = $1 WHERE id = $2",
                        sponsorId, dogId
                    );
                }

                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                Sponsorship s;
                s.id = result[0]["id"].as<std::string>();
                s.sponsor_id = result[0]["sponsor_id"].as<std::string>();
                s.dog_id = result[0]["dog_id"].as<std::string>();
                s.amount = result[0]["amount"].as<double>();
                s.covers_full_fee = result[0]["covers_full_fee"].as<bool>();
                s.payment_status = result[0]["payment_status"].as<std::string>();
                s.payment_id = result[0]["payment_id"].is_null() ? "" : result[0]["payment_id"].as<std::string>();
                s.status = result[0]["status"].as<std::string>();
                s.message = result[0]["message"].is_null() ? "" : result[0]["message"].as<std::string>();
                s.created_at = result[0]["created_at"].as<std::string>();
                return s;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to create sponsorship: " + std::string(e.what()),
                    {{"sponsor_id", sponsorId}, {"dog_id", dogId}}
                );
                throw;
            }
        }
    );
}

std::optional<Sponsorship> SponsorshipService::findSponsorshipById(const std::string& id) {
    return SelfHealing::getInstance().executeOrThrow<std::optional<Sponsorship>>(
        "find_sponsorship",
        [&]() -> std::optional<Sponsorship> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT * FROM sponsorships WHERE id = $1", id
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                Sponsorship s;
                s.id = result[0]["id"].as<std::string>();
                s.sponsor_id = result[0]["sponsor_id"].as<std::string>();
                s.dog_id = result[0]["dog_id"].as<std::string>();
                s.amount = result[0]["amount"].as<double>();
                s.covers_full_fee = result[0]["covers_full_fee"].as<bool>();
                s.payment_status = result[0]["payment_status"].as<std::string>();
                s.payment_id = result[0]["payment_id"].is_null() ? "" : result[0]["payment_id"].as<std::string>();
                s.status = result[0]["status"].as<std::string>();
                s.used_at = result[0]["used_at"].is_null() ? "" : result[0]["used_at"].as<std::string>();
                s.adopter_id = result[0]["adopter_id"].is_null() ? "" : result[0]["adopter_id"].as<std::string>();
                s.message = result[0]["message"].is_null() ? "" : result[0]["message"].as<std::string>();
                s.created_at = result[0]["created_at"].as<std::string>();
                return s;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find sponsorship: " + std::string(e.what()),
                    {{"sponsorship_id", id}}
                );
                throw;
            }
        }
    );
}

std::vector<Sponsorship> SponsorshipService::findSponsorshipsByDog(const std::string& dogId) {
    return SelfHealing::getInstance().executeOrThrow<std::vector<Sponsorship>>(
        "find_sponsorships_by_dog",
        [&]() -> std::vector<Sponsorship> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT * FROM sponsorships WHERE dog_id = $1 AND status = 'active' "
                    "ORDER BY created_at DESC", dogId
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                std::vector<Sponsorship> sponsorships;
                sponsorships.reserve(result.size());
                for (const auto& row : result) {
                    Sponsorship s;
                    s.id = row["id"].as<std::string>();
                    s.sponsor_id = row["sponsor_id"].as<std::string>();
                    s.dog_id = row["dog_id"].as<std::string>();
                    s.amount = row["amount"].as<double>();
                    s.covers_full_fee = row["covers_full_fee"].as<bool>();
                    s.payment_status = row["payment_status"].as<std::string>();
                    s.payment_id = row["payment_id"].is_null() ? "" : row["payment_id"].as<std::string>();
                    s.status = row["status"].as<std::string>();
                    s.message = row["message"].is_null() ? "" : row["message"].as<std::string>();
                    s.created_at = row["created_at"].as<std::string>();
                    sponsorships.push_back(s);
                }
                return sponsorships;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find sponsorships by dog: " + std::string(e.what()),
                    {{"dog_id", dogId}}
                );
                throw;
            }
        }
    );
}

std::vector<Sponsorship> SponsorshipService::findSponsorshipsBySponsor(
    const std::string& sponsorId, int limit, int offset)
{
    return SelfHealing::getInstance().executeOrThrow<std::vector<Sponsorship>>(
        "find_sponsorships_by_sponsor",
        [&]() -> std::vector<Sponsorship> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT s.*, d.name as dog_name FROM sponsorships s "
                    "LEFT JOIN dogs d ON s.dog_id = d.id "
                    "WHERE s.sponsor_id = $1 ORDER BY s.created_at DESC "
                    "LIMIT $2 OFFSET $3",
                    sponsorId, limit, offset
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                std::vector<Sponsorship> sponsorships;
                sponsorships.reserve(result.size());
                for (const auto& row : result) {
                    Sponsorship s;
                    s.id = row["id"].as<std::string>();
                    s.sponsor_id = row["sponsor_id"].as<std::string>();
                    s.dog_id = row["dog_id"].as<std::string>();
                    s.amount = row["amount"].as<double>();
                    s.covers_full_fee = row["covers_full_fee"].as<bool>();
                    s.payment_status = row["payment_status"].as<std::string>();
                    s.status = row["status"].as<std::string>();
                    s.message = row["message"].is_null() ? "" : row["message"].as<std::string>();
                    s.created_at = row["created_at"].as<std::string>();
                    sponsorships.push_back(s);
                }
                return sponsorships;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find sponsorships by sponsor: " + std::string(e.what()),
                    {{"sponsor_id", sponsorId}}
                );
                throw;
            }
        }
    );
}

bool SponsorshipService::markSponsorshipUsed(
    const std::string& sponsorshipId, const std::string& adopterId)
{
    return SelfHealing::getInstance().executeOrThrow<bool>(
        "mark_sponsorship_used",
        [&]() -> bool {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "UPDATE sponsorships SET status = 'used', used_at = NOW(), "
                    "adopter_id = $1 WHERE id = $2 AND status = 'active' "
                    "RETURNING id",
                    adopterId, sponsorshipId
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);
                return !result.empty();

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to mark sponsorship used: " + std::string(e.what()),
                    {{"sponsorship_id", sponsorshipId}}
                );
                throw;
            }
        }
    );
}

bool SponsorshipService::updatePaymentStatus(
    const std::string& sponsorshipId, const std::string& status, const std::string& paymentId)
{
    return SelfHealing::getInstance().executeOrThrow<bool>(
        "update_payment_status",
        [&]() -> bool {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                pqxx::result result;

                if (paymentId.empty()) {
                    result = txn.exec_params(
                        "UPDATE sponsorships SET payment_status = $1 WHERE id = $2 RETURNING id",
                        status, sponsorshipId
                    );
                } else {
                    result = txn.exec_params(
                        "UPDATE sponsorships SET payment_status = $1, payment_id = $2 "
                        "WHERE id = $3 RETURNING id",
                        status, paymentId, sponsorshipId
                    );
                }

                txn.commit();
                ConnectionPool::getInstance().release(conn);
                return !result.empty();

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to update payment status: " + std::string(e.what()),
                    {{"sponsorship_id", sponsorshipId}, {"status", status}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// CORPORATE SPONSORS
// ============================================================================

std::vector<CorporateSponsor> SponsorshipService::getActiveCorporateSponsors() {
    return SelfHealing::getInstance().executeOrThrow<std::vector<CorporateSponsor>>(
        "get_active_corporate_sponsors",
        [&]() -> std::vector<CorporateSponsor> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec(
                    "SELECT * FROM corporate_sponsors WHERE is_active = TRUE "
                    "ORDER BY total_dogs DESC"
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                std::vector<CorporateSponsor> sponsors;
                sponsors.reserve(result.size());
                for (const auto& row : result) {
                    CorporateSponsor cs;
                    cs.id = row["id"].as<std::string>();
                    cs.company_name = row["company_name"].as<std::string>();
                    cs.logo_url = row["logo_url"].is_null() ? "" : row["logo_url"].as<std::string>();
                    cs.website = row["website"].is_null() ? "" : row["website"].as<std::string>();
                    cs.contact_email = row["contact_email"].is_null() ? "" : row["contact_email"].as<std::string>();
                    cs.monthly_budget = row["monthly_budget"].is_null() ? 0 : row["monthly_budget"].as<double>();
                    cs.dogs_per_month = row["dogs_per_month"].is_null() ? 0 : row["dogs_per_month"].as<int>();
                    cs.total_sponsored = row["total_sponsored"].as<double>();
                    cs.total_dogs = row["total_dogs"].as<int>();
                    cs.is_active = row["is_active"].as<bool>();
                    cs.created_at = row["created_at"].as<std::string>();
                    sponsors.push_back(cs);
                }
                return sponsors;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to get corporate sponsors: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

std::optional<CorporateSponsor> SponsorshipService::findCorporateSponsorById(const std::string& id) {
    return SelfHealing::getInstance().executeOrThrow<std::optional<CorporateSponsor>>(
        "find_corporate_sponsor",
        [&]() -> std::optional<CorporateSponsor> {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT * FROM corporate_sponsors WHERE id = $1", id
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                if (result.empty()) return std::nullopt;

                CorporateSponsor cs;
                cs.id = result[0]["id"].as<std::string>();
                cs.company_name = result[0]["company_name"].as<std::string>();
                cs.logo_url = result[0]["logo_url"].is_null() ? "" : result[0]["logo_url"].as<std::string>();
                cs.website = result[0]["website"].is_null() ? "" : result[0]["website"].as<std::string>();
                cs.is_active = result[0]["is_active"].as<bool>();
                cs.created_at = result[0]["created_at"].as<std::string>();
                return cs;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to find corporate sponsor: " + std::string(e.what()),
                    {{"id", id}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value SponsorshipService::getSponsorStats(const std::string& sponsorId) {
    return SelfHealing::getInstance().executeOrThrow<Json::Value>(
        "get_sponsor_stats",
        [&]() -> Json::Value {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec_params(
                    "SELECT s.total_sponsored, s.dogs_sponsored, "
                    "COUNT(sp.id) FILTER (WHERE sp.status = 'used') as dogs_adopted, "
                    "COUNT(sp.id) FILTER (WHERE sp.status = 'active') as active_sponsorships "
                    "FROM sponsors s "
                    "LEFT JOIN sponsorships sp ON s.id = sp.sponsor_id "
                    "WHERE s.id = $1 GROUP BY s.id",
                    sponsorId
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                Json::Value stats;
                if (!result.empty()) {
                    stats["total_sponsored"] = result[0]["total_sponsored"].as<double>();
                    stats["dogs_sponsored"] = result[0]["dogs_sponsored"].as<int>();
                    stats["dogs_adopted"] = result[0]["dogs_adopted"].as<int>();
                    stats["active_sponsorships"] = result[0]["active_sponsorships"].as<int>();
                }
                return stats;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to get sponsor stats: " + std::string(e.what()),
                    {{"sponsor_id", sponsorId}}
                );
                throw;
            }
        }
    );
}

Json::Value SponsorshipService::getGlobalSponsorshipStats() {
    return SelfHealing::getInstance().executeOrThrow<Json::Value>(
        "get_global_sponsorship_stats",
        [&]() -> Json::Value {
            auto conn = ConnectionPool::getInstance().acquire();
            try {
                pqxx::work txn(*conn);
                auto result = txn.exec(
                    "SELECT "
                    "COUNT(DISTINCT sponsor_id) as total_sponsors, "
                    "COUNT(*) as total_sponsorships, "
                    "SUM(amount) as total_amount, "
                    "COUNT(*) FILTER (WHERE status = 'used') as sponsorships_used, "
                    "COUNT(*) FILTER (WHERE status = 'active') as sponsorships_active "
                    "FROM sponsorships WHERE payment_status = 'completed'"
                );
                txn.commit();
                ConnectionPool::getInstance().release(conn);

                Json::Value stats;
                if (!result.empty()) {
                    stats["total_sponsors"] = result[0]["total_sponsors"].as<int>();
                    stats["total_sponsorships"] = result[0]["total_sponsorships"].as<int>();
                    stats["total_amount"] = result[0]["total_amount"].is_null() ? 0.0 : result[0]["total_amount"].as<double>();
                    stats["sponsorships_used"] = result[0]["sponsorships_used"].as<int>();
                    stats["sponsorships_active"] = result[0]["sponsorships_active"].as<int>();
                }
                return stats;

            } catch (const std::exception& e) {
                ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ErrorCategory::DATABASE,
                    "Failed to get global sponsorship stats: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

} // namespace wtl::core::services

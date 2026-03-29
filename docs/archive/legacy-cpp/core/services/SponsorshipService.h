/**
 * @file SponsorshipService.h
 * @brief Service for managing dog adoption fee sponsorships
 *
 * PURPOSE:
 * Handles sponsor creation, sponsorship transactions, corporate sponsor
 * management, and sponsor notification workflows for the Sponsor-a-Fee program.
 *
 * USAGE:
 * auto& service = SponsorshipService::getInstance();
 * auto sponsor = service.createSponsor(name, email, isAnonymous);
 * auto sponsorship = service.sponsorDog(sponsorId, dogId, amount, message);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - SelfHealing (resilient execution)
 * - ErrorCapture (error tracking)
 * - DogService (dog lookups)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <json/json.h>

namespace wtl::core::services {

/**
 * Sponsor data structure
 */
struct Sponsor {
    std::string id;
    std::string user_id;
    std::string name;
    std::string email;
    bool is_anonymous;
    std::string display_name;
    double total_sponsored;
    int dogs_sponsored;
    std::string created_at;

    Json::Value toJson() const;
    template<typename Row>
    static Sponsor fromDbRow(const Row& row);
};

/**
 * Sponsorship transaction data
 */
struct Sponsorship {
    std::string id;
    std::string sponsor_id;
    std::string dog_id;
    double amount;
    bool covers_full_fee;
    std::string payment_status;
    std::string payment_id;
    std::string status;
    std::string used_at;
    std::string adopter_id;
    std::string message;
    std::string created_at;

    Json::Value toJson() const;
    template<typename Row>
    static Sponsorship fromDbRow(const Row& row);
};

/**
 * Corporate sponsor data
 */
struct CorporateSponsor {
    std::string id;
    std::string company_name;
    std::string logo_url;
    std::string website;
    std::string contact_email;
    double monthly_budget;
    int dogs_per_month;
    std::vector<std::string> states;
    double total_sponsored;
    int total_dogs;
    bool is_active;
    std::string created_at;

    Json::Value toJson() const;
    template<typename Row>
    static CorporateSponsor fromDbRow(const Row& row);
};

/**
 * SponsorshipService - Manages the Sponsor-a-Fee program
 */
class SponsorshipService {
public:
    // ====================================================================
    // SINGLETON ACCESS
    // ====================================================================

    static SponsorshipService& getInstance();

    SponsorshipService(const SponsorshipService&) = delete;
    SponsorshipService& operator=(const SponsorshipService&) = delete;
    SponsorshipService(SponsorshipService&&) = delete;
    SponsorshipService& operator=(SponsorshipService&&) = delete;

    // ====================================================================
    // SPONSOR MANAGEMENT
    // ====================================================================

    std::optional<Sponsor> createSponsor(const std::string& name,
                                          const std::string& email,
                                          bool isAnonymous,
                                          const std::string& userId = "");

    std::optional<Sponsor> findSponsorById(const std::string& id);
    std::optional<Sponsor> findSponsorByUserId(const std::string& userId);

    // ====================================================================
    // SPONSORSHIP TRANSACTIONS
    // ====================================================================

    std::optional<Sponsorship> sponsorDog(const std::string& sponsorId,
                                           const std::string& dogId,
                                           double amount,
                                           const std::string& message = "",
                                           const std::string& paymentId = "");

    std::optional<Sponsorship> findSponsorshipById(const std::string& id);
    std::vector<Sponsorship> findSponsorshipsByDog(const std::string& dogId);
    std::vector<Sponsorship> findSponsorshipsBySponsor(const std::string& sponsorId,
                                                        int limit = 20, int offset = 0);

    bool markSponsorshipUsed(const std::string& sponsorshipId,
                              const std::string& adopterId);

    bool updatePaymentStatus(const std::string& sponsorshipId,
                              const std::string& status,
                              const std::string& paymentId = "");

    // ====================================================================
    // CORPORATE SPONSORS
    // ====================================================================

    std::vector<CorporateSponsor> getActiveCorporateSponsors();
    std::optional<CorporateSponsor> findCorporateSponsorById(const std::string& id);

    // ====================================================================
    // STATISTICS
    // ====================================================================

    Json::Value getSponsorStats(const std::string& sponsorId);
    Json::Value getGlobalSponsorshipStats();

private:
    SponsorshipService() = default;
    std::mutex mutex_;
};

} // namespace wtl::core::services

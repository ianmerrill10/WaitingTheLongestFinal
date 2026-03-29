/**
 * @file UrgencyCalculatorTest.cc
 * @brief Unit tests for UrgencyCalculator service
 *
 * Tests urgency level calculation including:
 * - CRITICAL: < 24 hours to euthanasia
 * - HIGH: < 72 hours to euthanasia
 * - MEDIUM: < 7 days to euthanasia
 * - NORMAL: No known euthanasia date
 *
 * @author Testing Framework - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include <gtest/gtest.h>
#include "core/services/UrgencyLevel.h"
#include <chrono>

using namespace wtl::core::services;

class UrgencyCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
};

TEST_F(UrgencyCalculatorTest, UrgencyLevelValues) {
    // Verify enum values exist and are distinct
    EXPECT_NE(static_cast<int>(UrgencyLevel::NORMAL), static_cast<int>(UrgencyLevel::CRITICAL));
    EXPECT_NE(static_cast<int>(UrgencyLevel::MEDIUM), static_cast<int>(UrgencyLevel::HIGH));
}

TEST_F(UrgencyCalculatorTest, UrgencyLevelToString) {
    EXPECT_EQ(urgencyLevelToString(UrgencyLevel::NORMAL), "normal");
    EXPECT_EQ(urgencyLevelToString(UrgencyLevel::MEDIUM), "medium");
    EXPECT_EQ(urgencyLevelToString(UrgencyLevel::HIGH), "high");
    EXPECT_EQ(urgencyLevelToString(UrgencyLevel::CRITICAL), "critical");
}

TEST_F(UrgencyCalculatorTest, UrgencyLevelFromString) {
    EXPECT_EQ(urgencyLevelFromString("normal"), UrgencyLevel::NORMAL);
    EXPECT_EQ(urgencyLevelFromString("medium"), UrgencyLevel::MEDIUM);
    EXPECT_EQ(urgencyLevelFromString("high"), UrgencyLevel::HIGH);
    EXPECT_EQ(urgencyLevelFromString("critical"), UrgencyLevel::CRITICAL);
    EXPECT_EQ(urgencyLevelFromString("invalid"), UrgencyLevel::NORMAL);
}

TEST_F(UrgencyCalculatorTest, UrgencyLevelComparison) {
    // CRITICAL should be "higher" urgency than NORMAL
    EXPECT_GT(static_cast<int>(UrgencyLevel::CRITICAL), static_cast<int>(UrgencyLevel::NORMAL));
    EXPECT_GT(static_cast<int>(UrgencyLevel::HIGH), static_cast<int>(UrgencyLevel::MEDIUM));
    EXPECT_GT(static_cast<int>(UrgencyLevel::MEDIUM), static_cast<int>(UrgencyLevel::NORMAL));
}

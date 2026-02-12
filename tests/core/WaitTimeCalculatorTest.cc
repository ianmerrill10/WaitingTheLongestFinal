/**
 * @file WaitTimeCalculatorTest.cc
 * @brief Unit tests for WaitTimeCalculator
 *
 * Tests the wait time calculation logic including:
 * - Basic time component calculation
 * - Edge cases (zero time, large values)
 * - Formatting functions
 *
 * @author Testing Framework - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include <gtest/gtest.h>
#include "core/utils/WaitTimeCalculator.h"
#include <chrono>

using namespace wtl::core::utils;

class WaitTimeCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(WaitTimeCalculatorTest, CalculateZeroWaitTime) {
    auto now = std::chrono::system_clock::now();
    auto components = WaitTimeCalculator::calculate(now, now);

    EXPECT_EQ(components.years, 0);
    EXPECT_EQ(components.months, 0);
    EXPECT_EQ(components.days, 0);
    EXPECT_EQ(components.hours, 0);
    EXPECT_EQ(components.minutes, 0);
    EXPECT_EQ(components.seconds, 0);
}

TEST_F(WaitTimeCalculatorTest, CalculateOneDayWaitTime) {
    auto now = std::chrono::system_clock::now();
    auto oneDay = now - std::chrono::hours(24);
    auto components = WaitTimeCalculator::calculate(oneDay, now);

    EXPECT_EQ(components.days, 1);
    EXPECT_EQ(components.hours, 0);
    EXPECT_EQ(components.minutes, 0);
}

TEST_F(WaitTimeCalculatorTest, FormatWaitTime) {
    WaitTimeComponents components;
    components.years = 1;
    components.months = 2;
    components.days = 15;
    components.hours = 8;
    components.minutes = 30;
    components.seconds = 45;

    std::string formatted = WaitTimeCalculator::format(components);

    // Expected format: YY:MM:DD:HH:MM:SS
    EXPECT_EQ(formatted, "01:02:15:08:30:45");
}

TEST_F(WaitTimeCalculatorTest, FormatZeroValues) {
    WaitTimeComponents components;
    components.years = 0;
    components.months = 0;
    components.days = 0;
    components.hours = 0;
    components.minutes = 0;
    components.seconds = 0;

    std::string formatted = WaitTimeCalculator::format(components);

    EXPECT_EQ(formatted, "00:00:00:00:00:00");
}

TEST_F(WaitTimeCalculatorTest, CalculateTotalDays) {
    WaitTimeComponents components;
    components.years = 1;
    components.months = 0;
    components.days = 0;
    components.hours = 0;
    components.minutes = 0;
    components.seconds = 0;

    int totalDays = WaitTimeCalculator::totalDays(components);

    // Approximately 365 days
    EXPECT_GE(totalDays, 365);
    EXPECT_LE(totalDays, 366);
}

/**
 * @file test_main.cc
 * @brief Main entry point for WaitingTheLongest test suite
 *
 * PURPOSE:
 * Entry point for Google Test framework. Initializes the test
 * environment and runs all registered test cases.
 *
 * USAGE:
 * Build with -DBUILD_TESTS=ON and run the test executable:
 *   ./WaitingTheLongest_tests
 *
 * @author Testing Framework - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

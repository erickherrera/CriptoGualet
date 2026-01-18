#include "TestUtils.h"

// This file can be used for shared test utilities.

// Global test counters
namespace TestGlobals {
    int g_testsRun = 0;
    int g_testsPassed = 0;
    int g_testsFailed = 0;
}

// Mock time management
std::chrono::steady_clock::time_point MockTime::mockTime_;
bool MockTime::useMockTime_ = false;

void MockTime::enable() {
    useMockTime_ = true;
    mockTime_ = std::chrono::steady_clock::now();
}

void MockTime::disable() {
    useMockTime_ = false;
}

std::chrono::steady_clock::time_point MockTime::now() {
    return useMockTime_ ? mockTime_ : std::chrono::steady_clock::now();
}

void MockTime::advance(std::chrono::minutes minutes) {
    mockTime_ += minutes;
}

void MockTime::reset() {
    mockTime_ = std::chrono::steady_clock::now();
    useMockTime_ = false;
}
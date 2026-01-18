#pragma once

#include <chrono>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

namespace TestGlobals {
    extern int g_testsRun;
    extern int g_testsPassed;
    extern int g_testsFailed;
}

class MockTime {
private:
    static std::chrono::steady_clock::time_point mockTime_;
    static bool useMockTime_;
    
public:
    static void enable();
    static void disable();
    static std::chrono::steady_clock::time_point now();
    static void advance(std::chrono::minutes minutes);
    static void reset();
};
// test_framework.hpp — a tiny, dependency-free unit-test framework.
//
// Why hand-rolled instead of GoogleTest / Catch2 / doctest?
//   * Zero third-party dependencies → the test suite builds completely offline
//     and never adds to the project's already-heavy dependency surface
//     (GDAL + Qt + OpenGL).
//   * Single header, ~150 lines, easy to audit.
//
// Usage:
//   #include "test_framework.hpp"
//   TEST_CASE("description") { CHECK(1 + 1 == 2); }
// Then link every test translation unit together with main.cpp, which calls
// mt::run_all(). Run the binary with no arguments to execute every case, or
// pass substrings to run only matching cases (used for CTest granularity).

#pragma once

#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace mt {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

// Meyers-singleton registries so the framework stays header-only without ODR
// violations across translation units.
inline std::vector<TestCase> &registry()
{
    static std::vector<TestCase> r;
    return r;
}

struct Counters {
    int checks = 0;
    int failedChecks = 0;
};

inline Counters &counters()
{
    static Counters c;
    return c;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn)
    {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

// Thrown by REQUIRE to abort the current test case (but not the whole run).
struct RequireFailure : std::exception {
    const char *what() const noexcept override { return "REQUIRE failed"; }
};

inline bool useColor()
{
    static const bool tty = std::getenv("NO_COLOR") == nullptr;
    return tty;
}

inline const char *red()   { return useColor() ? "\033[31m" : ""; }
inline const char *green() { return useColor() ? "\033[32m" : ""; }
inline const char *dim()   { return useColor() ? "\033[2m"  : ""; }
inline const char *reset() { return useColor() ? "\033[0m"  : ""; }

inline void report(bool ok, const char *expr, const char *file, int line,
                   const std::string &extra = "")
{
    counters().checks++;
    if (!ok) {
        counters().failedChecks++;
        std::cerr << "    " << red() << "[CHECK FAILED]" << reset() << " " << file
                  << ":" << line << "  " << expr;
        if (!extra.empty()) std::cerr << "  " << dim() << "(" << extra << ")" << reset();
        std::cerr << "\n";
    }
}

// Runs all registered cases (optionally filtered by argv substrings).
// Returns the number of failed cases (0 == success), suitable as a process
// exit code.
inline int run_all(int argc, char **argv)
{
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) filters.emplace_back(argv[i]);

    int casesRun = 0, casesFailed = 0;
    for (const auto &tc : registry()) {
        bool match = filters.empty();
        for (const auto &f : filters)
            if (tc.name.find(f) != std::string::npos) match = true;
        if (!match) continue;

        ++casesRun;
        const int before = counters().failedChecks;
        std::cout << dim() << "[ RUN  ]" << reset() << " " << tc.name << "\n";

        try {
            tc.fn();
        } catch (const RequireFailure &) {
            // The failing REQUIRE already reported itself.
        } catch (const std::exception &e) {
            counters().failedChecks++;
            std::cerr << "    " << red() << "[EXCEPTION]" << reset() << " " << e.what() << "\n";
        } catch (...) {
            counters().failedChecks++;
            std::cerr << "    " << red() << "[EXCEPTION]" << reset() << " unknown\n";
        }

        const bool ok = counters().failedChecks == before;
        if (ok) {
            std::cout << green() << "[ PASS ]" << reset() << " " << tc.name << "\n";
        } else {
            std::cout << red() << "[ FAIL ]" << reset() << " " << tc.name << "\n";
            ++casesFailed;
        }
    }

    std::cout << "\n"
              << (casesFailed ? red() : green())
              << (casesFailed ? "FAILED" : "PASSED") << reset() << ": "
              << (casesRun - casesFailed) << "/" << casesRun << " cases, "
              << counters().checks << " checks, " << counters().failedChecks
              << " failed\n";
    return casesFailed;
}

}  // namespace mt

// ── Macros ──────────────────────────────────────────────────────────────────
#define MT_CONCAT_(a, b) a##b
#define MT_CONCAT(a, b) MT_CONCAT_(a, b)

#define TEST_CASE(NAME)                                                        \
    static void MT_CONCAT(mt_test_, __LINE__)();                               \
    static ::mt::Registrar MT_CONCAT(mt_reg_, __LINE__)(                       \
        NAME, &MT_CONCAT(mt_test_, __LINE__));                                 \
    static void MT_CONCAT(mt_test_, __LINE__)()

#define CHECK(expr) ::mt::report((expr), #expr, __FILE__, __LINE__)

#define CHECK_FALSE(expr) ::mt::report(!(expr), "!(" #expr ")", __FILE__, __LINE__)

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        const bool _mt_ok = (expr);                                            \
        ::mt::report(_mt_ok, #expr, __FILE__, __LINE__);                        \
        if (!_mt_ok) throw ::mt::RequireFailure{};                             \
    } while (0)

#define CHECK_APPROX(a, b, eps)                                                \
    ::mt::report(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= \
                     (eps),                                                    \
                 #a " ~= " #b, __FILE__, __LINE__,                             \
                 "|" + std::to_string(static_cast<double>(a)) + " - " +        \
                     std::to_string(static_cast<double>(b)) + "| > " +         \
                     std::to_string(static_cast<double>(eps)))

#define CHECK_THROWS(expr)                                                     \
    do {                                                                       \
        bool _mt_threw = false;                                                \
        try {                                                                  \
            (void)(expr);                                                      \
        } catch (...) {                                                        \
            _mt_threw = true;                                                  \
        }                                                                      \
        ::mt::report(_mt_threw, #expr " throws", __FILE__, __LINE__);          \
    } while (0)

#define CHECK_THROWS_AS(expr, ExType)                                          \
    do {                                                                       \
        bool _mt_threw = false;                                                \
        try {                                                                  \
            (void)(expr);                                                      \
        } catch (const ExType &) {                                             \
            _mt_threw = true;                                                  \
        } catch (...) {                                                        \
            _mt_threw = false;                                                 \
        }                                                                      \
        ::mt::report(_mt_threw, #expr " throws " #ExType, __FILE__, __LINE__); \
    } while (0)

#pragma once

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mini_test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(TestCase{ name, std::move(fn) });
    }
};

struct AssertionFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline void require(bool cond, const char* expr, const char* file, int line) {
    if (cond) return;
    std::ostringstream oss;
    oss << file << ":" << line << ": REQUIRE failed: " << expr;
    throw AssertionFailure(oss.str());
}

inline void require_close(double a, double b, double eps, const char* exprA, const char* exprB,
                          const char* file, int line) {
    if (std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= eps) return;
    std::ostringstream oss;
    oss << file << ":" << line << ": REQUIRE_CLOSE failed: "
        << exprA << "=" << a << " vs " << exprB << "=" << b << " (eps=" << eps << ")";
    throw AssertionFailure(oss.str());
}

inline int run_all() {
    int failed = 0;
    std::cout << "Running " << registry().size() << " tests...\n";
    for (auto& tc : registry()) {
        try {
            tc.fn();
            std::cout << "[OK] " << tc.name << "\n";
        } catch (const AssertionFailure& e) {
            ++failed;
            std::cout << "[FAIL] " << tc.name << "\n" << "  " << e.what() << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << tc.name << "\n" << "  Unexpected exception: " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "[FAIL] " << tc.name << "\n" << "  Unknown exception\n";
        }
    }
    std::cout << (failed == 0 ? "All tests passed.\n" : "Tests failed: " + std::to_string(failed) + "\n");
    return failed == 0 ? 0 : 1;
}

} // namespace mini_test

#define TEST_CASE(NAME) \
    static void NAME(); \
    static mini_test::Registrar NAME##_registrar(#NAME, &NAME); \
    static void NAME()

#define REQUIRE(EXPR) mini_test::require((EXPR), #EXPR, __FILE__, __LINE__)

#define REQUIRE_CLOSE(A,B,EPS) \
    mini_test::require_close((double)(A), (double)(B), (double)(EPS), #A, #B, __FILE__, __LINE__)

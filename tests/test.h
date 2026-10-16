#pragma once
// Minimal host test harness (not compiled into the bot: lives outside src/).
#include <cmath>
#include <cstdio>
#include <vector>
struct TestCase { const char* name; void (*fn)(); };
inline std::vector<TestCase>& test_registry() { static std::vector<TestCase> r; return r; }
inline int& test_failures() { static int n = 0; return n; }
struct TestRegistrar { TestRegistrar(const char* n, void (*f)()) { test_registry().push_back({n, f}); } };
#define TEST(name) static void name(); static TestRegistrar name##_reg(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++test_failures(); } } while (0)
#define CHECK_NEAR(a, b, eps) do { const double a_ = (a), b_ = (b); if (std::fabs(a_ - b_) > (eps)) { \
    std::printf("  FAIL %s:%d: %s = %g, expected %g\n", __FILE__, __LINE__, #a, a_, b_); ++test_failures(); } } while (0)

// De-RAS - tests/TestMain.h : tiny dependency-free test harness.
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace dt {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
struct Reg { Reg(const char* n, std::function<void()> f) { cases().push_back({n, f}); } };
}  // namespace dt

#define TEST(name) static void name(); static dt::Reg reg_##name(#name, name); static void name()
#define CHECK(cond) do { ++dt::checks(); if (!(cond)) { ++dt::failures(); \
    std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++dt::checks(); double _a = (a), _b = (b); \
    if (!(std::fabs(_a - _b) <= (tol))) { ++dt::failures(); \
    std::printf("  FAIL %s:%d: %s = %.9g, expected %.9g (tol %.3g)\n", __FILE__, __LINE__, #a, _a, _b, (double)(tol)); } } while (0)

#ifndef DERAS_TEST_CUSTOM_MAIN
int main() {
    for (auto& c : dt::cases()) {
        int before = dt::failures();
        c.fn();
        std::printf("%s %s\n", dt::failures() == before ? "[ OK ]" : "[FAIL]", c.name);
    }
    std::printf("%d checks, %d failures\n", dt::checks(), dt::failures());
    return dt::failures() == 0 ? 0 : 1;
}
#endif

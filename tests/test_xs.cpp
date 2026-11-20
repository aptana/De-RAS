// Cross-section hydraulic properties against hand calculations.
#include "TestMain.h"
#include "engines/common/XsHydraulics.h"
using namespace deras;
using namespace deras::hyd;

static CrossSection rect(double b, double wall, double n) {
    CrossSection xs;
    xs.staElev = {{0, wall}, {0, 0}, {b, 0}, {b, wall}};
    xs.manning = {{0, n}};
    xs.bankLeft = 0; xs.bankRight = b;
    return xs;
}

TEST(rectangle_properties) {
    auto g = XsGeom::from(rect(10, 5, 0.03), UnitSystem::SI);
    auto p = properties(g, 2.0);
    CHECK_NEAR(p.area, 20.0, 1e-12);
    CHECK_NEAR(p.perimeter, 14.0, 1e-12);
    CHECK_NEAR(p.topWidth, 10.0, 1e-12);
    double K = (1.0 / 0.03) * 20.0 * std::pow(20.0 / 14.0, 2.0 / 3.0);
    CHECK_NEAR(p.conveyance, K, 1e-9);
    CHECK_NEAR(p.alpha, 1.0, 1e-12);
    CHECK(!p.wallsExtended);
}
TEST(rectangle_us_units) {
    auto g = XsGeom::from(rect(10, 5, 0.03), UnitSystem::USCustomary);
    auto p = properties(g, 2.0);
    double K = (1.486 / 0.03) * 20.0 * std::pow(20.0 / 14.0, 2.0 / 3.0);
    CHECK_NEAR(p.conveyance, K, 1e-9);
}
TEST(trapezoid_partial_segments) {
    CrossSection xs;  // bottom 4, side slope 2H:1V, depth 3
    xs.staElev = {{0, 3}, {6, 0}, {10, 0}, {16, 3}};
    xs.manning = {{0, 0.025}};
    xs.bankLeft = 0; xs.bankRight = 16;
    auto p = properties(XsGeom::from(xs, UnitSystem::SI), 1.5);
    CHECK_NEAR(p.area, 4 * 1.5 + 2 * 1.5 * 1.5, 1e-12);
    CHECK_NEAR(p.perimeter, 4 + 2 * 1.5 * std::sqrt(5.0), 1e-12);
    CHECK_NEAR(p.topWidth, 4 + 2 * 2 * 1.5, 1e-12);
}
TEST(overbank_subdivision_and_alpha) {
    // channel 0..10 deep 2 m, overbanks flat at 2 m out to -20 and 30
    CrossSection xs;
    xs.staElev = {{-20, 3}, {-20, 2}, {0, 2}, {0, 0}, {10, 0}, {10, 2}, {30, 2}, {30, 3}};
    xs.manning = {{-20, 0.08}, {-10, 0.05}, {0, 0.03}, {10, 0.06}};
    xs.bankLeft = 0; xs.bankRight = 10;
    auto g = XsGeom::from(xs, UnitSystem::SI);
    auto p = properties(g, 2.5);
    // left overbank split at -10: two pieces 10 m wide, 0.5 m deep
    auto kp = [](double n, double a, double per) { return (1 / n) * a * std::pow(a / per, 2.0 / 3.0); };
    double kl = kp(0.08, 5, 10 + 0.5) + kp(0.05, 5, 10);
    double kc = kp(0.03, 25, 10 + 2 + 2);
    double kr = kp(0.06, 10, 20 + 0.5);
    CHECK_NEAR(p.lob.conveyance, kl, 1e-9);
    CHECK_NEAR(p.ch.conveyance, kc, 1e-9);
    CHECK_NEAR(p.rob.conveyance, kr, 1e-9);
    double at = 45, kt = kl + kc + kr;
    double alpha = at * at * (kl * kl * kl / 100 + kc * kc * kc / 625 + kr * kr * kr / 100) / (kt * kt * kt);
    CHECK_NEAR(p.alpha, alpha, 1e-9);
    CHECK(p.alpha > 1.0);
}
TEST(composite_channel_n) {
    // rectangular channel with two n values on the bed (Horton/Einstein composite)
    CrossSection xs;
    xs.staElev = {{0, 3}, {0, 0}, {5, 0}, {10, 0}, {10, 3}};
    xs.manning = {{0, 0.02}, {5, 0.04}};
    xs.bankLeft = 0; xs.bankRight = 10;
    auto p = properties(XsGeom::from(xs, UnitSystem::SI), 1.0);
    double P = 1 + 5 + 5 + 1;
    double nc = std::pow((6 * std::pow(0.02, 1.5) + 6 * std::pow(0.04, 1.5)) / P, 2.0 / 3.0);
    double K = (1 / nc) * 10 * std::pow(10 / P, 2.0 / 3.0);
    CHECK_NEAR(p.conveyance, K, 1e-9);
}
TEST(walls_extended) {
    CrossSection xs;
    xs.staElev = {{0, 1}, {0, 0}, {10, 0}, {10, 1}};
    xs.manning = {{0, 0.03}};
    xs.bankLeft = 0; xs.bankRight = 10;
    auto p = properties(XsGeom::from(xs, UnitSystem::SI), 1.5);
    CHECK(p.wallsExtended);
    CHECK_NEAR(p.area, 15, 1e-12);
    CHECK_NEAR(p.perimeter, 13, 1e-12);
}
TEST(critical_rectangle_analytic) {
    auto g = XsGeom::from(rect(8, 10, 0.03), UnitSystem::SI);
    for (double q : {5.0, 20.0, 60.0}) {
        double yc = std::cbrt((q / 8) * (q / 8) / 9.81);
        CHECK_NEAR(criticalWS(g, q, 0.003), yc, 0.0005);
    }
    auto gu = XsGeom::from(rect(8, 20, 0.03), UnitSystem::USCustomary);
    double yc = std::cbrt((500.0 / 8) * (500.0 / 8) / 32.174);
    CHECK_NEAR(criticalWS(gu, 500, 0.01), yc, 0.001);
}
TEST(critical_trapezoid_reference) {
    CrossSection xs;
    xs.staElev = {{0, 6}, {9, 0}, {14, 0}, {23, 6}};  // b=5, z=1.5
    xs.manning = {{0, 0.03}};
    xs.bankLeft = 0; xs.bankRight = 23;
    auto g = XsGeom::from(xs, UnitSystem::SI);
    double q = 40;
    // reference: Q^2 T / (g A^3) = 1 by bisection
    auto f = [&](double y) { double A = (5 + 1.5 * y) * y, T = 5 + 3 * y; return q * q * T / (9.81 * A * A * A) - 1; };
    double lo = 0.01, hi = 5;
    for (int i = 0; i < 200; ++i) { double m = 0.5 * (lo + hi); (f(m) > 0 ? lo : hi) = m; }
    CHECK_NEAR(criticalWS(g, q, 0.003), 0.5 * (lo + hi), 0.0005);
}
TEST(normal_depth_rectangle_reference) {
    auto g = XsGeom::from(rect(6, 10, 0.025), UnitSystem::SI);
    double q = 30, s = 0.0008;
    auto f = [&](double y) { double A = 6 * y, P = 6 + 2 * y; return (1 / 0.025) * A * std::pow(A / P, 2.0 / 3.0) * std::sqrt(s) - q; };
    double lo = 0.01, hi = 9;
    for (int i = 0; i < 200; ++i) { double m = 0.5 * (lo + hi); (f(m) < 0 ? lo : hi) = m; }
    CHECK_NEAR(normalWS(g, q, s, 0.003), 0.5 * (lo + hi), 1e-5);
}

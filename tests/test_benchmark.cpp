// Analytical/numerical benchmarks for the standard-step solver on a
// prismatic rectangular channel:
//  B1 uniform flow  : normal-depth boundary -> depth equals normal depth everywhere
//  B2 M1 backwater  : compared with a fine RK4 integration of the GVF equation
//  B3 M2 drawdown   : critical-depth boundary, compared with RK4 reference
//  B4 US units      : same M1 case in ft/cfs
// The reference GVF integration is independent of the De-RAS hydraulics code.
#include "TestMain.h"
#include "engines/steady/SteadySolver.h"
#include <cstdio>
using namespace deras;

struct Chan { double b, n, s0, q, g, k; };

static double sf(const Chan& c, double y) {
    double A = c.b * y, P = c.b + 2 * y, K = (c.k / c.n) * A * std::pow(A / P, 2.0 / 3.0);
    return (c.q / K) * (c.q / K);
}
static double dydx(const Chan& c, double y) {  // x measured upstream
    double V = c.q / (c.b * y), Fr2 = V * V / (c.g * y);
    return (sf(c, y) - c.s0) / (1 - Fr2);
}
// depth at distance L upstream of a section with depth y0
static double rk4(const Chan& c, double y0, double L, double h = 0.05) {
    double y = y0;
    int n = static_cast<int>(std::ceil(L / h));
    double dx = L / n;
    for (int i = 0; i < n; ++i) {
        double k1 = dydx(c, y), k2 = dydx(c, y + 0.5 * dx * k1), k3 = dydx(c, y + 0.5 * dx * k2), k4 = dydx(c, y + dx * k3);
        y += dx * (k1 + 2 * k2 + 2 * k3 + k4) / 6;
    }
    return y;
}
static double yNormal(const Chan& c) {
    double lo = 1e-4, hi = 50;
    for (int i = 0; i < 200; ++i) { double m = 0.5 * (lo + hi); (sf(c, m) > c.s0 ? lo : hi) = m; }
    return 0.5 * (lo + hi);
}

struct Model { Geometry g; SteadyFlow f; Plan p; };

static Model build(const Chan& c, int nxs, double dx, BoundaryType bt, double dnValue, double tol, double wall) {
    Model m;
    Reach r; r.river = "R"; r.reach = "A";
    for (int i = 0; i < nxs; ++i) {
        double dist = (nxs - 1 - i) * dx, z = c.s0 * dist;
        CrossSection xs;
        xs.rs = std::to_string(static_cast<int>(dist));
        xs.staElev = {{0, z + wall}, {0, z}, {c.b, z}, {c.b, z + wall}};
        xs.manning = {{0, c.n}};
        xs.bankLeft = 0; xs.bankRight = c.b;
        xs.lenLeft = xs.lenChannel = xs.lenRight = (i + 1 < nxs) ? dx : 0;
        xs.contraction = 0; xs.expansion = 0;  // the GVF equation has no eddy losses
        r.nodes.push_back(xs);
    }
    m.g.reaches.push_back(r);
    m.f.profileNames = {"PF1"};
    m.f.changes.push_back({"R", "A", r.nodes.front().rs, {c.q}});
    auto& b = m.f.boundaryRef("R", "A", 0);
    b.downstream.type = bt;
    b.downstream.slope = dnValue;
    b.downstream.knownWS = dnValue;
    m.p.stdStepTol = tol; m.p.criticalTol = tol; m.p.maxTrials = 40; m.p.maxErrorTol = 0.1;
    return m;
}

TEST(B1_uniform_flow) {
    Chan c{10, 0.03, 0.001, 30, 9.81, 1.0};
    double yn = yNormal(c);
    auto m = build(c, 21, 100, BoundaryType::NormalDepth, c.s0, 1e-4, 20);
    auto r = steady::solve(m.g, m.f, m.p, UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    double maxErr = 0;
    for (const auto& x : r.profiles[0].xs) maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - yn));
    std::printf("  B1 normal depth %.4f m, max |error| %.5f m\n", yn, maxErr);
    CHECK(maxErr < 0.001);
}

TEST(B2_M1_backwater_SI) {
    Chan c{10, 0.03, 0.001, 30, 9.81, 1.0};
    double yn = yNormal(c), y0 = 2.0 * yn;
    for (double dx : {50.0, 100.0, 200.0}) {
        int nxs = static_cast<int>(4000 / dx) + 1;
        auto m = build(c, nxs, dx, BoundaryType::KnownWS, y0, 1e-5, 20);
        auto r = steady::solve(m.g, m.f, m.p, UnitSystem::SI, {});
        CHECK(r.status == RunStatus::Succeeded);
        double maxErr = 0;
        for (const auto& x : r.profiles[0].xs) {
            double ref = rk4(c, y0, x.channelDistance);
            maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - ref));
        }
        std::printf("  B2 dx=%5.0f m: max |WS - RK4| = %.5f m\n", dx, maxErr);
        CHECK(maxErr < 0.003);
    }
    // default tolerance (0.003 m) still within 0.01 m of the reference
    auto m = build(c, 41, 100, BoundaryType::KnownWS, y0, 0.003, 20);
    auto r = steady::solve(m.g, m.f, m.p, UnitSystem::SI, {});
    double maxErr = 0;
    for (const auto& x : r.profiles[0].xs) maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - rk4(c, y0, x.channelDistance)));
    std::printf("  B2 default tol: max |WS - RK4| = %.5f m\n", maxErr);
    CHECK(maxErr < 0.01);
}

TEST(B3_M2_drawdown_SI) {
    Chan c{10, 0.03, 0.0005, 30, 9.81, 1.0};
    double yc = std::cbrt((3.0) * (3.0) / 9.81);
    auto m = build(c, 81, 25, BoundaryType::CriticalDepth, 0, 1e-5, 20);
    auto r = steady::solve(m.g, m.f, m.p, UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    CHECK_NEAR(r.profiles[0].xs.back().ws - r.profiles[0].xs.back().minChEl, yc, 0.0005);
    // compare away from the singular point at critical depth (start RK4 from 5 % above yc)
    double maxErr = 0;
    const auto& xs = r.profiles[0].xs;
    // anchor the reference at the computed depth 250 m upstream, then compare further upstream
    const XsResult* anchor = nullptr;
    for (const auto& x : xs) if (std::fabs(x.channelDistance - 250) < 1e-6) anchor = &x;
    CHECK(anchor != nullptr);
    if (anchor) {
        for (const auto& x : xs) {
            if (x.channelDistance <= 250) continue;
            double ref = rk4(c, anchor->ws - anchor->minChEl, x.channelDistance - 250);
            maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - ref));
        }
    }
    std::printf("  B3 M2: max |WS - RK4| (x > 250 m) = %.5f m\n", maxErr);
    CHECK(maxErr < 0.003);
}

TEST(B4_M1_backwater_US) {
    Chan c{30, 0.03, 0.001, 1000, 32.174, 1.486};
    double yn = yNormal(c), y0 = 1.8 * yn;
    auto m = build(c, 41, 300, BoundaryType::KnownWS, y0, 1e-4, 60);
    auto r = steady::solve(m.g, m.f, m.p, UnitSystem::USCustomary, {});
    CHECK(r.status == RunStatus::Succeeded);
    double maxErr = 0;
    for (const auto& x : r.profiles[0].xs) maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - rk4(c, y0, x.channelDistance)));
    std::printf("  B4 US: yn=%.3f ft, max |WS - RK4| = %.5f ft\n", yn, maxErr);
    CHECK(maxErr < 0.01);
}

TEST(B5_energy_balance_residual) {
    // Every converged step satisfies the energy equation within the tolerance.
    Chan c{10, 0.03, 0.001, 30, 9.81, 1.0};
    auto m = build(c, 21, 100, BoundaryType::KnownWS, 2.0 * yNormal(c), 1e-4, 20);
    auto r = steady::solve(m.g, m.f, m.p, UnitSystem::SI, {});
    const auto& xs = r.profiles[0].xs;
    double maxRes = 0;
    for (size_t i = 0; i + 1 < xs.size(); ++i) {
        double lhs = xs[i].eg, rhs = xs[i + 1].eg + xs[i].frictionLoss + xs[i].ceLoss;
        maxRes = std::max(maxRes, std::fabs(lhs - rhs));
    }
    std::printf("  B5 max energy residual %.6f m\n", maxRes);
    CHECK(maxRes < 2e-4);
}

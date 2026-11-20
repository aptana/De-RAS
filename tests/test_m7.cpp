// M7 tests (build 0.2.3): quasi-unsteady sediment transport.
//  T1 fall velocity and transport functions: thresholds and trends
//  E1 equilibrium inflow in uniform flow: the bed does not change
//  E2 clear-water inflow: the upstream bed erodes, never below the erodible depth
//  E3 sediment mass balance: in - out = bed change
//  E4 inflow above capacity (rating curve): the upstream bed rises, self-limited
//  J1 setup and results file round trip
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "TestMain.h"
#include "app/Templates.h"
#include "engines/sediment/Sediment.h"
#include "io/ModuleData.h"

using namespace deras;

namespace {
Geometry channel() {
    app::PrismaticSpec s;
    s.river = "Sungai";
    s.reach = "Hilir";
    s.count = 11;
    s.spacing = 200;
    s.bottomWidth = 20;
    s.sideSlope = 2;
    s.bankHeight = 3;
    s.floodplainWidth = 30;
    s.floodplainRise = 2;
    s.bedSlope = 0.001;
    s.downstreamInvert = 100;
    s.nChannel = 0.03;
    s.nOverbank = 0.06;
    Geometry g;
    Reach r;
    r.river = s.river;
    r.reach = s.reach;
    for (int i = 0; i < s.count; ++i) {
        const double dist = (s.count - 1 - i) * s.spacing;
        r.nodes.push_back(app::trapezoidXs(s, s.downstreamInvert + s.bedSlope * dist, io::formatPlain(dist), i + 1 < s.count ? s.spacing : 0));
    }
    g.reaches.push_back(r);
    return g;
}
SedimentSetup setup(SedimentInflow in) {
    SedimentSetup st;
    st.flow.river = "Sungai";
    st.flow.reach = "Hilir";
    st.flow.dsSlope = 0.001;
    st.flow.records = {{48, 60, 6}, {48, 120, 6}, {48, 40, 6}};
    st.data.gradation = {{0.1, 5}, {0.3, 50}, {1.0, 90}, {2.0, 100}};
    st.data.inflow = in;
    st.data.maxErodibleDepth = 0.5;
    st.data.function = TransportFunction::EngelundHansen;
    st.outputHours = 24;
    return st;
}
}  // namespace

TEST(T1_functions) {
    const double nu = sediment::kinematicViscosity(20);
    CHECK(std::fabs(nu - 1.0e-6) < 0.05e-6);
    const double w = sediment::fallVelocity(0.0002, 2.65, nu);
    std::printf("  fall velocity 0.2 mm at 20 C: %.4f m/s\n", w);
    CHECK(w > 0.018 && w < 0.030);
    sediment::Hydraulics h{60, 1.0, 2.0, 0.001, 30, 0.03};
    for (auto f : {TransportFunction::MeyerPeterMuller, TransportFunction::EngelundHansen, TransportFunction::Yang}) {
        const double c1 = sediment::capacity(f, h, 0.0005, 0.001, 2.65, nu);
        sediment::Hydraulics h2 = h;
        h2.V = 1.5;
        h2.Q = 90;
        h2.S = 0.00225;
        const double c2 = sediment::capacity(f, h2, 0.0005, 0.001, 2.65, nu);
        std::printf("  %-20s Qs(V=1.0) %.5f  Qs(V=1.5) %.5f m3/s\n", transportFunctionName(f), c1, c2);
        CHECK(c1 > 0 && c2 > c1);
    }
    // below the Meyer-Peter Muller threshold: no transport of coarse gravel
    sediment::Hydraulics low{5, 0.3, 0.5, 0.0005, 20, 0.03};
    CHECK(sediment::capacity(TransportFunction::MeyerPeterMuller, low, 0.02, 0.04, 2.65, nu) == 0);
    CHECK(std::fabs(gradationDiameter({{0.1, 10}, {1.0, 90}}, 50) - std::sqrt(0.1 * 1.0)) < 1e-9);
}

TEST(E1_equilibrium) {
    const Geometry g = channel();
    const auto r = sediment::run(g, setup(SedimentInflow::Equilibrium), UnitSystem::SI);
    for (const auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str());
    CHECK(r.status == RunStatus::Succeeded);
    double maxDz = 0;
    for (double d : r.change.back()) maxDz = std::max(maxDz, std::fabs(d));
    std::printf("  max |bed change| %.5f m, %zu output times\n", maxDz, r.hours.size());
    CHECK(maxDz < 0.005);
    CHECK(r.hours.size() == 7);
    CHECK(r.massIn > 0);
}

TEST(E2_E3_clear_water) {
    const Geometry g = channel();
    auto st = setup(SedimentInflow::ClearWater);
    st.data.maxErodibleDepth = 3;
    const auto r = sediment::run(g, st, UnitSystem::SI);
    CHECK(r.status == RunStatus::Succeeded);
    const auto& ch = r.change.back();
    std::printf("  bed change upstream %.3f, middle %.3f, downstream %.3f m\n", ch.front(), ch[ch.size() / 2], ch.back());
    CHECK(ch.front() < -0.05 && ch.front() < ch.back());
    for (double d : ch) CHECK(d <= 1e-9 && d >= -3 - 1e-9);
    // a shallow erodible layer limits the erosion
    st.data.maxErodibleDepth = 0.2;
    const auto r2 = sediment::run(g, st, UnitSystem::SI);
    for (double d : r2.change.back()) CHECK(d >= -0.2 - 1e-9);
    CHECK(r.massIn == 0);
    const double err = r.massIn - r.massOut - r.massChange;
    std::printf("  mass in %.1f t, out %.1f t, bed change %.1f t, closure %.2e t\n", r.massIn, r.massOut, r.massChange, err);
    CHECK(std::fabs(err) < 1e-6 * std::max(1.0, r.massOut));
    // inverts follow the bed change
    for (size_t i = 0; i < r.rs.size(); ++i) CHECK(std::fabs(r.invert.back()[i] - (r.invert0[i] + ch[i])) < 1e-6);
}

TEST(E4_overloaded) {
    const Geometry g = channel();
    auto st = setup(SedimentInflow::Equilibrium);
    st.data.function = TransportFunction::MeyerPeterMuller;
    st.data.gradation = {{0.5, 0}, {2, 50}, {8, 100}};
    const auto eq = sediment::run(g, st, UnitSystem::SI);
    CHECK(eq.status == RunStatus::Succeeded);
    // rating curve at three times the equilibrium load of each flow
    st.data.inflow = SedimentInflow::Rating;
    st.data.rating.clear();
    for (size_t k = 1; k < eq.hours.size(); ++k) st.data.rating.emplace_back(eq.flow[k], 3 * eq.load[k].front());
    std::sort(st.data.rating.begin(), st.data.rating.end());
    st.data.rating.erase(std::unique(st.data.rating.begin(), st.data.rating.end(), [](auto& a, auto& b) { return a.first == b.first; }), st.data.rating.end());
    const auto r = sediment::run(g, st, UnitSystem::SI);
    CHECK(r.status == RunStatus::Succeeded);
    for (const auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str());
    std::printf("  upstream bed change %.3f m (MPM, d50 %.2f mm)\n", r.change.back().front(), r.d50);
    CHECK(r.change.back().front() > 0.01 && r.change.back().front() < 3.0);
    const double err = r.massIn - r.massOut - r.massChange;
    CHECK(std::fabs(err) < 1e-6 * std::max(1.0, r.massIn));
}

TEST(J1_roundtrip) {
    io::ProjectSettings s;
    auto st = setup(SedimentInflow::Rating);
    st.data.rating = {{50, 100}, {150, 900}};
    st.flow.records[1].dsStage = 104.2;
    io::setSediment(s, st);
    const auto back = io::sedimentFor(s);
    CHECK(back.flow.records.size() == 3 && back.flow.records[1].dsStage == 104.2 && std::isnan(back.flow.records[0].dsStage));
    CHECK(back.data.gradation == st.data.gradation && back.data.rating == st.data.rating && back.data.inflow == SedimentInflow::Rating);
    CHECK(back.data.function == TransportFunction::EngelundHansen && back.flow.river == "Sungai");
    const Geometry g = channel();
    auto r = sediment::run(g, back, UnitSystem::SI);
    CHECK(r.status == RunStatus::Succeeded);
    const auto path = io::fs::temp_directory_path() / "deras_test_m7.deras-sedimen.json";
    std::string err;
    CHECK(io::writeSedimentResult(path, r, err));
    SedimentResult r2;
    CHECK(io::readSedimentResult(path, r2, err));
    CHECK(r2.rs == r.rs && r2.change.size() == r.change.size() && std::fabs(r2.massOut - r.massOut) < 1e-6 * std::max(1.0, r.massOut));
    std::error_code ec;
    io::fs::remove(path, ec);
}

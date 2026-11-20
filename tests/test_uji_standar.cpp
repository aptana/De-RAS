// Standard test project (review R5 of build 0.2.0): tests/data/uji-standar/
// DerasUji1km.prj, a 1 km river with a section every 200 m and a 20 m wide
// channel. Every analysis feature is tested on this project; results are
// recorded in docs/TESTING.md (S1–S4).
//  S1 open–save identical      : ctest "roundtrip_uji_standar" (deras-roundtrip)
//  S2 Model Doctor             : no errors, no warnings
//  S3 steady run               : the plan stored in the project runs to the end
//  S4 uniform flow             : WS depth equals the normal depth computed here
//                                independently (Manning, LOB/channel/ROB
//                                subdivision at the bank stations)
//  S5 M6 example (plan p03)    : Model Doctor clean (errors), the lateral weir
//                                spills into the 2D area, 2D volume balanced
//  S6 M7 example (sediment)    : clear-water run erodes the upstream bed,
//                                mass balance closes
#include "TestMain.h"
#include "app/ModelDoctor.h"
#include "engines/steady/SteadySolver.h"
#include "engines/unsteady/UnsteadySolver.h"
#include "io/GeoTiff.h"
#include "io/ModuleData.h"
#include "engines/design/Design.h"
#include "engines/sediment/Sediment.h"
#include "io/Project.h"
#include <cstdio>
using namespace deras;

static std::unique_ptr<io::Project> openUji() {
    MessageList msgs;
    auto p = io::Project::open(io::toPath(std::string(DERAS_TEST_DATA) + "/uji-standar/DerasUji1km.prj"), msgs);
    for (const auto& m : msgs) std::printf("  open: %s\n", m.text(Language::EN).c_str());
    return p;
}

// Area and wetted perimeter of the ground polyline below water surface ws.
static void wetted(const std::vector<std::pair<double, double>>& pts, double ws, double& A, double& P) {
    A = P = 0;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const double x1 = pts[i].first, z1 = pts[i].second, x2 = pts[i + 1].first, z2 = pts[i + 1].second;
        const double h1 = ws - z1, h2 = ws - z2;
        if (h1 <= 0 && h2 <= 0) continue;
        if (h1 >= 0 && h2 >= 0) {
            A += 0.5 * (h1 + h2) * (x2 - x1);
            P += std::hypot(x2 - x1, z2 - z1);
        } else {
            const double f = h1 / (h1 - h2), xc = x1 + f * (x2 - x1);
            if (h1 > 0) { A += 0.5 * h1 * (xc - x1); P += std::hypot(xc - x1, f * (z2 - z1)); }
            else { A += 0.5 * h2 * (x2 - xc); P += std::hypot(x2 - xc, (1 - f) * (z2 - z1)); }
        }
    }
}

// Normal depth (above the channel invert) for the specification in
// tools/testdata/make_uji_standar.cpp, independent of the De-RAS hydraulics.
static double normalDepth(double q) {
    const std::vector<std::pair<double, double>> lob{{0, 4.5}, {30, 3}}, ch{{30, 3}, {33, 0}, {47, 0}, {50, 3}}, rob{{50, 3}, {80, 4.5}};
    auto flow = [&](double y) {
        double k = 0, A, P;
        for (auto [pts, n] : {std::pair{&lob, 0.06}, std::pair{&ch, 0.035}, std::pair{&rob, 0.06}}) {
            wetted(*pts, y, A, P);
            if (A > 0) k += (1.0 / n) * A * std::pow(A / P, 2.0 / 3.0);
        }
        return k * std::sqrt(0.001);
    };
    double lo = 1e-4, hi = 4.5;
    for (int i = 0; i < 200; ++i) { const double m = 0.5 * (lo + hi); (flow(m) < q ? lo : hi) = m; }
    return 0.5 * (lo + hi);
}

TEST(S2_model_doctor_clean) {
    auto p = openUji();
    CHECK(p != nullptr);
    if (!p) return;
    const auto& pl = p->plans.at(p->info().currentPlan).model;
    const auto msgs = app::checkSteadyRun(pl, &p->geometries.at(pl.geomFile).model, &p->steadyFlows.at(pl.flowFile).model, p->units());
    int e = 0, w = 0;
    for (const auto& m : msgs) {
        if (m.severity == Severity::Error) ++e;
        if (m.severity == Severity::Warning) ++w;
        if (m.severity != Severity::Info) std::printf("  %s\n", m.text(Language::EN).c_str());
    }
    std::printf("  S2 Model Doctor: %d errors, %d warnings\n", e, w);
    CHECK(e == 0);
    CHECK(w == 0);
}

TEST(S3_S4_steady_uniform_flow) {
    auto p = openUji();
    CHECK(p != nullptr);
    if (!p) return;
    const auto& pl = p->plans.at(p->info().currentPlan).model;
    const auto r = steady::solve(p->geometries.at(pl.geomFile).model, p->steadyFlows.at(pl.flowFile).model, pl, p->units(), {});
    std::printf("  S3 run status: %s\n", runStatusLabel(r.status, Language::EN));
    CHECK(r.status == RunStatus::Succeeded);
    CHECK(r.profiles.size() == 3);
    const double flows[] = {20, 60, 150};
    for (size_t k = 0; k < r.profiles.size() && k < 3; ++k) {
        const double yn = normalDepth(flows[k]);
        double maxErr = 0;
        for (const auto& x : r.profiles[k].xs) maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - yn));
        const auto& up = r.profiles[k].xs.front();
        std::printf("  S4 %-5s Q=%6.1f  normal depth %.3f m  RS %s WS %.3f  Fr %.3f  max |depth - yn| %.4f m\n", r.profiles[k].name.c_str(), flows[k], yn,
                    up.rs.c_str(), up.ws, up.froudeCh, maxErr);
        CHECK(r.profiles[k].xs.size() == 6);
        CHECK(maxErr < 0.01);
    }
}

TEST(S5_lateral_into_2d_area) {
    auto p = openUji();
    CHECK(p != nullptr);
    if (!p) return;
    const Plan* pl = nullptr;
    for (const auto& [k, f] : p->plans) if (f.model.shortId == "Dataran2D") pl = &f.model;
    CHECK(pl != nullptr);
    if (!pl) return;
    const auto& g = p->geometries.at(pl->geomFile).model;
    const auto& u = p->unsteadyFlows.at(pl->flowFile).model;
    int e = 0;
    for (const auto& m : app::checkUnsteadyRun(*pl, &g, &u, p->units())) {
        if (m.severity == Severity::Error) ++e;
        if (m.severity != Severity::Info) std::printf("  doctor: %s\n", m.text(Language::EN).c_str());
    }
    CHECK(e == 0);
    std::vector<io::Raster> terr(1);
    std::string err;
    CHECK(io::readGeoTiff(io::toPath(std::string(DERAS_TEST_DATA) + "/uji-standar/Terrain/DerasUji1km_terrain.tif"), terr[0], err));
    unsteady::Inputs in;
    in.terrains = &terr;
    const auto r = unsteady::solve(g, u, *pl, p->units(), {}, in);
    std::printf("  S5 run status: %s\n", runStatusLabel(r.status, Language::EN));
    for (const auto& m : r.messages) if (m.severity != Severity::Info || m.code == "SOLV_2D_DONE") std::printf("  %s\n", m.text(Language::EN).c_str());
    CHECK(r.status == RunStatus::Succeeded);
    double latMax = -1;
    for (const auto& x : r.extra) if (x.kind == "lateral") latMax = x.maxValue;
    std::printf("  S5 max lateral flow %.2f m3/s\n", latMax);
    CHECK(latMax > 1 && latMax < 75);
    CHECK(r.grids.size() == 1);
    if (!r.grids.empty()) {
        double dmax = 0;
        for (double d : r.grids[0].maxDepth) dmax = std::max(dmax, d);
        std::printf("  S5 2D cells %zu, max depth %.2f m\n", r.grids[0].maxDepth.size(), dmax);
        CHECK(dmax > 0.05);
    }
    for (const auto& m : r.messages)
        if (m.code == "SOLV_2D_DONE") CHECK(std::fabs(std::atof(m.args[4].c_str())) < 1.0);
}

TEST(S6_sediment_example) {
    auto p = openUji();
    CHECK(p != nullptr);
    if (!p) return;
    const auto st = io::sedimentFor(p->settings);
    CHECK(!st.empty() && p->geometries.count(st.geomFile));
    if (st.empty() || !p->geometries.count(st.geomFile)) return;
    const auto r = sediment::run(p->geometries.at(st.geomFile).model, st, p->units());
    for (const auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str());
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    std::printf("  S6 bed change RS %s %.3f m, RS %s %.3f m\n", r.rs.front().c_str(), r.change.back().front(), r.rs.back().c_str(), r.change.back().back());
    CHECK(r.change.back().front() < -0.01);
    CHECK(std::fabs(r.massIn - r.massOut - r.massChange) < 1e-6 * std::max(1.0, r.massOut));
}

// S7 (M8): the water quality example runs on the p02 flood: the tracer pulse
// reaches the downstream end and the mass balance closes.
TEST(S7_water_quality_example) {
    auto p = openUji();
    CHECK(p != nullptr);
    if (!p) return;
    const auto w = io::waterQualityFor(p->settings);
    CHECK(w.plan == "p02" && p->plans.count(w.plan));
    if (!p->plans.count(w.plan)) return;
    const Plan& pl = p->plans.at(w.plan).model;
    const Geometry& g = p->geometries.at(pl.geomFile).model;
    const auto hyd = unsteady::solve(g, p->unsteadyFlows.at(pl.flowFile).model, pl, p->units(), {});
    CHECK(hyd.status == RunStatus::Succeeded);
    const auto r = design::waterQuality(g, hyd, w.input, p->units());
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    double peak = 0, tPeak = 0;
    for (size_t t = 0; t < r.hours.size(); ++t)
        if (r.conc[t].back() > peak) { peak = r.conc[t].back(); tPeak = r.hours[t]; }
    const double bal = (r.massStored - r.massStored0) - (r.massIn - r.massOut - r.massDecayed + r.massCorrection);
    std::printf("  S7 downstream peak %.2f mg/L at %.2f h, mass in %.4g g, correction %.3g g, balance %.3g g\n", peak, tPeak, r.massIn, r.massCorrection, bal);
    CHECK(peak > 50 && peak <= 100.01 && tPeak > 3);
    CHECK(std::fabs(bal) < 1e-6 * r.massIn);
    CHECK(std::fabs(r.massCorrection) < 0.01 * r.massIn);
}

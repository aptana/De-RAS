// M8 tests (build 0.2.3)
//  V2 project copy converted SI -> US (original untouched)
//  U1 uniform flow and critical depth (rectangular channel, hand computation)
//  P1 HEC-18 pier scour: CSU equation and the K2 table (L/a = 8, 15 degrees ~ 2.0)
//  C1 contraction scour: no contraction -> no live-bed scour; clear-water equation
//  A1 Froehlich abutment scour
//  K1 Manning's n calibration recovers a known factor
//  Q1 water quality: tracer reaches the downstream end, mass balance closes
//  V1 unit conversion SI -> US -> SI round trip
//  R1 autosave recovery round trip
//  D1 Word report: zip structure and CRC-32
#include <cmath>
#include <cstdio>

#include "TestMain.h"
#include "app/ConvertProject.h"
#include "app/Report.h"
#include "app/Templates.h"
#include "engines/design/Design.h"
#include "engines/steady/SteadySolver.h"
#include "engines/unsteady/UnsteadySolver.h"
#include "io/ModuleData.h"
#include "io/Project.h"

using namespace deras;

namespace {
Geometry channel(double n = 0.035) {
    app::PrismaticSpec s;
    s.river = "S";
    s.reach = "R";
    s.count = 11;
    s.spacing = 200;
    s.bottomWidth = 20;
    s.sideSlope = 2;
    s.bankHeight = 3;
    s.floodplainWidth = 30;
    s.floodplainRise = 2;
    s.bedSlope = 0.001;
    s.downstreamInvert = 100;
    s.nChannel = n;
    s.nOverbank = 0.06;
    Geometry g;
    Reach r;
    r.uid = nextUid();
    r.river = s.river;
    r.reach = s.reach;
    for (int i = 0; i < s.count; ++i) {
        const double dist = (s.count - 1 - i) * s.spacing;
        auto x = app::trapezoidXs(s, s.downstreamInvert + s.bedSlope * dist, io::formatPlain(dist), i + 1 < s.count ? s.spacing : 0);
        x.uid = nextUid();
        r.nodes.push_back(x);
    }
    g.reaches.push_back(r);
    return g;
}
SteadyFlow flow(double q) {
    SteadyFlow f;
    f.profileNames = {"P1"};
    f.changes.push_back({"S", "R", "2000", {q}});
    auto& b = f.boundaryRef("S", "R", 0);
    b.downstream.type = BoundaryType::KnownWS;
    b.downstream.knownWS = 102.6;
    return f;
}
}  // namespace

TEST(U1_uniform) {
    design::Trapezoid c{10, 0, 0.03, 0.001};
    const auto r = design::uniformFromDepth(c, 2, UnitSystem::SI);
    const double qHand = 1 / 0.03 * 20 * std::pow(20.0 / 14.0, 2.0 / 3.0) * std::sqrt(0.001);
    std::printf("  Q %.4f (hand %.4f), yc %.4f\n", r.flow, qHand, r.criticalDepth);
    CHECK(r.ok && std::fabs(r.flow - qHand) < 1e-9);
    const double yc = std::cbrt(std::pow(qHand / 10, 2) / 9.80665);
    CHECK(std::fabs(r.criticalDepth - yc) < 1e-6);
    const auto n = design::uniformFromFlow(c, qHand, UnitSystem::SI);
    CHECK(n.ok && std::fabs(n.depth - 2) < 1e-6);
    CHECK(std::fabs(r.shear - 9806.65 * (20.0 / 14.0) * 0.001) < 1e-6);
}

TEST(P1_pier_scour) {
    design::PierScourInput in;
    in.a = 1.5;
    in.L = 1.5;
    in.y1 = 3;
    in.V1 = 2;
    in.nose = design::PierNose::Round;
    in.K3 = 1.1;
    const auto r = design::pierScour(in, UnitSystem::SI);
    const double fr = 2 / std::sqrt(9.80665 * 3);
    const double hand = 1.5 * 2.0 * 1.1 * std::pow(2.0, 0.35) * std::pow(fr, 0.43);
    std::printf("  ys %.3f m (hand %.3f), Fr %.3f\n", r.scour, hand, r.froude);
    CHECK(std::fabs(r.scour - hand) < 1e-9 && !r.limited);
    in.L = 12;
    in.angle = 15;
    const auto r2 = design::pierScour(in, UnitSystem::SI);
    std::printf("  K2 (L/a = 8, 15 deg) %.3f\n", r2.K2);
    CHECK(std::fabs(r2.K2 - 2.0) < 0.1);
}

TEST(C1_contraction) {
    design::ContractionInput in;
    in.liveBed = true;
    in.y1 = in.y0 = 2;
    in.Q1 = in.Q2 = 100;
    in.W1 = in.W2 = 40;
    const auto r = design::contractionScour(in, UnitSystem::SI);
    CHECK(std::fabs(r.scour) < 1e-9);
    in.W2 = 20;
    const auto r2 = design::contractionScour(in, UnitSystem::SI);
    CHECK(r2.scour > 0.5);
    in.liveBed = false;
    in.Q2 = 100;
    in.W2 = 20;
    in.D50mm = 2;
    const auto r3 = design::contractionScour(in, UnitSystem::SI);
    const double hand = std::pow(0.025 * 100 * 100 / (std::pow(1.25 * 0.002, 2.0 / 3.0) * 400), 3.0 / 7.0);
    std::printf("  live-bed y2 %.3f (k1 %.2f), clear-water y2 %.3f (hand %.3f)\n", r2.y2, r2.k1, r3.y2, hand);
    CHECK(std::fabs(r3.y2 - hand) < 1e-9);
}

TEST(A1_abutment) {
    design::AbutmentInput in;
    const auto r = design::abutmentScour(in, UnitSystem::SI);
    const double fr = 0.5 / std::sqrt(9.80665 * 2);
    const double hand = 2 * (2.27 * std::pow(15.0, 0.43) * std::pow(fr, 0.61) + 1);
    CHECK(std::fabs(r.scour - hand) < 1e-9);
}

TEST(K1_calibration) {
    const Geometry g = channel(0.035);
    Geometry truth = g;
    design::scaleManning(truth, 1.3, 0);
    Plan p;
    p.stdStepTol = 1e-4;
    p.criticalTol = 1e-4;
    p.maxTrials = 60;
    const SteadyFlow f = flow(80);
    const auto rt = steady::solve(truth, f, p, UnitSystem::SI, {});
    CHECK(rt.status == RunStatus::Succeeded);
    std::vector<design::Observation> obs;
    for (const auto& x : rt.profiles[0].xs)
        if (x.rs == "2000" || x.rs == "1200" || x.rs == "600") obs.push_back({x.river, x.reach, x.rs, x.ws});
    const auto c = design::calibrateN(g, f, p, UnitSystem::SI, 0, obs, 0);
    std::printf("  factor %.4f, RMS %.4f -> %.4f m, %d runs\n", c.factor, c.rmsBefore, c.rmsAfter, c.runs);
    CHECK(c.ok && std::fabs(c.factor - 1.3) < 0.01 && c.rmsAfter < 0.005 && c.rmsAfter < c.rmsBefore);
}

TEST(Q1_water_quality) {
    const Geometry g = channel();
    UnsteadyFlow f;
    UnsteadyBoundary up;
    up.river = "S";
    up.reach = "R";
    up.rs = "2000";
    up.type = UBoundaryType::FlowHydrograph;
    up.values = {50, 50, 60, 70, 60, 50, 50, 50, 50, 50, 50, 50, 50};
    f.boundaries.push_back(up);
    UnsteadyBoundary dn;
    dn.river = "S";
    dn.reach = "R";
    dn.rs = "0";
    dn.type = UBoundaryType::NormalDepth;
    dn.slope = 0.001;
    dn.interval.clear();
    f.boundaries.push_back(dn);
    Plan p;
    p.flowFile = "u01";
    p.simStartDate = "01JAN2024";
    p.simStartTime = "0000";
    p.simEndDate = "01JAN2024";
    p.simEndTime = "1200";
    p.compInterval = "1MIN";
    p.outputInterval = "10MIN";
    const auto r = unsteady::solve(g, f, p, UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    design::WqInput in;
    in.initial = 0;
    in.upstream = {{0, 10}, {12, 10}};
    in.dispersion = 5;
    const auto w = design::waterQuality(g, r, in, UnitSystem::SI);
    CHECK(w.status == RunStatus::Succeeded);
    if (w.status != RunStatus::Succeeded) return;
    std::printf("  C downstream after %.1f h: %.3f mg/L\n", w.hours.back(), w.conc.back().back());
    CHECK(w.conc.back().back() > 9.5 && w.conc.back().back() < 10.01);
    const double bal = (w.massStored - w.massStored0) - (w.massIn - w.massOut - w.massDecayed + w.massCorrection);
    std::printf("  mass in %.4g, out %.4g, stored %.4g, correction %.3g, balance error %.3g\n", w.massIn, w.massOut, w.massStored, w.massCorrection, bal);
    CHECK(std::fabs(bal) < 1e-6 * std::max(1.0, w.massIn));
    // decay lowers the downstream concentration
    in.decayPerDay = 5;
    const auto w2 = design::waterQuality(g, r, in, UnitSystem::SI);
    CHECK(w2.conc.back().back() < w.conc.back().back());
    const double bal2 = (w2.massStored - w2.massStored0) - (w2.massIn - w2.massOut - w2.massDecayed + w2.massCorrection);
    CHECK(std::fabs(bal2) < 1e-6 * std::max(1.0, w2.massIn));
}

TEST(V1_units) {
    const Geometry g0 = channel();
    Geometry g = g0;
    design::convertGeometry(g, false);
    CHECK(std::fabs(g.reaches[0].nodes[0].staElev[1].elev - g0.reaches[0].nodes[0].staElev[1].elev / 0.3048) < 1e-9);
    design::convertGeometry(g, true);
    double maxd = 0;
    for (size_t i = 0; i < g.reaches[0].nodes.size(); ++i)
        for (size_t k = 0; k < g.reaches[0].nodes[i].staElev.size(); ++k)
            maxd = std::max(maxd, std::fabs(g.reaches[0].nodes[i].staElev[k].elev - g0.reaches[0].nodes[i].staElev[k].elev));
    CHECK(maxd < 1e-9);
    SteadyFlow f = flow(100);
    design::convertSteadyFlow(f, false);
    CHECK(std::fabs(f.changes[0].flows[0] - 100 / std::pow(0.3048, 3)) < 1e-6);
    // the same channel in feet carries the same flow at the same depth
    Plan p;
    const auto rs = steady::solve(g0, flow(80), p, UnitSystem::SI, {});
    Geometry gu = g0;
    design::convertGeometry(gu, false);
    SteadyFlow fu = flow(80);
    design::convertSteadyFlow(fu, false);
    Plan pu = p;
    design::convertPlan(pu, false);
    const auto ru = steady::solve(gu, fu, pu, UnitSystem::USCustomary, {});
    CHECK(rs.status == RunStatus::Succeeded && ru.status == RunStatus::Succeeded);
    if (rs.status == RunStatus::Succeeded && ru.status == RunStatus::Succeeded) {
        const double d = ru.profiles[0].xs[0].ws * 0.3048 - rs.profiles[0].xs[0].ws;
        std::printf("  upstream WS SI %.4f m, US %.4f ft (= %.4f m)\n", rs.profiles[0].xs[0].ws, ru.profiles[0].xs[0].ws, ru.profiles[0].xs[0].ws * 0.3048);
        CHECK(std::fabs(d) < 0.01);
    }
}

TEST(R1_recovery) {
    const auto dir = io::fs::temp_directory_path() / "deras_test_m8_prj";
    const auto rec = io::fs::temp_directory_path() / "deras_test_m8_rec";
    std::error_code ec;
    io::fs::remove_all(dir, ec);
    io::fs::remove_all(rec, ec);
    {
        auto p = io::Project::create(dir, "Rec", "Recovery", UnitSystem::SI, {});
        const std::string ge = p->addGeometry("G");
        p->geometries.at(ge).model = channel();
        MessageList m;
        CHECK(p->save(m));
    }
    MessageList m;
    auto p = io::Project::open(dir / "Rec.prj", m);
    CHECK(p != nullptr);
    if (!p) return;
    p->geometries.at("g01").model.reaches[0].nodes[3].manning[1].n = 0.041;
    p->settings.notes = "edited";
    CHECK(p->isModified());
    CHECK(p->writeRecovery(rec, m));
    auto q = io::Project::open(dir / "Rec.prj", m);
    CHECK(q && !q->isModified());
    CHECK(q->applyRecovery(rec, m));
    CHECK(q->isModified() && q->geometries.at("g01").model.reaches[0].nodes[3].manning[1].n == 0.041 && q->settings.notes == "edited");
    CHECK(q->save(m));
    auto z = io::Project::open(dir / "Rec.prj", m);
    CHECK(z && z->geometries.at("g01").model.reaches[0].nodes[3].manning[1].n == 0.041);
    io::fs::remove_all(dir, ec);
    io::fs::remove_all(rec, ec);
}

TEST(D1_docx) {
    CHECK(app::crc32("123456789") == 0xCBF43926ul);
    RunResult r;
    r.status = RunStatus::Succeeded;
    r.meta.units = UnitSystem::SI;
    r.profiles.push_back({"P1", {}});
    XsResult x;
    x.river = "S";
    x.reach = "R";
    x.rs = "100";
    x.ws = 101.234;
    r.profiles[0].xs.push_back(x);
    const auto blocks = app::runReportBlocks(r, Language::ID, "Proyek <uji> & co", "Plan 1");
    const std::string z = app::docxBytes(blocks);
    CHECK(z.compare(0, 4, std::string("PK\x03\x04", 4)) == 0);
    CHECK(z.find("word/document.xml") != std::string::npos && z.find("[Content_Types].xml") != std::string::npos);
    CHECK(z.find("Proyek &lt;uji&gt; &amp; co") != std::string::npos);
    CHECK(z.find("101.234") != std::string::npos);
    CHECK(z.compare(z.size() - 22, 4, std::string("PK\x05\x06", 4)) == 0);
}

TEST(V2_convert_project_copy) {
    const auto dir = io::fs::temp_directory_path() / "deras_test_m8_conv";
    std::error_code ec;
    io::fs::remove_all(dir, ec);
    {
        auto p = io::Project::create(dir, "Conv", "Conversion", UnitSystem::SI, {});
        const std::string ge = p->addGeometry("G");
        p->geometries.at(ge).model = channel();
        const std::string fe = p->addSteadyFlow("F");
        p->steadyFlows.at(fe).model = flow(80);
        p->addPlan("P", "P", ge, fe);
        SedimentSetup st;
        st.flow.records = {{24, 100, 24}};
        st.data.maxErodibleDepth = 2;
        st.data.gradation = {{1, 50}};
        io::setSediment(p->settings, st);
        MessageList m;
        CHECK(p->save(m));
    }
    MessageList m;
    std::vector<std::string> notes;
    CHECK(app::convertProjectCopy(dir / "Conv.prj", dir, "Conv_US", UnitSystem::USCustomary, m, notes));
    auto q = io::Project::open(dir / "Conv_US.prj", m);
    CHECK(q && q->units() == UnitSystem::USCustomary);
    if (!q) return;
    auto o = io::Project::open(dir / "Conv.prj", m);
    CHECK(o && o->units() == UnitSystem::SI);
    const double e0 = o->geometries.at("g01").model.reaches[0].nodes[0].staElev[0].elev;
    const double e1 = q->geometries.at("g01").model.reaches[0].nodes[0].staElev[0].elev;
    std::printf("  elev %.6f m -> %.6f ft, Q -> %.4f cfs\n", e0, e1, q->steadyFlows.at("f01").model.changes[0].flows[0]);
    CHECK(std::fabs(e1 * 0.3048 - e0) < 0.002);
    CHECK(std::fabs(q->steadyFlows.at("f01").model.changes[0].flows[0] * std::pow(0.3048, 3) - 80) < 0.01);
    const auto st = io::sedimentFor(q->settings);
    CHECK(std::fabs(st.data.maxErodibleDepth * 0.3048 - 2) < 1e-9 && std::fabs(st.flow.records[0].flow * std::pow(0.3048, 3) - 100) < 1e-6);
    // converting to the same system is refused
    MessageList m2;
    CHECK(!app::convertProjectCopy(dir / "Conv_US.prj", dir, "Conv_US2", UnitSystem::USCustomary, m2, notes));
    io::fs::remove_all(dir, ec);
}

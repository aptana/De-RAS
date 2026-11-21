// M5 tests (build 0.2.2): 1D unsteady flow.
//  T1 dates and intervals
//  U1 constant inflow stays at the steady backwater profile
//  U2 volume balance of a flood wave (inflow - outflow = storage change)
//  U3 flood peak travels at about the kinematic wave celerity 5/3 V
//  U4 downstream stage hydrograph with almost no flow fills the reach level
//  U5 junction: continuity and equal water surfaces at steady state
//  U6 lateral inflow adds to the downstream flow
//  F1 .u## parse / unchanged write byte-identical / edited block; plan unsteady lines
//  D1 Model Doctor for unsteady data
#include "TestMain.h"
#include "app/ModelDoctor.h"
#include "engines/common/XsHydraulics.h"
#include "engines/unsteady/UnsteadySolver.h"
#include "io/ProjectFiles.h"
#include <cstdio>
using namespace deras;

namespace {
CrossSection rect(const std::string& rs, double b, double z, double n, double len) {
    CrossSection xs;
    xs.rs = rs;
    xs.staElev = {{0, z + 10}, {0, z}, {b, z}, {b, z + 10}};
    xs.manning = {{0, n}};
    xs.bankLeft = 0;
    xs.bankRight = b;
    xs.lenLeft = xs.lenChannel = xs.lenRight = len;
    return xs;
}

// nX sections dx apart, bed slope s, width b; RS = distance from the downstream end
Reach channel(const std::string& river, const std::string& reach, int nX, double dx, double b, double s, double zDn, double n = 0.03) {
    Reach r;
    r.river = river;
    r.reach = reach;
    for (int i = 0; i < nX; ++i) {
        const double d = dx * (nX - 1 - i);
        r.nodes.push_back(rect(std::to_string(int(d + 0.5)), b, zDn + s * d, n, i + 1 < nX ? dx : 0));
    }
    return r;
}

UnsteadyBoundary hydro(const Reach& r, bool up, UBoundaryType t, std::vector<double> v, const std::string& interval = "1HOUR") {
    UnsteadyBoundary b;
    b.river = r.river;
    b.reach = r.reach;
    b.rs = up ? r.nodes.front().rs : r.nodes.back().rs;
    b.type = t;
    b.values = std::move(v);
    b.interval = interval;
    return b;
}

UnsteadyBoundary normal(const Reach& r, double s) {
    UnsteadyBoundary b;
    b.river = r.river;
    b.reach = r.reach;
    b.rs = r.nodes.back().rs;
    b.type = UBoundaryType::NormalDepth;
    b.slope = s;
    b.interval.clear();
    return b;
}

Plan uplan(const std::string& endDate, const std::string& endTime, const std::string& dt = "1MIN", const std::string& out = "10MIN") {
    Plan p;
    p.flowFile = "u01";
    p.simStartDate = "01JAN2024";
    p.simStartTime = "0000";
    p.simEndDate = endDate;
    p.simEndTime = endTime;
    p.compInterval = dt;
    p.outputInterval = out;
    p.theta = 1.0;
    p.unetZTol = 0.001;
    p.unetMaxIter = 30;
    p.stdStepTol = 1e-4;
    p.criticalTol = 1e-4;
    p.maxTrials = 60;
    return p;
}

int idx(const RunResult& r, const std::string& reach, const std::string& rs) {
    const auto& xs = r.profiles[0].xs;
    for (size_t i = 0; i < xs.size(); ++i) if (xs[i].reach == reach && xs[i].rs == rs) return int(i);
    return -1;
}

bool hasCode(const MessageList& m, const std::string& code) {
    for (const auto& x : m) if (x.code == code) return true;
    return false;
}

void dump(const RunResult& r) {
    for (const auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str());
}
}  // namespace

TEST(T1_dates_and_intervals) {
    CHECK_NEAR(rasDateTimeSeconds("01JAN2024", "0000"), 1704067200.0, 0.5);
    CHECK_NEAR(rasDateTimeSeconds("01JAN2024", "2400") - rasDateTimeSeconds("02JAN2024", "0000"), 0, 0.5);
    CHECK(rasDateText(rasDateTimeSeconds("29FEB2024", "1330")) == "29FEB2024");
    CHECK(rasTimeText(rasDateTimeSeconds("29FEB2024", "1330")) == "1330");
    CHECK(std::isnan(rasDateTimeSeconds("31XYZ2024", "0000")));
    CHECK_NEAR(intervalSeconds("15MIN"), 900, 1e-9);
    CHECK_NEAR(intervalSeconds("1HOUR"), 3600, 1e-9);
    CHECK_NEAR(intervalSeconds("1DAY"), 86400, 1e-9);
    CHECK_NEAR(intervalSeconds("30SEC"), 30, 1e-9);
    CHECK(intervalSeconds("abc") == 0);
    UnsteadyBoundary b;
    b.interval = "1HOUR";
    b.values = {10, 20, 40};
    CHECK_NEAR(unsteady::seriesValue(b, 0.5), 15, 1e-12);
    CHECK_NEAR(unsteady::seriesValue(b, 1.75), 35, 1e-12);
    CHECK_NEAR(unsteady::seriesValue(b, 9), 40, 1e-12);
}

TEST(U1_constant_inflow_steady) {
    Geometry g;
    g.reaches.push_back(channel("R", "A", 21, 250, 20, 0.0005, 100));
    UnsteadyFlow f;
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, {50, 50, 50, 50, 50, 50, 50}));
    f.boundaries.push_back(normal(g.reaches[0], 0.0005));
    auto r = unsteady::solve(g, f, uplan("01JAN2024", "0600"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { dump(r); return; }
    double maxDz = 0, maxDq = 0;
    for (size_t t = 0; t < r.series.hours.size(); ++t)
        for (size_t k = 0; k < r.series.ws[t].size(); ++k) {
            maxDz = std::max(maxDz, std::fabs(r.series.ws[t][k] - r.series.ws[0][k]));
            maxDq = std::max(maxDq, std::fabs(r.series.q[t][k] - 50));
        }
    CHECK(maxDz < 0.005);
    CHECK(maxDq < 0.2);
    CHECK(r.series.hours.size() == 37);
    CHECK(r.profiles.front().name == "Max WS");
    std::printf("  U1 max WS drift %.5f m, max Q drift %.4f m3/s\n", maxDz, maxDq);
}

TEST(U2_U3_flood_wave_volume_and_celerity) {
    const double b = 30, s = 0.0005, n = 0.03, dx = 500;
    const int nX = 41;  // 20 km
    Geometry g;
    g.reaches.push_back(channel("R", "A", nX, dx, b, s, 100, n));
    std::vector<double> hyd;
    for (int i = 0; i <= 48; ++i) {  // 30-minute steps: base 50, triangular peak 250 at hour 4, back to 50 at hour 10
        const double t = 0.5 * i;
        double q = 50;
        if (t > 1 && t <= 4) q = 50 + 200 * (t - 1) / 3;
        else if (t > 4 && t < 10) q = 250 - 200 * (t - 4) / 6;
        hyd.push_back(q);
    }
    UnsteadyFlow f;
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, hyd, "30MIN"));
    f.boundaries.push_back(normal(g.reaches[0], s));
    auto r = unsteady::solve(g, f, uplan("02JAN2024", "0000", "1MIN", "5MIN"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { dump(r); return; }
    const auto& T = r.series.hours;
    const size_t nT = T.size(), nS = r.series.ws[0].size();
    // U2: volume balance with the trapezoidal rule
    double vin = 0, vout = 0;
    for (size_t t = 1; t < nT; ++t) {
        const double h = (T[t] - T[t - 1]) * 3600;
        vin += 0.5 * h * (r.series.q[t][0] + r.series.q[t - 1][0]);
        vout += 0.5 * h * (r.series.q[t][nS - 1] + r.series.q[t - 1][nS - 1]);
    }
    auto storage = [&](size_t t) {
        double v = 0;
        for (size_t k = 0; k + 1 < nS; ++k) {
            const double d1 = r.series.ws[t][k] - (100 + s * dx * (nX - 1 - k)), d2 = r.series.ws[t][k + 1] - (100 + s * dx * (nX - 2 - k));
            v += 0.5 * b * (d1 + d2) * dx;
        }
        return v;
    };
    const double dS = storage(nT - 1) - storage(0);
    const double err = (vin - vout - dS) / (vin - 50 * 86400);
    CHECK(std::fabs(err) < 0.01);
    // U3: peak travel time between the sections at 15 km and 5 km from the outlet (10 km apart)
    auto peakTime = [&](size_t k) {
        size_t best = 0;
        for (size_t t = 0; t < nT; ++t) if (r.series.q[t][k] > r.series.q[best][k]) best = t;
        return T[best];
    };
    const size_t kUp = 10, kDn = 30;  // 15 km and 5 km from the outlet
    const double tt = (peakTime(kDn) - peakTime(kUp)) * 3600;
    // kinematic celerity at the attenuated peak flow (Manning, wide rectangle approximated with R = A/P)
    double qpk = 0;
    for (size_t t = 0; t < nT; ++t) qpk = std::max(qpk, r.series.q[t][(kUp + kDn) / 2]);
    // normal depth of the peak flow (Manning, bisection)
    double lo = 0.01, hi = 20;
    for (int it = 0; it < 200; ++it) {
        const double m = 0.5 * (lo + hi), A = b * m, P = b + 2 * m;
        if (A * std::pow(A / P, 2.0 / 3.0) * std::sqrt(s) / n < qpk) lo = m; else hi = m;
    }
    const double y = 0.5 * (lo + hi);
    const double A = b * y, P = b + 2 * y, V = qpk / A;
    // dQ/dA for Manning in a rectangle: c = V (5/3 - 4/3 * y/P)
    const double c = V * (5.0 / 3.0 - 4.0 / 3.0 * y / P);
    const double tKin = 10000 / c;
    CHECK(std::fabs(tt - tKin) / tKin < 0.2);
    std::printf("  U2 volume error %.2e %%; U3 peak travel %.0f s, kinematic %.0f s (peak Q %.1f)\n", err * 100, tt, tKin, qpk);
}

TEST(U4_downstream_stage_fills_reach) {
    Geometry g;
    g.reaches.push_back(channel("R", "A", 11, 200, 20, 0.0002, 100));
    UnsteadyFlow f;
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, {1, 1, 1, 1, 1, 1, 1, 1, 1}));
    f.boundaries.push_back(hydro(g.reaches[0], false, UBoundaryType::StageHydrograph, {101, 101.5, 102, 102, 102, 102, 102, 102, 102}));
    auto r = unsteady::solve(g, f, uplan("01JAN2024", "0800", "1MIN", "1HOUR"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { dump(r); return; }
    const auto& last = r.series.ws.back();
    CHECK_NEAR(last.back(), 102, 1e-6);
    CHECK_NEAR(last.front(), 102, 0.01);
    std::printf("  U4 upstream WS at the end %.4f (downstream 102)\n", last.front());
}

TEST(U5_junction_continuity) {
    Geometry g;
    Reach t1 = channel("Kiri", "Hulu", 6, 200, 10, 0.001, 101);
    Reach t2 = channel("Kanan", "Hulu", 6, 200, 12, 0.001, 101);
    Reach m = channel("Utama", "Hilir", 11, 200, 20, 0.0005, 100);
    for (auto* r : {&t1, &t2}) for (auto& n : r->nodes) n.rs = std::to_string(std::stoi(n.rs) + 3000);
    g.reaches = {t1, t2, m};
    Junction j;
    j.name = "J1";
    j.up = {{"Kiri", "Hulu"}, {"Kanan", "Hulu"}};
    j.dn = {{"Utama", "Hilir"}};
    j.lengths = {10, 10};
    g.junctions.push_back(j);
    g.junctionCount = 1;
    UnsteadyFlow f;
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, {20, 20, 20, 20}));
    f.boundaries.push_back(hydro(g.reaches[1], true, UBoundaryType::FlowHydrograph, {10, 30, 30, 30}));
    f.boundaries.push_back(normal(g.reaches[2], 0.0005));
    auto r = unsteady::solve(g, f, uplan("01JAN2024", "0300", "1MIN", "30MIN"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { dump(r); return; }
    const auto& q = r.series.q.back();
    const auto& w = r.series.ws.back();
    int k1 = -1, k2 = -1, k3 = idx(r, "Hilir", "2000");
    for (size_t i = 0; i < r.profiles[0].xs.size(); ++i) {
        const auto& x = r.profiles[0].xs[i];
        if (x.river == "Kiri" && x.rs == "3000") k1 = int(i);
        if (x.river == "Kanan" && x.rs == "3000") k2 = int(i);
    }
    CHECK(k1 >= 0 && k2 >= 0 && k3 >= 0);
    if (k1 < 0 || k2 < 0 || k3 < 0) return;
    CHECK_NEAR(q[size_t(k1)] + q[size_t(k2)], q[size_t(k3)], 0.05);
    CHECK_NEAR(q[size_t(k3)], 50, 0.5);
    CHECK_NEAR(w[size_t(k1)], w[size_t(k3)], 1e-6);
    CHECK_NEAR(w[size_t(k2)], w[size_t(k3)], 1e-6);
    std::printf("  U5 Q %.3f + %.3f = %.3f; WS %.4f\n", q[size_t(k1)], q[size_t(k2)], q[size_t(k3)], w[size_t(k3)]);
}

TEST(U6_lateral_inflow) {
    Geometry g;
    g.reaches.push_back(channel("R", "A", 11, 300, 15, 0.0005, 100));
    UnsteadyFlow f;
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, {30, 30, 30, 30, 30}));
    f.boundaries.push_back(normal(g.reaches[0], 0.0005));
    UnsteadyBoundary lat;
    lat.river = "R";
    lat.reach = "A";
    lat.rs = "1500";
    lat.type = UBoundaryType::LateralInflow;
    lat.values = {0, 12, 12, 12, 12};
    lat.interval = "1HOUR";
    f.boundaries.push_back(lat);
    auto r = unsteady::solve(g, f, uplan("01JAN2024", "0400", "1MIN", "1HOUR"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { dump(r); return; }
    const auto& q = r.series.q.back();
    CHECK_NEAR(q.front(), 30, 0.05);
    CHECK_NEAR(q.back(), 42, 0.3);
    CHECK_NEAR(r.series.q[0].back(), 30, 0.05);
    std::printf("  U6 downstream Q at the start %.3f, at the end %.3f\n", r.series.q[0].back(), q.back());
}

TEST(F1_unsteady_file_roundtrip) {
    const std::string text =
        "Flow Title=Banjir Uji\r\n"
        "Program Version=6.30\r\n"
        "Use Restart= 0 \r\n"
        "Initial Flow Loc=Sungai          ,Hulu            ,2000    ,40\r\n"
        "Boundary Location=Sungai          ,Hulu            ,2000    ,        ,                ,                ,                ,                \r\n"
        "Interval=1HOUR\r\n"
        "Flow Hydrograph= 12 \r\n"
        "      40      60     100     180     260     300     260     200     150     110\r\n"
        "      80      60\r\n"
        "Stage Hydrograph TW Check=0\r\n"
        "Flow Hydrograph QMult= 1 \r\n"
        "DSS Path=\r\n"
        "Use DSS=False\r\n"
        "Use Fixed Start Time=False\r\n"
        "Fixed Start Date/Time=,\r\n"
        "Is Critical Boundary=False\r\n"
        "Critical Boundary Flow=\r\n"
        "Boundary Location=Sungai          ,Hulu            ,1000    ,        ,                ,                ,                ,                \r\n"
        "Interval=1HOUR\r\n"
        "Lateral Inflow Hydrograph= 3 \r\n"
        "       0       5       0\r\n"
        "DSS Path=\r\n"
        "Use DSS=False\r\n"
        "Boundary Location=Sungai          ,Hulu            ,0       ,        ,                ,                ,                ,                \r\n"
        "Friction Slope=0.0008,0\r\n"
        "Met Point Raster Parameters=,,,,\r\n";
    io::UnsteadyFlowFile u;
    MessageList msgs;
    CHECK(u.parse(text, "uji.u01", msgs));
    CHECK(msgs.empty());
    CHECK(u.model.title == "Banjir Uji");
    CHECK(u.model.initialFlows.size() == 1 && u.model.initialFlows[0].q == 40 && u.model.initialFlows[0].rs == "2000");
    CHECK(u.model.boundaries.size() == 3);
    if (u.model.boundaries.size() != 3) return;
    const auto& b0 = u.model.boundaries[0];
    CHECK(b0.type == UBoundaryType::FlowHydrograph && b0.values.size() == 12 && b0.values[5] == 300 && b0.interval == "1HOUR");
    CHECK(b0.otherLines.size() == 8);
    CHECK(u.model.boundaries[1].type == UBoundaryType::LateralInflow && u.model.boundaries[1].values[1] == 5);
    CHECK(u.model.boundaries[2].type == UBoundaryType::NormalDepth && u.model.boundaries[2].slope == 0.0008);
    CHECK(!u.isModified());
    CHECK(u.serialize() == text);
    // edit one hydrograph value: the file is regenerated, other lines kept
    u.model.boundaries[0].values[5] = 320;
    const std::string out = u.serialize();
    CHECK(out.find("     320") != std::string::npos);
    CHECK(out.find("Flow Hydrograph QMult= 1 ") != std::string::npos);
    CHECK(out.find("Met Point Raster Parameters=,,,,") != std::string::npos);
    CHECK(out.find("Friction Slope=0.0008,0") != std::string::npos);
    io::UnsteadyFlowFile u2;
    CHECK(u2.parse(out, "uji.u01", msgs));
    CHECK(u2.model.boundaries[0].values[5] == 320);
    u2.model.boundaries[0].values[5] = 300;
    CHECK(u2.model == u.model ? false : true);
    // new file
    auto nf = io::UnsteadyFlowFile::createNew("Baru", "7.00");
    nf.model.boundaries.push_back(u.model.boundaries[2]);
    io::UnsteadyFlowFile u3;
    CHECK(u3.parse(nf.serialize(), "baru.u01", msgs));
    CHECK(u3.model.boundaries.size() == 1 && u3.model.boundaries[0].slope == 0.0008);
    // plan: unsteady lines written and read back
    Plan p = uplan("02JAN2024", "1200", "2MIN", "15MIN");
    p.title = "Uji";
    p.geomFile = "g01";
    p.theta = 0.8;
    auto pf = io::PlanFile::createNew(p, UnitSystem::SI);
    io::PlanFile pf2;
    CHECK(pf2.parse(pf.serialize(), "uji.p01", msgs));
    CHECK(pf2.model.simEndDate == "02JAN2024" && pf2.model.simEndTime == "1200" && pf2.model.compInterval == "2MIN" &&
          pf2.model.outputInterval == "15MIN" && pf2.model.theta == 0.8);
    CHECK(pf2.serialize() == pf.serialize());
}

TEST(D1_doctor_unsteady) {
    Geometry g;
    g.reaches.push_back(channel("R", "A", 6, 200, 10, 0.001, 100));
    UnsteadyFlow f;
    Plan p = uplan("01JAN2024", "0600");
    auto m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_U_BC_MISSING"));
    f.boundaries.push_back(hydro(g.reaches[0], true, UBoundaryType::FlowHydrograph, {10, 20}));
    f.boundaries.push_back(normal(g.reaches[0], 0.001));
    m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(!hasErrors(m));
    CHECK(hasCode(m, "MD_U_BC_SHORT"));
    f.boundaries[0].type = UBoundaryType::NormalDepth;
    m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_U_BC_TYPE"));
    f.boundaries[0].type = UBoundaryType::FlowHydrograph;
    p.simEndDate = "31DEC2023";
    m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_U_TIME"));
    p.simEndDate = "01JAN2024";
    p.outputInterval = "7MIN";
    p.compInterval = "2MIN";
    m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_U_INTERVAL"));
    p.outputInterval = "10MIN";
    CrossSection br;
    br.type = NodeType::Bridge;
    br.rs = "500";
    g.reaches[0].nodes.insert(g.reaches[0].nodes.begin() + 3, br);
    m = app::checkUnsteadyRun(p, &g, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_U_STRUCTURE"));
}

// M2 tests (build 0.2.2): supercritical and mixed regimes, junctions,
// ineffective flow areas, blocked obstructions and levees.
//  P1 supercritical uniform flow on a steep channel = normal depth
//  P2 supercritical S2 profile from critical depth, compared with RK4
//  P3 mixed regime on a mild channel = subcritical result
//  P4 mixed regime, steep -> mild slope break: hydraulic jump, momentum check
//  J1 confluence and J2 split through junctions (energy balance, flow continuity)
//  H1 ineffective areas, H2 blocked obstructions, H3 levees (section properties)
//  F1 parsing of junction, ineffective, obstruction and levee lines
#include "TestMain.h"
#include "engines/common/XsHydraulics.h"
#include "engines/steady/SteadySolver.h"
#include "io/GeometryFile.h"
#include <cstdio>
using namespace deras;

namespace {

struct Chan { double b, n, s0, q, g; };

double sfr(const Chan& c, double y) {
    double A = c.b * y, P = c.b + 2 * y, K = (1.0 / c.n) * A * std::pow(A / P, 2.0 / 3.0);
    return (c.q / K) * (c.q / K);
}
double yNormal(const Chan& c) {
    double lo = 1e-4, hi = 50;
    for (int i = 0; i < 200; ++i) { double m = 0.5 * (lo + hi); (sfr(c, m) > c.s0 ? lo : hi) = m; }
    return 0.5 * (lo + hi);
}
// depth at distance L downstream of a section with depth y0 (dy/dx = (S0 - Sf) / (1 - Fr^2))
double rk4Down(const Chan& c, double y0, double L, double h = 0.02) {
    auto f = [&](double y) { double V = c.q / (c.b * y), Fr2 = V * V / (c.g * y); return (c.s0 - sfr(c, y)) / (1 - Fr2); };
    double y = y0;
    int n = static_cast<int>(std::ceil(L / h));
    double dx = L / n;
    for (int i = 0; i < n; ++i) {
        double k1 = f(y), k2 = f(y + 0.5 * dx * k1), k3 = f(y + 0.5 * dx * k2), k4 = f(y + dx * k3);
        y += dx * (k1 + 2 * k2 + 2 * k3 + k4) / 6;
    }
    return y;
}

CrossSection rect(const std::string& rs, double b, double z, double n, double len) {
    CrossSection xs;
    xs.rs = rs;
    xs.staElev = {{0, z + 20}, {0, z}, {b, z}, {b, z + 20}};
    xs.manning = {{0, n}};
    xs.bankLeft = 0;
    xs.bankRight = b;
    xs.lenLeft = xs.lenChannel = xs.lenRight = len;
    xs.contraction = 0;
    xs.expansion = 0;
    return xs;
}

// Reach of nxs rectangular sections, spacing dx, bed elevation z(dist) given by `bed`.
Reach reach(const std::string& name, int nxs, double dx, double b, double n, const std::function<double(double)>& bed,
            double rsOffset = 0) {
    Reach r;
    r.river = "R";
    r.reach = name;
    for (int i = 0; i < nxs; ++i) {
        double dist = (nxs - 1 - i) * dx;
        r.nodes.push_back(rect(std::to_string(static_cast<int>(dist + rsOffset)), b, bed(dist), n, i + 1 < nxs ? dx : 0));
    }
    return r;
}

Plan plan(FlowRegime reg, double tol = 1e-5) {
    Plan p;
    p.regime = reg;
    p.stdStepTol = tol;
    p.criticalTol = tol;
    p.maxTrials = 60;
    p.maxErrorTol = 0.1;
    return p;
}

bool hasCode(const MessageList& m, const std::string& code) {
    for (const auto& x : m) if (x.code == code) return true;
    return false;
}

}  // namespace

TEST(P1_supercritical_uniform) {
    Chan c{10, 0.013, 0.02, 30, 9.81};
    double yn = yNormal(c);
    Geometry g;
    g.reaches.push_back(reach("A", 21, 50, c.b, c.n, [&](double d) { return c.s0 * d; }));
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "A", g.reaches[0].nodes.front().rs, {c.q}});
    auto& b = f.boundaryRef("R", "A", 0);
    b.upstream.type = BoundaryType::NormalDepth;
    b.upstream.slope = c.s0;
    auto r = steady::solve(g, f, plan(FlowRegime::Supercritical), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    double maxErr = 0, minFr = 1e9;
    for (const auto& x : r.profiles[0].xs) {
        maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - yn));
        minFr = std::min(minFr, x.froudeCh);
    }
    std::printf("  P1 normal depth %.4f m, max |error| %.5f m, min Fr %.2f\n", yn, maxErr, minFr);
    CHECK(maxErr < 0.001);
    CHECK(minFr > 1.0);
}

TEST(P2_supercritical_S2_profile) {
    Chan c{10, 0.013, 0.02, 30, 9.81};
    double yc = std::cbrt(3.0 * 3.0 / 9.81);
    Geometry g;
    g.reaches.push_back(reach("A", 81, 5, c.b, c.n, [&](double d) { return c.s0 * d; }));
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "A", g.reaches[0].nodes.front().rs, {c.q}});
    f.boundaryRef("R", "A", 0).upstream.type = BoundaryType::CriticalDepth;
    auto r = steady::solve(g, f, plan(FlowRegime::Supercritical), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    const auto& xs = r.profiles[0].xs;
    CHECK_NEAR(xs.front().ws - xs.front().minChEl, yc, 0.001);
    // anchor 50 m downstream of the critical section, compare further downstream
    const double total = 400;
    const XsResult* anchor = nullptr;
    for (const auto& x : xs) if (std::fabs(x.channelDistance - (total - 50)) < 1e-6) anchor = &x;
    CHECK(anchor != nullptr);
    double maxErr = 0;
    if (anchor)
        for (const auto& x : xs) {
            if (x.channelDistance >= total - 50) continue;
            double ref = rk4Down(c, anchor->ws - anchor->minChEl, (total - 50) - x.channelDistance);
            maxErr = std::max(maxErr, std::fabs((x.ws - x.minChEl) - ref));
        }
    std::printf("  P2 S2: max |WS - RK4| = %.5f m (yc %.3f, yn %.3f)\n", maxErr, yc, yNormal(c));
    CHECK(maxErr < 0.01);
}

static Geometry slopeBreak(const Chan& steep, const Chan& mild, int nUp, int nDn, double dx) {
    // upstream half steep, downstream half mild, one reach
    const double lenDn = (nDn - 1) * dx;
    Geometry g;
    g.reaches.push_back(reach("A", nUp + nDn - 1, dx, steep.b, steep.n, [&](double d) {
        return d <= lenDn ? mild.s0 * d : mild.s0 * lenDn + steep.s0 * (d - lenDn);
    }));
    return g;
}

TEST(P3_mixed_equals_subcritical_on_mild_channel) {
    Chan c{10, 0.03, 0.001, 30, 9.81};
    Geometry g;
    g.reaches.push_back(reach("A", 21, 100, c.b, c.n, [&](double d) { return c.s0 * d; }));
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "A", g.reaches[0].nodes.front().rs, {c.q}});
    auto& b = f.boundaryRef("R", "A", 0);
    b.downstream.type = BoundaryType::KnownWS;
    b.downstream.knownWS = 4.0;
    b.upstream.type = BoundaryType::CriticalDepth;
    auto rs = steady::solve(g, f, plan(FlowRegime::Subcritical), UnitSystem::SI, {});
    auto rm = steady::solve(g, f, plan(FlowRegime::Mixed), UnitSystem::SI, {});
    CHECK(rs.status == RunStatus::Succeeded && rm.status == RunStatus::Succeeded);
    if (rm.status != RunStatus::Succeeded || rs.status != RunStatus::Succeeded) return;
    double maxDiff = 0;
    for (size_t i = 0; i < rs.profiles[0].xs.size(); ++i)
        maxDiff = std::max(maxDiff, std::fabs(rs.profiles[0].xs[i].ws - rm.profiles[0].xs[i].ws));
    std::printf("  P3 mixed vs subcritical: max |dWS| = %.6f m\n", maxDiff);
    CHECK(maxDiff < 1e-9);
}

TEST(P4_mixed_hydraulic_jump) {
    Chan steep{10, 0.013, 0.02, 30, 9.81}, mild{10, 0.013, 0.0005, 30, 9.81};
    Geometry g = slopeBreak(steep, mild, 21, 41, 10);
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "A", g.reaches[0].nodes.front().rs, {30}});
    auto& b = f.boundaryRef("R", "A", 0);
    b.upstream.type = BoundaryType::CriticalDepth;
    b.downstream.type = BoundaryType::KnownWS;
    b.downstream.knownWS = 2.6;  // tailwater well above the sequent depth of the supercritical flow
    auto rm = steady::solve(g, f, plan(FlowRegime::Mixed), UnitSystem::SI, {});
    auto rs = steady::solve(g, f, plan(FlowRegime::Subcritical), UnitSystem::SI, {});
    CHECK(rm.status == RunStatus::Succeeded);
    if (rm.status != RunStatus::Succeeded) return;
    const auto& xs = rm.profiles[0].xs;
    CHECK(xs[3].supercritical && xs[3].froudeCh > 1.0);  // upstream: supercritical
    CHECK(!xs.back().supercritical && xs.back().froudeCh < 1.0);  // downstream: subcritical
    CHECK(hasCode(rm.messages, "SOLV_JUMP"));
    // At every section the chosen profile has the larger specific force, and downstream of
    // the jump the result equals the subcritical computation.
    size_t jump = 0;
    for (size_t i = 1; i < xs.size(); ++i)
        if (xs[i - 1].supercritical && !xs[i].supercritical) jump = i;
    CHECK(jump > 0);
    double maxDiff = 0;
    for (size_t i = jump; i < xs.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(xs[i].ws - rs.profiles[0].xs[i].ws));
    CHECK(maxDiff < 1e-9);
    std::printf("  P4 jump between RS %s and RS %s; Fr %.2f -> %.2f\n", xs[jump - 1].rs.c_str(), xs[jump].rs.c_str(),
                xs[jump - 1].froudeCh, xs[jump].froudeCh);
}

TEST(J1_confluence) {
    // two tributaries (20 + 30 m3/s) join the main reach (50 m3/s)
    const double n = 0.03, s0 = 0.001;
    Geometry g;
    g.reaches.push_back(reach("TribA", 6, 100, 8, n, [&](double d) { return 1.0 + s0 * d; }, 1000));
    g.reaches.push_back(reach("TribB", 6, 100, 8, n, [&](double d) { return 1.0 + s0 * d; }, 2000));
    g.reaches.push_back(reach("Main", 11, 100, 12, n, [&](double d) { return s0 * d; }));
    Junction j;
    j.name = "Pertemuan";
    j.up = {{"R", "TribA"}, {"R", "TribB"}};
    j.dn = {{"R", "Main"}};
    j.lengths = {50, 80};
    g.junctions.push_back(j);
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "TribA", g.reaches[0].nodes.front().rs, {20}});
    f.changes.push_back({"R", "TribB", g.reaches[1].nodes.front().rs, {30}});
    f.changes.push_back({"R", "Main", g.reaches[2].nodes.front().rs, {50}});
    auto& b = f.boundaryRef("R", "Main", 0);
    b.downstream.type = BoundaryType::NormalDepth;
    b.downstream.slope = s0;
    auto r = steady::solve(g, f, plan(FlowRegime::Subcritical), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    const auto& xs = r.profiles[0].xs;
    CHECK(xs.size() == 23u);
    const XsResult *lastA = nullptr, *lastB = nullptr, *firstMain = nullptr;
    for (const auto& x : xs) {
        if (x.reach == "TribA" && x.rs == "1000") lastA = &x;
        if (x.reach == "TribB" && x.rs == "2000") lastB = &x;
        if (x.reach == "Main" && x.rs == "1000") firstMain = &x;
    }
    CHECK(lastA && lastB && firstMain);
    if (!lastA || !lastB || !firstMain) return;
    // energy at each tributary end = main energy + friction over the junction length
    CHECK(lastA->eg > firstMain->eg && lastB->eg > firstMain->eg);
    CHECK(lastB->eg - firstMain->eg > lastA->eg - firstMain->eg);  // longer junction length -> larger loss
    CHECK_NEAR(lastA->eg - firstMain->eg, lastA->frictionLoss + lastA->ceLoss, 1e-4);
    CHECK(hasCode(r.messages, "SOLV_JUNCTION_DN"));
    std::printf("  J1 EG main %.4f, TribA %.4f, TribB %.4f\n", firstMain->eg, lastA->eg, lastB->eg);
}

TEST(J2_split) {
    const double n = 0.03, s0 = 0.001;
    Geometry g;
    g.reaches.push_back(reach("Up", 6, 100, 12, n, [&](double d) { return 2.0 + s0 * d; }, 3000));
    g.reaches.push_back(reach("Left", 11, 100, 8, n, [&](double d) { return 1.0 + s0 * d; }, 1000));
    g.reaches.push_back(reach("Right", 11, 100, 8, n, [&](double d) { return 1.0 + s0 * d; }, 2000));
    Junction j;
    j.name = "Percabangan";
    j.up = {{"R", "Up"}};
    j.dn = {{"R", "Left"}, {"R", "Right"}};
    j.lengths = {40, 40};
    g.junctions.push_back(j);
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "Up", g.reaches[0].nodes.front().rs, {50}});
    f.changes.push_back({"R", "Left", g.reaches[1].nodes.front().rs, {20}});
    f.changes.push_back({"R", "Right", g.reaches[2].nodes.front().rs, {30}});
    for (const char* rn : {"Left", "Right"}) {
        auto& b = f.boundaryRef("R", rn, 0);
        b.downstream.type = BoundaryType::NormalDepth;
        b.downstream.slope = s0;
    }
    auto r = steady::solve(g, f, plan(FlowRegime::Subcritical), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    const XsResult *lastUp = nullptr, *firstL = nullptr, *firstR = nullptr;
    for (const auto& x : r.profiles[0].xs) {
        if (x.reach == "Up" && x.rs == "3000") lastUp = &x;
        if (x.reach == "Left" && x.rs == "2000") firstL = &x;
        if (x.reach == "Right" && x.rs == "3000") firstR = &x;
    }
    CHECK(lastUp && firstL && firstR);
    if (!lastUp || !firstL || !firstR) return;
    // the upstream reach takes the higher of the two energy balances
    CHECK(lastUp->eg >= firstL->eg && lastUp->eg >= firstR->eg);
    std::printf("  J2 EG up %.4f, left %.4f, right %.4f\n", lastUp->eg, firstL->eg, firstR->eg);
}

static CrossSection compound() {
    CrossSection xs;
    xs.rs = "1";
    xs.staElev = {{0, 104}, {40, 102}, {50, 100}, {60, 100}, {70, 102}, {110, 104}};
    xs.manning = {{0, 0.06}, {50, 0.035}, {60, 0.06}};
    xs.bankLeft = 50;
    xs.bankRight = 60;
    return xs;
}

TEST(H1_ineffective_areas) {
    auto plain = compound();
    auto ie = compound();
    ie.ineffective = {{0, 40, 103.0, false}, {70, 110, 103.0, false}};
    auto gp = hyd::XsGeom::from(plain, UnitSystem::SI);
    auto gi = hyd::XsGeom::from(ie, UnitSystem::SI);
    // below the ineffective elevation only stations 40..70 carry flow
    auto pi = hyd::properties(gi, 102.8);
    double a, p, t;
    hyd::wettedSegment(gp.pts, 40, 70, 102.8, a, p, t);
    CHECK_NEAR(pi.area, a, 1e-9);
    CHECK_NEAR(pi.perimeter, p, 1e-9);
    CHECK_NEAR(pi.topWidth, t, 1e-9);
    CHECK(pi.area < hyd::properties(gp, 102.8).area);
    // above it the areas become effective again
    CHECK_NEAR(hyd::properties(gi, 103.5).conveyance, hyd::properties(gp, 103.5).conveyance, 1e-9);
    // permanent ones stay ineffective
    ie.ineffective[0].permanent = ie.ineffective[1].permanent = true;
    auto gpm = hyd::XsGeom::from(ie, UnitSystem::SI);
    hyd::wettedSegment(gp.pts, 40, 70, 103.5, a, p, t);
    CHECK_NEAR(hyd::properties(gpm, 103.5).area, a, 1e-9);
}

TEST(H2_blocked_obstruction) {
    auto xs = compound();
    xs.obstructions = {{0, 30, 103.5, false}};
    auto g = hyd::XsGeom::from(xs, UnitSystem::SI);
    // equivalent ground: raised to 103.5 between 0 and 30 (ground there is 104..102.5)
    auto ref = compound();
    ref.staElev = {{0, 104}, {20, 103}, {30, 103.5}, {30, 102.5}, {40, 102}, {50, 100}, {60, 100}, {70, 102}, {110, 104}};
    // the reference keeps the part of the ground above 103.5 (0..10 m, 104 -> 103.5)
    ref.staElev = {{0, 104}, {10, 103.5}, {30, 103.5}, {30, 102.5}, {40, 102}, {50, 100}, {60, 100}, {70, 102}, {110, 104}};
    auto gr = hyd::XsGeom::from(ref, UnitSystem::SI);
    for (double ws : {101.0, 103.0, 103.8}) {
        auto a = hyd::properties(g, ws), b = hyd::properties(gr, ws);
        CHECK_NEAR(a.area, b.area, 1e-9);
        CHECK_NEAR(a.perimeter, b.perimeter, 1e-9);
        CHECK_NEAR(a.conveyance, b.conveyance, 1e-6);
    }
}

TEST(H3_levee) {
    auto plain = compound();
    auto lv = compound();
    lv.leveeLeft = {true, 45, 103.0};
    auto gp = hyd::XsGeom::from(plain, UnitSystem::SI);
    auto gl = hyd::XsGeom::from(lv, UnitSystem::SI);
    // below the levee crest nothing left of station 45 carries flow
    auto p = hyd::properties(gl, 102.5);
    double a, pe, t;
    hyd::wettedSegment(gp.pts, 45, 110, 102.5, a, pe, t);
    CHECK_NEAR(p.area, a, 1e-9);
    CHECK_NEAR(p.topWidth, t, 1e-9);
    // the levee face adds wetted perimeter (102.5 - ground at 45 = 101.0)
    CHECK_NEAR(p.perimeter, pe + 1.5, 1e-9);
    // above the crest the left overbank flows again (the zero-width levee adds no area)
    CHECK_NEAR(hyd::properties(gl, 103.5).area, hyd::properties(gp, 103.5).area, 1e-9);
}

TEST(F1_parse_m2_lines) {
    const std::string g =
        "Geom Title=M2\r\nProgram Version=6.60\r\n\r\n"
        "River Reach=R               ,A               \r\nReach XY= 2 \r\n               0               0             100               0\r\n\r\n"
        "Type RM Length L Ch R = 1 ,100     ,10,10,10\r\n"
        "#Sta/Elev= 4 \r\n       0     104      40     100      60     100     100     104\r\n"
        "#Mann= 1 , 0 , 0 \r\n       0     .04       0\r\n"
        "Bank Sta=40,60\r\n"
        "#XS Ineff= 2 ,-1 \r\n       0      30   102.5      70     100   102.5\r\n"
        "Permanent Ineff=\r\n       F       T\r\n"
        "#Block Obstruct= 1 , 0 \r\n      80      90     103\r\n"
        "Levee=-1,35,103.5,-1,65,103.8,,,,\r\n"
        "Exp/Cntr=0.3,0.1\r\n\r\n"
        "Junct Name=J1              \r\nJunct Desc=, 0 , 0 ,-1 ,0\r\n"
        "Up River,Reach=R               ,A               \r\n"
        "Up River,Reach=R               ,B               \r\n"
        "Dn River,Reach=R               ,C               \r\n"
        "Junc L&A=120,0\r\nJunc L&A=75.5,0\r\n\r\n";
    io::GeometryFile gf;
    MessageList m;
    gf.parse(g, "t.g01", m);
    CHECK(gf.serialize() == g);
    const auto& x = gf.model.reaches.at(0).nodes.at(0);
    CHECK(x.ineffective.size() == 2 && x.ineffectiveMode == -1);
    CHECK(x.ineffective[1].staStart == 70 && x.ineffective[1].elev == 102.5 && x.ineffective[1].permanent && !x.ineffective[0].permanent);
    CHECK(x.obstructions.size() == 1 && x.obstructions[0].staEnd == 90 && x.obstructions[0].elev == 103);
    CHECK(x.leveeLeft.present && x.leveeLeft.station == 35 && x.leveeLeft.elev == 103.5);
    CHECK(x.leveeRight.present && x.leveeRight.station == 65 && x.leveeRight.elev == 103.8);
    CHECK(x.ignoredFeatures.empty());
    CHECK(gf.model.junctions.size() == 1);
    const auto& j = gf.model.junctions[0];
    CHECK(j.name == "J1" && j.up.size() == 2 && j.dn.size() == 1 && j.dn[0].second == "C");
    CHECK(j.lengthTo("R", "B") == 75.5 && j.lengthTo("R", "A") == 120);
}

TEST(F2_write_m2_lines) {
    const std::string g =
        "Geom Title=M2\r\nProgram Version=6.60\r\n\r\n"
        "River Reach=R               ,A               \r\nReach XY= 2 \r\n               0               0             100               0\r\n\r\n"
        "Type RM Length L Ch R = 1 ,100     ,10,10,10\r\n"
        "#Sta/Elev= 4 \r\n       0     104      40     100      60     100     100     104\r\n"
        "#Mann= 1 , 0 , 0 \r\n       0     .04       0\r\n"
        "Bank Sta=40,60\r\n"
        "Exp/Cntr=0.3,0.1\r\n\r\n";
    io::GeometryFile gf;
    MessageList m;
    gf.parse(g, "t.g01", m);
    auto& x = gf.model.reaches[0].nodes[0];
    x.ineffective = {{0, 30, 102.5, false}, {70, 100, 102.75, true}};
    x.ineffectiveMode = -1;
    x.obstructions = {{80, 90, 103, false}};
    x.leveeRight = {true, 65, 103.8};
    const std::string out = gf.serialize();
    io::GeometryFile back;
    back.parse(out, "t.g01", m);
    const auto& y = back.model.reaches[0].nodes[0];
    CHECK(y.ineffective.size() == 2 && y.ineffective[1].elev == 102.75 && y.ineffective[1].permanent && !y.ineffective[0].permanent);
    CHECK(y.ineffectiveMode == -1);
    CHECK(y.obstructions.size() == 1 && y.obstructions[0].staStart == 80);
    CHECK(!y.leveeLeft.present && y.leveeRight.present && y.leveeRight.station == 65 && y.leveeRight.elev == 103.8);
    CHECK(y.sameData(x) || (y.staElev.size() == x.staElev.size()));
    // removing them again removes the lines
    back.model.reaches[0].nodes[0].ineffective.clear();
    back.model.reaches[0].nodes[0].obstructions.clear();
    back.model.reaches[0].nodes[0].leveeRight = {};
    const std::string out2 = back.serialize();
    CHECK(out2.find("#XS Ineff=") == std::string::npos && out2.find("Permanent Ineff=") == std::string::npos);
    CHECK(out2.find("#Block Obstruct=") == std::string::npos && out2.find("Levee=") == std::string::npos);
}

TEST(F3_edit_junctions) {
    const std::string g =
        "Geom Title=J\r\nProgram Version=6.60\r\n\r\n"
        "River Reach=R               ,A               \r\nReach XY= 2 \r\n               0             100               0               0\r\n\r\n"
        "River Reach=R               ,C               \r\nReach XY= 2 \r\n               0               0               0            -100\r\n\r\n"
        "Junct Name=J1              \r\nJunct Desc=, 0 , 0 ,-1 ,0\r\nJunct X Y & Text X Y=0,0,0,0\r\n"
        "Up River,Reach=R               ,A               \r\n"
        "Dn River,Reach=R               ,C               \r\n"
        "Junc L&A=120,0\r\n\r\n"
        "Chan Stop Cuts=-1\r\n";
    io::GeometryFile gf;
    MessageList m;
    gf.parse(g, "t.g01", m);
    CHECK(!gf.isModified());
    CHECK(gf.serialize() == g);
    gf.model.junctions[0].lengths = {80};
    std::string out = gf.serialize();
    CHECK(out.find("Junc L&A=80,0") != std::string::npos);
    CHECK(out.find("Junct X Y & Text X Y=0,0,0,0") != std::string::npos);  // kept
    CHECK(out.find("Chan Stop Cuts=-1") != std::string::npos);
    io::GeometryFile back;
    back.parse(out, "t.g01", m);
    CHECK(back.model.junctions.size() == 1 && back.model.junctions[0].lengths == std::vector<double>{80});
    // a new junction written from scratch
    back.model.junctions.push_back({"J2", {{"R", "C"}}, {{"R", "A"}}, {15}, {}, {}});
    io::GeometryFile again;
    again.parse(back.serialize(), "t.g01", m);
    CHECK(again.model.junctions.size() == 2 && again.model.junctions[1].name == "J2" && again.model.junctions[1].lengths[0] == 15);
    // deleting all junctions removes the blocks
    again.model.junctions.clear();
    CHECK(again.serialize().find("Junct Name=") == std::string::npos);
}

#include "io/ResultsHdf.h"
#include <cstdlib>
#include <fstream>

TEST(R1_steady_results_hdf) {
    // results file with the HEC-RAS layout; checked with h5dump when it is installed
    RunResult r;
    r.status = RunStatus::Succeeded;
    for (int p = 0; p < 2; ++p) {
        ProfileResult pr;
        pr.name = p ? "Q150" : "Q20";
        for (int i = 0; i < 3; ++i) {
            XsResult x;
            x.river = "Sungai Uji"; x.reach = "Utama"; x.rs = std::to_string(1000 - 500 * i);
            x.ws = 100 + p + 0.25 * i; x.q = p ? 150 : 20; x.eg = x.ws + 0.1;
            pr.xs.push_back(x);
        }
        r.profiles.push_back(pr);
    }
    Plan plan; plan.title = "Plan"; plan.shortId = "P"; plan.geomFile = "g01"; plan.flowFile = "f01";
    const std::string bytes = io::steadyResultsHdf(r, plan, UnitSystem::SI, "test");
    CHECK(bytes.substr(0, 8) == std::string("\x89HDF\r\n\x1a\n", 8));
    const auto dir = io::fs::temp_directory_path() / "deras_hdf_test";
    io::fs::create_directories(dir);
    const auto path = dir / "T.p01.hdf";
    std::string err;
    CHECK(io::writeSteadyHdf(path, r, plan, UnitSystem::SI, "test", err));
    CHECK(io::hdfWritableByDeras(path));
    if (std::system("h5dump --version > /dev/null 2>&1") == 0) {
        const auto out = dir / "dump.txt";
        const std::string cmd = "h5dump -d \"/Results/Steady/Output/Output Blocks/Base Output/Steady Profiles/Cross Sections/Water Surface\" \"" +
                                path.string() + "\" > \"" + out.string() + "\" 2>&1";
        CHECK(std::system(cmd.c_str()) == 0);
        std::ifstream f(out);
        std::string dump((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        CHECK(dump.find("( 2, 3 )") != std::string::npos);
        CHECK(dump.find("100.5") != std::string::npos && dump.find("101.25") != std::string::npos);
        std::printf("  R1 h5dump read the results file (%zu bytes)\n", bytes.size());
    } else {
        std::printf("  R1 h5dump not installed: structure check skipped\n");
    }
    // a file from another program is never overwritten
    io::writeFileAtomic(path, "HEC-RAS results", err);
    CHECK(!io::hdfWritableByDeras(path));
    io::fs::remove_all(dir);
}

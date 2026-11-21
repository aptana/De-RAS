// M4 tests (build 0.2.2): bridges, culverts and inline structures.
//  W1 weir equation and Villemonte submergence
//  B1 bridge without piers, deck high above the water: same as the open channel
//  B2 bridge with piers: Yarnell loss is applied (highest energy)
//  B3 bridge pressure flow: the upstream energy satisfies Q = Cd A sqrt(2g h) + weir
//  C1 culvert inlet control (FHWA form 1) and C2 outlet control (full flow), checked by hand formulas
//  C3 culvert in a channel: solver result equals the barrel equations
//  I1 inline weir without gates: EG = crest + (Q / C L)^(2/3); I2 sluice gate
//  F1 structures written and read back unchanged; unchanged files are byte-identical
//  D1 Model Doctor: structures need the subcritical regime and two sections around them
#include "TestMain.h"
#include "app/ModelDoctor.h"
#include "engines/common/XsHydraulics.h"
#include "engines/steady/SteadySolver.h"
#include "engines/steady/Structures.h"
#include "io/GeometryFile.h"
#include <cstdio>
using namespace deras;

namespace {
const double g = 9.81;

CrossSection rect(const std::string& rs, double b, double z, double n, double len) {
    CrossSection xs;
    xs.rs = rs;
    xs.staElev = {{0, z + 20}, {0, z}, {b, z}, {b, z + 20}};
    xs.manning = {{0, n}};
    xs.bankLeft = 0;
    xs.bankRight = b;
    xs.lenLeft = xs.lenChannel = xs.lenRight = len;
    xs.contraction = 0.1;
    xs.expansion = 0.3;
    return xs;
}

// 7 rectangular sections 100 m apart (RS 600..0), bed slope 0.001, width b;
// a structure node is inserted between RS 400 and RS 300 when given.
Geometry channel(double b, const CrossSection* structure) {
    Geometry gm;
    Reach r;
    r.river = "R";
    r.reach = "A";
    for (int i = 0; i < 7; ++i) {
        const double d = 600 - 100.0 * i;
        r.nodes.push_back(rect(std::to_string(int(d)), b, 100 + 0.001 * d, 0.03, i < 6 ? 100 : 0));
        if (structure && d == 400) r.nodes.push_back(*structure);
    }
    gm.reaches.push_back(r);
    return gm;
}

SteadyFlow flowOf(double q, double dnWs = 0) {
    SteadyFlow f;
    f.profileNames = {"PF1"};
    f.changes.push_back({"R", "A", "600", {q}});
    auto& b = f.boundaryRef("R", "A", 0);
    if (dnWs > 0) { b.downstream.type = BoundaryType::KnownWS; b.downstream.knownWS = dnWs; }
    else { b.downstream.type = BoundaryType::NormalDepth; b.downstream.slope = 0.001; }
    return f;
}

Plan plan() {
    Plan p;
    p.stdStepTol = 1e-5;
    p.criticalTol = 1e-5;
    p.maxTrials = 60;
    return p;
}

const XsResult* at(const RunResult& r, const std::string& rs) {
    for (const auto& x : r.profiles[0].xs) if (x.rs == rs) return &x;
    return nullptr;
}

bool hasCode(const MessageList& m, const std::string& code) {
    for (const auto& x : m) if (x.code == code) return true;
    return false;
}

CrossSection bridge(double b, double low, double high, bool piers) {
    CrossSection n;
    n.type = NodeType::Bridge;
    n.rs = "350";
    auto& sd = n.structure;
    sd.parsed = true;
    sd.dist = 40;
    sd.width = 10;
    sd.weirCoef = 1.44;
    sd.deckUp = {{-1, high, low}, {b + 1, high, low}};
    sd.deckDn = sd.deckUp;
    if (piers)
        for (double s : {b / 3, 2 * b / 3}) {
            Pier p;
            p.upSta = p.dnSta = s;
            p.upWidth = p.dnWidth = {1.5, 1.5};
            p.upElev = p.dnElev = {90, 120};
            sd.piers.push_back(p);
        }
    return n;
}
}  // namespace

TEST(W1_weir_and_submergence) {
    std::vector<std::pair<double, double>> crest{{0, 100}, {10, 100}};
    CHECK_NEAR(steady::st::weirFlow(crest, 101, 0, 1.44), 14.4, 1e-6);
    const double f = std::pow(1 - std::pow(0.5, 1.5), 0.385);
    CHECK_NEAR(steady::st::weirFlow(crest, 101, 100.5, 1.44), 14.4 * f, 1e-6);
    CHECK_NEAR(steady::st::weirFlow(crest, 101, 0, 1.44, {{4, 6}}), 14.4 * 0.8, 1e-6);  // 2 m gate gap
}

TEST(B1_bridge_high_deck_equals_open_channel) {
    const double b = 12, q = 40;
    auto open = steady::solve(channel(b, nullptr), flowOf(q), plan(), UnitSystem::SI, {});
    CrossSection br = bridge(b, 130, 131, false);
    auto withB = steady::solve(channel(b, &br), flowOf(q), plan(), UnitSystem::SI, {});
    CHECK(open.status == RunStatus::Succeeded && withB.status == RunStatus::Succeeded);
    if (withB.status != RunStatus::Succeeded) { for (auto& m : withB.messages) std::printf("  %s\n", m.text(Language::EN).c_str()); return; }
    const XsResult *a = at(open, "400"), *c = at(withB, "400");
    CHECK(a && c);
    if (!a || !c) return;
    CHECK_NEAR(c->ws, a->ws, 0.005);
    CHECK(hasCode(c->notes, "SOLV_BRIDGE"));
    std::printf("  B1 WS open %.4f, with bridge %.4f\n", a->ws, c->ws);
}

TEST(B2_bridge_piers_yarnell) {
    const double b = 12, q = 40;
    CrossSection br = bridge(b, 130, 131, true);
    auto r = steady::solve(channel(b, &br), flowOf(q), plan(), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) return;
    const XsResult *u = at(r, "400"), *d = at(r, "300");
    // Yarnell by hand at the downstream section
    const double y2 = d->ws - (100 + 0.3), A2 = b * y2, V2 = q / A2, alpha = 2 * 1.5 * y2 / A2, w = V2 * V2 / (2 * g) / y2, K = 1.25;
    const double h3 = 2 * K * (K + 10 * w - 0.6) * (alpha + 15 * std::pow(alpha, 4)) * V2 * V2 / (2 * g);
    CHECK(u->ws >= d->ws + h3 - 1e-6);
    std::printf("  B2 WS dn %.4f, up %.4f, Yarnell H3 %.4f\n", d->ws, u->ws, h3);
}

TEST(B3_bridge_pressure_flow) {
    const double b = 12, q = 120;
    CrossSection br = bridge(b, 101.8, 103.0, false);
    auto r = steady::solve(channel(b, &br), flowOf(q), plan(), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { for (auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str()); return; }
    const XsResult *u = at(r, "400"), *d = at(r, "300");
    // recompute the flow through the opening at the solved upstream energy
    CrossSection up = rect("400", b, 100.4, 0.03, 100);
    const auto op = steady::st::bridgeOpening(up, br.structure.deckUp, {}, true);
    const double Z = op.maxLowChord - op.meanBed;
    const double qp = d->ws > 101.8 ? 0.8 * op.area * std::sqrt(2 * g * (u->eg - d->ws))
                                    : steady::st::sluiceCd((u->eg - op.meanBed) / Z) * op.area * std::sqrt(2 * g * (u->eg - op.meanBed - Z / 2));
    const double qw = steady::st::weirFlow({{-1, 103.0}, {b + 1, 103.0}}, u->eg, d->ws, 1.44);
    CHECK_NEAR(qp + qw, q, 0.01);
    CHECK(u->eg > 101.8);
    std::printf("  B3 EG up %.4f, WS dn %.4f, pressure %.2f + weir %.2f m3/s\n", u->eg, d->ws, qp, qw);
}

TEST(C1_C2_culvert_controls) {
    CulvertGroup c;
    c.shape = 1;
    c.rise = c.span = 1.2;
    c.length = 30;
    c.n = 0.013;
    c.ke = 0.5;
    c.kx = 1.0;
    c.chart = 1;
    c.scale = 1;
    c.upInvert = 100.3;
    c.dnInvert = 100.0;
    // C1: small flow, low tailwater -> inlet control, unsubmerged form 1
    const double q = 0.8, D = 1.2, A = 3.14159265358979323846 * D * D / 4, S = 0.3 / 30;
    auto s = steady::st::culvertEnergy(c, q, 100.2, 0, UnitSystem::SI, g);
    const double dc = steady::st::barrelCriticalDepth(c, q, g);
    double ac, tc, pc;
    steady::st::barrelSection(c, dc, ac, tc, pc);
    CHECK_NEAR(q * q * tc / (g * ac * ac * ac), 1.0, 1e-4);  // critical depth
    const double X = 1.811 * q / (A * std::sqrt(D));
    const double hw = (dc + q * q / (2 * g * ac * ac)) / D + 0.0098 * X * X - 0.5 * S;
    CHECK(X < 3.5);
    CHECK_NEAR(s.hwInlet, 100.3 + hw * D, 1e-6);
    CHECK(s.inletControls);
    CHECK_NEAR(steady::st::culvertFlow(c, s.egUp, 100.2, 0, UnitSystem::SI, g), q, 1e-5);
    // C2: high tailwater (above the crown) -> outlet control, full flow
    const double q2 = 2.5, tw = 102.0, V = q2 / A, hv = V * V / (2 * g), R = D / 4;
    const double sf = std::pow(q2 * 0.013 / (A * std::pow(R, 2.0 / 3.0)), 2);
    auto s2 = steady::st::culvertEnergy(c, q2, tw, 0.01, UnitSystem::SI, g);
    CHECK_NEAR(s2.hwOutlet, tw + 0.5 * hv + 30 * sf + 1.0 * (hv - 0.01) + 0.01, 1e-6);
    CHECK(!s2.inletControls);
    std::printf("  C1 HW inlet %.4f; C2 EG outlet %.4f\n", s.hwInlet, s2.hwOutlet);
}

TEST(C3_culvert_in_channel) {
    const double b = 12, q = 6;
    CrossSection cn;
    cn.type = NodeType::Culvert;
    cn.rs = "350";
    cn.structure.parsed = true;
    cn.structure.dist = 10;
    cn.structure.width = 20;
    cn.structure.weirCoef = 1.44;
    cn.structure.deckUp = {{-1, 106, 106}, {b + 1, 106, 106}};
    cn.structure.deckDn = cn.structure.deckUp;
    CulvertGroup c;
    c.shape = 2;
    c.span = 2;
    c.rise = 1.5;
    c.length = 20;
    c.upInvert = 100.4;
    c.dnInvert = 100.38;
    c.chart = 8;
    c.scale = 1;
    c.upStations = {4, 8};
    c.dnStations = {4, 8};
    cn.structure.culverts = {c};
    auto r = steady::solve(channel(b, &cn), flowOf(q), plan(), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { for (auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str()); return; }
    const XsResult *u = at(r, "400"), *d = at(r, "300");
    CrossSection dn = rect("300", b, 100.3, 0.03, 100);
    const auto pd = hyd::properties(hyd::XsGeom::from(dn, UnitSystem::SI), d->ws);
    const double hv2 = std::pow(q / pd.area, 2) / (2 * g);
    const double qb = steady::st::culvertFlow(c, u->eg, d->ws, hv2, UnitSystem::SI, g);
    CHECK_NEAR(2 * qb, q, 0.01);
    CHECK(hasCode(u->notes, "SOLV_CULVERT"));
    std::printf("  C3 EG up %.4f, WS dn %.4f\n", u->eg, d->ws);
}

TEST(I1_I2_inline_weir_and_gate) {
    const double b = 12, q = 20;
    CrossSection in;
    in.type = NodeType::InlineStructure;
    in.rs = "350";
    in.structure.parsed = true;
    in.structure.width = 5;
    in.structure.weirCoef = 1.7;
    in.structure.weir = {{0, 104}, {b, 104}};
    auto r = steady::solve(channel(b, &in), flowOf(q), plan(), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    if (r.status != RunStatus::Succeeded) { for (auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str()); return; }
    const XsResult* u = at(r, "400");
    CHECK_NEAR(u->eg, 104 + std::pow(q / (1.7 * b), 2.0 / 3.0), 1e-4);
    CHECK(hasCode(u->notes, "SOLV_INLINE"));
    // I2: one sluice gate 3 m wide opened 0.5 m at invert 100.4, weir crest high
    in.structure.weir = {{0, 110}, {b, 110}};
    GateGroup gt;
    gt.name = "Pintu";
    gt.width = 3;
    gt.height = 2;
    gt.invert = 100.4;
    gt.coef = 0.6;
    gt.stations = {6};
    gt.opening = 0.5;
    in.structure.gates = {gt};
    auto r2 = steady::solve(channel(b, &in), flowOf(5), plan(), UnitSystem::SI, {});
    CHECK(r2.status == RunStatus::Succeeded);
    if (r2.status != RunStatus::Succeeded) return;
    const XsResult* u2 = at(r2, "400");
    const double H = std::pow(5 / (0.6 * 3 * 0.5), 2) / (2 * g);
    CHECK_NEAR(u2->eg, 100.4 + H, 1e-4);
    std::printf("  I1 EG %.4f; I2 EG %.4f (sluice head %.3f)\n", u->eg, u2->eg, H);
}

TEST(F1_structure_file_roundtrip) {
    auto gf = io::GeometryFile::createNew("Uji", "7.00");
    CrossSection br = bridge(12, 101.8, 103, true);
    CrossSection cn = br;
    cn.type = NodeType::Culvert;
    cn.rs = "250";
    cn.structure.piers.clear();
    CulvertGroup c;
    c.name = "Box";
    c.shape = 2;
    c.span = 2;
    c.rise = 1.5;
    c.length = 20;
    c.upInvert = 100.4;
    c.dnInvert = 100.38;
    c.chart = 8;
    c.scale = 1;
    c.upStations = {4, 8};
    c.dnStations = {4, 8};
    cn.structure.culverts = {c};
    CrossSection in;
    in.type = NodeType::InlineStructure;
    in.rs = "150";
    in.structure.parsed = true;
    in.structure.dist = 5;
    in.structure.width = 5;
    in.structure.weirCoef = 1.7;
    in.structure.weir = {{0, 104}, {6, 103.5}, {12, 104}};
    GateGroup gt;
    gt.name = "Pintu 1";
    gt.width = 3;
    gt.height = 2;
    gt.invert = 100.4;
    gt.stations = {3, 9};
    in.structure.gates = {gt};
    Geometry gm = channel(12, &br);
    auto& nodes = gm.reaches[0].nodes;
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].rs == "300") { nodes.insert(nodes.begin() + long(i) + 1, cn); break; }
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].rs == "200") { nodes.insert(nodes.begin() + long(i) + 1, in); break; }
    gf.model = gm;
    const std::string text = gf.serialize();
    io::GeometryFile b;
    MessageList msgs;
    CHECK(b.parse(text, "uji.g01", msgs));
    for (const auto& m : msgs) if (m.severity != Severity::Info) std::printf("  %s\n", m.text(Language::EN).c_str());
    int found = 0;
    for (const auto& n : b.model.reaches[0].nodes) {
        if (n.rs == "350") { CHECK(n.type == NodeType::Bridge); CHECK(n.structure == br.structure); ++found; }
        if (n.rs == "250") { CHECK(n.type == NodeType::Culvert); CHECK(n.structure == cn.structure); ++found; }
        if (n.rs == "150") {
            CHECK(n.type == NodeType::InlineStructure);
            GateGroup g0 = in.structure.gates[0];
            CHECK(n.structure.gates.size() == 1 && n.structure.gates[0] == g0);
            CHECK(n.structure.weirEq(in.structure));
            ++found;
        }
    }
    CHECK(found == 3);
    CHECK(!b.isModified());
    CHECK(b.serialize() == text);
    // edit one value: only that node's structure lines change
    for (auto& n : b.model.reaches[0].nodes) if (n.rs == "350") n.structure.deckUp[0].high = 103.25;
    io::GeometryFile c2;
    CHECK(c2.parse(b.serialize(), "uji.g01", msgs));
    for (const auto& n : c2.model.reaches[0].nodes) if (n.rs == "350") CHECK_NEAR(n.structure.deckUp[0].high, 103.25, 1e-9);
}

TEST(D1_doctor_structures) {
    CrossSection br = bridge(12, 130, 131, false);
    Geometry gm = channel(12, &br);
    SteadyFlow f = flowOf(40);
    Plan p = plan();
    p.regime = FlowRegime::Mixed;
    auto m = app::checkSteadyRun(p, &gm, &f, UnitSystem::SI);
    CHECK(hasCode(m, "MD_STRUCTURE_REGIME"));
    p.regime = FlowRegime::Subcritical;
    m = app::checkSteadyRun(p, &gm, &f, UnitSystem::SI);
    CHECK(!hasErrors(m));
    CHECK(hasCode(m, "MD_FEATURE_PREVIEW"));
    gm.reaches[0].nodes.erase(gm.reaches[0].nodes.begin(), gm.reaches[0].nodes.begin() + 3);  // RS 600..400 removed
    m = app::checkGeometry(gm, UnitSystem::SI);
    CHECK(hasCode(m, "MD_STRUCTURE_PLACE"));
}

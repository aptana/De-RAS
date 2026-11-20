// Model Doctor: invalid input must be reported, never silently fixed.
#include "TestMain.h"
#include "app/ModelDoctor.h"
using namespace deras;

static bool hasCode(const MessageList& m, const std::string& c, Severity s) {
    for (auto& x : m) if (x.code == c && x.severity == s) return true;
    return false;
}
static CrossSection good() {
    CrossSection x; x.rs = "100";
    x.staElev = {{0, 5}, {10, 0}, {20, 5}}; x.manning = {{0, 0.03}};
    x.bankLeft = 0; x.bankRight = 20; x.lenLeft = x.lenChannel = x.lenRight = 100;
    return x;
}
TEST(good_section_has_no_errors) { CHECK(!hasErrors(app::checkCrossSection(good(), "L", false))); }
TEST(section_errors) {
    auto x = good(); x.staElev.pop_back();
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_TOO_FEW_POINTS", Severity::Error));
    x = good(); x.bankRight = 25;
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_BANKS_OUTSIDE", Severity::Error));
    x = good(); x.bankLeft.reset();
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_BANKS_MISSING", Severity::Error));
    x = good(); x.manning.clear();
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_NO_MANNING", Severity::Error));
    x = good(); x.manning[0].n = 0;
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_MANNING_NONPOS", Severity::Error));
    x = good(); x.manning[0].n = 0.5;
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_MANNING_RANGE", Severity::Warning));
    x = good(); x.staElev[2].station = 5;
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_STATION_ORDER", Severity::Error));
    x = good(); x.lenChannel = 0;
    CHECK(hasCode(app::checkCrossSection(x, "L", false), "XS_LENGTH_ZERO", Severity::Error));
    CHECK(!hasCode(app::checkCrossSection(x, "L", true), "XS_LENGTH_ZERO", Severity::Error));
}
TEST(flow_and_boundary_errors) {
    Geometry g; Reach r; r.river = "R"; r.reach = "A";
    auto a = good(); auto b = good(); b.rs = "0"; b.lenChannel = b.lenLeft = b.lenRight = 0;
    r.nodes = {a, b}; g.reaches.push_back(r);
    SteadyFlow f;
    CHECK(hasCode(app::checkSteadyFlow(f, g), "FLOW_NO_PROFILES", Severity::Error));
    f.profileNames = {"P"};
    auto m = app::checkSteadyFlow(f, g);
    CHECK(hasCode(m, "FLOW_NO_UPSTREAM", Severity::Error));
    CHECK(hasCode(m, "BC_MISSING_DN", Severity::Error));
    f.changes.push_back({"R", "A", "100", {-5}});
    f.boundaryRef("R", "A", 0).downstream.type = BoundaryType::NormalDepth;
    m = app::checkSteadyFlow(f, g);
    CHECK(hasCode(m, "FLOW_NONPOS", Severity::Error));
    CHECK(hasCode(m, "BC_BAD_SLOPE", Severity::Error));
    f.changes[0].flows[0] = 10;
    f.boundaryRef("R", "A", 0).downstream.slope = 0.001;
    CHECK(!hasErrors(app::checkSteadyFlow(f, g)));
    f.boundaryRef("R", "A", 0).downstream = Boundary{BoundaryType::KnownWS, -3, 0, {}};
    CHECK(hasCode(app::checkSteadyFlow(f, g), "BC_KNOWN_BELOW_BED", Severity::Error));
    f.changes[0].rs = "999";
    CHECK(hasCode(app::checkSteadyFlow(f, g), "FLOW_UNKNOWN_RS", Severity::Error));
}
TEST(geometry_order_and_structures) {
    Geometry g; Reach r; r.river = "R"; r.reach = "A";
    auto a = good(); a.rs = "0"; auto b = good(); b.rs = "100"; b.lenChannel = 0;
    r.nodes = {a, b}; g.reaches.push_back(r);
    CHECK(hasCode(app::checkGeometry(g, UnitSystem::SI), "XS_ORDER", Severity::Error));
    CrossSection br; br.type = NodeType::Bridge; br.rs = "50";
    g.reaches[0].nodes.insert(g.reaches[0].nodes.begin() + 1, br);
    CHECK(hasCode(app::checkGeometry(g, UnitSystem::SI), "MD_STRUCTURE_UNREAD", Severity::Error));
    Junction j; j.name = "J"; j.up = {{"R", "A"}}; j.dn = {{"R", "X"}};
    g.junctions.push_back(j);
    CHECK(hasCode(app::checkGeometry(g, UnitSystem::SI), "MD_JUNCTION_UNKNOWN", Severity::Error));
}
TEST(plan_regime) {
    Plan p; p.regime = FlowRegime::Mixed; p.flowFile = "f01";
    CHECK(!hasErrors(app::checkPlan(p, true, true)));
    CHECK(hasCode(app::checkPlan(p, true, true), "MD_FEATURE_PREVIEW", Severity::Info));
    p.flowFile = "u01";
    CHECK(hasCode(app::checkPlan(p, true, true), "MD_NO_FLOW", Severity::Error));
}
TEST(messages_bilingual) {
    auto m = makeMsg(Severity::Error, "XS_BANKS_ORDER", {"5", "3"}, "R / A RS 100");
    CHECK(m.text(Language::ID) == "[R / A RS 100] Tebing kiri (5) harus lebih kecil dari tebing kanan (3).");
    CHECK(m.text(Language::EN) == "[R / A RS 100] Left bank (5) must be less than right bank (3).");
}

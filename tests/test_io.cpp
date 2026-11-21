// HEC-RAS text file I/O: parse, byte-exact round trip (K1), in-place
// edits that keep unknown lines, new project create -> save -> reopen.
#include "TestMain.h"
#include "io/Project.h"
#include <filesystem>
using namespace deras;
using namespace deras::io;
namespace fs = std::filesystem;

static std::string data(const std::string& n) { return std::string(DERAS_TEST_DATA) + "/" + n; }
static std::string bytesOf(const fs::path& p) { std::string b; readFile(p, b); return b; }

TEST(parse_fixture_geometry) {
    GeometryFile g; MessageList m;
    CHECK(g.load(data("Fixture.g01"), m));
    CHECK(g.model.title == "Fixture geometry");
    CHECK(g.model.reaches.size() == 1);
    const auto& r = g.model.reaches[0];
    CHECK(r.river == "Kali Test" && r.reach == "Hulu");
    CHECK(r.xy.size() == 3);
    CHECK(r.nodes.size() == 3);
    const auto& x0 = r.nodes[0];
    CHECK(x0.rs == "500");
    CHECK(x0.lenChannel == 250);
    CHECK(x0.staElev.size() == 7);
    CHECK(x0.staElev[1].station == 20 && x0.staElev[1].elev == 104.5);
    CHECK(x0.manning.size() == 3 && x0.manning[1].n == 0.035);
    CHECK(x0.bankLeft.value() == 30 && x0.bankRight.value() == 50);
    CHECK(x0.expansion == 0.3 && x0.contraction == 0.1);
    CHECK(x0.description == "Upstream section");
    CHECK(r.nodes[1].ignoredFeatures.empty());
    CHECK(r.nodes[1].ineffective.size() == 1);
    CHECK(r.nodes[1].ineffective[0].staStart == 0 && r.nodes[1].ineffective[0].staEnd == 20);
    CHECK(r.nodes[1].ineffective[0].elev == 104.8 && !r.nodes[1].ineffective[0].permanent);
}
TEST(roundtrip_unchanged_is_identical) {
    MessageList m;
    auto p = Project::open(data("Fixture.prj"), m);
    CHECK(p != nullptr);
    if (!p) return;
    CHECK(p->units() == UnitSystem::SI);
    CHECK(!p->isModified());
    CHECK(p->geometries.at("g01").serialize() == bytesOf(data("Fixture.g01")));
    CHECK(p->steadyFlows.at("f01").serialize() == bytesOf(data("Fixture.f01")));
    CHECK(p->plans.at("p01").serialize() == bytesOf(data("Fixture.p01")));
}
TEST(edit_keeps_unknown_lines) {
    GeometryFile g; MessageList m;
    g.load(data("Fixture.g01"), m);
    g.model.reaches[0].nodes[1].manning[1].n = 0.04;  // edit the XS that has ineffective areas
    std::string out = g.serialize();
    CHECK(out.find("#XS Ineff= 1 , 0 ") != std::string::npos);
    CHECK(out.find("Permanent Ineff=") != std::string::npos);
    CHECK(out.find("     .04") != std::string::npos);
    CHECK(out.find("XS HTab Starting El and Incr=100.6,0.1, 200 ") != std::string::npos);  // other XS untouched
    CHECK(out.find("Composite Channel Slope=5") != std::string::npos);
    CHECK(out.find("Viewing Rectangle=") != std::string::npos);
    GeometryFile back; MessageList m2;
    back.parse(out, "x", m2);
    CHECK(back.model.reaches[0].nodes[1].manning[1].n == 0.04);
    CHECK(back.model.reaches[0].nodes[0].sameData(g.model.reaches[0].nodes[0]));
    // unchanged XS block is byte-identical
    std::string orig = bytesOf(data("Fixture.g01"));
    size_t a = orig.find("Type RM Length L Ch R = 1 ,500"), b = orig.find("Type RM Length L Ch R = 1 ,250");
    CHECK(out.find(orig.substr(a, b - a)) != std::string::npos);
}
TEST(add_and_delete_cross_section) {
    GeometryFile g; MessageList m;
    g.load(data("Fixture.g01"), m);
    auto& nodes = g.model.reaches[0].nodes;
    CrossSection n = nodes[1];
    n.uid = nextUid();
    n.rs = "375";
    n.ignoredFeatures.clear();
    nodes.insert(nodes.begin() + 1, n);
    nodes.erase(nodes.begin() + 3);  // delete RS 0... keep order check
    std::string out = g.serialize();
    GeometryFile back; MessageList m2;
    back.parse(out, "x", m2);
    CHECK(back.model.reaches[0].nodes.size() == 3);
    CHECK(back.model.reaches[0].nodes[1].rs == "375");
    CHECK(back.model.reaches[0].nodes[2].rs == "250");
    CHECK(out.find("Chan Stop Cuts=-1") != std::string::npos);
}
TEST(flow_and_plan_parse) {
    MessageList m;
    SteadyFlowFile f;
    f.load(data("Fixture.f01"), m);
    CHECK(f.model.profileCount() == 2);
    CHECK(f.model.changes.size() == 1 && f.model.changes[0].flows[1] == 60);
    auto* b1 = f.model.boundary("Kali Test", "Hulu", 0);
    auto* b2 = f.model.boundary("Kali Test", "Hulu", 1);
    CHECK(b1 && b1->downstream.type == BoundaryType::NormalDepth && b1->downstream.slope == 0.0012);
    CHECK(b2 && b2->downstream.type == BoundaryType::KnownWS && b2->downstream.knownWS == 102.4);
    f.model.changes[0].flows[0] = 25;
    std::string out = f.serialize();
    CHECK(out.find("DSS Import FillOption= 0 ") != std::string::npos);
    SteadyFlowFile back; back.parse(out, "x", m);
    CHECK(back.model.changes[0].flows[0] == 25);
    CHECK(back.model.boundary("Kali Test", "Hulu", 1)->downstream.knownWS == 102.4);
    PlanFile p; p.load(data("Fixture.p01"), m);
    CHECK(p.model.geomFile == "g01" && p.model.flowFile == "f01");
    CHECK(p.model.stdStepTol == 0.003 && p.model.regime == FlowRegime::Subcritical);
    p.model.maxTrials = 30;
    std::string po = p.serialize();
    CHECK(po.find("Num of Std Step Trials= 30 ") != std::string::npos);
    CHECK(po.find("Unsteady Friction Slope Method= 2 ") != std::string::npos);
}
TEST(new_project_save_reopen) {
    fs::path dir = fs::temp_directory_path() / "deras_test_newproj";
    fs::remove_all(dir);
    ProjectSettings s; s.coordinateSystem = "EPSG:32748";
    auto p = Project::create(dir, "Baru", "Proyek Baru", UnitSystem::SI, s);
    std::string g = p->addGeometry("Geo");
    std::string f = p->addSteadyFlow("Aliran");
    std::string pl = p->addPlan("Plan 1", "P1", g, f);
    CHECK(g == "g01" && f == "f01" && pl == "p01");
    Reach r; r.uid = nextUid(); r.river = "Sungai"; r.reach = "Hilir"; r.xy = {{0, 100}, {0, 0}};
    for (int i = 0; i < 2; ++i) {
        CrossSection x; x.uid = nextUid(); x.rs = i == 0 ? "100" : "0";
        x.staElev = {{0, 5}, {10, 0}, {20, 5}}; x.manning = {{0, 0.03}}; x.bankLeft = 0; x.bankRight = 20;
        x.lenLeft = x.lenChannel = x.lenRight = i == 0 ? 100 : 0;
        r.nodes.push_back(x);
    }
    p->geometries.at(g).model.reaches.push_back(r);
    auto& fl = p->steadyFlows.at(f).model;
    fl.profileNames = {"Q100"};
    fl.changes.push_back({"Sungai", "Hilir", "100", {50}});
    fl.boundaryRef("Sungai", "Hilir", 0).downstream.type = BoundaryType::CriticalDepth;
    MessageList m;
    CHECK(p->save(m));
    CHECK(!p->isModified());
    std::string prj = bytesOf(dir / "Baru.prj");
    CHECK(prj.rfind("Proj Title=Proyek Baru\r\n", 0) == 0);
    CHECK(prj.find("SI Units\r\n") != std::string::npos);
    CHECK(prj.find("Geom File=g01\r\nFlow File=f01\r\nPlan File=p01\r\n") != std::string::npos);
    CHECK(prj.find("Current Plan=p01") != std::string::npos);
    CHECK(fs::exists(dir / "Baru.deras.json"));
    auto q = Project::open(dir / "Baru.prj", m);
    CHECK(q != nullptr);
    if (!q) return;
    CHECK(q->settings.coordinateSystem == "EPSG:32748");
    const auto& rr = q->geometries.at("g01").model.reaches.at(0);
    CHECK(rr.nodes.size() == 2 && rr.nodes[0].staElev[1].elev == 0 && rr.nodes[1].rs == "0");
    CHECK(q->steadyFlows.at("f01").model.changes[0].flows[0] == 50);
    CHECK(q->steadyFlows.at("f01").model.boundary("Sungai", "Hilir", 0)->downstream.type == BoundaryType::CriticalDepth);
    // re-serialising the freshly written files is byte-identical (K1 on De-RAS output)
    CHECK(q->geometries.at("g01").serialize() == bytesOf(dir / "Baru.g01"));
    CHECK(q->steadyFlows.at("f01").serialize() == bytesOf(dir / "Baru.f01"));
    CHECK(q->plans.at("p01").serialize() == bytesOf(dir / "Baru.p01"));
    // duplicate project
    MessageList m3;
    CHECK(Project::duplicate(dir / "Baru.prj", dir / "copy", "Salinan", m3));
    CHECK(fs::exists(dir / "copy" / "Salinan.g01"));
    CHECK(Project::open(dir / "copy" / "Salinan.prj", m3) != nullptr);
    fs::remove_all(dir);
}
TEST(not_a_ras_project) {
    fs::path f = fs::temp_directory_path() / "deras_esri.prj";
    std::string err;
    writeFileAtomic(f, "PROJCS[\"WGS_1984_UTM_Zone_48S\"]", err);
    MessageList m;
    CHECK(Project::open(f, m) == nullptr);
    CHECK(hasErrors(m) && m.back().code == "PRJ_NOT_RAS");
    fs::remove(f);
}

// M6 tests (build 0.2.3): storage areas, lateral structures, connections,
// pumps and 2D flow areas in the unsteady solver, and their file I/O.
//  W1 weir equation (free, submerged, reversed)
//  S1 storage area alone: a constant inflow raises the stage by Q t / A
//  S2 connection between two storage areas: equal levels, volume kept
//  S3 pump station with on / off elevations drains a storage area
//  S4 lateral structure: a flood spills into a storage area; the stored volume equals the spilled flow
//  B1 breach lowers the crest over the formation time
//  Z1 2D area: inflow through a BC line, normal depth outflow; small volume error and wetting
//  Z2 1D-2D: a lateral structure spills a flood into a 2D area
//  F1 geometry file: storage area, 2D area, connection, BC line and lateral structure written and read back
//  F2 unsteady file: area and BC line boundaries, precipitation and initial storage
//  D1 Model Doctor accepts lateral structures in unsteady plans and checks their data
#include "TestMain.h"
#include "app/ModelDoctor.h"
#include "engines/unsteady/UnsteadySolver.h"
#include "io/GeometryFile.h"
#include "io/ProjectFiles.h"
#include "io/ResultsFile.h"
#include <cstdio>
#include <cstdlib>
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

Reach channel(int nX, double dx, double b, double s, double zDn) {
    Reach r;
    r.river = "R";
    r.reach = "A";
    for (int i = 0; i < nX; ++i) {
        const double d = dx * (nX - 1 - i);
        r.nodes.push_back(rect(std::to_string(int(d + 0.5)), b, zDn + s * d, 0.03, i + 1 < nX ? dx : 0));
    }
    return r;
}

Plan uplan(const std::string& endTime, const std::string& dt = "1MIN", const std::string& out = "10MIN") {
    Plan p;
    p.flowFile = "u01";
    p.simStartDate = "01JAN2024";
    p.simStartTime = "0000";
    p.simEndDate = "01JAN2024";
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

StorageArea sa(const std::string& name, double area, double minElev) {
    StorageArea a;
    a.name = name;
    a.area = area;
    a.minElev = minElev;
    return a;
}

UnsteadyBoundary areaFlow(const std::string& area, std::vector<double> v, UBoundaryType t = UBoundaryType::FlowHydrograph) {
    UnsteadyBoundary b;
    b.area = area;
    b.type = t;
    b.values = std::move(v);
    b.interval = "1HOUR";
    return b;
}

const ExtraSeries* extra(const RunResult& r, const std::string& kind, const std::string& name) {
    for (const auto& e : r.extra) if (e.kind == kind && e.name == name) return &e;
    return nullptr;
}

double integrate(const RunResult& r, const ExtraSeries& e) {
    double v = 0;
    for (size_t i = 1; i < e.values.size(); ++i)
        v += 0.5 * (e.values[i] + e.values[i - 1]) * (r.series.hours[i] - r.series.hours[i - 1]) * 3600.0;
    return v;
}

const Message* find(const MessageList& m, const std::string& code) {
    for (const auto& x : m) if (x.code == code) return &x;
    return nullptr;
}

void dump(const RunResult& r) {
    for (const auto& m : r.messages) std::printf("  %s\n", m.text(Language::EN).c_str());
}

// Plane terrain z = z0 + sx * x + sy * y, 1 x 1 km, 10 m pixels, top-left (0, 1000).
io::Raster plane(double z0, double sx, double sy) {
    io::Raster t;
    t.nx = t.ny = 100;
    t.x0 = 0;
    t.y0 = 1000;
    t.dx = t.dy = 10;
    t.z.resize(10000);
    for (int r = 0; r < 100; ++r)
        for (int c = 0; c < 100; ++c) t.z[size_t(r * 100 + c)] = float(z0 + sx * t.cellX(c) + sy * t.cellY(r));
    return t;
}

// Reach with a 300 m lateral weir on the right bank between sections 4 and 5 and a flood hydrograph.
void floodReach(Geometry& g, UnsteadyFlow& f, double crest, const std::string& to) {
    Reach r = channel(11, 200, 10, 0.001, 100);
    CrossSection lat;
    lat.rs = "1150";
    lat.type = NodeType::LateralStructure;
    lat.structure.parsed = true;
    lat.structure.dist = 20;
    lat.structure.width = 1;
    lat.structure.weirCoef = 1.5;
    lat.structure.weir = {{0, crest}, {150, crest}};
    lat.structure.lateral.side = 1;
    lat.structure.lateral.toName = to;
    r.nodes.insert(r.nodes.begin() + 5, lat);
    for (size_t i = 0; i < r.nodes.size(); ++i) r.xy.push_back({0, 0});
    r.xy = {{0, 1000}, {2000, 1000}};
    g.reaches.push_back(r);
    UnsteadyBoundary up;
    up.river = "R";
    up.reach = "A";
    up.rs = r.nodes.front().rs;
    up.type = UBoundaryType::FlowHydrograph;
    up.values = {20, 60, 120, 120, 60, 20, 20, 20};
    f.boundaries.push_back(up);
    UnsteadyBoundary dn;
    dn.river = "R";
    dn.reach = "A";
    dn.rs = r.nodes.back().rs;
    dn.type = UBoundaryType::NormalDepth;
    dn.slope = 0.001;
    dn.interval.clear();
    f.boundaries.push_back(dn);
}
}  // namespace

TEST(W1_weir) {
    // free: C L H^1.5
    CHECK_NEAR(unsteady::weirFlow(102, std::nan(""), 100, 1.7, 10), 1.7 * 10 * std::pow(2.0, 1.5), 1e-9);
    CHECK_NEAR(unsteady::weirFlow(99, std::nan(""), 100, 1.7, 10), 0, 1e-12);
    // tailwater below the crest does not reduce the flow
    CHECK_NEAR(unsteady::weirFlow(102, 99, 100, 1.7, 10), unsteady::weirFlow(102, std::nan(""), 100, 1.7, 10), 1e-9);
    // submerged: Villemonte, s = 0.5
    CHECK_NEAR(unsteady::weirFlow(102, 101, 100, 1.7, 10), 1.7 * 10 * std::pow(2.0, 1.5) * std::pow(1 - std::pow(0.5, 1.5), 0.385), 1e-9);
    // equal levels: no flow; reversed: negative
    CHECK_NEAR(unsteady::weirFlow(101, 101, 100, 1.7, 10), 0, 1e-9);
    CHECK(unsteady::weirFlow(101, 102, 100, 1.7, 10) < 0);
    CHECK_NEAR(unsteady::weirFlow(101, 102, 100, 1.7, 10), -unsteady::weirFlow(102, 101, 100, 1.7, 10), 1e-9);
}

TEST(S1_storage_area_inflow) {
    Geometry g;
    g.storageAreas.push_back(sa("Kolam", 10000, 0));
    UnsteadyFlow f;
    f.boundaries.push_back(areaFlow("Kolam", {10, 10, 10, 10}));
    auto r = unsteady::solve(g, f, uplan("0200"), UnitSystem::SI, {});
    if (r.status != RunStatus::Succeeded) dump(r);
    CHECK(r.status == RunStatus::Succeeded);
    const auto* e = extra(r, "storage", "Kolam");
    CHECK(e != nullptr);
    if (!e) return;
    CHECK_NEAR(e->values.back(), 10.0 * 7200 / 10000, 0.01);
    CHECK_NEAR(e->maxValue, 7.2, 0.01);
    // elevation-volume table instead of an area
    g.storageAreas[0].area = 0;
    g.storageAreas[0].volElev = {{0, 0}, {1, 5000}, {10, 140000}};
    r = unsteady::solve(g, f, uplan("0200"), UnitSystem::SI, {});
    e = extra(r, "storage", "Kolam");
    CHECK(e && r.status == RunStatus::Succeeded);
    if (e) CHECK_NEAR(e->values.back(), 1 + (72000.0 - 5000) / 15000.0, 0.01);
    // initial storage elevation
    f.initialStorage = {{"Kolam", 2.0}};
    f.boundaries[0].values = {0, 0, 0};
    r = unsteady::solve(g, f, uplan("0100"), UnitSystem::SI, {});
    e = extra(r, "storage", "Kolam");
    CHECK(e && e->values.front() == 2.0 && std::fabs(e->values.back() - 2.0) < 1e-9);
}

TEST(S2_connection) {
    Geometry g;
    g.storageAreas.push_back(sa("A", 10000, 0));
    g.storageAreas.push_back(sa("B", 10000, 0));
    SaConnection c;
    c.name = "Pelimpah";
    c.upName = "A";
    c.dnName = "B";
    c.coef = 1.5;
    c.weir = {{0, 1}, {20, 1}};
    g.connections.push_back(c);
    UnsteadyFlow f;
    f.initialStorage = {{"A", 5.0}, {"B", 0.0}};
    auto r = unsteady::solve(g, f, uplan("1200", "1MIN", "30MIN"), UnitSystem::SI, {});
    if (r.status != RunStatus::Succeeded) dump(r);
    CHECK(r.status == RunStatus::Succeeded);
    const auto* a = extra(r, "storage", "A");
    const auto* b = extra(r, "storage", "B");
    const auto* q = extra(r, "connection", "Pelimpah");
    CHECK(a && b && q);
    if (!a || !b || !q) return;
    CHECK_NEAR(a->values.back(), b->values.back(), 0.05);
    CHECK_NEAR(a->values.back() + b->values.back(), 5.0, 0.01);  // volume kept (equal areas)
    CHECK(q->maxValue > 0);
}

TEST(S3_pump) {
    Geometry g;
    g.storageAreas.push_back(sa("Kolam", 10000, 0));
    PumpStation p;
    p.name = "Pompa 1";
    p.from.kind = 1;
    p.from.area = "Kolam";
    p.to.kind = 2;
    PumpGroup gp;
    gp.name = "G1";
    gp.count = 2;
    gp.capacity = 1.0;
    gp.onElev = 4.0;
    gp.offElev = 2.0;
    p.groups.push_back(gp);
    g.pumps.push_back(p);
    UnsteadyFlow f;
    f.initialStorage = {{"Kolam", 5.0}};
    auto r = unsteady::solve(g, f, uplan("0600"), UnitSystem::SI, {});
    CHECK(r.status == RunStatus::Succeeded);
    const auto* e = extra(r, "storage", "Kolam");
    const auto* q = extra(r, "pump", "Pompa 1");
    CHECK(e && q);
    if (!e || !q) return;
    CHECK(e->values.back() > 1.95 && e->values.back() < 2.05);
    CHECK_NEAR(q->maxValue, 2.0, 1e-9);
    CHECK_NEAR(q->values.back(), 0.0, 1e-9);
    // stage after 1 hour: 5 - 2 * 3600 / 10000
    CHECK_NEAR(e->values[6], 5 - 0.72, 0.01);
}

TEST(S4_lateral_to_storage) {
    Geometry g;
    UnsteadyFlow f;
    floodReach(g, f, 104.0, "Kolam");
    g.storageAreas.push_back(sa("Kolam", 200000, 100));
    auto r = unsteady::solve(g, f, uplan("0700", "30SEC", "5MIN"), UnitSystem::SI, {});
    if (r.status != RunStatus::Succeeded) dump(r);
    CHECK(r.status == RunStatus::Succeeded);
    const auto* s = extra(r, "storage", "Kolam");
    const auto* q = extra(r, "lateral", "R / A RS 1150");
    CHECK(s && q);
    if (!s || !q) return;
    CHECK(q->maxValue > 1.0);
    const double vol = (s->values.back() - 100) * 200000;
    const double spilled = integrate(r, *q);
    std::printf("  spilled %.0f m3, stored %.0f m3, max lateral %.2f m3/s\n", spilled, vol, q->maxValue);
    CHECK(vol > 0);
    CHECK_NEAR(vol / spilled, 1.0, 0.03);
    // without the structure the flood peak downstream is higher
    Geometry g2;
    UnsteadyFlow f2;
    floodReach(g2, f2, 120.0, "");
    auto r2 = unsteady::solve(g2, f2, uplan("0700", "30SEC", "5MIN"), UnitSystem::SI, {});
    CHECK(r2.status == RunStatus::Succeeded);
    const size_t last = r.profiles[0].xs.size() - 1;
    CHECK(r.profiles[0].xs[last].q < r2.profiles[0].xs[last].q - 1.0);
}

TEST(B1_breach) {
    Breach b;
    b.active = true;
    b.station = 50;
    b.bottomElev = 100;
    b.bottomWidth = 10;
    b.sideSlope = 2;
    b.startHours = 1;
    b.formHours = 2;
    CHECK_NEAR(breachedCrest(b, 50, 105, 0.5), 105, 1e-12);   // before the start
    CHECK_NEAR(breachedCrest(b, 50, 105, 2.0), 102.5, 1e-12); // half formed
    CHECK_NEAR(breachedCrest(b, 50, 105, 5.0), 100, 1e-12);   // complete
    CHECK_NEAR(breachedCrest(b, 57, 105, 5.0), 101, 1e-12);   // on the side slope: 100 + (7 - 5) / 2
    CHECK_NEAR(breachedCrest(b, 80, 105, 5.0), 105, 1e-12);   // outside the breach
    // a breach lets a storage area drain through a high connection
    Geometry g;
    g.storageAreas.push_back(sa("A", 10000, 0));
    g.storageAreas.push_back(sa("B", 10000, 0));
    SaConnection c;
    c.name = "Tanggul";
    c.upName = "A";
    c.dnName = "B";
    c.weir = {{0, 6}, {20, 6}};
    c.breach = b;
    c.breach.station = 10;
    c.breach.bottomElev = 1;
    c.breach.bottomWidth = 4;
    c.breach.startHours = 0.5;
    c.breach.formHours = 0.5;
    g.connections.push_back(c);
    UnsteadyFlow f;
    f.initialStorage = {{"A", 5.0}, {"B", 0.0}};
    auto r = unsteady::solve(g, f, uplan("0600", "1MIN", "30MIN"), UnitSystem::SI, {});
    const auto* bs = extra(r, "storage", "B");
    CHECK(bs && bs->values.back() > 1.5);
    g.connections[0].breach.active = false;
    r = unsteady::solve(g, f, uplan("0600", "1MIN", "30MIN"), UnitSystem::SI, {});
    bs = extra(r, "storage", "B");
    CHECK(bs && bs->values.back() < 1e-6);
}

TEST(Z1_2d_bc_lines) {
    // terrain falls 1 m per km towards the west; inflow along the east edge, normal depth at the west edge
    std::vector<io::Raster> terr{plane(10, 0.001, 0)};
    Geometry g;
    StorageArea a;
    a.name = "Dataran";
    a.is2D = true;
    a.cellSize = 50;
    a.manningN = 0.05;
    a.polygon = {{50, 50}, {950, 50}, {950, 950}, {50, 950}};
    g.storageAreas.push_back(a);
    g.bcLines.push_back({"Hulu", "Dataran", {{940, 100}, {940, 900}}, {}});
    g.bcLines.push_back({"Hilir", "Dataran", {{60, 100}, {60, 900}}, {}});
    UnsteadyFlow f;
    auto in = areaFlow("Dataran", {20, 20, 20, 20, 20, 20, 20});
    in.bcLine = "Hulu";
    f.boundaries.push_back(in);
    UnsteadyBoundary out;
    out.area = "Dataran";
    out.bcLine = "Hilir";
    out.type = UBoundaryType::NormalDepth;
    out.slope = 0.001;
    f.boundaries.push_back(out);
    unsteady::Inputs inp;
    inp.terrains = &terr;
    auto r = unsteady::solve(g, f, uplan("0600", "1MIN", "30MIN"), UnitSystem::SI, {}, inp);
    if (r.status != RunStatus::Succeeded) dump(r);
    CHECK(r.status == RunStatus::Succeeded);
    CHECK(r.grids.size() == 1);
    if (r.grids.empty()) return;
    const auto& gr = r.grids[0];
    CHECK(gr.nx == 18 && gr.ny == 18);
    const auto* m = find(r.messages, "SOLV_2D_DONE");
    CHECK(m != nullptr);
    if (m) {
        std::printf("  %s\n", m->text(Language::EN).c_str());
        CHECK(std::fabs(std::atof(m->args[4].c_str())) < 1.0);
    }
    // water reached the west edge, the east edge is deeper than the cells beyond the BC line are dry
    const int east = gr.index[size_t(9 * gr.nx + gr.nx - 2)], west = gr.index[size_t(9 * gr.nx + 1)];
    CHECK(east >= 0 && west >= 0);
    if (east >= 0 && west >= 0) {
        CHECK(gr.maxDepth[size_t(east)] > 0.05);
        CHECK(!std::isnan(gr.arrivalHours[size_t(west)]));
        CHECK(gr.arrivalHours[size_t(east)] < gr.arrivalHours[size_t(west)]);
        CHECK(gr.maxVel[size_t(west)] > 0);
    }
    CHECK(gr.wse.size() == gr.hours.size() && gr.hours.size() == 13);
    // 2D results file and extra series survive a write / read
    {
        const auto dir = io::fs::temp_directory_path() / "deras_test_m6";
        std::error_code ec;
        io::fs::create_directories(dir, ec);
        std::string err;
        CHECK(io::writeGrids2D(io::grid2dPathFor(dir, "uji", "p01"), r.grids, err));
        CHECK(io::writeResults(io::resultsPathFor(dir, "uji", "p01"), r, err));
        RunResult back;
        CHECK(io::readResults(io::resultsPathFor(dir, "uji", "p01"), back, err));
        CHECK(back.grids.size() == 1 && back.grids[0].index == gr.index && back.grids[0].wse.size() == gr.wse.size());
        if (!back.grids.empty()) CHECK(back.grids[0].maxDepth == gr.maxDepth && back.grids[0].x0 == gr.x0 && back.grids[0].area == "Dataran");
        io::fs::remove_all(dir, ec);
    }
    // no terrain: a clear error
    auto r2 = unsteady::solve(g, f, uplan("0600", "1MIN", "30MIN"), UnitSystem::SI, {});
    CHECK(r2.status == RunStatus::Failed && find(r2.messages, "SOLV_2D_BUILD_FAIL"));
    // precipitation on a closed area: stored depth = rain depth
    UnsteadyFlow f3;
    auto rain = areaFlow("Dataran", {0, 10, 10, 0}, UBoundaryType::Precipitation);  // mm per hour
    f3.boundaries.push_back(rain);
    auto r3 = unsteady::solve(g, f3, uplan("0400", "5MIN", "30MIN"), UnitSystem::SI, {}, inp);
    CHECK(r3.status == RunStatus::Succeeded);
    const auto* m3 = find(r3.messages, "SOLV_2D_DONE");
    if (m3) std::printf("  %s\n", m3->text(Language::EN).c_str());
    CHECK(m3 && std::fabs(std::atof(m3->args[4].c_str())) < 0.5);
}

TEST(Z2_lateral_to_2d) {
    std::vector<io::Raster> terr{plane(101, 0, 0.0005)};  // floodplain at about 101-101.4 m south of the river
    Geometry g;
    UnsteadyFlow f;
    floodReach(g, f, 104.0, "Dataran");
    StorageArea a;
    a.name = "Dataran";
    a.is2D = true;
    a.cellSize = 40;
    a.polygon = {{0, 100}, {1000, 100}, {1000, 980}, {0, 980}};
    g.storageAreas.push_back(a);
    unsteady::Inputs inp;
    inp.terrains = &terr;
    auto r = unsteady::solve(g, f, uplan("0700", "30SEC", "10MIN"), UnitSystem::SI, {}, inp);
    if (r.status != RunStatus::Succeeded) dump(r);
    CHECK(r.status == RunStatus::Succeeded);
    const auto* q = extra(r, "lateral", "R / A RS 1150");
    CHECK(q && q->maxValue > 1.0);
    const auto* m = find(r.messages, "SOLV_2D_DONE");
    CHECK(m != nullptr);
    if (m) {
        std::printf("  %s\n", m->text(Language::EN).c_str());
        CHECK(std::fabs(std::atof(m->args[4].c_str())) < 1.0);
        CHECK(std::atoi(m->args[3].c_str()) > 5);
    }
}

TEST(F1_geometry_areas_roundtrip) {
    auto gf = io::GeometryFile::createNew("Uji M6", "7.00");
    Reach r = channel(4, 100, 10, 0.001, 100);
    r.xy = {{0, 0}, {300, 0}};
    CrossSection lat;
    lat.rs = "150";
    lat.type = NodeType::LateralStructure;
    lat.structure.parsed = true;
    lat.structure.dist = 10;
    lat.structure.width = 5;
    lat.structure.weirCoef = 1.4;
    lat.structure.weir = {{0, 103}, {40, 102.5}, {80, 103}};
    lat.structure.lateral.side = 0;
    lat.structure.lateral.toName = "Kolam";
    r.nodes.insert(r.nodes.begin() + 2, lat);
    gf.model.reaches.push_back(r);
    StorageArea s;
    s.name = "Kolam";
    s.polygon = {{0, -10}, {300, -10}, {300, -200}, {0, -200}};
    s.volElev = {{100, 0}, {101, 5000}, {103, 30000}};
    gf.model.storageAreas.push_back(s);
    StorageArea d;
    d.name = "Dataran";
    d.is2D = true;
    d.cellSize = 25;
    d.manningN = 0.07;
    d.polygon = {{0, 10}, {300, 10}, {300, 300}, {0, 300}};
    gf.model.storageAreas.push_back(d);
    SaConnection c;
    c.name = "Pintu";
    c.upName = "Kolam";
    c.dnName = "Dataran";
    c.line = {{310, -100}, {310, 100}};
    c.width = 6;
    c.coef = 1.3;
    c.weir = {{0, 104}, {200, 104}};
    gf.model.connections.push_back(c);
    gf.model.bcLines.push_back({"Hilir", "Dataran", {{300, 20}, {300, 290}}, {}});
    const std::string text = gf.serialize();
    io::GeometryFile g2;
    MessageList msgs;
    CHECK(g2.parse(text, "uji.g01", msgs));
    for (const auto& m : msgs) std::printf("  %s\n", m.text(Language::EN).c_str());
    CHECK(g2.model.storageAreas.size() == 2 && g2.model.connections.size() == 1 && g2.model.bcLines.size() == 1);
    if (g2.model.storageAreas.size() != 2 || g2.model.connections.size() != 1 || g2.model.bcLines.size() != 1) return;
    CHECK(g2.model.storageAreas[0].name == "Kolam" && !g2.model.storageAreas[0].is2D && g2.model.storageAreas[0].volElev.size() == 3);
    CHECK(g2.model.storageAreas[0].polygon.size() == 4);
    CHECK(g2.model.storageAreas[1].is2D && g2.model.storageAreas[1].cellSize == 25 && g2.model.storageAreas[1].manningN == 0.07);
    const auto& c2 = g2.model.connections[0];
    CHECK(c2.upName == "Kolam" && c2.dnName == "Dataran" && c2.weir.size() == 2 && c2.coef == 1.3 && c2.line.size() == 2);
    CHECK(g2.model.bcLines[0].name == "Hilir" && g2.model.bcLines[0].area == "Dataran" && g2.model.bcLines[0].line.size() == 2);
    CHECK(g2.model.reaches.size() == 1 && g2.model.reaches[0].nodes.size() == 5);
    if (g2.model.reaches.size() == 1 && g2.model.reaches[0].nodes.size() == 5) {
        const auto& l2 = g2.model.reaches[0].nodes[2];
        CHECK(l2.type == NodeType::LateralStructure && l2.structure.parsed);
        CHECK(l2.structure.weir.size() == 3 && l2.structure.weirCoef == 1.4 && l2.structure.dist == 10);
        CHECK(l2.structure.lateral.side == 0 && l2.structure.lateral.toName == "Kolam");
    }
    // unchanged: identical text; a reparse is not modified
    CHECK(!g2.isModified());
    CHECK(g2.serialize() == text);
    // edit one area: only that block changes, the rest is kept
    g2.model.storageAreas[1].cellSize = 30;
    const std::string t2 = g2.serialize();
    io::GeometryFile g3;
    CHECK(g3.parse(t2, "uji.g01", msgs));
    CHECK(g3.model.storageAreas.size() == 2 && g3.model.storageAreas[1].cellSize == 30 && g3.model.storageAreas[0] == g2.model.storageAreas[0]);
    CHECK(g3.model.connections == g2.model.connections && g3.model.bcLines == g2.model.bcLines);
}

TEST(F2_unsteady_areas_roundtrip) {
    auto uf = io::UnsteadyFlowFile::createNew("Uji M6", "7.00");
    uf.model.initialStorage = {{"Kolam", 101.5}, {"Dataran", 99.0}};
    auto b1 = areaFlow("Kolam", {0, 5, 10, 5});
    uf.model.boundaries.push_back(b1);
    UnsteadyBoundary b2;
    b2.area = "Dataran";
    b2.bcLine = "Hilir";
    b2.type = UBoundaryType::StageHydrograph;
    b2.values = {99, 99.5, 100};
    b2.interval = "1HOUR";
    uf.model.boundaries.push_back(b2);
    uf.model.boundaries.push_back(areaFlow("Dataran", {0, 12.5, 3}, UBoundaryType::Precipitation));
    UnsteadyBoundary b4;
    b4.area = "Dataran";
    b4.bcLine = "Keluar";
    b4.type = UBoundaryType::NormalDepth;
    b4.slope = 0.002;
    b4.interval.clear();
    uf.model.boundaries.push_back(b4);
    const std::string text = uf.serialize();
    io::UnsteadyFlowFile u2;
    MessageList msgs;
    CHECK(u2.parse(text, "uji.u01", msgs));
    CHECK(u2.model.initialStorage.size() == 2 && u2.model.initialStorage[0].first == "Kolam" && u2.model.initialStorage[0].second == 101.5);
    CHECK(u2.model.boundaries.size() == 4);
    if (u2.model.boundaries.size() != 4) return;
    CHECK(u2.model.boundaries[0].area == "Kolam" && u2.model.boundaries[0].type == UBoundaryType::FlowHydrograph && u2.model.boundaries[0].values.size() == 4);
    CHECK(u2.model.boundaries[1].area == "Dataran" && u2.model.boundaries[1].bcLine == "Hilir" && u2.model.boundaries[1].type == UBoundaryType::StageHydrograph);
    CHECK(u2.model.boundaries[2].type == UBoundaryType::Precipitation && u2.model.boundaries[2].values[1] == 12.5);
    CHECK(u2.model.boundaries[3].bcLine == "Keluar" && u2.model.boundaries[3].slope == 0.002);
    CHECK(!u2.isModified());
    CHECK(u2.serialize() == text);
    CHECK(text.find("Initial Storage Elev=Kolam") != std::string::npos);
}

TEST(D1_doctor_lateral) {
    Geometry g;
    UnsteadyFlow f;
    floodReach(g, f, 104.0, "Kolam");
    g.storageAreas.push_back(sa("Kolam", 200000, 100));
    auto m = app::checkUnsteadyRun(uplan("0700"), &g, &f, UnitSystem::SI);
    for (const auto& x : m) if (x.severity == Severity::Error) std::printf("  %s\n", x.text(Language::EN).c_str());
    CHECK(!hasErrors(m));
    // unknown target area
    g.reaches[0].nodes[5].structure.lateral.toName = "Tidak ada";
    m = app::checkUnsteadyRun(uplan("0700"), &g, &f, UnitSystem::SI);
    CHECK(hasErrors(m));
    // steady plans: lateral structures are not computed (warning, not an error)
    g.reaches[0].nodes[5].structure.lateral.toName = "Kolam";
    Plan sp;
    sp.flowFile = "f01";
    SteadyFlow sf;
    sf.profileNames = {"P1"};
    sf.changes.push_back({"R", "A", g.reaches[0].nodes[0].rs, {50}});
    auto& b = sf.boundaryRef("R", "A", 0);
    b.downstream.type = BoundaryType::NormalDepth;
    b.downstream.slope = 0.001;
    m = app::checkSteadyRun(sp, &g, &sf, UnitSystem::SI);
    CHECK(!hasErrors(m));
    CHECK(find(m, "MD_LATERAL_STEADY") != nullptr);
}

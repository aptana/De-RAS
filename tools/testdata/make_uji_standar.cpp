// De-RAS - tools/testdata/make_uji_standar.cpp
// Developer tool (not shipped): writes the standard test project
// tests/data/uji-standar/DerasUji1km.* (review R5 of build 0.2.0). The files
// are committed; run this tool only to recreate them after a deliberate change
// of the specification in docs/TESTING.md (S1–S4).
//   deras-make-uji-standar <output-dir>
//
// Specification (Aptana: 1 km river, a section every 200 m, 20 m wide; the
// other values were proposed by the notes thread and adopted unchanged):
//   1 river, 1 reach, 1000 m; 6 sections RS 1000 (upstream) .. 0 (downstream)
//   main channel 20 m wide at the bank tops, 3 m deep, 1V:1H walls
//   (bottom width 14 m); 30 m overbanks rising 1.5 m to the end points
//   (the proposed 0.5 m would put the Q150 water surface above the end points)
//   bed slope 0.001 (invert 100.0 m at RS 0, 101.0 m at RS 1000)
//   Manning's n 0.035 channel, 0.060 overbanks; coefficients 0.1 / 0.3
//   3 steady profiles 20, 60, 150 m3/s; normal depth S = 0.001 downstream
//   (and upstream, stored for supercritical/mixed from M2); subcritical plan.
// Build 0.2.2 (M3) adds a synthetic terrain, Terrain/DerasUji1km_terrain.tif:
//   1 m cells in local coordinates (no EPSG) along the schematic centreline
//   x = 0, y = 1000 (RS 1000) .. 0 (RS 0); each row is the RS 0 section shifted
//   by the bed slope (station s at x = 40 - s, left bank towards +x), rising
//   1:10 beyond the section end points. The hydraulic data is unchanged.
// Build 0.2.3 (M6) widens the terrain to x = -400 on the right bank (-x): a
// 1 m levee ridge 10 m beyond the end points, then a floodplain 3.5 m below
// the end points (101.0 m at y = 0, 102.0 m at y = 1000, falling 0.001
// towards the west). Geometry g02 adds a 100 m lateral weir on the right bank
// below RS 800 (crest 104.6 -> 104.5 m, about 0.8 m above the bank top) that
// spills the flood of u02 into the 2D flow area "Dataran Kanan" (15 m cells,
// n 0.06); its BC line "Hilir Dataran" lets the water leave at normal depth.
// Plan p03 runs it. g01, f01, u01, p01 and p02 are unchanged.
// Build 0.2.3 (M7) also stores sediment data in the .deras.json: a 22-day
// quasi-unsteady series on g01 (20, 60, 150, 60 m3/s), Meyer-Peter Mueller,
// sandy gravel d50 2 mm, clear-water inflow (as below a dam), 2 m erodible depth.
#include <cmath>
#include <cstdio>

#include "app/Templates.h"
#include "io/GeoTiff.h"
#include "io/ModuleData.h"
#include "io/Project.h"
#include "io/RasText.h"

using namespace deras;

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: deras-make-uji-standar <output-dir>\n"); return 2; }
    const io::fs::path dir = io::toPath(argv[1]);
    io::ProjectSettings st;
    st.timeZone = "UTC+07:00";
    st.notes = "Proyek uji standar De-RAS (R5). Data sintetis, bukan data lapangan.";
    auto p = io::Project::create(dir, "DerasUji1km", "Sungai Uji 1 km", UnitSystem::SI, st);
    const std::string g = p->addGeometry("Geometri uji 1 km");
    const std::string f = p->addSteadyFlow("Debit uji 20-60-150");
    const std::string pl = p->addPlan("Plan steady subkritis", "Uji1km", g, f);

    app::PrismaticSpec s;
    s.river = "Sungai Uji";
    s.reach = "Utama";
    s.count = 6;
    s.spacing = 200;
    s.bottomWidth = 14;
    s.sideSlope = 1;
    s.bankHeight = 3;
    s.floodplainWidth = 30;
    s.floodplainRise = 1.5;  // 0.5 m (proposal) let Q150 overtop the end points; see TESTING.md S4
    s.bedSlope = 0.001;
    s.downstreamInvert = 100;
    s.nChannel = 0.035;
    s.nOverbank = 0.060;

    auto& geo = p->geometries.at(g).model;
    Reach r;
    r.uid = nextUid();
    r.river = s.river;
    r.reach = s.reach;
    r.xy = {{0, 1000}, {0, 0}};  // schematic centreline, no georeference
    for (int i = 0; i < s.count; ++i) {
        const double dist = 1000 - i * s.spacing;
        r.nodes.push_back(app::trapezoidXs(s, s.downstreamInvert + s.bedSlope * dist, io::formatPlain(dist), i + 1 < s.count ? s.spacing : 0));
    }
    geo.reaches.push_back(r);

    auto& fl = p->steadyFlows.at(f).model;
    fl.profileNames = {"Q20", "Q60", "Q150"};
    FlowChange fc;
    fc.river = s.river;
    fc.reach = s.reach;
    fc.rs = r.nodes.front().rs;
    fc.flows = {20, 60, 150};
    fl.changes.push_back(fc);
    for (int k = 0; k < 3; ++k) {
        auto& b = fl.boundaryRef(s.river, s.reach, k);
        b.downstream.type = BoundaryType::NormalDepth;
        b.downstream.slope = 0.001;
        b.upstream.type = BoundaryType::NormalDepth;
        b.upstream.slope = 0.001;
    }
    // synthetic terrain (M3)
    {
        io::Raster t;
        t.nx = 481;
        t.ny = 1061;
        t.x0 = -400.5;
        t.y0 = 1030.5;
        t.dx = t.dy = 1;
        const auto& se = r.nodes.back().staElev;  // RS 0, invert 100
        auto elev = [&](double st) {
            if (st <= se.front().station) return se.front().elev + 0.1 * (se.front().station - st);
            if (st >= se.back().station) {  // right bank: levee ridge, then the floodplain (M6)
                const double d = st - se.back().station, e = se.back().elev;
                if (d <= 10) return e + 0.1 * d;
                if (d <= 20) return e + 1.0;
                if (d <= 35) return e + 1.0 - 0.3 * (d - 20);
                return e - 3.5 - 0.001 * (d - 35);
            }
            for (size_t i = 1; i < se.size(); ++i)
                if (st <= se[i].station)
                    return se[i - 1].elev + (st - se[i - 1].station) / (se[i].station - se[i - 1].station) * (se[i].elev - se[i - 1].elev);
            return se.back().elev;
        };
        t.z.resize(size_t(t.nx) * t.ny);
        for (int row = 0; row < t.ny; ++row)
            for (int c = 0; c < t.nx; ++c)
                t.z[size_t(row) * t.nx + c] = float(std::round((elev(40 - t.cellX(c)) + s.bedSlope * t.cellY(row)) * 1000) / 1000);
        std::error_code ec;
        io::fs::create_directories(dir / "Terrain", ec);
        std::string err;
        if (!io::writeGeoTiff(dir / "Terrain" / "DerasUji1km_terrain.tif", t, err)) { std::fprintf(stderr, "terrain: %s\n", err.c_str()); return 1; }
        p->settings.terrains = {"Terrain/DerasUji1km_terrain.tif"};
    }
    auto unsteadyPlan = [&](const std::string& title, const std::string& id, const std::string& geom, const std::string& flow) {
        const std::string key = p->addPlan(title, id, geom, flow);
        Plan pu = p->plans.at(key).model;
        pu.simStartDate = "01JAN2024";
        pu.simStartTime = "0000";
        pu.simEndDate = "02JAN2024";
        pu.simEndTime = "0000";
        pu.compInterval = "1MIN";
        pu.outputInterval = "30MIN";
        p->plans.erase(key);
        p->plans.emplace(key, io::PlanFile::createNew(pu, p->units()));
    };
    UnsteadyFlow uf1;
    // unsteady flood (M5): triangular hydrograph 20 -> 150 -> 20 m3/s over 18 hours, normal depth downstream
    {
        const std::string u = p->addUnsteadyFlow("Banjir uji 24 jam");
        auto& uf = p->unsteadyFlows.at(u).model;
        uf.initialFlows.push_back({s.river, s.reach, r.nodes.front().rs, 20});
        UnsteadyBoundary up;
        up.river = s.river;
        up.reach = s.reach;
        up.rs = r.nodes.front().rs;
        up.type = UBoundaryType::FlowHydrograph;
        up.interval = "1HOUR";
        for (int h = 0; h <= 24; ++h) up.values.push_back(std::round((h <= 6 ? 20 + 130.0 * h / 6 : h <= 18 ? 150 - 130.0 * (h - 6) / 12 : 20) * 10) / 10);
        uf.boundaries.push_back(up);
        UnsteadyBoundary dn;
        dn.river = s.river;
        dn.reach = s.reach;
        dn.rs = r.nodes.back().rs;
        dn.type = UBoundaryType::NormalDepth;
        dn.slope = 0.001;
        dn.interval.clear();
        uf.boundaries.push_back(dn);
        unsteadyPlan("Plan unsteady banjir uji", "Banjir24j", g, u);
        uf1 = uf;
    }
    // M6: lateral weir into a 2D floodplain (g02, u02, p03)
    {
        const std::string g2 = p->addGeometry("Geometri uji + dataran 2D");
        auto& gm = p->geometries.at(g2).model;
        Reach r2 = r;
        r2.uid = nextUid();
        for (auto& n : r2.nodes) n.uid = nextUid();
        CrossSection lat;
        lat.uid = nextUid();
        lat.rs = "790";
        lat.type = NodeType::LateralStructure;
        lat.description = "Pelimpah samping tanggul kanan (sintetis)";
        lat.structure.parsed = true;
        lat.structure.dist = 10;
        lat.structure.width = 2;
        lat.structure.weirCoef = 1.1;
        lat.structure.weir = {{0, 104.6}, {100, 104.5}};
        lat.structure.lateral.side = 1;
        lat.structure.lateral.toName = "Dataran Kanan";
        r2.nodes.insert(r2.nodes.begin() + 2, lat);  // between RS 800 and RS 600
        gm.reaches.push_back(r2);
        StorageArea a;
        a.name = "Dataran Kanan";
        a.is2D = true;
        a.cellSize = 15;
        a.manningN = 0.06;
        a.polygon = {{-75, 900}, {-75, 60}, {-390, 60}, {-390, 900}};
        gm.storageAreas.push_back(a);
        BcLine bl;
        bl.name = "Hilir Dataran";
        bl.area = a.name;
        bl.line = {{-385, 65}, {-80, 65}};
        gm.bcLines.push_back(bl);

        const std::string u2 = p->addUnsteadyFlow("Banjir uji 24 jam + dataran 2D");
        auto& uf2 = p->unsteadyFlows.at(u2).model;
        uf2.initialFlows = uf1.initialFlows;
        uf2.boundaries = uf1.boundaries;
        UnsteadyBoundary out;
        out.area = a.name;
        out.bcLine = bl.name;
        out.type = UBoundaryType::NormalDepth;
        out.interval.clear();
        out.slope = 0.001;
        uf2.boundaries.push_back(out);
        unsteadyPlan("Plan unsteady banjir + dataran 2D", "Dataran2D", g2, u2);
    }
    // M7: sediment data on g01 (Analisis Sedimen)
    {
        SedimentSetup sd;
        sd.geomFile = g;
        sd.flow.river = s.river;
        sd.flow.reach = s.reach;
        sd.flow.dsSlope = 0.001;
        sd.flow.records = {{240, 20, 24}, {120, 60, 12}, {48, 150, 6}, {120, 60, 12}};
        sd.data.function = TransportFunction::MeyerPeterMuller;
        sd.data.gradation = {{0.5, 5}, {1.0, 20}, {2.0, 50}, {5.0, 84}, {10.0, 100}};
        sd.data.inflow = SedimentInflow::ClearWater;
        sd.data.maxErodibleDepth = 2.0;
        sd.outputHours = 24;
        io::setSediment(p->settings, sd);
    }
    // M8: water quality on the p02 flood (Analisis Kualitas Air)
    {
        io::WqSetup wq;
        wq.plan = "p02";
        wq.constituent = "Tracer";
        wq.input.initial = 0;
        wq.input.upstream = {{0, 0}, {2, 0}, {3, 100}, {5, 100}, {6, 0}, {24, 0}};
        wq.input.dispersion = 10;
        wq.input.decayPerDay = 0;
        io::setWaterQuality(p->settings, wq);
    }
    p->info().currentPlan = pl;
    p->info().description =
        "Proyek uji standar De-RAS (data sintetis, bukan data lapangan).\n"
        "Sungai 1 km, penampang setiap 200 m (RS 1000 sampai 0), saluran utama lebar 20 m dan dalam 3 m, "
        "bantaran 30 m di kiri dan kanan. Kemiringan dasar 0,001; n alur 0,035, n bantaran 0,060.\n"
        "Debit 20, 60, dan 150 m3/s dengan batas hilir kedalaman normal. Dipakai untuk menguji setiap fitur analisis.\n"
        "Plan p02: banjir unsteady 24 jam (hidrograf segitiga 20-150-20 m3/s).\n"
        "Plan p03 (Pratinjau M6): banjir yang sama, pelimpah samping 100 m di tanggul kanan mengalirkan air ke area 2D \"Dataran Kanan\".\n"
        "Analisis Sedimen (Pratinjau M7): 22 hari aliran quasi-unsteady di g01, kerikil berpasir d50 2 mm, air jernih dari hulu (seperti di hilir bendungan).\n"
        "Kualitas Air (Pratinjau M8): pelacak 100 mg/L selama 3 jam masuk di hulu saat banjir p02; hitung Analisis Unsteady p02 dulu.";
    MessageList msgs;
    if (!p->save(msgs)) {
        for (const auto& m : msgs) std::fprintf(stderr, "%s\n", m.text(Language::EN).c_str());
        return 1;
    }
    std::printf("written %s\n", io::fromPath(p->prjPath()).c_str());
    return 0;
}

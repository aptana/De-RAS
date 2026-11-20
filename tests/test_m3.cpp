// M3 tests (build 0.2.2): GeoTIFF reader/writer and RAS Mapper computations.
//  G1 GeoTIFF fixtures (none/LZW/Deflate strips, Deflate tiles with the
//     floating-point predictor, big-endian int16 with predictor 2): size,
//     georeference, EPSG, values and no data
//  G2 write -> read round trip; G3 bilinear sampling of a planar surface
//  C1 cut lines placed across a schematic centreline
//  S1 sampling a cross section from a terrain built from that section
//  I1 inundation of a prismatic channel: depth, wet width, extent
//  F1 XS GIS Cut Line read/write round trip, unchanged lines kept
#include "TestMain.h"
#include "app/Templates.h"
#include "engines/mapping/Mapping.h"
#include "io/GeoTiff.h"
#include "io/GeometryFile.h"
#include <cstdio>
using namespace deras;

namespace {
const std::string kDir = std::string(DERAS_TEST_DATA) + "/geotiff/";
double zFix(int c, int r) { return 100 + 0.25 * c - 0.5 * r; }

// Standard-project-like reach: centreline (0,1000) -> (0,0), RS 1000..0 every 200 m.
Reach testReach() {
    app::PrismaticSpec s;
    s.river = "Sungai Uji";
    s.reach = "Utama";
    s.count = 6;
    s.spacing = 200;
    s.bottomWidth = 14;
    s.sideSlope = 1;
    s.bankHeight = 3;
    s.floodplainWidth = 30;
    s.floodplainRise = 1.5;
    s.bedSlope = 0.001;
    s.downstreamInvert = 100;
    Reach r;
    r.river = s.river;
    r.reach = s.reach;
    r.xy = {{0, 1000}, {0, 0}};
    for (int i = 0; i < s.count; ++i) {
        const double d = 1000 - i * 200.0;
        r.nodes.push_back(app::trapezoidXs(s, 100 + 0.001 * d, io::formatPlain(d), i + 1 < s.count ? 200 : 0));
    }
    return r;
}

double xsElev(const CrossSection& xs, double st) {
    const auto& p = xs.staElev;
    if (st <= p.front().station) return p.front().elev + 0.1 * (p.front().station - st);
    if (st >= p.back().station) return p.back().elev + 0.1 * (st - p.back().station);
    for (size_t i = 1; i < p.size(); ++i)
        if (st <= p[i].station) {
            const double t = (st - p[i - 1].station) / (p[i].station - p[i - 1].station);
            return p[i - 1].elev + t * (p[i].elev - p[i - 1].elev);
        }
    return p.back().elev;
}

// Terrain whose cross sections equal the reach sections (x = 40 - station), cell centres on integers.
io::Raster testTerrain(const Reach& r) {
    io::Raster t;
    t.nx = 121;
    t.ny = 1041;
    t.x0 = -60.5;
    t.y0 = 1020.5;
    t.dx = t.dy = 1;
    t.epsg = 32748;
    const CrossSection& xs0 = r.nodes.back();  // RS 0, invert 100
    t.z.resize(size_t(t.nx) * t.ny);
    for (int row = 0; row < t.ny; ++row)
        for (int c = 0; c < t.nx; ++c) {
            const double x = t.cellX(c), y = t.cellY(row);
            t.z[size_t(row) * t.nx + c] = float(xsElev(xs0, 40 - x) + 0.001 * y);
        }
    return t;
}
}  // namespace

TEST(G1_geotiff_fixtures) {
    for (const char* f : {"strip_none.tif", "strip_lzw.tif", "strip_deflate.tif", "tile_deflate_pred3.tif", "strip_int16_pred2_be.tif"}) {
        io::Raster r;
        std::string err;
        const bool ok = io::readGeoTiff(io::toPath(kDir + f), r, err);
        if (!ok) std::printf("  %s: %s\n", f, err.c_str());
        CHECK(ok);
        if (!ok) continue;
        const bool i16 = std::string(f).find("int16") != std::string::npos;
        CHECK(r.nx == 37 && r.ny == 23);
        CHECK_NEAR(r.x0, 500000, 1e-9);
        CHECK_NEAR(r.y0, 9100000, 1e-9);
        CHECK_NEAR(r.dx, 2, 1e-12);
        CHECK(r.epsg == 32748);
        CHECK(std::isnan(r.cell(2, 1)));
        double worst = 0;
        for (int row = 0; row < r.ny; ++row)
            for (int c = 0; c < r.nx; ++c) {
                if (c == 2 && row == 1) continue;
                const double v = i16 ? r.cell(c, row) / 10.0 + 90 : r.cell(c, row);
                worst = std::max(worst, std::fabs(v - zFix(c, row)));
            }
        CHECK(worst <= (i16 ? 0.051 : 1e-5));
        std::printf("  %s: max error %.2g\n", f, worst);
    }
}

TEST(G2_geotiff_roundtrip) {
    io::Raster r;
    std::string err;
    CHECK(io::readGeoTiff(io::toPath(kDir + "tile_deflate_pred3.tif"), r, err));
    io::Raster b;
    CHECK(io::parseGeoTiff(io::buildGeoTiff(r), b, err));
    CHECK(b.nx == r.nx && b.ny == r.ny && b.epsg == 32748);
    CHECK_NEAR(b.x0, r.x0, 0);
    CHECK_NEAR(b.y0, r.y0, 0);
    bool same = true;
    for (size_t i = 0; i < r.z.size(); ++i)
        if (!(r.z[i] == b.z[i] || (std::isnan(r.z[i]) && std::isnan(b.z[i])))) same = false;
    CHECK(same);
}

TEST(G3_bilinear_sampling) {
    io::Raster r;
    std::string err;
    CHECK(io::readGeoTiff(io::toPath(kDir + "strip_none.tif"), r, err));
    CHECK_NEAR(r.sample(500000 + 10.3, 9100000 - 7.7), 100 + 0.25 * 4.65 - 0.5 * 3.35, 1e-5);
    CHECK(std::isnan(r.sample(499999, 9100000 - 5)));
}

TEST(C1_cut_lines_across_centreline) {
    Reach r = testReach();
    auto cl = mapping::cutLines(r);
    CHECK(cl.size() == 6);
    if (cl.size() != 6) return;
    // flowing towards -y: the left bank is +x; channel centre (station 40) on the centreline
    CHECK_NEAR(cl[0].line.front().x, 40, 1e-9);
    CHECK_NEAR(cl[0].line.front().y, 1000, 1e-9);
    CHECK_NEAR(cl[0].line.back().x, -40, 1e-9);
    CHECK_NEAR(cl[2].s, 400, 1e-9);
    CHECK_NEAR(cl[2].line.front().y, 600, 1e-9);
    CHECK(!cl[0].fromFile);
}

TEST(S1_section_from_terrain) {
    Reach r = testReach();
    std::vector<io::Raster> t{testTerrain(r)};
    auto cl = mapping::cutLines(r);
    const CrossSection& xs = r.nodes[2];  // RS 600
    int missing = -1;
    auto pts = mapping::sampleLine(t, cl[2].line, 1.0, 500, &missing);
    CHECK(missing == 0);
    CHECK(pts.size() == xs.staElev.size());
    for (size_t i = 0; i < std::min(pts.size(), xs.staElev.size()); ++i) {
        CHECK_NEAR(pts[i].station, xs.staElev[i].station, 1e-9);
        CHECK_NEAR(pts[i].elev, xs.staElev[i].elev, 0.006);
    }
}

TEST(I1_inundation_prismatic) {
    Reach r = testReach();
    Geometry g;
    g.reaches.push_back(r);
    std::vector<io::Raster> t{testTerrain(r)};
    ProfileResult pr;
    pr.name = "D2";
    for (const auto& n : r.nodes) {
        XsResult x;
        x.river = r.river;
        x.reach = r.reach;
        x.rs = n.rs;
        x.ws = n.channelInvert() + 2.0;
        pr.xs.push_back(x);
    }
    std::string err;
    auto m = mapping::inundation(g, pr, t, err);
    CHECK(err.empty());
    CHECK(m.reachesMapped == 1);
    CHECK_NEAR(m.maxDepth, 2.0, 1e-4);
    // row through y = 500, cell centres on integers: wet where |x| < 9 (top width 18 m)
    const int row = int(std::floor((m.depth.y0 - 500) / m.depth.dy));
    int wet = 0;
    bool outside = false;
    for (int c = 0; c < m.depth.nx; ++c) {
        if (std::isnan(m.depth.cell(c, row))) continue;
        ++wet;
        if (std::fabs(m.depth.cellX(c)) >= 9) outside = true;
    }
    CHECK(wet == 17);
    CHECK(!outside);
    const int c0 = int(std::floor((0 - m.depth.x0) / m.depth.dx));
    CHECK_NEAR(m.depth.cell(c0, row), 2.0, 1e-4);
    CHECK_NEAR(m.wse.cell(c0, row), 100 + 0.001 * m.depth.cellY(row) + 2.0, 1e-4);
    CHECK(m.depth.epsg == 32748);
    std::printf("  wet cells %ld, area %.0f m2\n", m.wetCells, m.wetArea);
}

TEST(F1_cut_line_file_roundtrip) {
    auto gf = io::GeometryFile::createNew("Uji", "7.00");
    Reach r = testReach();
    r.nodes[1].cutLine = {{500000.125, 9100000.5}, {500010, 9100020}, {500030.75, 9100031}};
    gf.model.reaches.push_back(r);
    const std::string text = gf.serialize();
    CHECK(text.find("XS GIS Cut Line=3") != std::string::npos);
    io::GeometryFile b;
    MessageList msgs;
    CHECK(b.parse(text, "uji.g01", msgs));
    CHECK(b.model.reaches.size() == 1);
    if (b.model.reaches.size() != 1) return;
    CHECK(b.model.reaches[0].nodes[1].cutLine == r.nodes[1].cutLine);
    CHECK(b.model.reaches[0].nodes[0].cutLine.empty());
    CHECK(!b.isModified());
    CHECK(b.serialize() == text);
    b.model.reaches[0].nodes[1].cutLine[0].first = 499999.5;
    io::GeometryFile c;
    CHECK(c.parse(b.serialize(), "uji.g01", msgs));
    CHECK_NEAR(c.model.reaches[0].nodes[1].cutLine[0].first, 499999.5, 1e-9);
    CHECK(c.model.reaches[0].nodes[1].cutLine.size() == 3);
}

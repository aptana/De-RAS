// De-RAS - engines/unsteady/UnsteadySolver.cpp
#include "UnsteadySolver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>

#include "Flow2D.h"
#include "engines/common/XsHydraulics.h"
#include "engines/mapping/Mapping.h"
#include "engines/steady/SteadySolver.h"

namespace deras::unsteady {

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

std::string fmt(double v, int d) {
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", d, v);
    return b;
}

bool sameRs(const std::string& a, const std::string& b) {
    if (a == b) return true;
    const double x = rsNumber(a), y = rsNumber(b);
    return !std::isnan(x) && !std::isnan(y) && std::fabs(x - y) < 1e-9;
}

// Hydraulic property table of one cross section (linear interpolation in z).
struct Table {
    double z0 = 0, dz = 1;
    std::vector<double> A, K, B, T;
    void build(const hyd::XsGeom& g) {
        double zmax = g.minEl;
        for (const auto& p : g.pts) zmax = std::max(zmax, p.elev);
        const double range = std::max(zmax - g.minEl, 0.1);
        z0 = g.minEl;
        const double top = zmax + 2 * range;
        const int n = 400;
        dz = (top - z0) / n;
        A.resize(n + 1);
        K.resize(n + 1);
        B.resize(n + 1);
        T.resize(n + 1);
        for (int i = 0; i <= n; ++i) {
            const auto p = hyd::properties(g, z0 + i * dz);
            A[size_t(i)] = p.area;
            K[size_t(i)] = p.conveyance;
            B[size_t(i)] = std::clamp(p.alpha, 1.0, 2.0);
            T[size_t(i)] = p.topWidth;
        }
    }
    double top() const { return z0 + dz * double(A.size() - 1); }
    void eval(double z, double& a, double& k, double& b) const {
        const double x = (z - z0) / dz;
        const int n = int(A.size()) - 1;
        int i = int(std::floor(x));
        i = std::clamp(i, 0, n - 1);
        const double t = x - i;  // may be outside [0,1]: linear extrapolation
        auto li = [&](const std::vector<double>& v) { return v[size_t(i)] + t * (v[size_t(i) + 1] - v[size_t(i)]); };
        a = std::max(li(A), 1e-6);
        k = std::max(li(K), 1e-6);
        b = std::clamp(li(B), 1.0, 2.0);
    }
};

struct Node {
    const CrossSection* xs = nullptr;
    hyd::XsGeom g;
    Table tab;
    double dx = 0;      // channel length to the next node downstream
    double dist = 0;    // channel distance from the downstream end of the reach
    double zMin = 0;
};

struct Aff {  // c + p * parameter
    double c = 0, p = 0;
};

struct RReach {
    const Reach* reach = nullptr;
    std::vector<Node> n;
    std::vector<double> z, q, zOld, qOld;     // state at the new / old time level
    std::vector<double> ql, qlOld;             // lateral inflow per unit length on each segment
    std::vector<double> qp;                    // pump flow per unit length on each segment (this step)
    std::vector<std::vector<int>> segPieces;   // lateral weir pieces on each segment
    int upJ = -1, dnJ = -1;
    const UnsteadyBoundary* upBC = nullptr;
    const UnsteadyBoundary* dnBC = nullptr;
    struct Lat { const UnsteadyBoundary* b; size_t i1, i2; };
    std::vector<Lat> lats;
    // linearised system of the current Newton iteration
    struct Seg { double P[2][2], R[2][2], c[2]; };
    std::vector<Seg> seg;
    std::vector<double> fa, fb;  // relation alpha dz + beta dQ = gamma (downstream-going sweep), gamma affine in a
    std::vector<Aff> fg;
    Aff dQdn;            // dQ at the downstream end = c + p a + (coef) b -> stored separately
    double dQdnB = 0;
    Aff dQup;            // dQ at the upstream end = c + p b  + (coef) a
    double dQupA = 0;
};

double hydroValue(const std::vector<double>& v, double stepHours, double tHours) {
    if (v.empty() || !(stepHours > 0)) return kNaN;
    const double x = tHours / stepHours;
    if (x <= 0) return v.front();
    const size_t i = size_t(std::floor(x));
    if (i + 1 >= v.size()) return v.back();
    const double t = x - double(i);
    return v[i] + t * (v[i + 1] - v[i]);
}

double ratingFlow(const std::vector<std::pair<double, double>>& rating, double z, double& dqdz) {
    auto r = rating;
    std::sort(r.begin(), r.end());
    dqdz = 0;
    if (r.size() < 2) return kNaN;
    size_t i = 1;
    while (i + 1 < r.size() && r[i].first < z) ++i;
    const auto& a = r[i - 1];
    const auto& b = r[i];
    if (b.first == a.first) return a.second;
    dqdz = (b.second - a.second) / (b.first - a.first);
    return std::max(0.0, a.second + dqdz * (z - a.first));
}

// Dense Gaussian elimination with partial pivoting (small global system).
bool solveDense(std::vector<std::vector<double>>& M, std::vector<double>& x) {
    const size_t n = x.size();
    for (size_t k = 0; k < n; ++k) {
        size_t p = k;
        for (size_t i = k + 1; i < n; ++i) if (std::fabs(M[i][k]) > std::fabs(M[p][k])) p = i;
        if (std::fabs(M[p][k]) < 1e-300) return false;
        std::swap(M[p], M[k]);
        std::swap(x[p], x[k]);
        for (size_t i = k + 1; i < n; ++i) {
            const double f = M[i][k] / M[k][k];
            if (f == 0) continue;
            for (size_t j = k; j < n; ++j) M[i][j] -= f * M[k][j];
            x[i] -= f * x[k];
        }
    }
    for (size_t k = n; k-- > 0;) {
        double s = x[k];
        for (size_t j = k + 1; j < n; ++j) s -= M[k][j] * x[j];
        x[k] = s / M[k][k];
    }
    return true;
}

// Storage area at run time (level pool).
struct SArt {
    const StorageArea* a = nullptr;
    std::vector<std::pair<double, double>> ve;  // (elevation, volume), elevation increasing
    double zMin = 0, z = 0, zOld = 0, planArea = 0;
    const UnsteadyBoundary* stage = nullptr;
    std::vector<const UnsteadyBoundary*> inflow, rain;
    double pumpQ = 0;
    double V(double zz) const {
        if (ve.size() < 2 || zz <= ve.front().first) return 0;
        for (size_t i = 1; i < ve.size(); ++i)
            if (zz <= ve[i].first) {
                const double t = (zz - ve[i - 1].first) / std::max(ve[i].first - ve[i - 1].first, 1e-12);
                return ve[i - 1].second + t * (ve[i].second - ve[i - 1].second);
            }
        const auto& a1 = ve[ve.size() - 2];
        const auto& a2 = ve.back();
        const double slope = (a2.second - a1.second) / std::max(a2.first - a1.first, 1e-12);
        return a2.second + slope * (zz - a2.first);
    }
};

// One piece of a weir: a part of a lateral structure (reach to area) or of a
// storage-area connection (area to area). Positive flow goes from the up
// side to the down side.
struct Piece {
    int owner = 0;                 // index in the owner list
    int r = -1;                    // lateral: reach index, segment and position on the segment
    size_t j = 0;
    double t = 0;
    int upKind = 0, upIdx = -1, upCell = -1;  // connection up side: 1 storage area, 2 2D area
    int dnKind = 0, dnIdx = -1, dnCell = -1;  // 0 out of the system
    double len = 0, crest = 0, crestNow = 0, sta = 0, coef = 0;
};

struct Owner {
    std::string kind, name;
    const Breach* breach = nullptr;
    double q = 0;
    size_t series = 0;
};

double polygonArea(const std::vector<XY>& p) {
    double a = 0;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) a += (p[j].first + p[i].first) * (p[j].second - p[i].second);
    return std::fabs(a) / 2;
}

// Incremental precipitation depth per interval -> rate in length per second.
double rainRate(const UnsteadyBoundary& b, double tHours, UnitSystem u) {
    const double sec = intervalSeconds(b.interval);
    if (b.values.empty() || !(sec > 0)) return 0;
    size_t i = size_t(std::floor(tHours * 3600.0 / sec - 1e-9)) + 1;
    if (tHours <= 0) i = 0;
    if (i >= b.values.size()) return 0;
    const double depth = std::max(0.0, b.values[i]) / (u == UnitSystem::SI ? 1000.0 : 12.0);
    return depth / sec;
}

}  // namespace

double weirFlow(double zu, double zd, double crest, double coef, double length) {
    if (!(length > 0) || !(coef > 0) || !std::isfinite(zu)) return 0;
    double sgn = 1;
    if (std::isfinite(zd) && zd > zu) { std::swap(zu, zd); sgn = -1; }
    const double H = zu - crest;
    if (H <= 0) return 0;
    double q = coef * length * std::pow(H, 1.5);
    if (std::isfinite(zd) && zd > crest) {
        // Villemonte; linear to zero above s = 0.98 (finite slope near equal levels)
        const double s = std::min((zd - crest) / H, 1.0);
        const double s0 = 0.98, f0 = std::pow(1.0 - std::pow(s0, 1.5), 0.385);
        q *= s <= s0 ? std::pow(1.0 - std::pow(s, 1.5), 0.385) : f0 * (1.0 - s) / (1.0 - s0);
    }
    return sgn * q;
}

double seriesHours(const UnsteadyBoundary& b) {
    const double h = intervalSeconds(b.interval) / 3600.0;
    return b.values.empty() || !(h > 0) ? 0 : h * double(b.values.size() - 1);
}

double seriesValue(const UnsteadyBoundary& b, double tHours) {
    return hydroValue(b.values, intervalSeconds(b.interval) / 3600.0, tHours);
}

const UnsteadyBoundary* endBoundary(const UnsteadyFlow& f, const Reach& r, bool upstream) {
    const auto xs = r.crossSections();
    if (xs.empty()) return nullptr;
    const std::string& rs = upstream ? xs.front()->rs : xs.back()->rs;
    for (const auto& b : f.boundaries) {
        if (b.river != r.river || b.reach != r.reach) continue;
        if (b.type == UBoundaryType::LateralInflow || b.type == UBoundaryType::UniformLateralInflow || b.type == UBoundaryType::None ||
            b.type == UBoundaryType::Other)
            continue;
        if (sameRs(b.rs, rs)) return &b;
    }
    return nullptr;
}

RunResult solve(const Geometry& geom, const UnsteadyFlow& flow, const Plan& plan, UnitSystem units, const Callbacks& cb, const Inputs& in) {
    RunResult res;
    res.status = RunStatus::Failed;
    res.meta.method = "preissmann-implicit-unsteady";
    const double g = constantsFor(units).gravity;
    const double hMin = units == UnitSystem::SI ? 0.01 : 0.03;
    const double dzLimit = units == UnitSystem::SI ? 1.0 : 3.0;
    const double theta = std::clamp(plan.theta, 0.5, 1.0);

    // ---- network ----
    std::vector<RReach> R;
    for (const auto& r : geom.reaches) {
        RReach rr;
        rr.reach = &r;
        for (const CrossSection* x : r.crossSections()) {
            Node nd;
            nd.xs = x;
            nd.g = hyd::XsGeom::from(*x, units);
            nd.tab.build(nd.g);
            nd.zMin = nd.g.minEl;
            nd.dx = x->lenChannel;
            rr.n.push_back(std::move(nd));
        }
        if (rr.n.size() < 2) continue;
        for (size_t i = rr.n.size() - 1; i-- > 0;) rr.n[i].dist = rr.n[i + 1].dist + rr.n[i].dx;
        rr.upBC = endBoundary(flow, r, true);
        rr.dnBC = endBoundary(flow, r, false);
        for (const auto& b : flow.boundaries) {
            if (b.river != r.river || b.reach != r.reach) continue;
            if (b.type != UBoundaryType::LateralInflow && b.type != UBoundaryType::UniformLateralInflow) continue;
            size_t i1 = rr.n.size(), i2 = rr.n.size();
            for (size_t i = 0; i < rr.n.size(); ++i) {
                if (sameRs(rr.n[i].xs->rs, b.rs)) i1 = i;
                if (!b.rsEnd.empty() && sameRs(rr.n[i].xs->rs, b.rsEnd)) i2 = i;
            }
            if (i1 >= rr.n.size()) continue;
            if (b.type == UBoundaryType::LateralInflow || i2 >= rr.n.size() || i2 <= i1) {
                const size_t s = std::min(i1, rr.n.size() - 2);
                rr.lats.push_back({&b, s, s + 1});
            } else {
                rr.lats.push_back({&b, i1, i2});
            }
        }
        R.push_back(std::move(rr));
    }
    if (R.empty() && geom.storageAreas.empty()) {
        res.messages.push_back(makeMsg(Severity::Error, "MD_NO_REACH"));
        return res;
    }
    auto findR = [&](const std::string& river, const std::string& reach) {
        for (size_t i = 0; i < R.size(); ++i) if (R[i].reach->river == river && R[i].reach->reach == reach) return int(i);
        return -1;
    };
    struct JEnd { int r; bool dn; };  // dn = the reach's downstream end joins the junction
    std::vector<std::vector<JEnd>> J;
    for (const auto& j : geom.junctions) {
        std::vector<JEnd> m;
        for (const auto& u : j.up) if (int i = findR(u.first, u.second); i >= 0) { m.push_back({i, true}); R[size_t(i)].dnJ = int(J.size()); }
        for (const auto& d : j.dn) if (int i = findR(d.first, d.second); i >= 0) { m.push_back({i, false}); R[size_t(i)].upJ = int(J.size()); }
        J.push_back(m);
    }


    // ---- M6: storage areas, 2D flow areas, lateral structures, connections, pumps ----
    static const std::vector<io::Raster> kNoTerrain;
    const std::vector<io::Raster>& terrains = in.terrains ? *in.terrains : kNoTerrain;
    std::vector<SArt> SA;
    std::vector<std::unique_ptr<flow2d::Area>> A2;
    std::vector<int> areaKind(geom.storageAreas.size(), 0), areaIdx(geom.storageAreas.size(), -1);
    auto areaRef = [&](const std::string& name, int& kind, int& idx) {
        kind = 0;
        idx = -1;
        for (size_t i = 0; i < geom.storageAreas.size(); ++i)
            if (geom.storageAreas[i].name == name && areaIdx[i] >= 0) { kind = areaKind[i]; idx = areaIdx[i]; return true; }
        return false;
    };
    for (size_t i = 0; i < geom.storageAreas.size(); ++i) {
        const StorageArea& a = geom.storageAreas[i];
        if (a.is2D) {
            auto ar = std::make_unique<flow2d::Area>();
            std::string err;
            if (!ar->build(a, terrains, units, err)) {
                res.messages.push_back(makeMsg(Severity::Error, "SOLV_2D_BUILD_FAIL", {a.name, err}));
                return res;
            }
            areaKind[i] = 2;
            areaIdx[i] = int(A2.size());
            A2.push_back(std::move(ar));
            continue;
        }
        SArt s;
        s.a = &a;
        if (a.volElev.size() >= 2) {
            s.ve = a.volElev;
            std::sort(s.ve.begin(), s.ve.end());
        } else if (a.area > 0) {
            s.ve = {{a.minElev, 0}, {a.minElev + 1000, a.area * 1000}};
        } else if (a.polygon.size() >= 3 && !terrains.empty()) {
            // elevation-volume curve from the terrain inside the polygon
            mapping::Line poly;
            double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
            for (const auto& p : a.polygon) {
                poly.push_back({p.first, p.second});
                x0 = std::min(x0, p.first); x1 = std::max(x1, p.first);
                y0 = std::min(y0, p.second); y1 = std::max(y1, p.second);
            }
            const int ns = 150;
            const double sx = (x1 - x0) / ns, sy = (y1 - y0) / ns;
            std::vector<double> e;
            for (int iy = 0; iy < ns; ++iy)
                for (int ix = 0; ix < ns; ++ix) {
                    const mapping::Pt pt{x0 + (ix + 0.5) * sx, y0 + (iy + 0.5) * sy};
                    if (!mapping::insidePolygon(poly, pt)) continue;
                    const double z = mapping::terrainAt(terrains, pt.x, pt.y);
                    if (std::isfinite(z)) e.push_back(z);
                }
            if (!e.empty()) {
                std::sort(e.begin(), e.end());
                const double lo = e.front(), hi = std::max(e.back(), lo + 0.1), ca = sx * sy;
                for (int k = 0; k <= 60; ++k) {
                    const double zz = lo + (hi - lo) * k / 60.0;
                    double v = 0;
                    for (double x : e) if (zz > x) v += (zz - x) * ca;
                    s.ve.emplace_back(zz, v);
                }
                s.ve.emplace_back(hi + (hi - lo), s.ve.back().second + double(e.size()) * ca * (hi - lo));
            }
        }
        if (s.ve.size() < 2) {
            res.messages.push_back(makeMsg(Severity::Error, "SOLV_SA_NO_VOLUME", {a.name}));
            return res;
        }
        s.zMin = s.ve.front().first;
        s.planArea = a.polygon.size() >= 3 ? polygonArea(a.polygon) : a.area;
        areaKind[i] = 1;
        areaIdx[i] = int(SA.size());
        SA.push_back(std::move(s));
    }
    auto zOf = [&](int kind, int idx, int cell) -> double {
        if (kind == 1) return SA[size_t(idx)].z;
        if (kind == 2 && cell >= 0) return A2[size_t(idx)]->z[size_t(cell)];
        return kNaN;
    };
    auto zMinOf = [&](int kind, int idx, int cell) -> double {
        if (kind == 1) return SA[size_t(idx)].zMin;
        if (kind == 2 && cell >= 0) return A2[size_t(idx)]->cell(size_t(cell)).zMin;
        return kNaN;
    };
    // boundary conditions of storage and 2D areas
    struct Bc2D { int area; const UnsteadyBoundary* b; flow2d::BoundaryCells cells; };
    std::vector<Bc2D> bc2d;
    for (const auto& b : flow.boundaries) {
        if (b.area.empty()) continue;
        int k, idx;
        if (!areaRef(b.area, k, idx)) {
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_AREA_BC_UNUSED", {b.area, b.bcLine.empty() ? "-" : b.bcLine}));
            continue;
        }
        if (k == 1) {
            SArt& s = SA[size_t(idx)];
            if (b.type == UBoundaryType::StageHydrograph) s.stage = &b;
            else if (b.type == UBoundaryType::FlowHydrograph || b.type == UBoundaryType::LateralInflow || b.type == UBoundaryType::UniformLateralInflow) s.inflow.push_back(&b);
            else if (b.type == UBoundaryType::Precipitation) s.rain.push_back(&b);
            else res.messages.push_back(makeMsg(Severity::Warning, "SOLV_AREA_BC_UNUSED", {b.area, "-"}));
            continue;
        }
        Bc2D x{idx, &b, {}};
        if (!b.bcLine.empty()) {
            const BcLine* bl = nullptr;
            for (const auto& l : geom.bcLines) if (l.name == b.bcLine && (l.area.empty() || l.area == b.area)) bl = &l;
            if (bl) x.cells = A2[size_t(idx)]->boundaryCells(bl->line, terrains);
            if (x.cells.cell.empty()) {
                res.messages.push_back(makeMsg(Severity::Warning, "SOLV_AREA_BC_UNUSED", {b.area, b.bcLine}));
                continue;
            }
        } else if (b.type == UBoundaryType::StageHydrograph || b.type == UBoundaryType::NormalDepth) {
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_AREA_BC_UNUSED", {b.area, "-"}));
            continue;
        }
        bc2d.push_back(std::move(x));
    }
    // weir pieces of lateral structures and storage-area connections
    std::vector<Owner> owners;
    std::vector<Piece> pieces;
    const double defCoef = units == UnitSystem::SI ? 1.1 : 2.0;
    bool noGeoWarned = false;
    for (size_t ri = 0; ri < R.size(); ++ri) {
        RReach& rr = R[ri];
        const Reach& r = *rr.reach;
        const size_t N = rr.n.size();
        rr.qp.assign(N - 1, 0.0);
        rr.segPieces.assign(N - 1, {});
        std::vector<mapping::CutLine> cuts;
        bool cutsDone = false;
        int xsBefore = -1;
        for (size_t k = 0; k < r.nodes.size(); ++k) {
            const CrossSection& nd = r.nodes[k];
            if (nd.type == NodeType::CrossSection) { ++xsBefore; continue; }
            if (nd.type != NodeType::LateralStructure) continue;
            const StructureData& st = nd.structure;
            int tk = 0, ti = -1;
            if (st.weir.size() < 2 || (!st.lateral.toName.empty() && !areaRef(st.lateral.toName, tk, ti))) {
                res.messages.push_back(makeMsg(Severity::Warning, "SOLV_LAT_SKIPPED", {r.label(), nd.rs}));
                continue;
            }
            const int oi = int(owners.size());
            owners.push_back({"lateral", r.label() + " RS " + nd.rs, &st.lateral.breach, 0, 0});
            const double sta0 = st.weir.front().station, total = st.weir.back().station - sta0;
            const double maxLen = std::max(total / 40.0, 1e-6);
            const size_t iu = std::min(size_t(std::max(xsBefore, 0)), N - 2);
            for (size_t w = 0; w + 1 < st.weir.size(); ++w) {
                const double Ls = st.weir[w + 1].station - st.weir[w].station;
                if (!(Ls > 0)) continue;
                const int m = std::max(1, int(std::ceil(Ls / maxLen)));
                for (int q = 0; q < m; ++q) {
                    Piece pc;
                    pc.owner = oi;
                    pc.r = int(ri);
                    const double f = (q + 0.5) / m;
                    pc.sta = st.weir[w].station + f * Ls;
                    pc.crest = pc.crestNow = st.weir[w].elev + f * (st.weir[w + 1].elev - st.weir[w].elev);
                    pc.len = Ls / m;
                    pc.coef = st.weirCoef > 0 ? st.weirCoef : defCoef;
                    double rem = st.dist + (pc.sta - sta0);
                    size_t j = iu;
                    while (j < N - 2 && rem > rr.n[j].dx) { rem -= rr.n[j].dx; ++j; }
                    pc.j = j;
                    pc.t = std::clamp(rem / std::max(rr.n[j].dx, 1e-9), 0.0, 1.0);
                    pc.dnKind = tk;
                    pc.dnIdx = ti;
                    if (tk == 2) {
                        if (!cutsDone) { cuts = mapping::cutLines(r); cutsDone = true; }
                        auto bank = [&](size_t nn, mapping::Pt& pt) {
                            for (const auto& c : cuts)
                                if (c.nodeIndex >= 0 && &r.nodes[size_t(c.nodeIndex)] == rr.n[nn].xs && c.line.size() >= 2) {
                                    pt = st.lateral.side == 0 ? c.line.front() : c.line.back();
                                    return true;
                                }
                            return false;
                        };
                        mapping::Pt p1, p2;
                        if (!r.xy.empty() && bank(j, p1) && bank(j + 1, p2)) {
                            pc.dnCell = A2[size_t(ti)]->nearestCell(p1.x + pc.t * (p2.x - p1.x), p1.y + pc.t * (p2.y - p1.y));
                        } else {
                            pc.dnCell = A2[size_t(ti)]->lowestCell();
                            if (!noGeoWarned) res.messages.push_back(makeMsg(Severity::Warning, "SOLV_LAT_NO_GEO", {r.label(), nd.rs}));
                            noGeoWarned = true;
                        }
                    }
                    rr.segPieces[j].push_back(int(pieces.size()));
                    pieces.push_back(pc);
                }
            }
        }
    }
    for (const auto& c : geom.connections) {
        int uk, ui, dk, di;
        if (c.weir.size() < 2 || !areaRef(c.upName, uk, ui) || !areaRef(c.dnName, dk, di)) {
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_CONN_SKIPPED", {c.name}));
            continue;
        }
        const int oi = int(owners.size());
        owners.push_back({"connection", c.name, &c.breach, 0, 0});
        mapping::Line l;
        for (const auto& p : c.line) l.push_back({p.first, p.second});
        const double L = mapping::length(l);
        const double sta0 = c.weir.front().station, total = std::max(c.weir.back().station - sta0, 1e-9);
        const double maxLen = std::max(total / 40.0, 1e-6);
        auto cellFor = [&](int kind, int idx, double sta) {
            if (kind != 2) return -1;
            if (l.size() >= 2) {
                const auto pt = mapping::pointAt(l, (sta - sta0) / total * L);
                return A2[size_t(idx)]->nearestCell(pt.x, pt.y);
            }
            return A2[size_t(idx)]->lowestCell();
        };
        for (size_t w = 0; w + 1 < c.weir.size(); ++w) {
            const double Ls = c.weir[w + 1].station - c.weir[w].station;
            if (!(Ls > 0)) continue;
            const int m = std::max(1, int(std::ceil(Ls / maxLen)));
            for (int q = 0; q < m; ++q) {
                Piece pc;
                pc.owner = oi;
                const double f = (q + 0.5) / m;
                pc.sta = c.weir[w].station + f * Ls;
                pc.crest = pc.crestNow = c.weir[w].elev + f * (c.weir[w + 1].elev - c.weir[w].elev);
                pc.len = Ls / m;
                pc.coef = c.coef > 0 ? c.coef : defCoef;
                pc.upKind = uk; pc.upIdx = ui; pc.upCell = cellFor(uk, ui, pc.sta);
                pc.dnKind = dk; pc.dnIdx = di; pc.dnCell = cellFor(dk, di, pc.sta);
                pieces.push_back(pc);
            }
        }
    }
    auto pieceUpZ = [&](const Piece& pc) {
        if (pc.r >= 0) {
            const RReach& rr = R[size_t(pc.r)];
            return rr.z[pc.j] + pc.t * (rr.z[pc.j + 1] - rr.z[pc.j]);
        }
        return zOf(pc.upKind, pc.upIdx, pc.upCell);
    };
    auto pieceQ = [&](const Piece& pc) { return weirFlow(pieceUpZ(pc), zOf(pc.dnKind, pc.dnIdx, pc.dnCell), pc.crestNow, pc.coef, pc.len); };
    // pieces touching each storage area / 2D area: (piece, +1 receives / -1 gives, cell)
    struct Touch { int piece; double side; int cell; };
    std::vector<std::vector<Touch>> saTouch(SA.size()), a2Touch(A2.size());
    for (size_t i = 0; i < pieces.size(); ++i) {
        const Piece& pc = pieces[i];
        if (pc.dnKind == 1) saTouch[size_t(pc.dnIdx)].push_back({int(i), 1, -1});
        if (pc.dnKind == 2) a2Touch[size_t(pc.dnIdx)].push_back({int(i), 1, pc.dnCell});
        if (pc.r < 0 && pc.upKind == 1) saTouch[size_t(pc.upIdx)].push_back({int(i), -1, -1});
        if (pc.r < 0 && pc.upKind == 2) a2Touch[size_t(pc.upIdx)].push_back({int(i), -1, pc.upCell});
    }
    // pump stations
    struct End { int kind = 3, idx = -1, cell = -1; size_t node = 0; };  // kind 0 reach node, 1 SA, 2 2D cell, 3 out
    struct PumpRt { const PumpStation* p; End from, to; std::vector<bool> on; int owner; double q = 0; };
    std::vector<PumpRt> pumps;
    auto resolveEnd = [&](const PumpEnd& e, End& out) {
        if (e.kind == 2) { out.kind = 3; return true; }
        if (e.kind == 0) {
            const int ri = findR(e.river, e.reach);
            if (ri < 0) return false;
            for (size_t k = 0; k < R[size_t(ri)].n.size(); ++k)
                if (sameRs(R[size_t(ri)].n[k].xs->rs, e.rs)) { out.kind = 0; out.idx = ri; out.node = k; return true; }
            return false;
        }
        int k, idx;
        if (!areaRef(e.area, k, idx)) return false;
        out.kind = k;
        out.idx = idx;
        if (k == 2) {
            out.cell = A2[size_t(idx)]->cellAt(e.point.first, e.point.second);
            if (out.cell < 0) out.cell = A2[size_t(idx)]->lowestCell();
        }
        return true;
    };
    for (const auto& p : geom.pumps) {
        PumpRt pr{&p, {}, {}, std::vector<bool>(p.groups.size(), false), 0, 0};
        if (!resolveEnd(p.from, pr.from) || pr.from.kind == 3 || !resolveEnd(p.to, pr.to) || p.groups.empty()) {
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_PUMP_SKIPPED", {p.name}));
            continue;
        }
        pr.owner = int(owners.size());
        owners.push_back({"pump", p.name, nullptr, 0, 0});
        pumps.push_back(std::move(pr));
    }
    auto endZ = [&](const End& e, double& zMin) {
        if (e.kind == 0) { zMin = R[size_t(e.idx)].n[e.node].zMin; return R[size_t(e.idx)].z[e.node]; }
        zMin = zMinOf(e.kind, e.idx, e.cell);
        return zOf(e.kind, e.idx, e.cell);
    };
    std::vector<std::vector<std::pair<int, double>>> a2Pump(A2.size());
    auto applyPumps = [&]() {
        for (auto& rr : R) std::fill(rr.qp.begin(), rr.qp.end(), 0.0);
        for (auto& s : SA) s.pumpQ = 0;
        for (auto& v : a2Pump) v.clear();
        for (auto& pr : pumps) {
            double zMin;
            const double zs = endZ(pr.from, zMin);
            pr.q = 0;
            for (size_t gi = 0; gi < pr.p->groups.size(); ++gi) {
                const PumpGroup& gp = pr.p->groups[gi];
                if (zs >= gp.onElev) pr.on[gi] = true;
                else if (zs <= gp.offElev) pr.on[gi] = false;
                if (pr.on[gi]) pr.q += std::max(0, gp.count) * std::max(0.0, gp.capacity);
            }
            if (!(zs > zMin + hMin)) pr.q = 0;
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                const End& e = sgn < 0 ? pr.from : pr.to;
                const double q = sgn * pr.q;
                if (e.kind == 0) {
                    RReach& rr = R[size_t(e.idx)];
                    const size_t j = std::min(e.node, rr.n.size() - 2);
                    rr.qp[j] += q / std::max(rr.n[j].dx, 1e-3);
                } else if (e.kind == 1) {
                    SA[size_t(e.idx)].pumpQ += q;
                } else if (e.kind == 2) {
                    a2Pump[size_t(e.idx)].push_back({e.cell, q});
                }
            }
        }
    };

    // ---- time ----
    const double t0 = rasDateTimeSeconds(plan.simStartDate, plan.simStartTime);
    const double t1 = rasDateTimeSeconds(plan.simEndDate, plan.simEndTime);
    const double dt = intervalSeconds(plan.compInterval);
    const double outDt = intervalSeconds(plan.outputInterval);
    if (!(t1 > t0) || !(dt > 0) || !(outDt >= dt)) {
        res.messages.push_back(makeMsg(Severity::Error, "MD_U_TIME", {plan.simStartDate + " " + plan.simStartTime, plan.simEndDate + " " + plan.simEndTime}));
        return res;
    }
    const int nSteps = int(std::ceil((t1 - t0) / dt - 1e-9));
    const int outEvery = std::max(1, int(std::lround(outDt / dt)));

    // lateral inflow per unit length on each segment at time tHours
    auto lateral = [&](RReach& rr, double tHours, std::vector<double>& out) {
        out.assign(rr.n.size() - 1, 0.0);
        for (const auto& l : rr.lats) {
            const double q = seriesValue(*l.b, tHours);
            if (!std::isfinite(q)) continue;
            double L = 0;
            for (size_t s = l.i1; s < l.i2; ++s) L += rr.n[s].dx;
            if (L <= 0) continue;
            for (size_t s = l.i1; s < l.i2; ++s) out[s] += q / L;
        }
    };

    // ---- initial conditions: steady backwater with the initial flows ----
    if (!R.empty()) {
        SteadyFlow sf;
        sf.profileNames = {"Init"};
        std::vector<double> qDown(R.size(), kNaN);  // flow at each reach's downstream end
        std::vector<bool> done(R.size(), false);
        for (size_t pass = 0; pass < R.size() + 1; ++pass) {
            for (size_t i = 0; i < R.size(); ++i) {
                if (done[i]) continue;
                RReach& rr = R[i];
                double qUp = kNaN;
                std::string rsUp = rr.n.front().xs->rs;
                for (const auto& f : flow.initialFlows)
                    if (f.river == rr.reach->river && f.reach == rr.reach->reach) { qUp = f.q; rsUp = f.rs.empty() ? rsUp : f.rs; break; }
                if (std::isnan(qUp) && rr.upBC && rr.upBC->type == UBoundaryType::FlowHydrograph) qUp = seriesValue(*rr.upBC, 0);
                if (std::isnan(qUp) && rr.upJ >= 0) {
                    double sum = 0;
                    int nOut = 0;
                    bool ready = true;
                    for (const auto& m : J[size_t(rr.upJ)]) {
                        if (m.dn) { if (!done[size_t(m.r)]) ready = false; else sum += qDown[size_t(m.r)]; }
                        else ++nOut;
                    }
                    if (!ready) continue;
                    qUp = sum / std::max(1, nOut);
                }
                if (std::isnan(qUp)) qUp = 0;
                // flow changes: upstream flow plus lateral inflows at t = 0 (added below their location)
                std::vector<double> ql;
                lateral(rr, 0, ql);
                double q = qUp;
                bool startWritten = false;
                for (size_t k = 0; k < rr.n.size(); ++k) {
                    if (k > 0) q += ql[k - 1] * rr.n[k - 1].dx;
                    const bool changed = k == 0 || (k > 0 && ql[k - 1] != 0);
                    if (!startWritten && (sameRs(rr.n[k].xs->rs, rsUp) || k == 0)) {
                        sf.changes.push_back({rr.reach->river, rr.reach->reach, rr.n[k].xs->rs, {std::max(q, 1e-3)}});
                        startWritten = true;
                    } else if (changed) {
                        sf.changes.push_back({rr.reach->river, rr.reach->reach, rr.n[k].xs->rs, {std::max(q, 1e-3)}});
                    }
                }
                qDown[i] = q;
                done[i] = true;
                if (rr.dnJ < 0) {
                    auto& b = sf.boundaryRef(rr.reach->river, rr.reach->reach, 0);
                    if (rr.dnBC && rr.dnBC->type == UBoundaryType::StageHydrograph) {
                        b.downstream.type = BoundaryType::KnownWS;
                        b.downstream.knownWS = seriesValue(*rr.dnBC, 0);
                    } else if (rr.dnBC && rr.dnBC->type == UBoundaryType::RatingCurve) {
                        b.downstream.type = BoundaryType::RatingCurve;
                        b.downstream.rating = rr.dnBC->rating;
                    } else {
                        b.downstream.type = BoundaryType::NormalDepth;
                        b.downstream.slope = rr.dnBC && rr.dnBC->type == UBoundaryType::NormalDepth && rr.dnBC->slope > 0 ? rr.dnBC->slope : 0.001;
                    }
                }
            }
        }
        Plan sp = plan;
        sp.regime = FlowRegime::Subcritical;
        const auto k = constantsFor(units);
        if (!(sp.stdStepTol > 0)) sp.stdStepTol = k.defaultStdStepTol;
        if (!(sp.criticalTol > 0)) sp.criticalTol = k.defaultCriticalTol;
        if (sp.maxTrials <= 0) sp.maxTrials = 20;
        RunResult r0 = steady::solve(geom, sf, sp, units, {});
        if (r0.status != RunStatus::Succeeded || r0.profiles.empty()) {
            for (const auto& m : r0.messages) if (m.severity == Severity::Error) res.messages.push_back(m);
            res.messages.push_back(makeMsg(Severity::Error, "SOLV_U_INIT_FAIL"));
            return res;
        }
        for (auto& rr : R) {
            rr.z.assign(rr.n.size(), kNaN);
            rr.q.assign(rr.n.size(), kNaN);
            for (const auto& x : r0.profiles[0].xs)
                if (x.river == rr.reach->river && x.reach == rr.reach->reach)
                    for (size_t k2 = 0; k2 < rr.n.size(); ++k2)
                        if (rr.n[k2].xs->rs == x.rs) { rr.z[k2] = x.ws; rr.q[k2] = x.q; }
            for (size_t k2 = 0; k2 < rr.n.size(); ++k2)
                if (!std::isfinite(rr.z[k2])) {
                    res.messages.push_back(makeMsg(Severity::Error, "SOLV_U_INIT_FAIL"));
                    return res;
                }
        }
    }

    // storage and 2D areas: initial storage elevation, else dry
    auto initialElev = [&](const std::string& name) {
        for (const auto& p : flow.initialStorage) if (p.first == name) return p.second;
        return kNaN;
    };
    for (auto& s : SA) {
        const double e = initialElev(s.a->name);
        s.z = std::isfinite(e) ? std::max(e, s.zMin) : s.zMin;
        if (s.stage) { const double v = seriesValue(*s.stage, 0); if (std::isfinite(v)) s.z = std::max(v, s.zMin); }
        s.zOld = s.z;
    }
    for (auto& ar : A2) {
        const double e = initialElev(ar->name());
        if (std::isfinite(e))
            for (size_t i = 0; i < ar->size(); ++i) ar->z[i] = std::max(e, ar->cell(i).zMin);
        ar->zOld = ar->z;
    }

    // ---- output ----
    std::vector<std::pair<size_t, size_t>> order;  // (reach, node) in result order
    for (size_t i = 0; i < R.size(); ++i)
        for (size_t k = 0; k < R[i].n.size(); ++k) order.emplace_back(i, k);
    std::vector<double> maxWs(order.size(), -1e300), qAtMax(order.size(), 0);
    res.series.startSeconds = t0;
    double maxFr = 0;
    std::string frLoc, frRs;
    auto makeProfile = [&](const std::string& name, const std::vector<double>& ws, const std::vector<double>& qq) {
        ProfileResult p;
        p.name = name;
        for (size_t o = 0; o < order.size(); ++o) {
            const RReach& rr = R[order[o].first];
            const Node& nd = rr.n[order[o].second];
            XsResult x;
            x.river = rr.reach->river;
            x.reach = rr.reach->reach;
            x.rs = nd.xs->rs;
            steady::fillXsResult(x, nd.g, ws[o], qq[o], g);
            x.channelDistance = nd.dist;
            x.critWs = kNaN;
            if (x.froudeCh > maxFr) { maxFr = x.froudeCh; frLoc = rr.reach->label(); frRs = x.rs; }
            p.xs.push_back(std::move(x));
        }
        return p;
    };
    std::vector<ProfileResult> outProfiles;
    for (const auto& sa : SA) res.extra.push_back({"storage", sa.a->name, "stage", {}, -1e300});
    for (auto& o : owners) {
        o.series = res.extra.size();
        res.extra.push_back({o.kind, o.name, "flow", {}, 0});
    }
    auto ownerFlows = [&]() {
        for (auto& o : owners) o.q = 0;
        for (const auto& pc : pieces) owners[size_t(pc.owner)].q += pieceQ(pc);
        for (const auto& pr : pumps) owners[size_t(pr.owner)].q = pr.q;
    };
    for (const auto& ar : A2) res.grids.push_back(ar->skeleton());
    const int nRecords = nSteps / outEvery + 2;
    const int frameEvery = std::max(1, (nRecords + 99) / 100);
    int recordNo = 0;
    auto record = [&](double tSec, bool last) {
        ownerFlows();
        for (size_t i = 0; i < SA.size(); ++i) res.extra[i].values.push_back(SA[i].z);
        for (const auto& o : owners) res.extra[o.series].values.push_back(o.q);
        if (recordNo % frameEvery == 0 || last)
            for (size_t a = 0; a < A2.size(); ++a) {
                res.grids[a].hours.push_back((tSec - t0) / 3600.0);
                std::vector<float> w(A2[a]->size());
                for (size_t i = 0; i < w.size(); ++i) w[i] = float(A2[a]->z[i]);
                res.grids[a].wse.push_back(std::move(w));
            }
        ++recordNo;
        std::vector<double> ws(order.size()), qq(order.size());
        for (size_t o = 0; o < order.size(); ++o) {
            ws[o] = R[order[o].first].z[order[o].second];
            qq[o] = R[order[o].first].q[order[o].second];
        }
        res.series.hours.push_back((tSec - t0) / 3600.0);
        res.series.ws.push_back(ws);
        res.series.q.push_back(qq);
        outProfiles.push_back(makeProfile(rasDateText(tSec) + " " + rasTimeText(tSec), ws, qq));
    };
    auto trackMax = [&]() {
        for (size_t o = 0; o < order.size(); ++o) {
            const double z = R[order[o].first].z[order[o].second];
            if (z > maxWs[o]) { maxWs[o] = z; qAtMax[o] = R[order[o].first].q[order[o].second]; }
        }
    };
    const double hArrive = units == UnitSystem::SI ? 0.03 : 0.1;
    auto trackAreas = [&](double tH) {
        ownerFlows();
        for (size_t i = 0; i < SA.size(); ++i) res.extra[i].maxValue = std::max(res.extra[i].maxValue, SA[i].z);
        for (const auto& o : owners)
            if (std::fabs(o.q) > std::fabs(res.extra[o.series].maxValue)) res.extra[o.series].maxValue = o.q;
        for (size_t a = 0; a < A2.size(); ++a) {
            auto& gr = res.grids[a];
            const auto& ar = *A2[a];
            const auto vel = ar.cellVelocity();
            for (size_t i = 0; i < ar.size(); ++i) {
                const float zz = float(ar.z[i]);
                gr.maxWse[i] = std::max(gr.maxWse[i], zz);
                gr.maxDepth[i] = std::max(gr.maxDepth[i], float(ar.z[i] - ar.cell(i).zMin));
                gr.maxVel[i] = std::max(gr.maxVel[i], float(vel[i]));
                if (std::isnan(gr.arrivalHours[i]) && ar.z[i] - ar.cell(i).zMin > hArrive) gr.arrivalHours[i] = float(tH);
            }
        }
    };
    record(t0, false);
    trackMax();
    trackAreas(0);
    std::vector<double> volIn(A2.size(), 0.0), vol0(A2.size(), 0.0);
    std::vector<bool> hasStage(A2.size(), false);
    for (size_t a = 0; a < A2.size(); ++a) vol0[a] = A2[a]->totalVolume();
    for (const auto& b : bc2d) if (b.b->type == UBoundaryType::StageHydrograph) hasStage[size_t(b.area)] = true;
    double unitArea = units == UnitSystem::SI ? 0.3 : 1.0;
    // one implicit update of every storage area and 2D area with the current reach stages
    bool areaFail = false;
    auto updateAreas = [&](double tH, double h) {
        double maxCh = 0;
        // storage areas: Newton on all free storage-area stages together
        std::vector<size_t> freeSa;
        std::vector<double> qFix(SA.size(), 0.0);
        for (size_t si = 0; si < SA.size(); ++si) {
            SArt& s = SA[si];
            if (s.stage) {
                const double v = seriesValue(*s.stage, tH);
                if (std::isfinite(v)) { maxCh = std::max(maxCh, std::fabs(std::max(v, s.zMin) - s.z)); s.z = std::max(v, s.zMin); }
                continue;
            }
            freeSa.push_back(si);
            double q = s.pumpQ;
            for (const auto* b : s.inflow) { const double v = seriesValue(*b, tH); if (std::isfinite(v)) q += v; }
            for (const auto* b : s.rain) q += rainRate(*b, tH, units) * s.planArea;
            qFix[si] = q;
        }
        if (!freeSa.empty()) {
            const size_t nf = freeSa.size();
            std::vector<double> z0(nf);
            for (size_t k = 0; k < nf; ++k) z0[k] = SA[freeSa[k]].z;
            auto resid = [&](std::vector<double>& gv) {
                for (size_t k = 0; k < nf; ++k) {
                    const size_t si = freeSa[k];
                    const SArt& s = SA[si];
                    double q = qFix[si];
                    for (const auto& t : saTouch[si]) q += t.side * pieceQ(pieces[size_t(t.piece)]);
                    gv[k] = s.V(s.z) - s.V(s.zOld) - h * q;
                }
            };
            std::vector<double> g0(nf), g1(nf), dzv(nf);
            for (int it = 0; it < 40; ++it) {
                resid(g0);
                std::vector<std::vector<double>> J(nf, std::vector<double>(nf, 0.0));
                for (size_t c = 0; c < nf; ++c) {
                    SArt& s = SA[freeSa[c]];
                    const double e = 1e-4;
                    s.z += e;
                    resid(g1);
                    s.z -= e;
                    for (size_t rI = 0; rI < nf; ++rI) J[rI][c] = (g1[rI] - g0[rI]) / e;
                    if (!(J[c][c] > 1e-6)) J[c][c] = std::max(s.planArea, 1.0);
                }
                for (size_t k = 0; k < nf; ++k) dzv[k] = -g0[k];
                if (!solveDense(J, dzv)) break;
                double mx = 0;
                for (size_t k = 0; k < nf; ++k) mx = std::max(mx, std::fabs(dzv[k]));
                const double sc = mx > dzLimit ? dzLimit / mx : 1.0;
                for (size_t k = 0; k < nf; ++k) {
                    SArt& s = SA[freeSa[k]];
                    s.z = std::max(s.z + sc * dzv[k], s.zMin);
                }
                if (mx < 1e-7) break;
            }
            for (size_t k = 0; k < nf; ++k) maxCh = std::max(maxCh, std::fabs(SA[freeSa[k]].z - z0[k]));
        }
        for (size_t ai = 0; ai < A2.size(); ++ai) {
            auto& ar = *A2[ai];
            std::fill(ar.src.begin(), ar.src.end(), 0.0);
            std::fill(ar.srcD.begin(), ar.srcD.end(), 0.0);
            ar.fixed.clear();
            for (const auto& pq : a2Pump[ai]) if (pq.first >= 0) ar.src[size_t(pq.first)] += pq.second;
            for (const auto& bc : bc2d) {
                if (size_t(bc.area) != ai) continue;
                const UnsteadyBoundary& b = *bc.b;
                if (b.type == UBoundaryType::Precipitation) {
                    const double rate = rainRate(b, tH, units) * ar.cellArea();
                    for (auto& x : ar.src) x += rate;
                } else if (b.type == UBoundaryType::StageHydrograph) {
                    const double v = seriesValue(b, tH);
                    if (std::isfinite(v)) for (int c : bc.cells.cell) ar.fixed.push_back({c, v});
                } else if (b.type == UBoundaryType::NormalDepth) {
                    const double S = b.slope > 0 ? b.slope : 0.001;
                    for (size_t k = 0; k < bc.cells.cell.size(); ++k) {
                        const size_t c = size_t(bc.cells.cell[k]);
                        const auto& f = bc.cells.edge[k];
                        const double q0 = f.conveyance(ar.z[c], ar.kOverN()) * std::sqrt(S);
                        const double q1 = f.conveyance(ar.z[c] + 1e-3, ar.kOverN()) * std::sqrt(S);
                        ar.src[c] -= q0;
                        ar.srcD[c] -= (q1 - q0) / 1e-3;
                    }
                } else {
                    const double q = seriesValue(b, tH);
                    if (!std::isfinite(q)) continue;
                    if (bc.cells.cell.empty()) { const int c = ar.lowestCell(); if (c >= 0) ar.src[size_t(c)] += q; continue; }
                    double zRef = -1e300, eMin = 1e300;
                    for (size_t k = 0; k < bc.cells.cell.size(); ++k) {
                        zRef = std::max(zRef, ar.z[size_t(bc.cells.cell[k])]);
                        eMin = std::min(eMin, bc.cells.edge[k].zMin);
                    }
                    zRef = std::max(zRef, eMin + unitArea);
                    std::vector<double> w(bc.cells.cell.size());
                    double sw = 0;
                    for (size_t k = 0; k < w.size(); ++k) { w[k] = bc.cells.edge[k].conveyance(zRef, 1.0); sw += w[k]; }
                    for (size_t k = 0; k < w.size(); ++k)
                        ar.src[size_t(bc.cells.cell[k])] += sw > 0 ? q * w[k] / sw : q / double(w.size());
                }
            }
            for (const auto& t : a2Touch[ai]) {
                if (t.cell < 0) continue;
                const Piece& pc = pieces[size_t(t.piece)];
                const double zc = ar.z[size_t(t.cell)];
                const double q0 = pieceQ(pc);
                ar.z[size_t(t.cell)] = zc + 1e-3;
                const double q1 = pieceQ(pc);
                ar.z[size_t(t.cell)] = zc;
                ar.src[size_t(t.cell)] += t.side * q0;
                ar.srcD[size_t(t.cell)] += t.side * (q1 - q0) / 1e-3;
            }
            double ch = 0;
            if (!ar.iterate(h, ch)) { areaFail = true; return kNaN; }
            maxCh = std::max(maxCh, ch);
        }
        return maxCh;
    };

    // ---- time stepping ----
    int noConv = 0;
    double worstDz = 0;
    bool dryWarned = false;
    for (auto& rr : R) lateral(rr, 0, rr.ql);
    for (int step = 1; step <= nSteps; ++step) {
        if (cb.cancelled && cb.cancelled()) {
            res.status = RunStatus::Cancelled;
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_CANCELLED"));
            return res;
        }
        const double tNew = std::min(t1, t0 + step * dt);
        const double h = tNew - (t0 + (step - 1) * dt);
        const double tH = (tNew - t0) / 3600.0;
        for (auto& rr : R) {
            rr.zOld = rr.z;
            rr.qOld = rr.q;
            rr.qlOld = rr.ql;
            lateral(rr, tH, rr.ql);
        }
        for (auto& sa : SA) sa.zOld = sa.z;
        for (auto& ar : A2) ar->beginStep();
        for (auto& pc : pieces) {
            const Breach* br = owners[size_t(pc.owner)].breach;
            pc.crestNow = br && br->active ? breachedCrest(*br, pc.sta, pc.crest, tH) : pc.crest;
        }
        applyPumps();
        bool converged = false;
        double lastMax = 0;
        for (int it = 0; it < std::max(1, plan.unetMaxIter); ++it) {
            // 1. linearise every segment and sweep each reach
            for (auto& rr : R) {
                const size_t N = rr.n.size();
                rr.seg.resize(N - 1);
                std::vector<double> An(N), Kn(N), Bn(N), Ao(N), Ko(N), Bo(N);
                for (size_t k = 0; k < N; ++k) {
                    rr.n[k].tab.eval(rr.z[k], An[k], Kn[k], Bn[k]);
                    rr.n[k].tab.eval(rr.zOld[k], Ao[k], Ko[k], Bo[k]);
                }
                for (size_t j = 0; j + 1 < N; ++j) {
                    const double dx = std::max(rr.n[j].dx, 1e-3);
                    const double qlT = theta * rr.ql[j] + (1 - theta) * rr.qlOld[j];
                    const double A1o = Ao[j], A2o = Ao[j + 1], Q1o = rr.qOld[j], Q2o = rr.qOld[j + 1];
                    const double F1o = Bo[j] * Q1o * Q1o / A1o, F2o = Bo[j + 1] * Q2o * Q2o / A2o;
                    const double Qmo = 0.5 * (Q1o + Q2o), Kmo = 0.5 * (Ko[j] + Ko[j + 1]);
                    const double Sfo = Qmo * std::fabs(Qmo) / (Kmo * Kmo);
                    const double dzo = (rr.zOld[j + 1] - rr.zOld[j]) / dx;
                    auto resid = [&](double z1, double q1, double z2, double q2, double& C, double& M) {
                        double a1, k1, b1, a2, k2, b2;
                        rr.n[j].tab.eval(z1, a1, k1, b1);
                        rr.n[j + 1].tab.eval(z2, a2, k2, b2);
                        double latQ = 0;  // weir flow out of the reach on this segment (implicit in z1, z2)
                        for (int pi : rr.segPieces[j]) {
                            const Piece& pc = pieces[size_t(pi)];
                            latQ += weirFlow(z1 + pc.t * (z2 - z1), zOf(pc.dnKind, pc.dnIdx, pc.dnCell), pc.crestNow, pc.coef, pc.len);
                        }
                        C = (a1 + a2 - A1o - A2o) / (2 * h) + (theta * (q2 - q1) + (1 - theta) * (Q2o - Q1o)) / dx - qlT - rr.qp[j] + latQ / dx;
                        const double F1 = b1 * q1 * q1 / a1, F2 = b2 * q2 * q2 / a2;
                        const double Qm = 0.5 * (q1 + q2), Km = 0.5 * (k1 + k2);
                        const double Sf = Qm * std::fabs(Qm) / (Km * Km);
                        const double Abar = theta * 0.5 * (a1 + a2) + (1 - theta) * 0.5 * (A1o + A2o);
                        M = (q1 + q2 - Q1o - Q2o) / (2 * h) + (theta * (F2 - F1) + (1 - theta) * (F2o - F1o)) / dx +
                            g * Abar * (theta * (z2 - z1) / dx + (1 - theta) * dzo + theta * Sf + (1 - theta) * Sfo);
                    };
                    double x0[4] = {rr.z[j], rr.q[j], rr.z[j + 1], rr.q[j + 1]};
                    double C0, M0;
                    resid(x0[0], x0[1], x0[2], x0[3], C0, M0);
                    double dC[4], dM[4];
                    for (int v = 0; v < 4; ++v) {
                        double x[4] = {x0[0], x0[1], x0[2], x0[3]};
                        const double e = (v % 2 == 0) ? 1e-5 * std::max(1.0, std::fabs(x[v] - rr.n[j + v / 2].zMin)) : 1e-6 * std::max(1.0, std::fabs(x[v]));
                        x[v] += e;
                        double C1, M1;
                        resid(x[0], x[1], x[2], x[3], C1, M1);
                        dC[v] = (C1 - C0) / e;
                        dM[v] = (M1 - M0) / e;
                    }
                    auto& s = rr.seg[j];
                    s.P[0][0] = dC[0]; s.P[0][1] = dC[1]; s.R[0][0] = dC[2]; s.R[0][1] = dC[3]; s.c[0] = -C0;
                    s.P[1][0] = dM[0]; s.P[1][1] = dM[1]; s.R[1][0] = dM[2]; s.R[1][1] = dM[3]; s.c[1] = -M0;
                }
                // downstream-going sweep: start dz_0 = a  ->  relation alpha dz + beta dQ = gamma(a)
                rr.fa.assign(N, 0);
                rr.fb.assign(N, 0);
                rr.fg.assign(N, Aff{});
                rr.fa[0] = 1;
                rr.fb[0] = 0;
                rr.fg[0] = {0, 1};
                for (size_t j = 0; j + 1 < N; ++j) {
                    const auto& s = rr.seg[j];
                    // columns for (dz_j, dQ_j) in (eq1, eq2, relation)
                    const double c1[3] = {s.P[0][0], s.P[1][0], rr.fa[j]};
                    const double c2[3] = {s.P[0][1], s.P[1][1], rr.fb[j]};
                    const double w[3] = {c1[1] * c2[2] - c1[2] * c2[1], c1[2] * c2[0] - c1[0] * c2[2], c1[0] * c2[1] - c1[1] * c2[0]};
                    // w . (P X_j + R X_{j+1} - c) over eq1, eq2 and w3 (rel X_j - gamma) = 0
                    double al = w[0] * s.R[0][0] + w[1] * s.R[1][0];
                    double be = w[0] * s.R[0][1] + w[1] * s.R[1][1];
                    Aff ga{w[0] * s.c[0] + w[1] * s.c[1] + w[2] * rr.fg[j].c, w[2] * rr.fg[j].p};
                    const double nrm = std::hypot(al, be);
                    if (!(nrm > 0) || !std::isfinite(nrm)) { al = 1; be = 0; ga = {0, 0}; }
                    else { al /= nrm; be /= nrm; ga.c /= nrm; ga.p /= nrm; }
                    rr.fa[j + 1] = al;
                    rr.fb[j + 1] = be;
                    rr.fg[j + 1] = ga;
                }
                // dQ at the downstream end: beta dQ = gamma(a) - alpha b
                {
                    const double be = std::fabs(rr.fb[N - 1]) > 1e-14 ? rr.fb[N - 1] : 1e-14;
                    rr.dQdn = {rr.fg[N - 1].c / be, rr.fg[N - 1].p / be};
                    rr.dQdnB = -rr.fa[N - 1] / be;
                }
                // upstream-going sweep: start dz_{N-1} = b
                double ua = 1, ub = 0;
                Aff ug{0, 1};
                for (size_t j = N - 1; j-- > 0;) {
                    const auto& s = rr.seg[j];
                    const double c1[3] = {s.R[0][0], s.R[1][0], ua};
                    const double c2[3] = {s.R[0][1], s.R[1][1], ub};
                    const double w[3] = {c1[1] * c2[2] - c1[2] * c2[1], c1[2] * c2[0] - c1[0] * c2[2], c1[0] * c2[1] - c1[1] * c2[0]};
                    double al = w[0] * s.P[0][0] + w[1] * s.P[1][0];
                    double be = w[0] * s.P[0][1] + w[1] * s.P[1][1];
                    Aff ga{w[0] * s.c[0] + w[1] * s.c[1] + w[2] * ug.c, w[2] * ug.p};
                    const double nrm = std::hypot(al, be);
                    if (!(nrm > 0) || !std::isfinite(nrm)) { al = 1; be = 0; ga = {0, 0}; }
                    else { al /= nrm; be /= nrm; ga.c /= nrm; ga.p /= nrm; }
                    ua = al;
                    ub = be;
                    ug = ga;
                }
                {
                    const double be = std::fabs(ub) > 1e-14 ? ub : 1e-14;
                    rr.dQup = {ug.c / be, ug.p / be};  // + (-ua/be) a
                    rr.dQupA = -ua / be;
                }
            }
            // 2. global system for the end stage changes (a_r, b_r)
            const size_t nU = 2 * R.size();
            std::vector<std::vector<double>> M(nU, std::vector<double>(nU, 0.0));
            std::vector<double> rhs(nU, 0.0);
            size_t row = 0;
            auto upQ = [&](size_t r, std::vector<double>& m, double& c) {  // dQ_up = c + m.(a,b)
                m[2 * r] += R[r].dQupA;
                m[2 * r + 1] += R[r].dQup.p;
                c += R[r].dQup.c;
            };
            auto dnQ = [&](size_t r, std::vector<double>& m, double& c) {
                m[2 * r] += R[r].dQdn.p;
                m[2 * r + 1] += R[r].dQdnB;
                c += R[r].dQdn.c;
            };
            for (size_t r = 0; r < R.size(); ++r) {
                RReach& rr = R[r];
                const size_t N = rr.n.size();
                if (rr.upJ < 0) {
                    const UnsteadyBoundary* b = rr.upBC;
                    if (b && b->type == UBoundaryType::StageHydrograph) {
                        M[row][2 * r] = 1;
                        rhs[row] = seriesValue(*b, tH) - rr.z[0];
                    } else {
                        const double qb = b && b->type == UBoundaryType::FlowHydrograph ? seriesValue(*b, tH) : rr.qOld[0];
                        double c = 0;
                        upQ(r, M[row], c);
                        rhs[row] = qb - rr.q[0] - c;
                    }
                    ++row;
                }
                if (rr.dnJ < 0) {
                    const UnsteadyBoundary* b = rr.dnBC;
                    if (b && b->type == UBoundaryType::StageHydrograph) {
                        M[row][2 * r + 1] = 1;
                        rhs[row] = seriesValue(*b, tH) - rr.z[N - 1];
                    } else if (b && b->type == UBoundaryType::FlowHydrograph) {
                        double c = 0;
                        dnQ(r, M[row], c);
                        rhs[row] = seriesValue(*b, tH) - rr.q[N - 1] - c;
                    } else {
                        double qz, dq;
                        if (b && b->type == UBoundaryType::RatingCurve) {
                            qz = ratingFlow(b->rating, rr.z[N - 1], dq);
                        } else {
                            const double S = b && b->type == UBoundaryType::NormalDepth && b->slope > 0 ? b->slope : 0.001;
                            double a1, k1, b1, a2, k2, b2;
                            const double e = 1e-4;
                            rr.n[N - 1].tab.eval(rr.z[N - 1], a1, k1, b1);
                            rr.n[N - 1].tab.eval(rr.z[N - 1] + e, a2, k2, b2);
                            qz = k1 * std::sqrt(S);
                            dq = (k2 - k1) / e * std::sqrt(S);
                        }
                        double c = 0;
                        dnQ(r, M[row], c);
                        M[row][2 * r + 1] -= dq;
                        rhs[row] = qz - rr.q[N - 1] - c;
                    }
                    ++row;
                }
            }
            for (const auto& mem : J) {
                if (mem.empty()) continue;
                auto zEnd = [&](const JEnd& e) { return e.dn ? R[size_t(e.r)].z.back() : R[size_t(e.r)].z.front(); };
                auto col = [&](const JEnd& e) { return 2 * size_t(e.r) + (e.dn ? 1 : 0); };
                for (size_t k = 1; k < mem.size() && row < nU; ++k, ++row) {
                    M[row][col(mem[k])] = 1;
                    M[row][col(mem[0])] = -1;
                    rhs[row] = zEnd(mem[0]) - zEnd(mem[k]);
                }
                if (row >= nU) break;
                double c = 0, q = 0;
                for (const auto& e : mem) {
                    std::vector<double> m(nU, 0.0);
                    double ce = 0;
                    if (e.dn) { dnQ(size_t(e.r), m, ce); q += R[size_t(e.r)].q.back(); }
                    else { upQ(size_t(e.r), m, ce); q -= R[size_t(e.r)].q.front(); }
                    const double sg = e.dn ? 1 : -1;
                    for (size_t u = 0; u < nU; ++u) M[row][u] += sg * m[u];
                    c += sg * ce;
                }
                rhs[row] = -q - c;
                ++row;
            }
            if (row != nU || !solveDense(M, rhs)) {
                res.messages.push_back(makeMsg(Severity::Error, "SOLV_U_UNSTABLE", {rasDateText(tNew) + " " + rasTimeText(tNew), R[0].reach->label(), R[0].n[0].xs->rs}));
                return res;
            }
            // 3. back-substitution per reach and update
            double maxDz = 0;
            std::vector<std::vector<double>> DZ(R.size()), DQ(R.size());
            for (size_t r = 0; r < R.size(); ++r) {
                RReach& rr = R[r];
                const size_t N = rr.n.size();
                const double a = rhs[2 * r], b = rhs[2 * r + 1];
                auto& dz = DZ[r];
                auto& dq = DQ[r];
                dz.assign(N, 0);
                dq.assign(N, 0);
                dz[N - 1] = b;
                dq[N - 1] = rr.dQdn.c + rr.dQdn.p * a + rr.dQdnB * b;
                for (size_t j = N - 1; j-- > 0;) {
                    const auto& s = rr.seg[j];
                    const double s1 = s.c[0] - s.R[0][0] * dz[j + 1] - s.R[0][1] * dq[j + 1];
                    const double s2 = s.c[1] - s.R[1][0] * dz[j + 1] - s.R[1][1] * dq[j + 1];
                    const double gm = rr.fg[j].c + rr.fg[j].p * a;
                    // pick the best conditioned pair among (relation, eq1), (relation, eq2), (eq1, eq2)
                    struct Eq { double x, y, r; };
                    const Eq e[3] = {{rr.fa[j], rr.fb[j], gm}, {s.P[0][0], s.P[0][1], s1}, {s.P[1][0], s.P[1][1], s2}};
                    int bi = 0, bk = 1;
                    double best = -1;
                    for (int i1 = 0; i1 < 3; ++i1)
                        for (int k1 = i1 + 1; k1 < 3; ++k1) {
                            const double n1 = std::hypot(e[i1].x, e[i1].y), n2 = std::hypot(e[k1].x, e[k1].y);
                            if (n1 <= 0 || n2 <= 0) continue;
                            const double d = std::fabs(e[i1].x * e[k1].y - e[i1].y * e[k1].x) / (n1 * n2);
                            if (d > best) { best = d; bi = i1; bk = k1; }
                        }
                    const double det = e[bi].x * e[bk].y - e[bi].y * e[bk].x;
                    if (std::fabs(det) < 1e-300) { dz[j] = 0; dq[j] = 0; continue; }
                    dz[j] = (e[bi].r * e[bk].y - e[bi].y * e[bk].r) / det;
                    dq[j] = (e[bi].x * e[bk].r - e[bi].r * e[bk].x) / det;
                }
                for (size_t k = 0; k < N; ++k) maxDz = std::max(maxDz, std::fabs(dz[k]));
            }
            if (!std::isfinite(maxDz)) {
                res.messages.push_back(makeMsg(Severity::Error, "SOLV_U_UNSTABLE", {rasDateText(tNew) + " " + rasTimeText(tNew), R[0].reach->label(), R[0].n[0].xs->rs}));
                return res;
            }
            const double scale = maxDz > dzLimit ? dzLimit / maxDz : 1.0;
            for (size_t r = 0; r < R.size(); ++r) {
                RReach& rr = R[r];
                for (size_t k = 0; k < rr.n.size(); ++k) {
                    rr.z[k] += scale * DZ[r][k];
                    rr.q[k] += scale * DQ[r][k];
                    const double floor = rr.n[k].zMin + hMin;
                    if (rr.z[k] < floor) {
                        if (!dryWarned) {
                            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_U_DRY", {rr.reach->label(), rr.n[k].xs->rs, rasDateText(tNew) + " " + rasTimeText(tNew)}));
                            dryWarned = true;
                        }
                        rr.z[k] = floor;
                    }
                    if (!std::isfinite(rr.z[k]) || rr.z[k] > rr.n[k].tab.top()) {
                        res.messages.push_back(makeMsg(Severity::Error, "SOLV_U_UNSTABLE", {rasDateText(tNew) + " " + rasTimeText(tNew), rr.reach->label(), rr.n[k].xs->rs}));
                        return res;
                    }
                }
            }
            const double areaDz = updateAreas(tH, h);
            if (areaFail || !std::isfinite(areaDz)) {
                res.messages.push_back(makeMsg(Severity::Error, "SOLV_2D_UNSTABLE", {rasDateText(tNew) + " " + rasTimeText(tNew)}));
                return res;
            }
            lastMax = std::max(maxDz, areaDz);
            if ((maxDz * scale < plan.unetZTol * 0.5 || maxDz < 1e-6) && areaDz < plan.unetZTol * 0.5) { converged = true; break; }
        }
        if (!converged) {
            ++noConv;
            worstDz = std::max(worstDz, lastMax);
        }
        trackMax();
        trackAreas(tH);
        for (size_t a = 0; a < A2.size(); ++a) volIn[a] += A2[a]->lastSourceVolume;
        if (step % outEvery == 0 || step == nSteps) record(tNew, step == nSteps);
        if (cb.progress) cb.progress(step, nSteps);
    }

    // ---- results ----
    res.profiles.clear();
    res.profiles.push_back(makeProfile("Max WS", maxWs, qAtMax));
    const size_t nOut = outProfiles.size();
    const size_t every = nOut > 100 ? (nOut + 99) / 100 : 1;
    for (size_t i = 0; i < nOut; ++i)
        if (i % every == 0 || i + 1 == nOut) res.profiles.push_back(std::move(outProfiles[i]));
    if (noConv > 0)
        res.messages.push_back(makeMsg(Severity::Warning, "SOLV_U_NOCONV", {std::to_string(noConv), std::to_string(plan.unetMaxIter), fmt(worstDz, 4)}));
    if (maxFr > 1) res.messages.push_back(makeMsg(Severity::Warning, "SOLV_U_FROUDE", {frLoc, frRs, fmt(maxFr, 2)}));
    for (size_t a = 0; a < A2.size(); ++a) {
        const double vEnd = A2[a]->totalVolume();
        const double err = hasStage[a] ? kNaN : (vEnd - vol0[a] - volIn[a]) / std::max({std::fabs(volIn[a]), vEnd, 1e-9}) * 100.0;
        long wet = 0;
        for (float d : res.grids[a].maxDepth) if (d > hArrive) ++wet;
        res.messages.push_back(makeMsg(Severity::Info, "SOLV_2D_DONE", {A2[a]->name(), std::to_string(A2[a]->size()), fmt(A2[a]->cellSize(), 2),
                                                                         std::to_string(wet), std::isfinite(err) ? fmt(err, 2) + " %" : "-"}));
    }
    res.messages.push_back(makeMsg(Severity::Info, "SOLV_U_DONE", {std::to_string(nSteps), plan.compInterval, std::to_string(res.series.hours.size())}));
    res.status = RunStatus::Succeeded;
    return res;
}

}  // namespace deras::unsteady

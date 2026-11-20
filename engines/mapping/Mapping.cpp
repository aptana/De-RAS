// De-RAS - engines/mapping/Mapping.cpp
#include "Mapping.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace deras::mapping {

double length(const Line& l) {
    double s = 0;
    for (size_t i = 1; i < l.size(); ++i) s += std::hypot(l[i].x - l[i - 1].x, l[i].y - l[i - 1].y);
    return s;
}

Pt pointAt(const Line& l, double s, Pt* dir) {
    if (l.empty()) return {};
    if (dir) *dir = {1, 0};
    if (l.size() == 1) return l[0];
    s = std::clamp(s, 0.0, length(l));
    double acc = 0;
    for (size_t i = 1; i < l.size(); ++i) {
        const double seg = std::hypot(l[i].x - l[i - 1].x, l[i].y - l[i - 1].y);
        if (seg <= 0) continue;
        if (dir) *dir = {(l[i].x - l[i - 1].x) / seg, (l[i].y - l[i - 1].y) / seg};
        if (acc + seg >= s || i + 1 == l.size()) {
            const double t = std::clamp((s - acc) / seg, 0.0, 1.0);
            return {l[i - 1].x + (l[i].x - l[i - 1].x) * t, l[i - 1].y + (l[i].y - l[i - 1].y) * t};
        }
        acc += seg;
    }
    return l.back();
}

void project(const Line& l, Pt p, double& s, double& offset) {
    double best = std::numeric_limits<double>::max(), acc = 0;
    s = 0;
    offset = 0;
    for (size_t i = 1; i < l.size(); ++i) {
        const double vx = l[i].x - l[i - 1].x, vy = l[i].y - l[i - 1].y, seg2 = vx * vx + vy * vy;
        if (seg2 <= 0) continue;
        const double seg = std::sqrt(seg2);
        const double t = std::clamp(((p.x - l[i - 1].x) * vx + (p.y - l[i - 1].y) * vy) / seg2, 0.0, 1.0);
        const double qx = l[i - 1].x + vx * t, qy = l[i - 1].y + vy * t;
        const double d = std::hypot(p.x - qx, p.y - qy);
        if (d < best) {
            best = d;
            s = acc + t * seg;
            const double cross = vx * (p.y - l[i - 1].y) - vy * (p.x - l[i - 1].x);  // > 0: left
            offset = cross > 0 ? -d : d;
        }
        acc += seg;
    }
}

double distanceTo(const Line& l, Pt p) {
    if (l.size() == 1) return std::hypot(p.x - l[0].x, p.y - l[0].y);
    double best = std::numeric_limits<double>::max();
    for (size_t i = 1; i < l.size(); ++i) {
        const double vx = l[i].x - l[i - 1].x, vy = l[i].y - l[i - 1].y, seg2 = vx * vx + vy * vy;
        const double t = seg2 > 0 ? std::clamp(((p.x - l[i - 1].x) * vx + (p.y - l[i - 1].y) * vy) / seg2, 0.0, 1.0) : 0.0;
        best = std::min(best, std::hypot(p.x - (l[i - 1].x + vx * t), p.y - (l[i - 1].y + vy * t)));
    }
    return best;
}

bool intersect(const Line& a, const Line& b, double& sa, double& sb) {
    double accA = 0;
    for (size_t i = 1; i < a.size(); ++i) {
        const double ax = a[i].x - a[i - 1].x, ay = a[i].y - a[i - 1].y, la = std::hypot(ax, ay);
        double accB = 0;
        for (size_t j = 1; j < b.size(); ++j) {
            const double bx = b[j].x - b[j - 1].x, by = b[j].y - b[j - 1].y, lb = std::hypot(bx, by);
            const double den = ax * by - ay * bx;
            if (std::fabs(den) > 1e-12) {
                const double dx = b[j - 1].x - a[i - 1].x, dy = b[j - 1].y - a[i - 1].y;
                const double t = (dx * by - dy * bx) / den, u = (dx * ay - dy * ax) / den;
                if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                    sa = accA + t * la;
                    sb = accB + u * lb;
                    return true;
                }
            }
            accB += lb;
        }
        accA += la;
    }
    return false;
}

bool insidePolygon(const Line& poly, Pt p) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x))
            in = !in;
    }
    return in;
}

Line centreline(const Reach& r) {
    Line l;
    for (const auto& p : r.xy) l.push_back({p.first, p.second});
    return l;
}

namespace {
double channelCentre(const CrossSection& xs) {
    if (xs.bankLeft && xs.bankRight) return (*xs.bankLeft + *xs.bankRight) / 2;
    if (xs.staElev.empty()) return 0;
    return (xs.staElev.front().station + xs.staElev.back().station) / 2;
}
}  // namespace

std::vector<CutLine> cutLines(const Reach& r) {
    std::vector<CutLine> out;
    const Line cl = centreline(r);
    const double L = length(cl);
    std::vector<int> idx;
    for (size_t i = 0; i < r.nodes.size(); ++i)
        if (r.nodes[i].type == NodeType::CrossSection && r.nodes[i].staElev.size() >= 2) idx.push_back(int(i));
    if (idx.empty()) return out;
    // main-channel distance from the upstream section
    std::vector<double> dist(idx.size(), 0);
    for (size_t k = 1; k < idx.size(); ++k) {
        double d = 0;
        for (int j = idx[k - 1]; j < idx[k]; ++j) d += r.nodes[size_t(j)].lenChannel;
        dist[k] = dist[k - 1] + d;
    }
    const double total = dist.back();
    for (size_t k = 0; k < idx.size(); ++k) {
        const CrossSection& xs = r.nodes[size_t(idx[k])];
        CutLine c;
        c.nodeIndex = idx[k];
        if (xs.cutLine.size() >= 2) {
            for (const auto& p : xs.cutLine) c.line.push_back({p.first, p.second});
            c.fromFile = true;
            double sa = 0, sb = 0;
            if (cl.size() >= 2 && intersect(cl, c.line, sa, sb)) c.s = sa;
            else if (cl.size() >= 2) { double off; project(cl, pointAt(c.line, length(c.line) / 2), c.s, off); }
        } else {
            if (cl.size() < 2 || L <= 0) continue;
            c.s = total > 0 ? dist[k] / total * L : (idx.size() > 1 ? double(k) / (idx.size() - 1) * L : 0);
            Pt d;
            const Pt p = pointAt(cl, c.s, &d);
            const Pt right{d.y, -d.x};
            const double sc = channelCentre(xs);
            for (double st : {xs.staElev.front().station, xs.staElev.back().station})
                c.line.push_back({p.x + right.x * (st - sc), p.y + right.y * (st - sc)});
        }
        out.push_back(c);
    }
    return out;
}

Pt stationPoint(const CrossSection& xs, const Line& cut, double station) {
    if (xs.staElev.size() < 2) return cut.empty() ? Pt{} : cut.front();
    const double a = xs.staElev.front().station, b = xs.staElev.back().station;
    const double f = b > a ? (station - a) / (b - a) : 0;
    return pointAt(cut, f * length(cut));
}

double terrainAt(const std::vector<io::Raster>& terrains, double x, double y) {
    for (const auto& t : terrains) {
        const double v = t.sample(x, y);
        if (!std::isnan(v)) return v;
    }
    return std::nan("");
}

namespace {
void douglasPeucker(const std::vector<StaElev>& p, size_t a, size_t b, double tol, std::vector<bool>& keep) {
    if (b <= a + 1) return;
    double best = -1;
    size_t at = a;
    const double dx = p[b].station - p[a].station, dz = p[b].elev - p[a].elev;
    for (size_t i = a + 1; i < b; ++i) {
        const double t = dx != 0 ? (p[i].station - p[a].station) / dx : 0;
        const double d = std::fabs(p[i].elev - (p[a].elev + dz * t));  // vertical distance
        if (d > best) { best = d; at = i; }
    }
    if (best > tol) {
        keep[at] = true;
        douglasPeucker(p, a, at, tol, keep);
        douglasPeucker(p, at, b, tol, keep);
    }
}
}  // namespace

std::vector<StaElev> sampleLine(const std::vector<io::Raster>& terrains, const Line& line, double step, size_t maxPoints, int* missing) {
    std::vector<StaElev> raw;
    const double L = length(line);
    if (L <= 0 || step <= 0) return raw;
    const int n = std::max(2, int(std::ceil(L / step)) + 1);
    int miss = 0;
    for (int i = 0; i < n; ++i) {
        const double s = std::min(L, i * L / (n - 1));
        const Pt p = pointAt(line, s);
        const double z = terrainAt(terrains, p.x, p.y);
        if (std::isnan(z)) { ++miss; continue; }
        raw.push_back({std::round(s * 100) / 100, std::round(z * 100) / 100});
    }
    if (missing) *missing = miss;
    if (raw.size() < 2) return {};
    // thin: drop points closer than 1 cm vertically to the line through their neighbours, then
    // raise the tolerance until the HEC-RAS limit of points is met
    for (double tol = 0.01;; tol *= 1.5) {
        std::vector<bool> keep(raw.size(), false);
        keep.front() = keep.back() = true;
        douglasPeucker(raw, 0, raw.size() - 1, tol, keep);
        std::vector<StaElev> out;
        for (size_t i = 0; i < raw.size(); ++i) if (keep[i]) out.push_back(raw[i]);
        if (out.size() <= maxPoints || tol > 100) return out;
    }
}

MapResult inundation(const Geometry& g, const ProfileResult& profile, const std::vector<io::Raster>& terrains, std::string& error,
                     long maxCells) {
    MapResult res;
    if (terrains.empty() || terrains.front().empty()) { error = "no terrain"; return res; }
    const io::Raster& ref = terrains.front();
    std::map<std::string, double> wsOf;
    for (const auto& x : profile.xs) wsOf[x.river + "\x1f" + x.reach + "\x1f" + x.rs] = x.ws;

    struct ReachLines { const Reach* r; std::vector<CutLine> lines; std::vector<double> ws; };
    std::vector<ReachLines> rl;
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& r : g.reaches) {
        ReachLines e{&r, {}, {}};
        for (auto& c : cutLines(r)) {
            auto it = wsOf.find(r.river + "\x1f" + r.reach + "\x1f" + r.nodes[size_t(c.nodeIndex)].rs);
            if (it == wsOf.end() || !std::isfinite(it->second)) continue;
            for (const auto& p : c.line) { xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x); ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y); }
            e.ws.push_back(it->second);
            e.lines.push_back(std::move(c));
        }
        if (e.lines.size() >= 2) rl.push_back(std::move(e));
    }
    if (rl.empty()) { error = "no reach has two cross sections with results and cut lines"; return res; }

    // grid aligned to the reference terrain
    const double dx = ref.dx, dy = ref.dy;
    const double gx0 = ref.x0 + std::floor((xmin - ref.x0) / dx) * dx;
    const double gy0 = ref.y0 - std::floor((ref.y0 - ymax) / dy) * dy;
    const int nx = std::max(1, int(std::ceil((xmax - gx0) / dx)));
    const int ny = std::max(1, int(std::ceil((gy0 - ymin) / dy)));
    if (long(nx) * ny > maxCells) { error = "map area too large for the terrain cell size"; return res; }
    for (io::Raster* out : {&res.depth, &res.wse}) {
        out->nx = nx;
        out->ny = ny;
        out->x0 = gx0;
        out->y0 = gy0;
        out->dx = dx;
        out->dy = dy;
        out->epsg = ref.epsg;
        out->crsName = ref.crsName;
        out->geoKeys = ref.geoKeys;
        out->geoDoubles = ref.geoDoubles;
        out->geoAscii = ref.geoAscii;
        out->z.assign(size_t(nx) * ny, std::nanf(""));
    }
    for (const auto& e : rl) {
        bool any = false;
        for (size_t k = 0; k + 1 < e.lines.size(); ++k) {
            const Line& a = e.lines[k].line;
            const Line& b = e.lines[k + 1].line;
            Line poly = a;
            for (auto it = b.rbegin(); it != b.rend(); ++it) poly.push_back(*it);
            double pxmin = 1e300, pxmax = -1e300, pymin = 1e300, pymax = -1e300;
            for (const auto& p : poly) { pxmin = std::min(pxmin, p.x); pxmax = std::max(pxmax, p.x); pymin = std::min(pymin, p.y); pymax = std::max(pymax, p.y); }
            const int c0 = std::max(0, int(std::floor((pxmin - gx0) / dx))), c1 = std::min(nx - 1, int(std::ceil((pxmax - gx0) / dx)));
            const int r0 = std::max(0, int(std::floor((gy0 - pymax) / dy))), r1 = std::min(ny - 1, int(std::ceil((gy0 - pymin) / dy)));
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c) {
                    const Pt p{gx0 + (c + 0.5) * dx, gy0 - (r + 0.5) * dy};
                    if (!insidePolygon(poly, p)) continue;
                    const double da = distanceTo(a, p), db = distanceTo(b, p);
                    const double t = da + db > 0 ? da / (da + db) : 0;
                    const double ws = e.ws[k] * (1 - t) + e.ws[k + 1] * t;
                    const double z = terrainAt(terrains, p.x, p.y);
                    if (std::isnan(z) || ws <= z) continue;
                    const size_t i = size_t(r) * nx + c;
                    const double d = ws - z;
                    if (std::isnan(res.depth.z[i]) || d > res.depth.z[i]) {
                        res.depth.z[i] = float(d);
                        res.wse.z[i] = float(ws);
                    }
                    any = true;
                }
        }
        if (any) res.reachesMapped++;
    }
    for (float v : res.depth.z)
        if (!std::isnan(v)) { res.wetCells++; res.maxDepth = std::max(res.maxDepth, double(v)); }
    res.wetArea = res.wetCells * dx * dy;
    return res;
}

MapResult grid2dMap(const Grid2DResult& g, int variable, int timeIndex, const std::vector<io::Raster>& terrains, std::string& error, long maxCells) {
    MapResult m;
    if (g.empty()) { error = "no 2D results"; return m; }
    const bool depthVar = variable == 0 || variable == 1;
    const std::vector<float>* w = &g.maxWse;
    if (variable == 1) {
        if (timeIndex < 0 || size_t(timeIndex) >= g.wse.size()) { error = "no such output time"; return m; }
        w = &g.wse[size_t(timeIndex)];
    }
    io::Raster grid;
    grid.x0 = g.x0;
    grid.y0 = g.y0;
    grid.epsg = g.epsg;
    double px = g.cell;
    if (depthVar && !terrains.empty()) {
        const io::Raster& t = terrains.front();
        px = std::min(g.cell, t.dx);
        grid.geoKeys = t.geoKeys;
        grid.geoDoubles = t.geoDoubles;
        grid.geoAscii = t.geoAscii;
        grid.crsName = t.crsName;
        grid.epsg = t.epsg;
    }
    const double W = g.nx * g.cell, H = g.ny * g.cell;
    while (double(W / px) * double(H / px) > double(maxCells)) px *= 2;
    grid.dx = grid.dy = px;
    grid.nx = int(std::ceil(W / px - 1e-9));
    grid.ny = int(std::ceil(H / px - 1e-9));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    grid.z.assign(size_t(grid.nx) * size_t(grid.ny), nan);
    m.depth = grid;
    m.wse = grid;
    const double hMin = 0.001;
    for (int r = 0; r < grid.ny; ++r)
        for (int c = 0; c < grid.nx; ++c) {
            const double x = grid.cellX(c), y = grid.cellY(r);
            const int gc = int(std::floor((x - g.x0) / g.cell)), gr = int(std::floor((g.y0 - y) / g.cell));
            if (gc < 0 || gr < 0 || gc >= g.nx || gr >= g.ny) continue;
            const int idx = g.index[size_t(gr) * size_t(g.nx) + size_t(gc)];
            if (idx < 0) continue;
            const size_t k = size_t(idx);
            float v = nan, ws = nan;
            if (depthVar) {
                ws = (*w)[k];
                double z = terrains.empty() ? double(g.minElev[k]) : terrainAt(terrains, x, y);
                if (std::isnan(z)) z = g.minElev[k];
                const double d = ws - z;
                if (d > hMin) v = float(d);
                else ws = nan;
            } else if (g.maxDepth[k] > hMin) {
                v = variable == 2 ? g.maxVel[k] : g.arrivalHours[k];
            }
            const size_t o = size_t(r) * size_t(grid.nx) + size_t(c);
            m.depth.z[o] = v;
            m.wse.z[o] = ws;
            if (!std::isnan(v)) {
                ++m.wetCells;
                m.maxDepth = std::max(m.maxDepth, double(v));
                m.wetArea += px * px;
            }
        }
    m.reachesMapped = 0;
    return m;
}

}  // namespace deras::mapping

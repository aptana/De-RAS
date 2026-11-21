// De-RAS - engines/unsteady/Flow2D.cpp
#include "Flow2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engines/mapping/Mapping.h"

namespace deras::flow2d {

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();

double sampleTerrain(const std::vector<io::Raster>& t, double x, double y) { return mapping::terrainAt(t, x, y); }
}  // namespace

double Cell::volume(double z) const {
    const size_t m = size_t(std::lower_bound(se.begin(), se.end(), z) - se.begin());
    if (m == 0) return 0;
    return sampleArea * (double(m) * z - pre[m - 1]);
}

double Cell::wetArea(double z) const {
    const size_t m = size_t(std::lower_bound(se.begin(), se.end(), z) - se.begin());
    return sampleArea * double(m);
}

double Cell::level(double v) const {
    if (!(v > 0) || se.empty()) return zMin;
    const double vv = v / sampleArea;
    auto vat = [&](size_t k) { return double(k) * se[k] - (k > 0 ? pre[k - 1] : 0.0); };
    size_t lo = 0, hi = se.size() - 1;  // largest k with vat(k) <= vv
    if (vat(hi) <= vv) lo = hi;
    else
        while (hi - lo > 1) {
            const size_t mid = (lo + hi) / 2;
            if (vat(mid) <= vv) lo = mid; else hi = mid;
        }
    return (vv + pre[lo]) / double(lo + 1);
}

double Face::conveyance(double z, double kOverN) const {
    double k = 0;
    for (double v : e) {
        const double h = z - v;
        if (h > 0) k += kOverN * std::pow(h, 5.0 / 3.0) * stripWidth;
    }
    return k;
}

double Face::flowArea(double z) const {
    double a = 0;
    for (double v : e) if (z > v) a += (z - v) * stripWidth;
    return a;
}

bool Area::build(const StorageArea& sa, const std::vector<io::Raster>& terrains, UnitSystem units, std::string& error, long maxCells) {
    name_ = sa.name;
    cells_.clear();
    faces_.clear();
    if (sa.polygon.size() < 3) { error = "polygon"; return false; }
    if (terrains.empty()) { error = "terrain"; return false; }
    epsg_ = terrains.front().epsg;
    const double n = sa.manningN > 0 ? sa.manningN : 0.06;
    kOverN_ = constantsFor(units).manningK / n;
    mapping::Line poly;
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& p : sa.polygon) {
        poly.push_back({p.first, p.second});
        xmin = std::min(xmin, p.first); xmax = std::max(xmax, p.first);
        ymin = std::min(ymin, p.second); ymax = std::max(ymax, p.second);
    }
    cs_ = sa.cellSize > 0 ? sa.cellSize : std::max(xmax - xmin, ymax - ymin) / 60.0;
    if (!(cs_ > 0)) { error = "cell size"; return false; }
    nx_ = int(std::ceil((xmax - xmin) / cs_));
    ny_ = int(std::ceil((ymax - ymin) / cs_));
    if (nx_ <= 0 || ny_ <= 0 || double(nx_) * double(ny_) > double(maxCells) * 4.0) { error = "cells"; return false; }
    x0_ = xmin;
    y0_ = ymax;
    const double tdx = terrains.front().dx > 0 ? terrains.front().dx : cs_;
    int k = std::clamp(int(std::lround(cs_ / tdx)), 1, 6);
    index_.assign(size_t(nx_) * size_t(ny_), -1);
    long candidates = 0;
    for (int r = 0; r < ny_; ++r)
        for (int c = 0; c < nx_; ++c)
            if (mapping::insidePolygon(poly, {x0_ + (c + 0.5) * cs_, y0_ - (r + 0.5) * cs_})) ++candidates;
    if (candidates > maxCells) { error = "cells"; return false; }
    while (k > 1 && double(candidates) * k * k > 4e6) --k;
    for (int r = 0; r < ny_; ++r)
        for (int c = 0; c < nx_; ++c) {
            Cell cl;
            cl.c = c;
            cl.r = r;
            cl.x = x0_ + (c + 0.5) * cs_;
            cl.y = y0_ - (r + 0.5) * cs_;
            if (!mapping::insidePolygon(poly, {cl.x, cl.y})) continue;
            const double sub = cs_ / k;
            for (int i = 0; i < k; ++i)
                for (int j = 0; j < k; ++j) {
                    const double z = sampleTerrain(terrains, x0_ + c * cs_ + (j + 0.5) * sub, y0_ - r * cs_ - (i + 0.5) * sub);
                    if (std::isfinite(z)) cl.se.push_back(z);
                }
            if (cl.se.empty()) continue;
            std::sort(cl.se.begin(), cl.se.end());
            cl.pre.resize(cl.se.size());
            double acc = 0;
            for (size_t i = 0; i < cl.se.size(); ++i) { acc += cl.se[i]; cl.pre[i] = acc; }
            cl.sampleArea = cs_ * cs_ / double(cl.se.size());
            cl.zMin = cl.se.front();
            index_[size_t(r) * size_t(nx_) + size_t(c)] = int(cells_.size());
            cells_.push_back(std::move(cl));
        }
    if (cells_.empty()) { error = "cells"; return false; }
    cellFaces_.assign(cells_.size(), {});
    for (size_t i = 0; i < cells_.size(); ++i) {
        const Cell& a = cells_[i];
        for (int dir = 0; dir < 2; ++dir) {
            const int c2 = a.c + (dir == 0 ? 1 : 0), r2 = a.r + (dir == 1 ? 1 : 0);
            if (c2 >= nx_ || r2 >= ny_) continue;
            const int b = index_[size_t(r2) * size_t(nx_) + size_t(c2)];
            if (b < 0) continue;
            Face f;
            f.a = int(i);
            f.b = b;
            f.xFace = dir == 0;
            f.stripWidth = cs_ / k;
            const Cell& cb = cells_[size_t(b)];
            for (int s = 0; s < k; ++s) {
                double x, y;
                if (f.xFace) { x = x0_ + (a.c + 1) * cs_; y = y0_ - a.r * cs_ - (s + 0.5) * f.stripWidth; }
                else { x = x0_ + a.c * cs_ + (s + 0.5) * f.stripWidth; y = y0_ - (a.r + 1) * cs_; }
                double z = sampleTerrain(terrains, x, y);
                if (!std::isfinite(z)) z = std::max(a.zMin, cb.zMin);
                // a face is never lower than the lower of the two cells
                f.e.push_back(std::max(z, std::min(a.zMin, cb.zMin)));
            }
            f.zMin = *std::min_element(f.e.begin(), f.e.end());
            cellFaces_[i].push_back(int(faces_.size()));
            cellFaces_[size_t(b)].push_back(int(faces_.size()));
            faces_.push_back(std::move(f));
        }
    }
    setDry();
    src.assign(cells_.size(), 0);
    srcD.assign(cells_.size(), 0);
    return true;
}

void Area::setDry() {
    z.resize(cells_.size());
    for (size_t i = 0; i < cells_.size(); ++i) z[i] = cells_[i].zMin;
    zOld = z;
}

int Area::cellAt(double x, double y) const {
    const int c = int(std::floor((x - x0_) / cs_)), r = int(std::floor((y0_ - y) / cs_));
    if (c < 0 || r < 0 || c >= nx_ || r >= ny_) return -1;
    return index_[size_t(r) * size_t(nx_) + size_t(c)];
}

int Area::lowestCell() const {
    int best = -1;
    for (size_t i = 0; i < cells_.size(); ++i)
        if (best < 0 || cells_[i].zMin < cells_[size_t(best)].zMin) best = int(i);
    return best;
}

int Area::nearestCell(double x, double y) const {
    if (int i = cellAt(x, y); i >= 0) return i;
    int best = -1;
    double bd = 1e300;
    for (size_t i = 0; i < cells_.size(); ++i) {
        const double d = std::hypot(cells_[i].x - x, cells_[i].y - y);
        if (d < bd) { bd = d; best = int(i); }
    }
    return best;
}

BoundaryCells Area::boundaryCells(const std::vector<XY>& line, const std::vector<io::Raster>& terrains) const {
    BoundaryCells out;
    if (line.size() < 2) return out;
    mapping::Line l;
    for (const auto& p : line) l.push_back({p.first, p.second});
    const double L = mapping::length(l);
    out.length = L;
    const double ds = std::max(cs_ / 4, L / 4000);
    const int n = std::max(1, int(std::ceil(L / ds)));
    const double w = L / n;
    std::vector<int> slot(cells_.size(), -1);
    for (int i = 0; i < n; ++i) {
        const auto p = mapping::pointAt(l, (i + 0.5) * w);
        int c = cellAt(p.x, p.y);
        if (c < 0) {
            c = nearestCell(p.x, p.y);
            if (c < 0 || std::hypot(cells_[size_t(c)].x - p.x, cells_[size_t(c)].y - p.y) > 1.5 * cs_) continue;
        }
        double z = sampleTerrain(terrains, p.x, p.y);
        if (!std::isfinite(z)) z = cells_[size_t(c)].zMin;
        z = std::max(z, cells_[size_t(c)].zMin);
        if (slot[size_t(c)] < 0) {
            slot[size_t(c)] = int(out.cell.size());
            out.cell.push_back(c);
            Face f;
            f.a = c;
            f.stripWidth = w;
            out.edge.push_back(f);
        }
        out.edge[size_t(slot[size_t(c)])].e.push_back(z);
    }
    for (auto& f : out.edge) f.zMin = *std::min_element(f.e.begin(), f.e.end());
    return out;
}

double Area::faceAlpha(const Face& f, double za, double zb) const {
    const double zu = std::max(za, zb);
    if (zu <= f.zMin) return 0;
    const double K = f.conveyance(zu, kOverN_);
    return K / std::sqrt(cs_ * std::max(std::fabs(za - zb), 1e-3));
}

bool Area::iterate(double dt, double& maxChange) {
    const size_t n = cells_.size();
    std::vector<double> diag(n), rhs(n);
    const double aMin = 0.01 * cs_ * cs_;
    for (size_t i = 0; i < n; ++i) {
        const Cell& c = cells_[i];
        const double zs = z[i];
        const double A = std::max(c.wetArea(zs), aMin);
        diag[i] = A;
        rhs[i] = A * zs - (c.volume(zs) - c.volume(zOld[i]));
        const double d = std::min(srcD[i], 0.0);
        diag[i] += -dt * d;
        rhs[i] += dt * (src[i] - d * zs);
    }
    std::vector<double> off(faces_.size());
    for (size_t k = 0; k < faces_.size(); ++k) {
        const Face& f = faces_[k];
        const double a = dt * faceAlpha(f, z[size_t(f.a)], z[size_t(f.b)]);
        off[k] = a;
        diag[size_t(f.a)] += a;
        diag[size_t(f.b)] += a;
    }
    for (const auto& fx : fixed) {
        if (fx.first < 0 || size_t(fx.first) >= n) continue;
        const double s = std::max(fx.second, cells_[size_t(fx.first)].zMin);
        const double P = 1e6 * diag[size_t(fx.first)];
        diag[size_t(fx.first)] += P;
        rhs[size_t(fx.first)] += P * s;
    }
    auto mul = [&](const std::vector<double>& x, std::vector<double>& y) {
        for (size_t i = 0; i < n; ++i) y[i] = diag[i] * x[i];
        for (size_t k = 0; k < faces_.size(); ++k) {
            const Face& f = faces_[k];
            y[size_t(f.a)] -= off[k] * x[size_t(f.b)];
            y[size_t(f.b)] -= off[k] * x[size_t(f.a)];
        }
    };
    // preconditioned conjugate gradient, starting from the current iterate
    std::vector<double> x = z, r(n), p(n), q(n), s(n);
    mul(x, q);
    double bn = 0;
    for (size_t i = 0; i < n; ++i) { r[i] = rhs[i] - q[i]; bn += rhs[i] * rhs[i]; }
    bn = std::sqrt(bn);
    for (size_t i = 0; i < n; ++i) { s[i] = r[i] / diag[i]; p[i] = s[i]; }
    double rs = 0;
    for (size_t i = 0; i < n; ++i) rs += r[i] * s[i];
    const int maxIt = int(std::min<size_t>(5000, 50 + 4 * n));
    for (int it = 0; it < maxIt; ++it) {
        double rn = 0;
        for (size_t i = 0; i < n; ++i) rn += r[i] * r[i];
        if (std::sqrt(rn) <= 1e-10 * std::max(bn, 1e-30)) break;
        mul(p, q);
        double pq = 0;
        for (size_t i = 0; i < n; ++i) pq += p[i] * q[i];
        if (!(std::fabs(pq) > 0)) break;
        const double al = rs / pq;
        for (size_t i = 0; i < n; ++i) { x[i] += al * p[i]; r[i] -= al * q[i]; }
        double rs2 = 0;
        for (size_t i = 0; i < n; ++i) { s[i] = r[i] / diag[i]; rs2 += r[i] * s[i]; }
        const double be = rs2 / rs;
        rs = rs2;
        for (size_t i = 0; i < n; ++i) p[i] = s[i] + be * p[i];
    }
    for (size_t i = 0; i < n; ++i) if (!std::isfinite(x[i])) return false;
    // Volume update from the face flows of the solution, so that the step
    // conserves volume: outflows of a cell are scaled down when they would
    // take more water than the cell holds.
    std::vector<char> isFixed(n, 0);
    for (const auto& fx : fixed) if (fx.first >= 0 && size_t(fx.first) < n) isFixed[size_t(fx.first)] = 1;
    std::vector<double> vol(n), flux(faces_.size());
    for (size_t i = 0; i < n; ++i) {
        const double d = std::min(srcD[i], 0.0);
        vol[i] = cells_[i].volume(zOld[i]) + dt * (src[i] + d * (x[i] - z[i]));
    }
    lastSourceVolume = 0;
    for (size_t i = 0; i < n; ++i)
        if (!isFixed[i]) lastSourceVolume += dt * (src[i] + std::min(srcD[i], 0.0) * (x[i] - z[i]));
    for (size_t k = 0; k < faces_.size(); ++k) {
        const Face& f = faces_[k];
        flux[k] = off[k] * (x[size_t(f.a)] - x[size_t(f.b)]);  // volume a -> b during dt
    }
    // repeated passes: a cell whose inflow was reduced may need its outflow reduced too
    std::vector<double> out(n), inn(n);
    for (int pass = 0; pass < 20; ++pass) {
        std::fill(out.begin(), out.end(), 0.0);
        std::fill(inn.begin(), inn.end(), 0.0);
        for (size_t k = 0; k < faces_.size(); ++k) {
            const Face& f = faces_[k];
            const double F = flux[k];
            if (F > 0) { out[size_t(f.a)] += F; inn[size_t(f.b)] += F; }
            else { out[size_t(f.b)] -= F; inn[size_t(f.a)] -= F; }
        }
        bool changed = false;
        std::vector<double> ratio(n, 1.0);
        for (size_t i = 0; i < n; ++i)
            if (!isFixed[i] && out[i] > 0 && vol[i] + inn[i] < out[i]) {
                ratio[i] = std::clamp((vol[i] + inn[i]) / out[i], 0.0, 1.0);
                changed = true;
            }
        if (!changed) break;
        for (size_t k = 0; k < faces_.size(); ++k) {
            const Face& f = faces_[k];
            flux[k] *= flux[k] > 0 ? ratio[size_t(f.a)] : ratio[size_t(f.b)];
        }
    }
    for (size_t k = 0; k < faces_.size(); ++k) {
        const Face& f = faces_[k];
        vol[size_t(f.a)] -= flux[k];
        vol[size_t(f.b)] += flux[k];
    }
    // a sink (outflow boundary, pump) cannot take more than the cell holds:
    // the volume it could not take is not counted as removed
    for (size_t i = 0; i < n; ++i)
        if (!isFixed[i] && vol[i] < 0) {
            lastSourceVolume -= vol[i];
            vol[i] = 0;
        }
    maxChange = 0;
    for (size_t i = 0; i < n; ++i) {
        const double zn = isFixed[i] ? std::max(x[i], cells_[i].zMin) : cells_[i].level(std::max(vol[i], 0.0));
        maxChange = std::max(maxChange, std::fabs(zn - z[i]));
        z[i] = zn;
    }
    return true;
}

std::vector<double> Area::cellVelocity() const {
    const size_t n = cells_.size();
    std::vector<double> vx(n, 0), vy(n, 0), nx(n, 0), ny(n, 0);
    for (const Face& f : faces_) {
        const double za = z[size_t(f.a)], zb = z[size_t(f.b)];
        const double Q = faceAlpha(f, za, zb) * (za - zb);
        const double A = f.flowArea(std::max(za, zb));
        const double v = A > 1e-6 ? Q / A : 0;
        if (f.xFace) { vx[size_t(f.a)] += v; vx[size_t(f.b)] += v; nx[size_t(f.a)] += 1; nx[size_t(f.b)] += 1; }
        else { vy[size_t(f.a)] += v; vy[size_t(f.b)] += v; ny[size_t(f.a)] += 1; ny[size_t(f.b)] += 1; }
    }
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i)
        out[i] = std::hypot(nx[i] > 0 ? vx[i] / nx[i] : 0, ny[i] > 0 ? vy[i] / ny[i] : 0);
    return out;
}

double Area::totalVolume() const {
    double v = 0;
    for (size_t i = 0; i < cells_.size(); ++i) v += cells_[i].volume(z[i]);
    return v;
}

Grid2DResult Area::skeleton() const {
    Grid2DResult g;
    g.area = name_;
    g.x0 = x0_;
    g.y0 = y0_;
    g.cell = cs_;
    g.nx = nx_;
    g.ny = ny_;
    g.epsg = epsg_;
    g.index = index_;
    g.minElev.resize(cells_.size());
    for (size_t i = 0; i < cells_.size(); ++i) g.minElev[i] = float(cells_[i].zMin);
    g.maxWse = g.minElev;
    g.maxDepth.assign(cells_.size(), 0.0f);
    g.maxVel.assign(cells_.size(), 0.0f);
    g.arrivalHours.assign(cells_.size(), std::numeric_limits<float>::quiet_NaN());
    (void)kNaN;
    return g;
}

}  // namespace deras::flow2d

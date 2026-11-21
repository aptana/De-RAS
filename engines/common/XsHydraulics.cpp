// De-RAS - engines/common/XsHydraulics.cpp
#include "XsHydraulics.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace deras::hyd {

namespace {

double groundAt(const std::vector<StaElev>& pts, double x) {
    if (pts.empty()) return 0;
    if (x <= pts.front().station) return pts.front().elev;
    if (x >= pts.back().station) return pts.back().elev;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const auto& a = pts[i];
        const auto& b = pts[i + 1];
        if (x >= a.station && x <= b.station) {
            if (b.station == a.station) return std::max(a.elev, b.elev);
            return a.elev + (b.elev - a.elev) * (x - a.station) / (b.station - a.station);
        }
    }
    return pts.back().elev;
}

// Ground raised to elevation e between stations a and b (vertical faces at a and b).
std::vector<StaElev> raiseGround(const std::vector<StaElev>& pts, double a, double b, double e) {
    if (pts.size() < 2 || b <= a) return pts;
    a = std::max(a, pts.front().station);
    b = std::min(b, pts.back().station);
    if (b <= a) return pts;
    std::vector<StaElev> out;
    size_t i = 0;
    while (i < pts.size() && pts[i].station < a) out.push_back(pts[i++]);
    const double za = groundAt(pts, a);
    out.push_back({a, za});
    if (e > za) out.push_back({a, e});
    // interior: max(ground, e) with crossing points
    std::vector<StaElev> inner;
    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        double x1 = pts[k].station, x2 = pts[k + 1].station;
        if (x2 <= a || x1 >= b || x2 <= x1) continue;
        double xa = std::max(x1, a), xb = std::min(x2, b);
        double z1 = groundAt(pts, xa), z2 = groundAt(pts, xb);
        if ((z1 - e) * (z2 - e) < 0) {
            double xc = xa + (xb - xa) * (e - z1) / (z2 - z1);
            inner.push_back({xc, e});
        }
        if (xb < b) inner.push_back({xb, std::max(z2, e)});
    }
    for (const auto& p : inner) out.push_back(p);
    const double zb = groundAt(pts, b);
    if (e > zb) out.push_back({b, e});
    out.push_back({b, zb});
    while (i < pts.size() && pts[i].station <= b) ++i;
    while (i < pts.size()) out.push_back(pts[i++]);
    return out;
}

// Levee: a zero-width spike up to the levee elevation when it is above the ground.
std::vector<StaElev> addLevee(const std::vector<StaElev>& pts, const LeveePoint& lv) {
    if (!lv.present || pts.size() < 2) return pts;
    if (lv.station < pts.front().station || lv.station > pts.back().station) return pts;
    const double z = groundAt(pts, lv.station);
    if (lv.elev <= z) return pts;
    std::vector<StaElev> out;
    size_t i = 0;
    while (i < pts.size() && pts[i].station < lv.station) out.push_back(pts[i++]);
    out.push_back({lv.station, z});
    out.push_back({lv.station, lv.elev});
    out.push_back({lv.station, z});
    while (i < pts.size() && pts[i].station == lv.station) ++i;
    while (i < pts.size()) out.push_back(pts[i++]);
    return out;
}

}  // namespace

XsGeom XsGeom::from(const CrossSection& xs, UnitSystem units) {
    XsGeom g;
    g.units = units;
    g.pts = xs.staElev;
    g.n = xs.manning;
    std::sort(g.n.begin(), g.n.end(), [](const ManningBreak& a, const ManningBreak& b) { return a.station < b.station; });
    const double s0 = g.pts.empty() ? 0 : g.pts.front().station;
    const double s1 = g.pts.empty() ? 0 : g.pts.back().station;
    g.bankL = std::clamp(xs.bankLeft.value_or(s0), s0, s1);
    g.bankR = std::clamp(xs.bankRight.value_or(s1), s0, s1);
    g.minEl = xs.minElevation();
    g.invertCh = xs.channelInvert();
    if (!xs.obstructions.empty() || xs.leveeLeft.present || xs.leveeRight.present) {
        for (const auto& b : xs.obstructions) g.pts = raiseGround(g.pts, std::min(b.staStart, b.staEnd), std::max(b.staStart, b.staEnd), b.elev);
        g.pts = addLevee(g.pts, xs.leveeLeft);
        g.pts = addLevee(g.pts, xs.leveeRight);
        if (!xs.obstructions.empty()) {
            g.minEl = std::numeric_limits<double>::infinity();
            g.invertCh = std::numeric_limits<double>::infinity();
            for (const auto& p : g.pts) {
                g.minEl = std::min(g.minEl, p.elev);
                if (p.station >= g.bankL && p.station <= g.bankR) g.invertCh = std::min(g.invertCh, p.elev);
            }
            if (!std::isfinite(g.invertCh)) g.invertCh = g.minEl;
        }
    }
    g.leftTop = g.pts.empty() ? 0 : g.pts.front().elev;
    g.rightTop = g.pts.empty() ? 0 : g.pts.back().elev;
    g.ineffective = xs.ineffective;
    g.leveeLeft = xs.leveeLeft;
    g.leveeRight = xs.leveeRight;
    return g;
}

std::vector<std::pair<double, double>> XsGeom::activeIntervals(double ws) const {
    std::vector<std::pair<double, double>> act;
    if (pts.size() < 2) return act;
    const double s0 = pts.front().station, s1 = pts.back().station;
    act.push_back({s0, s1});
    if (!hasExclusions()) return act;
    std::vector<std::pair<double, double>> cut;
    for (const auto& b : ineffective)
        if (b.permanent || ws <= b.elev) cut.push_back({std::min(b.staStart, b.staEnd), std::max(b.staStart, b.staEnd)});
    if (leveeLeft.present && ws <= leveeLeft.elev) cut.push_back({s0, leveeLeft.station});
    if (leveeRight.present && ws <= leveeRight.elev) cut.push_back({leveeRight.station, s1});
    for (const auto& c : cut) {
        std::vector<std::pair<double, double>> next;
        for (const auto& a : act) {
            if (c.second <= a.first || c.first >= a.second) { next.push_back(a); continue; }
            if (c.first > a.first) next.push_back({a.first, c.first});
            if (c.second < a.second) next.push_back({c.second, a.second});
        }
        act.swap(next);
    }
    return act;
}

double XsGeom::nAt(double station) const {
    if (n.empty()) return std::numeric_limits<double>::quiet_NaN();
    double v = n.front().n;
    for (const auto& b : n) {
        if (b.station <= station + 1e-9) v = b.n;
        else break;
    }
    return v;
}

void wettedSegment(const std::vector<StaElev>& pts, double a, double b, double ws, double& area, double& perim,
                   double& top) {
    area = perim = top = 0;
    if (b <= a) return;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        double x1 = pts[i].station, z1 = pts[i].elev;
        double x2 = pts[i + 1].station, z2 = pts[i + 1].elev;
        if (x2 <= x1) continue;  // vertical or reversed segment: handled as zero-width
        // clip to [a, b]
        double xa = std::max(x1, a), xb = std::min(x2, b);
        if (xb <= xa) continue;
        auto zAt = [&](double x) { return z1 + (z2 - z1) * (x - x1) / (x2 - x1); };
        double za = zAt(xa), zb = zAt(xb);
        double da = ws - za, db = ws - zb;  // depths
        if (da <= 0 && db <= 0) continue;
        double w = xb - xa;
        if (da >= 0 && db >= 0) {
            area += 0.5 * (da + db) * w;
            perim += std::hypot(w, zb - za);
            top += w;
        } else {
            // partially wet: find the crossing
            double t = da / (da - db);  // fraction from xa where depth = 0
            double xc = xa + t * w;
            if (da > 0) {
                double ww = xc - xa;
                area += 0.5 * da * ww;
                perim += std::hypot(ww, da);
                top += ww;
            } else {
                double ww = xb - xc;
                area += 0.5 * db * ww;
                perim += std::hypot(ww, db);
                top += ww;
            }
        }
    }
    // Vertical segments (equal stations) add wetted perimeter to the side
    // the water is on: a wall going down (left high -> right low) faces the
    // right side, a wall going up faces the left side.
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        if (pts[i + 1].station != pts[i].station) continue;
        const double x = pts[i].station;
        const bool descending = pts[i + 1].elev < pts[i].elev;
        const bool inside = descending ? (x >= a && x < b) : (x > a && x <= b);
        if (!inside) continue;
        double lo = std::min(pts[i].elev, pts[i + 1].elev), hi = std::max(pts[i].elev, pts[i + 1].elev);
        if (ws > lo) perim += std::min(ws, hi) - lo;
    }
}

// Wetted properties of [a, b] restricted to the active intervals.
static void activeSegment(const XsGeom& g, const std::vector<std::pair<double, double>>& act, double a, double b, double ws,
                          double& area, double& perim, double& top) {
    area = perim = top = 0;
    for (const auto& iv : act) {
        double x0 = std::max(a, iv.first), x1 = std::min(b, iv.second);
        if (x1 <= x0) continue;
        double ar, pe, tw;
        wettedSegment(g.pts, x0, x1, ws, ar, pe, tw);
        area += ar;
        perim += pe;
        top += tw;
    }
}

static bool isActive(const std::vector<std::pair<double, double>>& act, double x) {
    for (const auto& iv : act)
        if (x >= iv.first - 1e-9 && x <= iv.second + 1e-9) return true;
    return false;
}

static Element element(const XsGeom& g, const std::vector<std::pair<double, double>>& act, double a, double b, double ws,
                       bool composite, double wallL, double wallR) {
    Element e;
    const double k = constantsFor(g.units).manningK;
    if (b <= a) return e;
    // breakpoints inside [a, b]
    std::vector<double> cuts{a};
    for (const auto& nb : g.n)
        if (nb.station > a + 1e-9 && nb.station < b - 1e-9) cuts.push_back(nb.station);
    cuts.push_back(b);
    if (composite) {
        double sumPn = 0;
        for (size_t i = 0; i + 1 < cuts.size(); ++i) {
            double ar, pe, tw;
            activeSegment(g, act, cuts[i], cuts[i + 1], ws, ar, pe, tw);
            if (i == 0) pe += wallL;
            if (i + 2 == cuts.size()) pe += wallR;
            double nn = g.nAt(cuts[i]);
            e.area += ar;
            e.perimeter += pe;
            e.topWidth += tw;
            sumPn += pe * std::pow(nn, 1.5);
        }
        if (e.area > 0 && e.perimeter > 0) {
            double nc = std::pow(sumPn / e.perimeter, 2.0 / 3.0);
            if (!(nc > 0)) nc = g.nAt(a);
            e.conveyance = (k / nc) * e.area * std::pow(e.area / e.perimeter, 2.0 / 3.0);
        }
    } else {
        for (size_t i = 0; i + 1 < cuts.size(); ++i) {
            double ar, pe, tw;
            activeSegment(g, act, cuts[i], cuts[i + 1], ws, ar, pe, tw);
            if (i == 0) pe += wallL;
            if (i + 2 == cuts.size()) pe += wallR;
            e.area += ar;
            e.perimeter += pe;
            e.topWidth += tw;
            if (ar > 0 && pe > 0) e.conveyance += (k / g.nAt(cuts[i])) * ar * std::pow(ar / pe, 2.0 / 3.0);
        }
    }
    return e;
}

XsProps properties(const XsGeom& g, double ws) {
    XsProps p;
    p.ws = ws;
    if (g.pts.size() < 2) return p;
    const double s0 = g.pts.front().station, s1 = g.pts.back().station;
    // Vertical walls are extended at an end point lying below the water
    // surface; the wall's wetted height is added to the perimeter of the
    // first/last subdivision of the element touching that end.
    const double wl = ws > g.leftTop ? ws - g.leftTop : 0.0;
    const double wr = ws > g.rightTop ? ws - g.rightTop : 0.0;
    const auto act = g.activeIntervals(ws);
    const double wlA = isActive(act, s0) ? wl : 0.0, wrA = isActive(act, s1) ? wr : 0.0;
    p.wallsExtended = wlA > 0 || wrA > 0;
    const bool lobExists = g.bankL > s0, robExists = g.bankR < s1;
    p.lob = element(g, act, s0, g.bankL, ws, false, wlA, 0);
    p.ch = element(g, act, g.bankL, g.bankR, ws, true, lobExists ? 0 : wlA, robExists ? 0 : wrA);
    p.rob = element(g, act, g.bankR, s1, ws, false, 0, wrA);

    p.area = p.lob.area + p.ch.area + p.rob.area;
    p.perimeter = p.lob.perimeter + p.ch.perimeter + p.rob.perimeter;
    p.topWidth = p.lob.topWidth + p.ch.topWidth + p.rob.topWidth;
    p.conveyance = p.lob.conveyance + p.ch.conveyance + p.rob.conveyance;
    if (p.conveyance > 0 && p.area > 0) {
        double s = 0;
        for (const Element* e : {&p.lob, &p.ch, &p.rob})
            if (e->area > 0 && e->conveyance > 0) s += std::pow(e->conveyance, 3) / (e->area * e->area);
        p.alpha = p.area * p.area * s / std::pow(p.conveyance, 3);
    }
    return p;
}

double specificForce(const XsGeom& g, double ws, double q) {
    XsProps p = properties(g, ws);
    if (p.area <= 0) return std::numeric_limits<double>::infinity();
    // first moment of the active wetted area about the water surface: integral of d^2/2 dx
    double moment = 0;
    for (const auto& iv : g.activeIntervals(ws)) {
        for (size_t i = 0; i + 1 < g.pts.size(); ++i) {
            double x1 = g.pts[i].station, x2 = g.pts[i + 1].station;
            if (x2 <= x1) continue;
            double xa = std::max(x1, iv.first), xb = std::min(x2, iv.second);
            if (xb <= xa) continue;
            double z1 = g.pts[i].elev, z2 = g.pts[i + 1].elev;
            double da = ws - (z1 + (z2 - z1) * (xa - x1) / (x2 - x1));
            double db = ws - (z1 + (z2 - z1) * (xb - x1) / (x2 - x1));
            double w = xb - xa;
            if (da >= 0 && db >= 0) moment += w * (da * da + da * db + db * db) / 6.0;
            else if (da > 0 || db > 0) {
                double d = std::max(da, db);
                double ww = w * d / (std::fabs(da) + std::fabs(db));
                moment += ww * d * d / 6.0;
            }
        }
    }
    const double gr = constantsFor(g.units).gravity;
    return q * q / (gr * p.area) + moment;
}

double totalEnergy(const XsGeom& g, double ws, double q) {
    XsProps p = properties(g, ws);
    if (p.area <= 0) return std::numeric_limits<double>::infinity();
    const double gr = constantsFor(g.units).gravity;
    double v = q / p.area;
    return ws + p.alpha * v * v / (2 * gr);
}

double criticalWS(const XsGeom& g, double q, double tol) {
    if (g.pts.size() < 2 || q <= 0) return std::numeric_limits<double>::quiet_NaN();
    double lo = g.minEl + 1e-6;
    double topEl = std::max(g.leftTop, g.rightTop);
    double hi = std::max(topEl, g.minEl + 1.0);
    // Make sure the minimum is inside [lo, hi]: extend the upper bound while H keeps falling there.
    for (int it = 0; it < 30; ++it) {
        double h1 = totalEnergy(g, hi, q), h0 = totalEnergy(g, hi - (hi - lo) * 0.01, q);
        if (h1 >= h0) break;
        hi = g.minEl + 2 * (hi - g.minEl);
    }
    const int N = 400;
    double bestW = lo, bestH = std::numeric_limits<double>::infinity();
    int bestI = 0;
    for (int i = 0; i <= N; ++i) {
        double w = lo + (hi - lo) * i / N;
        double h = totalEnergy(g, w, q);
        if (h < bestH) { bestH = h; bestW = w; bestI = i; }
    }
    double a = lo + (hi - lo) * std::max(0, bestI - 1) / N;
    double b = lo + (hi - lo) * std::min(N, bestI + 1) / N;
    // golden-section refinement
    const double gr = (std::sqrt(5.0) - 1) / 2;
    double c = b - gr * (b - a), d = a + gr * (b - a);
    double fc = totalEnergy(g, c, q), fd = totalEnergy(g, d, q);
    int guard = 0;
    while (b - a > std::max(tol * 0.01, 1e-7) && guard++ < 200) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - gr * (b - a); fc = totalEnergy(g, c, q); }
        else { a = c; c = d; fc = fd; d = a + gr * (b - a); fd = totalEnergy(g, d, q); }
    }
    double w = 0.5 * (a + b);
    if (!std::isfinite(w)) return bestW;
    return w;
}

double normalWS(const XsGeom& g, double q, double slope, double tol) {
    if (g.pts.size() < 2 || q <= 0 || slope <= 0) return std::numeric_limits<double>::quiet_NaN();
    const double target = q / std::sqrt(slope);
    double lo = g.minEl, hi = std::max(std::max(g.leftTop, g.rightTop), g.minEl + 1.0);
    for (int it = 0; it < 60 && properties(g, hi).conveyance < target; ++it) hi = g.minEl + 2 * (hi - g.minEl);
    if (properties(g, hi).conveyance < target) return std::numeric_limits<double>::quiet_NaN();
    for (int it = 0; it < 200 && hi - lo > tol * 1e-3; ++it) {
        double m = 0.5 * (lo + hi);
        if (properties(g, m).conveyance < target) lo = m; else hi = m;
    }
    return 0.5 * (lo + hi);
}

}  // namespace deras::hyd

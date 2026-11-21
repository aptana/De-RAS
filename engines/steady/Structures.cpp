// De-RAS - engines/steady/Structures.cpp
#include "Structures.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace deras::steady::st {

double defaultWeirCoef(UnitSystem u) { return u == UnitSystem::SI ? 1.44 : 2.6; }

double villemonte(double hu, double hd) {
    if (hu <= 0) return 0;
    if (hd <= 0) return 1;
    const double r = std::min(hd / hu, 1.0);
    return std::pow(std::max(0.0, 1 - std::pow(r, 1.5)), 0.385);
}

double weirFlow(const std::vector<std::pair<double, double>>& crest, double eg, double tw, double coef,
                const std::vector<std::pair<double, double>>& blocked) {
    double q = 0;
    for (size_t i = 1; i < crest.size(); ++i) {
        double a = crest[i - 1].first, b = crest[i].first;
        if (b <= a) continue;
        const double za = crest[i - 1].second, zb = crest[i].second;
        // integrate in small pieces so partially overtopped segments and gate gaps are exact enough
        const int n = std::max(1, int(std::ceil((b - a) / std::max(1e-3, (b - a) / 20))));
        for (int k = 0; k < n; ++k) {
            const double s0 = a + (b - a) * k / n, s1 = a + (b - a) * (k + 1) / n, sm = 0.5 * (s0 + s1);
            bool skip = false;
            for (const auto& bl : blocked) if (sm > bl.first && sm < bl.second) skip = true;
            if (skip) continue;
            const double z = za + (zb - za) * (sm - a) / (b - a);
            const double h = eg - z;
            if (h <= 0) continue;
            q += coef * (s1 - s0) * std::pow(h, 1.5) * villemonte(h, tw - z);
        }
    }
    return q;
}

namespace {
double interp(const std::vector<std::pair<double, double>>& v, double x) {
    if (v.empty()) return std::nan("");
    if (x <= v.front().first) return v.front().second;
    if (x >= v.back().first) return v.back().second;
    for (size_t i = 1; i < v.size(); ++i)
        if (x <= v[i].first) {
            const double t = (x - v[i - 1].first) / std::max(1e-12, v[i].first - v[i - 1].first);
            return v[i - 1].second + t * (v[i].second - v[i - 1].second);
        }
    return v.back().second;
}
}  // namespace

Opening bridgeOpening(const CrossSection& xs, const std::vector<DeckPoint>& deck, const std::vector<Pier>& piers, bool up) {
    Opening o;
    if (xs.staElev.size() < 2 || deck.size() < 2) return o;
    std::vector<std::pair<double, double>> ground, low;
    for (const auto& p : xs.staElev) ground.emplace_back(p.station, p.elev);
    for (const auto& d : deck) low.emplace_back(d.station, d.low);
    o.maxLowChord = -1e300;
    o.minLowChord = 1e300;
    for (const auto& d : deck) { o.maxLowChord = std::max(o.maxLowChord, d.low); o.minLowChord = std::min(o.minLowChord, d.low); }
    const double a = xs.staElev.front().station, b = xs.staElev.back().station;
    const int n = 2000;
    double area = 0, bedW = 0, bedSum = 0;
    for (int k = 0; k < n; ++k) {
        const double s = a + (b - a) * (k + 0.5) / n, ds = (b - a) / n;
        const double z = interp(ground, s), lc = interp(low, s);
        if (lc <= z) continue;
        double w = ds;
        for (const auto& p : piers) {
            const double c = up ? p.upSta : p.dnSta;
            const auto& wid = up ? p.upWidth : p.dnWidth;
            const double pw = wid.empty() ? 0 : *std::max_element(wid.begin(), wid.end());
            if (s > c - pw / 2 && s < c + pw / 2) w = 0;
        }
        area += w * (lc - z);
        bedW += ds;
        bedSum += ds * z;
    }
    o.area = area;
    o.meanBed = bedW > 0 ? bedSum / bedW : xs.minElevation();
    return o;
}

double sluiceCd(double r) {
    // Tabulated from the FHWA pressure-flow chart used by HEC-RAS (Y3/Z -> Cd).
    static const double x[] = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.8, 2.0, 2.2};
    static const double y[] = {0.27, 0.33, 0.37, 0.40, 0.43, 0.45, 0.47, 0.49, 0.50, 0.50};
    if (r <= x[0]) return y[0];
    for (int i = 1; i < 10; ++i)
        if (r <= x[i]) return y[i - 1] + (y[i] - y[i - 1]) * (r - x[i - 1]) / (x[i] - x[i - 1]);
    return 0.5;
}

bool inletCoefficients(int shape, int chart, int scale, InletCoef& o) {
    struct Row { int chart, scale; InletCoef c; };
    // FHWA HDS-5 table of inlet control coefficients (form 1), subset.
    static const Row rows[] = {
        {1, 1, {0.0098, 2.0, 0.0398, 0.67, false}},  // circular concrete, square edge with headwall
        {1, 2, {0.0018, 2.0, 0.0292, 0.74, false}},  // groove end with headwall
        {1, 3, {0.0045, 2.0, 0.0317, 0.69, false}},  // groove end projecting
        {2, 1, {0.0078, 2.0, 0.0379, 0.69, false}},  // circular CMP, headwall
        {2, 2, {0.0210, 1.33, 0.0463, 0.75, true}},  // mitered to slope
        {2, 3, {0.0340, 1.50, 0.0553, 0.54, false}}, // projecting
        {8, 1, {0.026, 1.0, 0.0347, 0.81, false}},   // box, wingwall flare 30-75 deg
        {8, 2, {0.061, 0.75, 0.0400, 0.80, false}},  // box, wingwall flare 90 and 15 deg
        {8, 3, {0.061, 0.75, 0.0423, 0.82, false}},  // box, wingwall flare 0 deg
    };
    for (const auto& r : rows)
        if (r.chart == chart && r.scale == scale) { o = r.c; return true; }
    o = shape == 2 ? rows[6].c : rows[0].c;
    return false;
}

void barrelSection(const CulvertGroup& c, double y, double& area, double& top, double& perim) {
    const double D = c.rise;
    y = std::clamp(y, 0.0, D);
    if (c.shape == 2) {
        const double B = c.span > 0 ? c.span : D;
        area = B * y;
        top = y < D ? B : 0;
        perim = B + 2 * y + (y >= D ? B : 0);
        return;
    }
    const double th = 2 * std::acos(std::clamp(1 - 2 * y / D, -1.0, 1.0));
    area = D * D / 8 * (th - std::sin(th));
    top = D * std::sin(th / 2);
    perim = D * th / 2;
}

double barrelCriticalDepth(const CulvertGroup& c, double q, double g) {
    const double D = c.rise;
    if (q <= 0 || D <= 0) return 0;
    auto f = [&](double y) {
        double a, t, p;
        barrelSection(c, y, a, t, p);
        if (a <= 0) return -1e30;
        return 1 - q * q * t / (g * a * a * a);  // increasing in y (0 at critical)
    };
    if (f(D * 0.999) < 0) return D;
    return bisect(f, 1e-6 * D, D * 0.999, 1e-7 * D);
}

CulvertState culvertEnergy(const CulvertGroup& c, double q, double tw, double hv2, UnitSystem u, double g) {
    CulvertState s;
    const double D = c.rise;
    double Af, Tf, Pf;
    barrelSection(c, D, Af, Tf, Pf);
    const double S = c.length > 0 ? (c.upInvert - c.dnInvert) / c.length : 0;
    const double ku = u == UnitSystem::SI ? 1.811 : 1.0;
    // inlet control
    InletCoef ic;
    inletCoefficients(c.shape, c.chart, c.scale, ic);
    const double dc = barrelCriticalDepth(c, q, g);
    double ac, tc, pc;
    barrelSection(c, std::max(dc, 1e-9), ac, tc, pc);
    const double hc = dc + (ac > 0 ? q * q / (2 * g * ac * ac) : 0);
    const double X = ku * q / (Af * std::sqrt(D));
    const double slopeTerm = ic.mitered ? 0.7 * S : -0.5 * S;
    auto unsub = [&](double x) { return hc / D + ic.K * std::pow(x, ic.M) + slopeTerm; };
    auto sub = [&](double x) { return ic.c * x * x + ic.Y + slopeTerm; };
    double hwD;
    if (X <= 3.5) hwD = unsub(X);
    else if (X >= 4.0) hwD = sub(X);
    else { const double w = (X - 3.5) / 0.5; hwD = (1 - w) * unsub(X) + w * sub(X); }
    s.hwInlet = c.upInvert + std::max(0.0, hwD) * D;
    // outlet control (full-flow losses above max(TW, (dc + D)/2))
    const double V = q / Af, hv = V * V / (2 * g);
    const double R = Pf > 0 ? Af / Pf : 0;
    const double k = u == UnitSystem::SI ? 1.0 : 1.486;
    const double sf = R > 0 ? std::pow(q * c.n / (k * Af * std::pow(R, 2.0 / 3.0)), 2) : 0;
    const double h0 = std::max(tw - c.dnInvert, (std::min(dc, D) + D) / 2);
    const double exitLoss = c.kx * std::max(0.0, hv - hv2);
    s.hwOutlet = c.dnInvert + h0 + c.ke * hv + c.length * sf + exitLoss + (tw - c.dnInvert >= D ? hv2 : 0);
    s.inletControls = s.hwInlet >= s.hwOutlet;
    s.egUp = std::max(s.hwInlet, s.hwOutlet);
    return s;
}

double culvertFlow(const CulvertGroup& c, double egUp, double tw, double hv2, UnitSystem u, double g) {
    if (egUp <= c.upInvert) return 0;
    auto f = [&](double q) { return culvertEnergy(c, q, tw, hv2, u, g).egUp - egUp; };
    double hi = 1;
    for (int i = 0; i < 60 && f(hi) < 0; ++i) hi *= 2;
    const double q = bisect(f, 0.0, hi, 1e-7 * hi);
    return std::isnan(q) ? 0 : q;
}

double gateFlow(const GateGroup& gt, double b, double egUp, double tw, double weirCoef, double g) {
    const double H = egUp - gt.invert;
    if (H <= 0 || b <= 0 || gt.width <= 0) return 0;
    const double hd = tw - gt.invert;
    if (H <= b) {  // water below the gate lip: weir flow over the sill
        const double cw = gt.weirCoef > 0 ? gt.weirCoef : weirCoef;
        return cw * gt.width * std::pow(H, 1.5) * villemonte(H, hd);
    }
    if (hd > 0 && hd / H > 0.67) {  // submerged: orifice
        const double dh = egUp - tw;
        return dh > 0 ? gt.coef * gt.width * b * std::sqrt(2 * g * dh) : 0;
    }
    return gt.coef * gt.width * b * std::sqrt(2 * g * H);
}

}  // namespace deras::steady::st

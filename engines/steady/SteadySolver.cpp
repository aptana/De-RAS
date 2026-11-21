// De-RAS - engines/steady/SteadySolver.cpp
#include "SteadySolver.h"
#include "Structures.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engines/common/XsHydraulics.h"

namespace deras::steady {

using hyd::XsGeom;
using hyd::XsProps;

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

bool finite(double x) { return std::isfinite(x); }

double ratingStage(const std::vector<std::pair<double, double>>& rating, double q, bool& extrapolated) {
    // rating: (stage, flow); sort by flow
    auto r = rating;
    std::sort(r.begin(), r.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    extrapolated = false;
    if (r.size() < 2) return kNaN;
    size_t i = 1;
    if (q <= r.front().second) { i = 1; extrapolated = q < r.front().second; }
    else if (q >= r.back().second) { i = r.size() - 1; extrapolated = q > r.back().second; }
    else while (i < r.size() - 1 && r[i].second < q) ++i;
    const auto& a = r[i - 1];
    const auto& b = r[i];
    if (b.second == a.second) return a.first;
    return a.first + (b.first - a.first) * (q - a.second) / (b.second - a.second);
}

struct SectionState {
    XsProps props;
    double q = 0;
    double hv = 0;  // alpha V^2 / 2g
};

SectionState stateAt(const XsGeom& g, double ws, double q, double grav) {
    SectionState s;
    s.props = hyd::properties(g, ws);
    s.q = q;
    if (s.props.area > 0) {
        double v = q / s.props.area;
        s.hv = s.props.alpha * v * v / (2 * grav);
    } else {
        s.hv = kNaN;
    }
    return s;
}

double frictionSlope(FrictionSlopeMethod m, double q1, double k1, double q2, double k2) {
    double sf1 = (q1 / k1) * (q1 / k1), sf2 = (q2 / k2) * (q2 / k2);
    switch (m) {
        case FrictionSlopeMethod::AverageConveyance: { double r = (q1 + q2) / (k1 + k2); return r * r; }
        case FrictionSlopeMethod::AverageFrictionSlope: return 0.5 * (sf1 + sf2);
        case FrictionSlopeMethod::GeometricMean: return std::sqrt(sf1 * sf2);
        case FrictionSlopeMethod::HarmonicMean: return 2 * sf1 * sf2 / (sf1 + sf2);
    }
    return kNaN;
}

struct Step {
    double wsComputed = kNaN, frictionLoss = 0, ceLoss = 0, length = 0;
};

// Energy balance between upstream section (assumed ws2) and the known downstream section.
Step energyStep(const XsGeom& g2, const CrossSection& xs2, double ws2, double q2, const SectionState& dn, double ws1,
                const Plan& plan, double grav) {
    Step st;
    SectionState up = stateAt(g2, ws2, q2, grav);
    if (!(up.props.conveyance > 0) || !finite(up.hv)) return st;
    const auto& p1 = dn.props;
    const auto& p2 = up.props;
    double sf = frictionSlope(plan.frictionMethod, dn.q, p1.conveyance, q2, p2.conveyance);
    auto qel = [](double q, double kel, double kt) { return kt > 0 ? q * kel / kt : 0; };
    double ql = 0.5 * (qel(dn.q, p1.lob.conveyance, p1.conveyance) + qel(q2, p2.lob.conveyance, p2.conveyance));
    double qc = 0.5 * (qel(dn.q, p1.ch.conveyance, p1.conveyance) + qel(q2, p2.ch.conveyance, p2.conveyance));
    double qr = 0.5 * (qel(dn.q, p1.rob.conveyance, p1.conveyance) + qel(q2, p2.rob.conveyance, p2.conveyance));
    double qs = ql + qc + qr;
    st.length = qs > 0 ? (xs2.lenLeft * ql + xs2.lenChannel * qc + xs2.lenRight * qr) / qs : xs2.lenChannel;
    st.frictionLoss = st.length * sf;
    double c = (dn.hv > up.hv) ? xs2.contraction : xs2.expansion;
    st.ceLoss = c * std::fabs(up.hv - dn.hv);
    st.wsComputed = ws1 + dn.hv + st.frictionLoss + st.ceLoss - up.hv;
    return st;
}

void fill(XsResult& r, const XsGeom& g, double ws, double q, double grav) {
    XsProps p = hyd::properties(g, ws);
    r.ws = ws;
    r.q = q;
    r.minChEl = g.invertCh;
    r.area = p.area;
    r.areaLob = p.lob.area;
    r.areaCh = p.ch.area;
    r.areaRob = p.rob.area;
    r.topWidth = p.topWidth;
    r.alpha = p.alpha;
    if (p.conveyance > 0) {
        r.qLob = q * p.lob.conveyance / p.conveyance;
        r.qCh = q * p.ch.conveyance / p.conveyance;
        r.qRob = q * p.rob.conveyance / p.conveyance;
        r.egSlope = (q / p.conveyance) * (q / p.conveyance);
    }
    r.velTotal = p.area > 0 ? q / p.area : 0;
    r.velCh = p.ch.area > 0 ? r.qCh / p.ch.area : 0;
    r.eg = ws + p.alpha * r.velTotal * r.velTotal / (2 * grav);
    double dch = p.ch.topWidth > 0 ? p.ch.area / p.ch.topWidth : 0;
    r.froudeCh = dch > 0 ? r.velCh / std::sqrt(grav * dch) : 0;
    if (p.wallsExtended)
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_WALLS_EXTENDED", {fmt(ws, 3)}));
}

bool resultFinite(const XsResult& r) {
    for (double v : {r.ws, r.eg, r.q, r.area, r.velCh, r.velTotal, r.topWidth, r.alpha, r.egSlope, r.froudeCh})
        if (!finite(v)) return false;
    return true;
}

}  // namespace

double flowAt(const SteadyFlow& flow, const Reach& reach, size_t idx, int profile,
              const std::vector<const CrossSection*>& xsList) {
    // walk upstream from idx to find the nearest flow-change location
    for (size_t k = idx + 1; k-- > 0;) {
        const std::string& rs = xsList[k]->rs;
        for (const auto& fc : flow.changes) {
            if (fc.river == reach.river && fc.reach == reach.reach && fc.rs == rs) {
                if (profile < static_cast<int>(fc.flows.size())) return fc.flows[static_cast<size_t>(profile)];
                return kNaN;
            }
        }
        // numeric match fallback (e.g. "1000" vs "1000.0")
        double v = xsList[k]->rsValue();
        for (const auto& fc : flow.changes) {
            if (fc.river == reach.river && fc.reach == reach.reach) {
                if (finite(v) && rsNumber(fc.rs) == v && profile < static_cast<int>(fc.flows.size()))
                    return fc.flows[static_cast<size_t>(profile)];
            }
        }
    }
    return kNaN;
}

namespace {

// Lengths/coefficients used for the losses between a section and the next
// one downstream; at a junction the junction length replaces the reach lengths.
CrossSection lossCarrier(const CrossSection& xs, double junctionLength) {
    CrossSection c;
    c.lenLeft = c.lenChannel = c.lenRight = junctionLength;
    c.contraction = xs.contraction;
    c.expansion = xs.expansion;
    return c;
}

struct Ctx {
    const Plan& plan;
    UnitSystem units;
    double grav, tol, ctol, maxErr, largeLoss, eps;
    int maxTrials;
};

// ---- subcritical: unknown upstream section i from known downstream state ----
XsResult subStep(const Ctx& c, const XsGeom& Gi, const CrossSection& lossXs, double Qi, const XsGeom& Gd, double wsD,
                 double Qd) {
    XsResult r;
    SectionState dn = stateAt(Gd, wsD, Qd, c.grav);
    const double minEl = Gi.minEl;
    auto clampWs = [&](double w) { return std::max(w, minEl + c.eps); };
    double a0 = clampWs(minEl + (wsD - Gd.minEl));
    Step s0 = energyStep(Gi, lossXs, a0, Qi, dn, wsD, c.plan, c.grav);
    double e0 = s0.wsComputed - a0;
    double bestA = a0, bestE = std::fabs(e0);
    Step bestS = s0;
    bool converged = finite(e0) && std::fabs(e0) < c.tol;
    double aConv = a0;
    Step sConv = s0;
    int trials = 1;
    double a1 = a0, e1 = e0;
    if (!converged) {
        a1 = clampWs(a0 + 0.7 * (finite(e0) ? e0 : 0.1));
        Step s1 = energyStep(Gi, lossXs, a1, Qi, dn, wsD, c.plan, c.grav);
        e1 = s1.wsComputed - a1;
        ++trials;
        if (finite(e1) && std::fabs(e1) < bestE) { bestE = std::fabs(e1); bestA = a1; bestS = s1; }
        if (finite(e1) && std::fabs(e1) < c.tol) { converged = true; aConv = a1; sConv = s1; }
        while (!converged && trials < c.maxTrials) {
            double an;
            if (finite(e0) && finite(e1) && e1 != e0) an = a1 - e1 * (a1 - a0) / (e1 - e0);
            else an = a1 + 0.7 * (finite(e1) ? e1 : 0.1);
            double depth = std::max(a1 - minEl, c.eps);
            if (std::fabs(an - a1) > 0.5 * depth) an = a1 + (an > a1 ? 0.5 : -0.5) * depth;
            an = clampWs(an);
            Step sn = energyStep(Gi, lossXs, an, Qi, dn, wsD, c.plan, c.grav);
            double en = sn.wsComputed - an;
            ++trials;
            a0 = a1; e0 = e1; a1 = an; e1 = en;
            if (finite(en) && std::fabs(en) < bestE) { bestE = std::fabs(en); bestA = an; bestS = sn; }
            if (finite(en) && std::fabs(en) < c.tol) { converged = true; aConv = an; sConv = sn; }
        }
    }
    double critI = hyd::criticalWS(Gi, Qi, c.ctol);
    double ws;
    Step used;
    if (converged) {
        ws = aConv;
        used = sConv;
    } else if (finite(bestE) && bestE <= c.maxErr) {
        ws = bestA;
        used = bestS;
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_NO_CONVERGE_MINERR",
                                  {std::to_string(c.maxTrials), fmt(bestE, 3), lengthUnit(c.units)}));
    } else {
        ws = critI;
        used = energyStep(Gi, lossXs, ws, Qi, dn, wsD, c.plan, c.grav);
        r.critical = true;
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_NO_CONVERGE_CRIT"));
    }
    if (finite(critI) && ws < critI - 1e-9 && !r.critical) {
        ws = critI;
        used = energyStep(Gi, lossXs, ws, Qi, dn, wsD, c.plan, c.grav);
        r.critical = true;
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_CRITICAL_ASSUMED"));
    }
    if (!finite(ws)) { r.ws = kNaN; return r; }
    fill(r, Gi, ws, Qi, c.grav);
    r.critWs = critI;
    r.trials = trials;
    r.frictionLoss = used.frictionLoss;
    r.ceLoss = used.ceLoss;
    r.lengthWeighted = used.length;
    double kUp = r.q > 0 && r.egSlope > 0 ? r.q / std::sqrt(r.egSlope) : 0;
    if (kUp > 0) {
        double ratio = dn.props.conveyance / kUp;
        if (ratio < 0.7 || ratio > 1.4) r.notes.push_back(makeMsg(Severity::Warning, "SOLV_CONV_RATIO", {fmt(ratio, 2)}));
    }
    if (used.frictionLoss + used.ceLoss > c.largeLoss)
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_ENERGY_LOSS_LARGE", {fmt(used.frictionLoss + used.ceLoss, 3), lengthUnit(c.units)}));
    return r;
}

// ---- supercritical: unknown downstream section d from known upstream section u ----
// The losses between u and d use the lengths/coefficients of u (lossXs).
XsResult superStep(const Ctx& c, const XsGeom& Gd, double Qd, const XsGeom& Gu, const CrossSection& lossXs, double wsU,
                   double Qu, bool& atCritical) {
    XsResult r;
    atCritical = false;
    const double critD = hyd::criticalWS(Gd, Qd, c.ctol);
    const double lo = Gd.minEl + c.eps;
    const double hi = finite(critD) ? critD : Gd.minEl + 1.0;
    // err(w) = upstream WS predicted from the energy equation with downstream WS w, minus the known upstream WS
    auto err = [&](double w, Step& st) {
        SectionState dn = stateAt(Gd, w, Qd, c.grav);
        st = energyStep(Gu, lossXs, wsU, Qu, dn, w, c.plan, c.grav);
        return st.wsComputed - wsU;
    };
    Step st;
    int trials = 0;
    double best = kNaN, bestE = std::numeric_limits<double>::infinity();
    Step bestS;
    auto consider = [&](double w) {
        Step s;
        double e = err(w, s);
        ++trials;
        if (finite(e) && std::fabs(e) < bestE) { bestE = std::fabs(e); best = w; bestS = s; }
        return e;
    };
    // secant from a depth-similar guess, kept inside the supercritical branch
    double a0 = std::clamp(Gd.minEl + (wsU - Gu.minEl), lo, hi);
    double a1 = std::clamp(a0 - 0.1 * (hi - lo), lo, hi);
    double e0 = consider(a0), e1 = consider(a1);
    while (bestE >= c.tol && trials < c.maxTrials && finite(e0) && finite(e1) && e1 != e0) {
        double an = std::clamp(a1 - e1 * (a1 - a0) / (e1 - e0), lo, hi);
        if (an == a1) break;
        a0 = a1; e0 = e1; a1 = an; e1 = consider(an);
    }
    if (bestE >= c.tol) {
        // bracket on [lo, hi] and bisect
        const int N = 40;
        double pw = lo, pe = consider(lo);
        for (int k = 1; k <= N && bestE >= c.tol; ++k) {
            double w = lo + (hi - lo) * k / N;
            double e = consider(w);
            if (finite(pe) && finite(e) && pe * e <= 0) {
                double xa = pw, xb = w, fa = pe;
                for (int it = 0; it < 60 && bestE >= c.tol * 0.1; ++it) {
                    double m = 0.5 * (xa + xb);
                    double fm = consider(m);
                    if (!finite(fm)) break;
                    if (fa * fm <= 0) xb = m; else { xa = m; fa = fm; }
                }
                break;
            }
            pw = w; pe = e;
        }
    }
    double ws;
    Step used;
    if (bestE < c.tol) { ws = best; used = bestS; }
    else if (bestE <= c.maxErr) {
        ws = best; used = bestS;
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_NO_CONVERGE_MINERR",
                                  {std::to_string(c.maxTrials), fmt(bestE, 3), lengthUnit(c.units)}));
    } else {
        ws = critD;
        err(ws, used);
        atCritical = true;
        r.critical = true;
        r.notes.push_back(makeMsg(Severity::Warning, "SOLV_SUPER_NO_SOLUTION"));
    }
    if (!finite(ws)) { r.ws = kNaN; return r; }
    fill(r, Gd, ws, Qd, c.grav);
    r.critWs = critD;
    r.trials = trials;
    r.supercritical = true;
    r.frictionLoss = used.frictionLoss;  // stored at d: losses from the section upstream
    r.ceLoss = used.ceLoss;
    r.lengthWeighted = used.length;
    if (ws >= critD - 1e-9) { r.critical = true; atCritical = true; }
    return r;
}


// ---- structures (M4): upstream section xsl[i] from the downstream section xsl[i+1] ----
// Upstream WS of a section for a given upstream energy (subcritical root).
double wsForEnergy(const XsGeom& g, double q, double eg, double crit, double grav) {
    auto f = [&](double ws) { return hyd::totalEnergy(g, ws, q) - eg; };
    const double lo = finite(crit) ? crit : g.minEl + 1e-4;
    if (f(lo) > 0) return kNaN;  // energy below the minimum specific energy
    double hi = std::max(eg, lo + 1e-3);
    for (int i = 0; i < 40 && f(hi) < 0; ++i) hi += 1.0;
    (void)grav;
    return st::bisect(f, lo, hi, 1e-6);
}

CrossSection withPiers(const CrossSection& xs, const std::vector<Pier>& piers, bool up, double top) {
    CrossSection c = xs;
    for (const auto& p : piers) {
        const auto& w = up ? p.upWidth : p.dnWidth;
        const auto& e = up ? p.upElev : p.dnElev;
        if (w.empty()) continue;
        const double pw = *std::max_element(w.begin(), w.end());
        const double ctr = up ? p.upSta : p.dnSta;
        const double ptop = e.empty() ? top : std::max(top, *std::max_element(e.begin(), e.end()));
        c.obstructions.push_back({ctr - pw / 2, ctr + pw / 2, ptop, false});
    }
    std::sort(c.obstructions.begin(), c.obstructions.end(), [](const StationBlock& a, const StationBlock& b) { return a.staStart < b.staStart; });
    return c;
}

XsResult structureSubStep(const Ctx& c, const CrossSection& node, const CrossSection& xsU, const XsGeom& GU, double QU,
                          const CrossSection& xsD, const XsGeom& GD, double wsD, double QD) {
    XsResult r;
    const StructureData& sd = node.structure;
    const double g = c.grav;
    const double critU = hyd::criticalWS(GU, QU, c.ctol);
    const XsProps pD = hyd::properties(GD, wsD);
    const double hv2 = pD.area > 0 ? pD.alpha * std::pow(QD / pD.area, 2) / (2 * g) : 0;
    const double weirC = sd.weirCoef > 0 ? sd.weirCoef : st::defaultWeirCoef(c.units);
    auto crestOf = [](const std::vector<DeckPoint>& d) {
        std::vector<std::pair<double, double>> v;
        for (const auto& p : d) v.emplace_back(p.station, p.high);
        return v;
    };
    double ws = kNaN, eg = kNaN;
    std::string how, howEn;
    double extraQ = 0, gateQ = 0;
    auto u3 = [&](double v) { return fmt(v, 3) + " " + lengthUnit(c.units); };

    if (node.type == NodeType::Bridge) {
        // ---- low flow: energy method through BD and BU, plus Yarnell when there are piers ----
        const double L = xsU.lenChannel;
        const double Ldn = std::max(0.0, L - sd.dist - sd.width);
        double maxLow = -1e300;
        for (const auto& d : sd.deckUp) maxLow = std::max(maxLow, d.low);
        CrossSection bd = withPiers(xsD, sd.piers, false, maxLow), bu = withPiers(xsU, sd.piers, true, maxLow);
        const XsGeom Gbd = XsGeom::from(bd, c.units), Gbu = XsGeom::from(bu, c.units);
        auto carrier = [](const CrossSection& from, double len) {
            CrossSection k;
            k.lenLeft = k.lenChannel = k.lenRight = len;
            k.contraction = from.contraction;
            k.expansion = from.expansion;
            return k;
        };
        XsResult rbd = subStep(c, Gbd, carrier(xsD, Ldn), QD, GD, wsD, QD);
        XsResult rbu = finite(rbd.ws) ? subStep(c, Gbu, carrier(xsD, sd.width), QU, Gbd, rbd.ws, QD) : XsResult{};
        XsResult r3 = finite(rbu.ws) ? subStep(c, GU, carrier(xsU, sd.dist), QU, Gbu, rbu.ws, QU) : XsResult{};
        double wsE = r3.ws, egE = finite(wsE) ? hyd::totalEnergy(GU, wsE, QU) : kNaN;
        std::string cls = (rbd.critical || rbu.critical) ? "B" : "A";
        how = "aliran rendah kelas " + cls + ", metode energi";
        howEn = "low flow class " + cls + ", energy method";
        if (!sd.piers.empty()) {
            // Yarnell (pier shape K = 1.25, square nose)
            double pierA = 0;
            for (const auto& p : sd.piers) {
                if (p.dnWidth.empty()) continue;
                const double pw = *std::max_element(p.dnWidth.begin(), p.dnWidth.end());
                double zb = 1e300;
                for (size_t k = 1; k < xsD.staElev.size(); ++k)
                    if (p.dnSta >= xsD.staElev[k - 1].station && p.dnSta <= xsD.staElev[k].station) {
                        const auto& a = xsD.staElev[k - 1];
                        const auto& b = xsD.staElev[k];
                        zb = a.elev + (b.elev - a.elev) * (p.dnSta - a.station) / std::max(1e-9, b.station - a.station);
                    }
                if (zb < wsD) pierA += pw * (wsD - zb);
            }
            const double A2 = pD.area, y2 = pD.topWidth > 0 ? pD.area / pD.topWidth : 0;
            if (A2 > 0 && y2 > 0) {
                const double V2 = QD / A2, alpha = pierA / A2, w = V2 * V2 / (2 * g) / y2, K = 1.25;
                const double h3 = 2 * K * (K + 10 * w - 0.6) * (alpha + 15 * std::pow(alpha, 4)) * V2 * V2 / (2 * g);
                const double wsY = wsD + std::max(0.0, h3);
                const double egY = hyd::totalEnergy(GU, wsY, QU);
                if (!finite(egE) || egY > egE) {
                    wsE = wsY;
                    egE = egY;
                    how = "aliran rendah, Yarnell (energi tertinggi)";
                    howEn = "low flow, Yarnell (highest energy)";
                }
            }
        }
        ws = wsE;
        eg = egE;
        // ---- high flow: pressure and/or weir when the energy reaches the low chord ----
        const double egBU = finite(rbu.ws) ? hyd::totalEnergy(Gbu, rbu.ws, QU) : kNaN;
        if (!finite(ws) || (finite(egBU) && egBU > maxLow) || (finite(eg) && eg > maxLow)) {
            const st::Opening op = st::bridgeOpening(bu, sd.deckUp, sd.piers, true);
            const st::Opening opD = st::bridgeOpening(xsD, sd.deckDn.empty() ? sd.deckUp : sd.deckDn, sd.piers, false);
            const bool orifice = wsD > opD.maxLowChord;
            const auto crest = crestOf(sd.deckUp);
            auto qOf = [&](double e) {
                double qp = 0;
                if (op.area > 0 && e > op.meanBed) {
                    if (orifice) qp = e > wsD ? 0.8 * op.area * std::sqrt(2 * g * (e - wsD)) : 0;
                    else {
                        const double Z = op.maxLowChord - op.meanBed;
                        const double head = e - op.meanBed - Z / 2;
                        qp = head > 0 && Z > 0 ? st::sluiceCd((e - op.meanBed) / Z) * op.area * std::sqrt(2 * g * head) : 0;
                    }
                }
                return qp + st::weirFlow(crest, e, wsD, weirC);
            };
            const double lo = std::max(op.maxLowChord, wsD);
            const double e3 = st::bisect([&](double e) { return qOf(e) - QU; }, lo, lo + 100, 1e-5);
            if (finite(e3)) {
                const double w3 = wsForEnergy(GU, QU, e3, critU, g);
                if (finite(w3)) {
                    ws = w3;
                    eg = e3;
                    extraQ = st::weirFlow(crest, e3, wsD, weirC);
                    how = orifice ? "aliran tekan (orifice)" : "aliran tekan (pintu sorong)";
                    howEn = orifice ? "pressure flow (orifice)" : "pressure flow (sluice gate)";
                    if (extraQ > 1e-6) { how += " + pelimpah di atas dek"; howEn += " + weir flow over the deck"; }
                }
            }
        }
        if (finite(ws))
            r.notes.push_back(makeMsg(Severity::Info, "SOLV_BRIDGE", {node.rs, how, fmt(ws, 3), fmt(eg, 3), u3(eg - (wsD + hv2)), howEn}));
    } else if (node.type == NodeType::Culvert) {
        const auto crest = crestOf(sd.deckUp);
        auto qOf = [&](double e) {
            double q = st::weirFlow(crest, e, wsD, weirC);
            for (const auto& cg : sd.culverts) q += double(cg.upStations.size()) * st::culvertFlow(cg, e, wsD, hv2, c.units, g);
            return q;
        };
        double lo = 1e300;
        for (const auto& cg : sd.culverts) lo = std::min(lo, cg.upInvert);
        lo = std::max(lo, wsD - 50);
        const double e3 = st::bisect([&](double e) { return qOf(e) - QU; }, lo, lo + 150, 1e-5);
        if (finite(e3)) {
            eg = std::max(e3, finite(critU) ? hyd::totalEnergy(GU, critU, QU) : e3);
            ws = wsForEnergy(GU, QU, eg, critU, g);
            extraQ = st::weirFlow(crest, e3, wsD, weirC);
            bool inlet = false;
            for (const auto& cg : sd.culverts) {
                const double qb = st::culvertFlow(cg, e3, wsD, hv2, c.units, g);
                if (qb > 0 && st::culvertEnergy(cg, qb, wsD, hv2, c.units, g).inletControls) inlet = true;
            }
            how = inlet ? "kendali inlet (FHWA)" : "kendali outlet";
            howEn = inlet ? "inlet control (FHWA)" : "outlet control";
            for (const auto& cg : sd.culverts) {
                st::InletCoef ic;
                if (!st::inletCoefficients(cg.shape, cg.chart, cg.scale, ic)) {
                    how += ", grafik " + std::to_string(cg.chart) + "/" + std::to_string(cg.scale) + " diganti nilai baku bentuknya";
                    howEn += ", chart " + std::to_string(cg.chart) + "/" + std::to_string(cg.scale) + " replaced by the default of its shape";
                    break;
                }
            }
            if (finite(ws))
                r.notes.push_back(makeMsg(Severity::Info, "SOLV_CULVERT", {node.rs, how, fmt(ws, 3), fmt(eg, 3), fmt(extraQ, 2) + " " + flowUnit(c.units), howEn}));
        }
    } else if (node.type == NodeType::InlineStructure) {
        std::vector<std::pair<double, double>> crest, blocked;
        for (const auto& p : sd.weir) crest.emplace_back(p.station, p.elev);
        for (const auto& gt : sd.gates)
            for (double s : gt.stations) blocked.emplace_back(s - gt.width / 2, s + gt.width / 2);
        auto gq = [&](double e) {
            double q = 0;
            for (const auto& gt : sd.gates) {
                const double b = gt.opening < 0 ? gt.height : std::min(gt.opening, gt.height);
                q += double(gt.stations.size()) * st::gateFlow(gt, b, e, wsD, weirC, g);
            }
            return q;
        };
        auto qOf = [&](double e) { return st::weirFlow(crest, e, wsD, weirC, blocked) + gq(e); };
        double lo = 1e300;
        for (const auto& p : sd.weir) lo = std::min(lo, p.elev);
        for (const auto& gt : sd.gates) lo = std::min(lo, gt.invert);
        const double e3 = st::bisect([&](double e) { return qOf(e) - QU; }, lo, lo + 100, 1e-5);
        if (finite(e3)) {
            eg = std::max(e3, finite(critU) ? hyd::totalEnergy(GU, critU, QU) : e3);
            ws = wsForEnergy(GU, QU, eg, critU, g);
            gateQ = gq(e3);
            extraQ = QU - gateQ;
            if (finite(ws))
                r.notes.push_back(makeMsg(Severity::Info, "SOLV_INLINE", {node.rs, fmt(extraQ, 2) + " " + flowUnit(c.units), fmt(gateQ, 2) + " " + flowUnit(c.units), fmt(ws, 3), fmt(eg, 3)}));
        }
    }
    if (!finite(ws)) {
        r.ws = kNaN;
        r.notes.push_back(makeMsg(Severity::Error, "SOLV_STRUCTURE_FAIL", {node.rs, fmt(QU, 2) + " " + flowUnit(c.units)}));
        return r;
    }
    auto keep = r.notes;
    fill(r, GU, ws, QU, g);
    r.notes.insert(r.notes.begin(), keep.begin(), keep.end());
    r.critWs = critU;
    if (finite(critU) && ws <= critU + 1e-6) r.critical = true;
    r.lengthWeighted = xsU.lenChannel;
    return r;
}

struct ReachRun {
    const Reach* reach = nullptr;
    std::vector<const CrossSection*> xsl;
    std::vector<const CrossSection*> structAfter;  // structure node between xsl[i] and xsl[i+1]
    std::vector<XsGeom> G;
    std::vector<double> Q, dist;
    int upJ = -1, dnJ = -1;
    std::vector<XsResult> sub, fin;
    bool subDone = false, finDone = false;
};

int findReachIdx(const std::vector<ReachRun>& rr, const std::string& river, const std::string& reach) {
    for (size_t i = 0; i < rr.size(); ++i)
        if (rr[i].reach->river == river && rr[i].reach->reach == reach) return static_cast<int>(i);
    return -1;
}

double sf(const XsGeom& g, const XsResult& r) { return hyd::specificForce(g, r.ws, r.q); }

}  // namespace

void fillXsResult(XsResult& r, const hyd::XsGeom& g, double ws, double q, double grav) { fill(r, g, ws, q, grav); }

RunResult solve(const Geometry& geom, const SteadyFlow& flow, const Plan& plan, UnitSystem units, const Callbacks& cb) {
    RunResult res;
    res.status = RunStatus::Failed;
    const auto K = constantsFor(units);
    Ctx c{plan, units, K.gravity,
          plan.stdStepTol > 0 ? plan.stdStepTol : K.defaultStdStepTol,
          plan.criticalTol > 0 ? plan.criticalTol : K.defaultCriticalTol,
          plan.maxErrorTol > 0 ? plan.maxErrorTol : K.defaultMaxErrorTol,
          units == UnitSystem::SI ? 0.3 : 1.0,
          units == UnitSystem::SI ? 0.0003 : 0.001,
          plan.maxTrials > 2 ? plan.maxTrials : 20};
    const bool doSub = plan.regime != FlowRegime::Supercritical;
    const bool doSup = plan.regime != FlowRegime::Subcritical;
    res.meta.method = plan.regime == FlowRegime::Subcritical ? "standard-step-subcritical"
                    : plan.regime == FlowRegime::Supercritical ? "standard-step-supercritical" : "standard-step-mixed";

    int totalXs = 0;
    for (const auto& r : geom.reaches) totalXs += static_cast<int>(r.crossSections().size());
    const int total = totalXs * flow.profileCount() * (doSub && doSup ? 2 : 1);
    int done = 0;
    auto tick = [&]() { ++done; if (cb.progress) cb.progress(std::min(done, total), total); };
    auto cancelled = [&]() {
        if (cb.cancelled && cb.cancelled()) {
            res.status = RunStatus::Cancelled;
            res.profiles.clear();
            res.messages.push_back(makeMsg(Severity::Warning, "SOLV_CANCELLED"));
            return true;
        }
        return false;
    };
    res.messages.push_back(makeMsg(Severity::Info, "SOLV_START_REGIME",
                                   {regimeName(plan.regime, false), regimeName(plan.regime, true),
                                    std::to_string(flow.profileCount()), std::to_string(totalXs)}));

    for (int p = 0; p < flow.profileCount(); ++p) {
        ProfileResult pr;
        pr.name = flow.profileNames[static_cast<size_t>(p)];
        // ---- prepare reaches ----
        std::vector<ReachRun> rr;
        for (const auto& reach : geom.reaches) {
            ReachRun x;
            x.reach = &reach;
            x.xsl = reach.crossSections();
            const size_t n = x.xsl.size();
            if (n == 0) continue;
            x.Q.resize(n);
            for (size_t i = 0; i < n; ++i) {
                x.G.push_back(XsGeom::from(*x.xsl[i], units));
                x.Q[i] = flowAt(flow, reach, i, p, x.xsl);
                if (!(x.Q[i] > 0)) {
                    res.messages.push_back(makeMsg(Severity::Error, "FLOW_NO_UPSTREAM", {reach.label(), x.xsl[0]->rs}));
                    return res;
                }
            }
            x.structAfter.assign(n, nullptr);
            {
                int lastXs = -1;
                for (const auto& nd : reach.nodes) {
                    if (nd.type == NodeType::CrossSection) { if (lastXs + 1 < int(n) && &nd == x.xsl[size_t(lastXs + 1)]) ++lastXs; continue; }
                    if ((nd.type == NodeType::Bridge || nd.type == NodeType::Culvert || nd.type == NodeType::InlineStructure) && lastXs >= 0 &&
                        lastXs + 1 < int(n))
                        x.structAfter[size_t(lastXs)] = &nd;
                }
            }
            x.dist.assign(n, 0);
            for (size_t i = n - 1; i-- > 0;) x.dist[i] = x.dist[i + 1] + x.xsl[i]->lenChannel;
            rr.push_back(std::move(x));
        }
        for (size_t j = 0; j < geom.junctions.size(); ++j) {
            const auto& J = geom.junctions[j];
            for (const auto& u : J.up) { int k = findReachIdx(rr, u.first, u.second); if (k >= 0) rr[static_cast<size_t>(k)].dnJ = static_cast<int>(j); }
            for (const auto& d : J.dn) { int k = findReachIdx(rr, d.first, d.second); if (k >= 0) rr[static_cast<size_t>(k)].upJ = static_cast<int>(j); }
        }
        auto junctionLen = [&](const Junction& J, const ReachRun& up, const ReachRun& dn) {
            return J.up.size() > 1 ? J.lengthTo(up.reach->river, up.reach->reach) : J.lengthTo(dn.reach->river, dn.reach->reach);
        };
        auto finish = [&](XsResult& r, const ReachRun& x, size_t i) {
            r.river = x.reach->river;
            r.reach = x.reach->reach;
            r.rs = x.xsl[i]->rs;
            r.channelDistance = x.dist[i];
        };

        // ---- subcritical pass: downstream reaches first ----
        if (doSub) {
            for (size_t guard = 0; guard <= rr.size(); ++guard) {
                bool progress = false;
                for (auto& x : rr) {
                    if (x.subDone) continue;
                    const size_t n = x.xsl.size(), d = n - 1;
                    x.sub.assign(n, XsResult{});
                    XsResult& rd = x.sub[d];
                    if (x.dnJ >= 0) {
                        const Junction& J = geom.junctions[static_cast<size_t>(x.dnJ)];
                        bool ready = true;
                        for (const auto& dn : J.dn) {
                            int k = findReachIdx(rr, dn.first, dn.second);
                            if (k < 0 || !rr[static_cast<size_t>(k)].subDone) ready = false;
                        }
                        if (!ready) continue;
                        bool have = false;
                        for (const auto& dn : J.dn) {
                            const ReachRun& D = rr[static_cast<size_t>(findReachIdx(rr, dn.first, dn.second))];
                            CrossSection carrier = lossCarrier(*x.xsl[d], junctionLen(J, x, D));
                            XsResult t = subStep(c, x.G[d], carrier, x.Q[d], D.G[0], D.sub[0].ws, D.Q[0]);
                            if (finite(t.ws) && (!have || t.ws > rd.ws)) { rd = t; have = true; }
                        }
                        if (!have) {
                            res.messages.push_back(makeMsg(Severity::Error, "SOLV_NAN", {}, x.reach->label() + " RS " + x.xsl[d]->rs));
                            return res;
                        }
                        rd.notes.push_back(makeMsg(Severity::Info, "SOLV_JUNCTION_DN", {J.name}));
                    } else {
                        const ReachBoundary* bc = flow.boundary(x.reach->river, x.reach->reach, p);
                        double critD = hyd::criticalWS(x.G[d], x.Q[d], c.ctol);
                        double wsD = kNaN;
                        if (!bc || bc->downstream.type == BoundaryType::None) {
                            res.messages.push_back(makeMsg(Severity::Error, "BC_MISSING_DN", {pr.name, x.reach->label()}));
                            return res;
                        }
                        const Boundary& b = bc->downstream;
                        switch (b.type) {
                            case BoundaryType::KnownWS: wsD = b.knownWS; break;
                            case BoundaryType::CriticalDepth:
                                wsD = critD;
                                rd.critical = true;
                                rd.notes.push_back(makeMsg(Severity::Info, "SOLV_BC_CRIT"));
                                break;
                            case BoundaryType::NormalDepth:
                                wsD = hyd::normalWS(x.G[d], x.Q[d], b.slope, c.tol);
                                if (!finite(wsD)) {
                                    res.messages.push_back(makeMsg(Severity::Error, "SOLV_BC_NORMAL_FAIL", {pr.name}));
                                    return res;
                                }
                                break;
                            case BoundaryType::RatingCurve: {
                                bool ex = false;
                                wsD = ratingStage(b.rating, x.Q[d], ex);
                                if (ex) rd.notes.push_back(makeMsg(Severity::Warning, "SOLV_BC_RATING_RANGE", {fmt(x.Q[d], 2)}));
                                break;
                            }
                            default: break;
                        }
                        if (!finite(wsD)) {
                            res.messages.push_back(makeMsg(Severity::Error, "SOLV_NAN", {}, x.reach->label() + " RS " + x.xsl[d]->rs));
                            return res;
                        }
                        if (finite(critD) && wsD < critD - 1e-9 && b.type != BoundaryType::CriticalDepth) {
                            rd.notes.push_back(makeMsg(Severity::Warning, "SOLV_DN_BELOW_CRIT", {fmt(wsD, 3), fmt(critD, 3)}));
                            wsD = critD;
                            rd.critical = true;
                        }
                        auto keepNotes = rd.notes;
                        bool crit = rd.critical;
                        fill(rd, x.G[d], wsD, x.Q[d], c.grav);
                        rd.notes.insert(rd.notes.begin(), keepNotes.begin(), keepNotes.end());
                        rd.critical = crit;
                        rd.critWs = critD;
                    }
                    finish(rd, x, d);
                    tick();
                    for (size_t i = d; i-- > 0;) {
                        if (cancelled()) return res;
                        XsResult r = x.structAfter[i] ? structureSubStep(c, *x.structAfter[i], *x.xsl[i], x.G[i], x.Q[i], *x.xsl[i + 1], x.G[i + 1], x.sub[i + 1].ws, x.Q[i + 1])
                                                      : subStep(c, x.G[i], *x.xsl[i], x.Q[i], x.G[i + 1], x.sub[i + 1].ws, x.Q[i + 1]);
                        if (!finite(r.ws)) {
                            res.messages.push_back(makeMsg(Severity::Error, "SOLV_NAN", {}, x.reach->label() + " RS " + x.xsl[i]->rs));
                            return res;
                        }
                        finish(r, x, i);
                        x.sub[i] = r;
                        tick();
                    }
                    x.subDone = true;
                    progress = true;
                }
                if (!progress) break;
            }
            for (const auto& x : rr)
                if (!x.subDone) {
                    res.messages.push_back(makeMsg(Severity::Error, "MD_JUNCTION_LOOP"));
                    return res;
                }
        }

        // ---- supercritical / mixed pass: upstream reaches first ----
        if (!doSup) {
            for (auto& x : rr) x.fin = x.sub;
        } else {
            for (size_t guard = 0; guard <= rr.size(); ++guard) {
                bool progress = false;
                for (auto& x : rr) {
                    if (x.finDone) continue;
                    const size_t n = x.xsl.size();
                    // starting supercritical state at the first section (if any)
                    bool start = false;
                    XsResult r0;
                    if (x.upJ >= 0) {
                        const Junction& J = geom.junctions[static_cast<size_t>(x.upJ)];
                        bool ready = true;
                        for (const auto& u : J.up) {
                            int k = findReachIdx(rr, u.first, u.second);
                            if (k < 0 || !rr[static_cast<size_t>(k)].finDone) ready = false;
                        }
                        if (!ready) continue;
                        for (const auto& u : J.up) {
                            const ReachRun& U = rr[static_cast<size_t>(findReachIdx(rr, u.first, u.second))];
                            const XsResult& last = U.fin.back();
                            if (!last.supercritical && !last.critical) continue;
                            CrossSection carrier = lossCarrier(*U.xsl.back(), junctionLen(J, U, x));
                            bool atC = false;
                            XsResult t = superStep(c, x.G[0], x.Q[0], U.G.back(), carrier, last.ws, U.Q.back(), atC);
                            if (finite(t.ws) && (!start || t.ws > r0.ws)) { r0 = t; start = true; }
                        }
                        if (start) r0.notes.push_back(makeMsg(Severity::Info, "SOLV_JUNCTION_UP", {J.name}));
                    } else {
                        const ReachBoundary* bc = flow.boundary(x.reach->river, x.reach->reach, p);
                        const Boundary* b = bc ? &bc->upstream : nullptr;
                        if (!b || b->type == BoundaryType::None) {
                            if (plan.regime == FlowRegime::Supercritical) {
                                res.messages.push_back(makeMsg(Severity::Error, "BC_MISSING_UP", {pr.name, x.reach->label()}));
                                return res;
                            }
                        } else {
                            double critU = hyd::criticalWS(x.G[0], x.Q[0], c.ctol);
                            double wsU = kNaN;
                            if (b->type == BoundaryType::KnownWS) wsU = b->knownWS;
                            else if (b->type == BoundaryType::CriticalDepth) wsU = critU;
                            else if (b->type == BoundaryType::NormalDepth) wsU = hyd::normalWS(x.G[0], x.Q[0], b->slope, c.tol);
                            if (!finite(wsU)) {
                                res.messages.push_back(makeMsg(Severity::Error, "SOLV_BC_UP_FAIL", {pr.name}, x.reach->label()));
                                return res;
                            }
                            bool crit = b->type == BoundaryType::CriticalDepth;
                            if (finite(critU) && wsU > critU + 1e-9) {
                                if (plan.regime == FlowRegime::Supercritical) {
                                    r0.notes.push_back(makeMsg(Severity::Warning, "SOLV_UP_ABOVE_CRIT", {fmt(wsU, 3), fmt(critU, 3)}));
                                    wsU = critU;
                                    crit = true;
                                } else {
                                    wsU = kNaN;  // mixed: subcritical boundary, no supercritical start here
                                }
                            }
                            if (finite(wsU)) {
                                auto keep = r0.notes;
                                fill(r0, x.G[0], wsU, x.Q[0], c.grav);
                                r0.notes.insert(r0.notes.begin(), keep.begin(), keep.end());
                                r0.critWs = critU;
                                r0.critical = crit || wsU >= critU - 1e-9;
                                r0.supercritical = true;
                                start = true;
                            }
                        }
                    }
                    if (plan.regime == FlowRegime::Supercritical && !start) {
                        res.messages.push_back(makeMsg(Severity::Error, "BC_MISSING_UP", {pr.name, x.reach->label()}));
                        return res;
                    }
                    x.fin = doSub ? x.sub : std::vector<XsResult>(n);
                    bool active = false;
                    XsResult prev;
                    if (start) {
                        finish(r0, x, 0);
                        if (!doSub || sf(x.G[0], r0) > sf(x.G[0], x.sub[0])) { x.fin[0] = r0; active = true; }
                    }
                    if (!active && doSub && x.sub[0].critical) active = true;  // supercritical may start from critical depth
                    tick();
                    for (size_t i = 1; i < n; ++i) {
                        if (cancelled()) return res;
                        if (!active) {
                            if (doSub && x.sub[i].critical) active = true;  // restart at a critical section
                            tick();
                            continue;
                        }
                        bool atC = false;
                        XsResult t = superStep(c, x.G[i], x.Q[i], x.G[i - 1], *x.xsl[i - 1], x.fin[i - 1].ws, x.Q[i - 1], atC);
                        if (!finite(t.ws)) {
                            res.messages.push_back(makeMsg(Severity::Error, "SOLV_NAN", {}, x.reach->label() + " RS " + x.xsl[i]->rs));
                            return res;
                        }
                        finish(t, x, i);
                        if (!doSub || sf(x.G[i], t) > sf(x.G[i], x.sub[i])) {
                            x.fin[i] = t;
                        } else {
                            x.fin[i].notes.push_back(makeMsg(Severity::Info, "SOLV_JUMP", {x.xsl[i - 1]->rs, x.xsl[i]->rs}));
                            active = x.sub[i].critical;
                        }
                        tick();
                    }
                    x.finDone = true;
                    progress = true;
                }
                if (!progress) break;
            }
            for (const auto& x : rr)
                if (!x.finDone) {
                    res.messages.push_back(makeMsg(Severity::Error, "MD_JUNCTION_LOOP"));
                    return res;
                }
        }

        // ---- collect ----
        for (auto& x : rr) {
            for (size_t i = 0; i < x.fin.size(); ++i) {
                XsResult& r = x.fin[i];
                finish(r, x, i);
                if (!resultFinite(r)) {
                    res.messages.push_back(makeMsg(Severity::Error, "SOLV_NAN", {}, x.reach->label() + " RS " + x.xsl[i]->rs));
                    return res;
                }
                for (auto m : r.notes) {
                    m.location = x.reach->label() + " RS " + x.xsl[i]->rs + " / " + pr.name;
                    res.messages.push_back(m);
                }
            }
            pr.xs.insert(pr.xs.end(), x.fin.begin(), x.fin.end());
        }
        res.messages.push_back(makeMsg(Severity::Info, "SOLV_PROFILE_DONE", {pr.name}));
        res.profiles.push_back(std::move(pr));
    }
    res.status = RunStatus::Succeeded;
    res.messages.push_back(makeMsg(Severity::Info, "SOLV_DONE"));
    return res;
}

}  // namespace deras::steady

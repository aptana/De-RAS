// De-RAS - engines/design/Design.cpp
#include "engines/design/Design.h"

#include <algorithm>
#include <cmath>

#include "engines/common/XsHydraulics.h"
#include "engines/steady/SteadySolver.h"

namespace deras::design {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double ftM = 0.3048;
double gOf(UnitSystem u) { return u == UnitSystem::SI ? 9.80665 : 32.174; }
double kOf(UnitSystem u) { return u == UnitSystem::SI ? 1.0 : 1.486; }
double gammaOf(UnitSystem u) { return u == UnitSystem::SI ? 9806.65 : 62.4; }   // N/m3 or lb/ft3
}  // namespace

// ---------------- uniform flow ----------------
UniformResult uniformFromDepth(const Trapezoid& c, double y, UnitSystem u) {
    UniformResult r;
    if (!(y > 0) || !(c.n > 0) || !(c.slope > 0) || c.bottom < 0 || c.sideSlope < 0 || (c.bottom == 0 && c.sideSlope == 0)) return r;
    r.depth = y;
    r.area = (c.bottom + c.sideSlope * y) * y;
    r.perimeter = c.bottom + 2 * y * std::sqrt(1 + c.sideSlope * c.sideSlope);
    r.radius = r.area / r.perimeter;
    r.topWidth = c.bottom + 2 * c.sideSlope * y;
    r.flow = kOf(u) / c.n * r.area * std::pow(r.radius, 2.0 / 3.0) * std::sqrt(c.slope);
    r.velocity = r.flow / r.area;
    r.froude = r.velocity / std::sqrt(gOf(u) * r.area / r.topWidth);
    r.shear = gammaOf(u) * r.radius * c.slope;
    // critical depth: Q^2 T / (g A^3) = 1
    double lo = 1e-6, hi = std::max(y, 1.0);
    auto f = [&](double d) {
        const double A = (c.bottom + c.sideSlope * d) * d, T = c.bottom + 2 * c.sideSlope * d;
        return r.flow * r.flow * T / (gOf(u) * A * A * A) - 1;
    };
    while (f(hi) > 0 && hi < 1e5) hi *= 2;
    for (int i = 0; i < 200; ++i) {
        const double m = 0.5 * (lo + hi);
        (f(m) > 0 ? lo : hi) = m;
    }
    r.criticalDepth = 0.5 * (lo + hi);
    r.ok = true;
    return r;
}

UniformResult uniformFromFlow(const Trapezoid& c, double q, UnitSystem u) {
    if (!(q > 0)) return {};
    double lo = 1e-6, hi = 1.0;
    for (int i = 0; i < 60 && uniformFromDepth(c, hi, u).flow < q; ++i) hi *= 2;
    if (!(uniformFromDepth(c, hi, u).flow >= q)) return {};
    for (int i = 0; i < 200; ++i) {
        const double m = 0.5 * (lo + hi);
        (uniformFromDepth(c, m, u).flow < q ? lo : hi) = m;
    }
    return uniformFromDepth(c, 0.5 * (lo + hi), u);
}

// ---------------- scour ----------------
double pierShapeK1(PierNose n) {
    switch (n) {
        case PierNose::Square: return 1.1;
        case PierNose::Sharp: return 0.9;
        default: return 1.0;
    }
}

PierScourResult pierScour(const PierScourInput& in, UnitSystem u) {
    PierScourResult r;
    if (!(in.a > 0) || !(in.y1 > 0) || !(in.V1 >= 0)) return r;
    const double th = in.angle * kPi / 180.0;
    r.K2 = std::pow(std::cos(th) + std::min(std::max(in.L, in.a) / in.a, 12.0) * std::sin(std::fabs(th)), 0.65);   // L/a at most 12
    // HEC-18: K1 = 1.0 when the attack angle exceeds 5 degrees (K2 governs)
    r.K1 = std::fabs(in.angle) > 5 ? 1.0 : pierShapeK1(in.nose);
    r.froude = in.V1 / std::sqrt(gOf(u) * in.y1);
    r.scour = in.a * 2.0 * r.K1 * r.K2 * in.K3 * std::pow(in.y1 / in.a, 0.35) * std::pow(r.froude, 0.43);
    r.limit = (r.froude <= 0.8 ? 2.4 : 3.0) * in.a;
    // the limit applies to round-nosed piers aligned with the flow
    if (std::fabs(in.angle) <= 5 && in.nose != PierNose::Square && r.scour > r.limit) {
        r.scour = r.limit;
        r.limited = true;
    }
    return r;
}

ContractionResult contractionScour(const ContractionInput& in, UnitSystem u) {
    ContractionResult r;
    if (!(in.y1 > 0) || !(in.Q1 > 0) || !(in.Q2 > 0) || !(in.W1 > 0) || !(in.W2 > 0)) return r;
    const double g = gOf(u);
    if (in.liveBed) {
        const double ustar = std::sqrt(g * in.y1 * std::max(in.S1, 0.0));
        const double dM = in.D50mm / 1000.0;
        // fall velocity (Soulsby 1997), SI, then back to the project units
        const double nu = 1.0e-6, s = 2.65;
        const double dstar = dM * std::cbrt((s - 1) * 9.80665 / (nu * nu));
        double w = nu / std::max(dM, 1e-9) * (std::sqrt(10.36 * 10.36 + 1.049 * dstar * dstar * dstar) - 10.36);
        if (u == UnitSystem::USCustomary) w /= ftM;
        r.ustarOverW = w > 0 ? ustar / w : 0;
        r.k1 = r.ustarOverW < 0.5 ? 0.59 : r.ustarOverW <= 2.0 ? 0.64 : 0.69;
        r.y2 = in.y1 * std::pow(in.Q2 / in.Q1, 6.0 / 7.0) * std::pow(in.W1 / in.W2, r.k1);
    } else {
        const double Ku = u == UnitSystem::SI ? 0.025 : 0.0077;
        const double Dm = 1.25 * in.D50mm / 1000.0 / (u == UnitSystem::SI ? 1.0 : ftM);   // project length unit
        r.y2 = std::pow(Ku * in.Q2 * in.Q2 / (std::pow(Dm, 2.0 / 3.0) * in.W2 * in.W2), 3.0 / 7.0);
    }
    r.scour = std::max(0.0, r.y2 - in.y0);
    return r;
}

AbutmentResult abutmentScour(const AbutmentInput& in, UnitSystem u) {
    AbutmentResult r;
    if (!(in.ya > 0) || !(in.Lprime > 0)) return r;
    r.K2 = std::pow(std::clamp(in.angle, 1.0, 179.0) / 90.0, 0.13);
    r.froude = in.Ve / std::sqrt(gOf(u) * in.ya);
    r.scour = in.ya * (2.27 * in.K1 * r.K2 * std::pow(in.Lprime / in.ya, 0.43) * std::pow(r.froude, 0.61) + 1.0);
    return r;
}

// ---------------- Manning's n calibration ----------------
void scaleManning(Geometry& g, double factor, int scope) {
    for (auto& r : g.reaches)
        for (auto& xs : r.nodes) {
            if (xs.type != NodeType::CrossSection) continue;
            const double bl = xs.bankLeft.value_or(-1e300), br = xs.bankRight.value_or(1e300);
            for (size_t i = 0; i < xs.manning.size(); ++i) {
                const double s0 = xs.manning[i].station;
                const bool inChannel = s0 >= bl - 1e-9 && s0 < br - 1e-9;
                if (scope == 1 || inChannel) xs.manning[i].n = std::round(xs.manning[i].n * factor * 1e5) / 1e5;
            }
        }
}

CalibrationResult calibrateN(const Geometry& g, const SteadyFlow& f, const Plan& p, UnitSystem u, int profile, const std::vector<Observation>& obs,
                             int scope, double lo, double hi) {
    CalibrationResult out;
    if (obs.empty()) { out.error = "no observations"; return out; }
    if (profile < 0 || profile >= f.profileCount()) { out.error = "profile"; return out; }
    SteadyFlow one = f;
    one.profileNames = {f.profileNames[size_t(profile)]};
    for (auto& c : one.changes) c.flows = {c.flows.size() > size_t(profile) ? c.flows[size_t(profile)] : 0.0};
    std::vector<ReachBoundary> bnd;
    for (const auto& b : f.boundaries)
        if (b.profile == profile) {
            ReachBoundary x = b;
            x.profile = 0;
            bnd.push_back(x);
        }
    one.boundaries = bnd;
    auto evaluate = [&](double factor, std::vector<double>& ws) -> double {
        Geometry gg = g;
        scaleManning(gg, factor, scope);
        ++out.runs;
        const RunResult r = steady::solve(gg, one, p, u, {});
        ws.assign(obs.size(), std::nan(""));
        if (r.status != RunStatus::Succeeded || r.profiles.empty()) return 1e300;
        double s = 0;
        for (size_t i = 0; i < obs.size(); ++i) {
            for (const auto& x : r.profiles[0].xs)
                if (x.river == obs[i].river && x.reach == obs[i].reach && x.rs == obs[i].rs) ws[i] = x.ws;
            if (!std::isfinite(ws[i])) return 1e300;
            s += (ws[i] - obs[i].ws) * (ws[i] - obs[i].ws);
        }
        return std::sqrt(s / double(obs.size()));
    };
    out.rmsBefore = evaluate(1.0, out.computedBefore);
    if (!(out.rmsBefore < 1e299)) { out.error = "run"; return out; }
    // golden-section search on log(factor)
    const double gr = (std::sqrt(5.0) - 1) / 2;
    double a = std::log(lo), b = std::log(hi);
    std::vector<double> tmp;
    double c = b - gr * (b - a), d = a + gr * (b - a);
    double fc = evaluate(std::exp(c), tmp), fd = evaluate(std::exp(d), tmp);
    for (int it = 0; it < 40 && b - a > 1e-4; ++it) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - gr * (b - a); fc = evaluate(std::exp(c), tmp); }
        else { a = c; c = d; fc = fd; d = a + gr * (b - a); fd = evaluate(std::exp(d), tmp); }
    }
    out.factor = std::round(std::exp(0.5 * (a + b)) * 1e4) / 1e4;
    out.rmsAfter = evaluate(out.factor, out.computedAfter);
    if (out.rmsAfter > out.rmsBefore) {   // no improvement inside the range: keep n
        out.factor = 1;
        out.rmsAfter = out.rmsBefore;
        out.computedAfter = out.computedBefore;
    }
    out.ok = true;
    return out;
}

// ---------------- water quality ----------------
WqResult waterQuality(const Geometry& g, const RunResult& r, const WqInput& in, UnitSystem u) {
    WqResult res;
    if (r.status != RunStatus::Succeeded || r.series.empty() || r.profiles.empty()) { res.error = "no unsteady results"; res.status = RunStatus::Failed; return res; }
    const auto& xs0 = r.profiles[0].xs;
    if (xs0.size() < 2) { res.error = "too few sections"; res.status = RunStatus::Failed; return res; }
    res.river = xs0[0].river;
    res.reach = xs0[0].reach;
    const Reach* reach = g.findReach(res.river, res.reach);
    if (!reach) { res.error = "reach"; res.status = RunStatus::Failed; return res; }
    // sections of the first reach (series columns follow profiles[0])
    std::vector<size_t> col;
    std::vector<const CrossSection*> sec;
    for (size_t i = 0; i < xs0.size(); ++i) {
        if (xs0[i].river != res.river || xs0[i].reach != res.reach) continue;
        for (const auto& n : reach->nodes)
            if (n.type == NodeType::CrossSection && n.rs == xs0[i].rs) { col.push_back(i); sec.push_back(&n); break; }
    }
    const size_t N = col.size();
    if (N < 2) { res.error = "too few sections"; res.status = RunStatus::Failed; return res; }
    std::vector<double> dx(N - 1);
    for (size_t i = 0; i + 1 < N; ++i) dx[i] = std::max(sec[i]->lenChannel, 1e-3);
    res.distance.assign(N, 0);
    for (size_t i = N - 1; i-- > 0;) res.distance[i] = res.distance[i + 1] + dx[i];
    for (size_t i = 0; i < N; ++i) res.rs.push_back(sec[i]->rs);
    std::vector<hyd::XsGeom> geo;
    for (const auto* s : sec) geo.push_back(hyd::XsGeom::from(*s, u));
    const auto& T = r.series.hours;
    auto areaAt = [&](size_t t, size_t i) { return std::max(hyd::properties(geo[i], r.series.ws[t][col[i]]).area, 1e-6); };
    auto upstreamC = [&](double h) {
        const auto& v = in.upstream;
        if (v.empty()) return in.initial;
        if (h <= v.front().first) return v.front().second;
        if (h >= v.back().first) return v.back().second;
        for (size_t k = 1; k < v.size(); ++k)
            if (h <= v[k].first) return v[k - 1].second + (h - v[k - 1].first) / std::max(v[k].first - v[k - 1].first, 1e-12) * (v[k].second - v[k - 1].second);
        return v.back().second;
    };
    const double kd = std::max(in.decayPerDay, 0.0) / 86400.0;
    // control volumes: half the reach lengths on both sides
    std::vector<double> len(N);
    for (size_t i = 0; i < N; ++i) len[i] = 0.5 * ((i > 0 ? dx[i - 1] : 0) + (i + 1 < N ? dx[i] : 0));
    std::vector<double> C(N, in.initial), A0(N), A1(N), a(N), b(N), c(N), d(N);
    C[0] = upstreamC(T.front());
    for (size_t i = 0; i < N; ++i) A0[i] = areaAt(0, i);
    for (size_t i = 0; i < N; ++i) res.massStored0 += C[i] * A0[i] * len[i];
    res.hours.push_back(T.front());
    res.conc.push_back(C);
    for (size_t t = 1; t < T.size(); ++t) {
        const double dtTot = (T[t] - T[t - 1]) * 3600.0;
        if (!(dtTot > 0)) continue;
        // sub-steps keep the Courant number near 1 for accuracy (the scheme is stable at any step)
        double vmax = 0;
        for (size_t i = 0; i < N; ++i) vmax = std::max(vmax, std::fabs(r.series.q[t][col[i]]) / areaAt(t, i));
        double dxmin = 1e300;
        for (double v : dx) dxmin = std::min(dxmin, v);
        const int m = std::clamp(int(std::ceil(dtTot * vmax / dxmin)), 1, 2000);
        const double dt = dtTot / m;
        for (int k = 0; k < m; ++k) {
            const double f0 = double(k) / m, f1 = double(k + 1) / m;
            std::vector<double> Q(N);
            for (size_t i = 0; i < N; ++i) {
                const double a0 = areaAt(t - 1, i), a1 = areaAt(t, i);
                A0[i] = a0 + f0 * (a1 - a0);
                A1[i] = a0 + f1 * (a1 - a0);
                Q[i] = r.series.q[t - 1][col[i]] + f1 * (r.series.q[t][col[i]] - r.series.q[t - 1][col[i]]);
            }
            const double hNow = T[t - 1] + (T[t] - T[t - 1]) * f1;
            const double cUp = upstreamC(hNow);
            // finite volume, implicit: V1 C1 - V0 C0 = dt (adv + disp - k V1 C1); face flow = average of the two sections
            for (size_t i = 0; i < N; ++i) { a[i] = b[i] = c[i] = d[i] = 0; }
            b[0] = 1;
            d[0] = cUp;
            std::vector<double> V0eff(N, 0.0);
            for (size_t i = 1; i < N; ++i) {
                const double V1 = A1[i] * len[i];
                // Start volume consistent with the face flows of this sub-step
                // (V1 - V0 = dt (in - out)), so that a uniform concentration
                // stays uniform even where the stored hydraulics do not close
                // continuity exactly. The difference to the hydraulic volume is
                // reported as massCorrection.
                const double qin = 0.5 * (Q[i - 1] + Q[i]);
                const double qout = i + 1 < N ? 0.5 * (Q[i] + Q[i + 1]) : std::max(Q[i], 0.0);
                V0eff[i] = std::max(V1 - dt * (qin - qout), 0.05 * V1);
                b[i] = V1 * (1 + dt * kd);
                d[i] = V0eff[i] * C[i];
                // face between i-1 and i
                {
                    const double qf = 0.5 * (Q[i - 1] + Q[i]);
                    const double Af = 0.5 * (A1[i - 1] + A1[i]);
                    const double Df = in.dispersion * Af / dx[i - 1];
                    // flux into i: upwind advection + dispersion
                    if (qf >= 0) a[i] -= dt * (qf + Df); else { b[i] += dt * (-qf); a[i] -= dt * Df; }
                    b[i] += dt * Df;
                    if (qf < 0) { /* outflow through the upstream face already in b */ }
                }
                if (i + 1 < N) {
                    const double qf = 0.5 * (Q[i] + Q[i + 1]);
                    const double Af = 0.5 * (A1[i] + A1[i + 1]);
                    const double Df = in.dispersion * Af / dx[i];
                    if (qf >= 0) b[i] += dt * qf; else c[i] -= dt * (-qf);
                    b[i] += dt * Df;
                    c[i] -= dt * Df;
                } else {
                    // downstream end: advective outflow only
                    b[i] += dt * std::max(Q[i], 0.0);
                }
            }
            // Thomas algorithm
            std::vector<double> cp(N), dp(N);
            cp[0] = c[0] / b[0];
            dp[0] = d[0] / b[0];
            for (size_t i = 1; i < N; ++i) {
                const double den = b[i] - a[i] * cp[i - 1];
                cp[i] = c[i] / den;
                dp[i] = (d[i] - a[i] * dp[i - 1]) / den;
            }
            std::vector<double> Cn(N);
            Cn[N - 1] = dp[N - 1];
            for (size_t i = N - 1; i-- > 0;) Cn[i] = dp[i] - cp[i] * Cn[i + 1];
            // mass bookkeeping (control volumes 1..N-1; node 0 is the boundary)
            {
                const double qf = 0.5 * (Q[0] + Q[1]);
                const double Af = 0.5 * (A1[0] + A1[1]);
                const double Df = in.dispersion * Af / dx[0];
                res.massIn += dt * ((qf >= 0 ? qf * Cn[0] : qf * Cn[1]) + Df * (Cn[0] - Cn[1]));
                res.massOut += dt * std::max(Q[N - 1], 0.0) * Cn[N - 1];
                for (size_t i = 1; i < N; ++i) res.massDecayed += dt * kd * A1[i] * len[i] * Cn[i];
                for (size_t i = 1; i < N; ++i) res.massCorrection += (V0eff[i] - A0[i] * len[i]) * C[i];
            }
            C = Cn;
        }
        res.hours.push_back(T[t]);
        res.conc.push_back(C);
    }
    res.massStored0 = 0;
    for (size_t i = 1; i < N; ++i) res.massStored0 += in.initial * areaAt(0, i) * len[i];
    for (size_t i = 1; i < N; ++i) res.massStored += C[i] * areaAt(T.size() - 1, i) * len[i];
    res.status = RunStatus::Succeeded;
    return res;
}

// ---------------- unit conversion ----------------
namespace {
struct F { double L, Q, A, V, C; };   // length, flow, area, volume, weir coefficient factors
F factors(bool toSI) {
    const double L = toSI ? ftM : 1 / ftM;
    return {L, L * L * L, L * L, L * L * L, toSI ? std::sqrt(ftM) : 1 / std::sqrt(ftM)};
}
void convXs(CrossSection& x, const F& f) {
    x.lenLeft *= f.L; x.lenChannel *= f.L; x.lenRight *= f.L;
    for (auto& p : x.staElev) { p.station *= f.L; p.elev *= f.L; }
    for (auto& m : x.manning) m.station *= f.L;
    if (x.bankLeft) *x.bankLeft *= f.L;
    if (x.bankRight) *x.bankRight *= f.L;
    for (auto& b : x.ineffective) { b.staStart *= f.L; b.staEnd *= f.L; b.elev *= f.L; }
    for (auto& b : x.obstructions) { b.staStart *= f.L; b.staEnd *= f.L; b.elev *= f.L; }
    for (auto* l : {&x.leveeLeft, &x.leveeRight}) { l->station *= f.L; l->elev *= f.L; }
    auto& s = x.structure;
    s.dist *= f.L; s.width *= f.L; s.weirCoef *= f.C;
    for (auto* d : {&s.deckUp, &s.deckDn}) for (auto& p : *d) { p.station *= f.L; p.high *= f.L; p.low *= f.L; }
    for (auto& p : s.piers) {
        p.upSta *= f.L; p.dnSta *= f.L;
        for (auto* v : {&p.upWidth, &p.upElev, &p.dnWidth, &p.dnElev}) for (auto& e : *v) e *= f.L;
    }
    for (auto& c : s.culverts) {
        c.span *= f.L; c.rise *= f.L; c.length *= f.L; c.upInvert *= f.L; c.dnInvert *= f.L;
        for (auto* v : {&c.upStations, &c.dnStations}) for (auto& e : *v) e *= f.L;
    }
    for (auto& p : s.weir) { p.station *= f.L; p.elev *= f.L; }
    for (auto& gt : s.gates) {
        gt.width *= f.L; gt.height *= f.L; gt.invert *= f.L; gt.weirCoef *= f.C;
        if (gt.opening >= 0) gt.opening *= f.L;
        for (auto& e : gt.stations) e *= f.L;
    }
    auto& b = s.lateral.breach;
    b.station *= f.L; b.bottomElev *= f.L; b.bottomWidth *= f.L;
}
}  // namespace

void convertGeometry(Geometry& g, bool toSI) {
    const F f = factors(toSI);
    for (auto& r : g.reaches) for (auto& x : r.nodes) convXs(x, f);
    for (auto& j : g.junctions) for (auto& l : j.lengths) l *= f.L;
    for (auto& a : g.storageAreas) {
        for (auto& p : a.volElev) { p.first *= f.L; p.second *= f.V; }
        a.area *= f.A;
        a.minElev *= f.L;
        a.cellSize *= f.L;
    }
    for (auto& c : g.connections) {
        c.width *= f.L; c.coef *= f.C;
        for (auto& p : c.weir) { p.station *= f.L; p.elev *= f.L; }
        c.breach.station *= f.L; c.breach.bottomElev *= f.L; c.breach.bottomWidth *= f.L;
    }
    for (auto& p : g.pumps)
        for (auto& gr : p.groups) { gr.capacity *= f.Q; gr.onElev *= f.L; gr.offElev *= f.L; }
}

void convertSteadyFlow(SteadyFlow& fl, bool toSI) {
    const F f = factors(toSI);
    for (auto& c : fl.changes) for (auto& q : c.flows) q *= f.Q;
    for (auto& b : fl.boundaries)
        for (auto* x : {&b.upstream, &b.downstream}) {
            x->knownWS *= f.L;
            for (auto& p : x->rating) { p.first *= f.L; p.second *= f.Q; }
        }
}

void convertUnsteadyFlow(UnsteadyFlow& fl, bool toSI) {
    const F f = factors(toSI);
    for (auto& i : fl.initialFlows) i.q *= f.Q;
    for (auto& s : fl.initialStorage) s.second *= f.L;
    for (auto& b : fl.boundaries) {
        switch (b.type) {
            case UBoundaryType::FlowHydrograph:
            case UBoundaryType::LateralInflow:
            case UBoundaryType::UniformLateralInflow:
                for (auto& v : b.values) v *= f.Q;
                break;
            case UBoundaryType::StageHydrograph:
                for (auto& v : b.values) v *= f.L;
                break;
            case UBoundaryType::Precipitation:   // mm <-> in per interval
                for (auto& v : b.values) v *= toSI ? 25.4 : 1 / 25.4;
                break;
            default: break;
        }
        for (auto& p : b.rating) { p.first *= f.L; p.second *= f.Q; }
    }
}

void convertPlan(Plan& p, bool toSI) {
    const F f = factors(toSI);
    p.stdStepTol *= f.L;
    p.criticalTol *= f.L;
    p.maxErrorTol *= f.L;
    p.unetZTol *= f.L;
}

}  // namespace deras::design

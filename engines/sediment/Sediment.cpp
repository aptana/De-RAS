// De-RAS - engines/sediment/Sediment.cpp
#include "engines/sediment/Sediment.h"

#include <algorithm>
#include <cmath>

#include "engines/common/XsHydraulics.h"
#include "engines/steady/SteadySolver.h"

namespace deras {

const char* transportFunctionName(TransportFunction f) {
    switch (f) {
        case TransportFunction::MeyerPeterMuller: return "Meyer-Peter Muller";
        case TransportFunction::EngelundHansen: return "Engelund-Hansen";
        case TransportFunction::Yang: return "Yang";
    }
    return "?";
}

double gradationDiameter(const std::vector<std::pair<double, double>>& curve, double percent) {
    std::vector<std::pair<double, double>> c;
    for (const auto& p : curve)
        if (p.first > 0 && std::isfinite(p.first) && std::isfinite(p.second)) c.push_back(p);
    if (c.empty()) return std::nan("");
    std::sort(c.begin(), c.end());
    if (c.size() == 1) return c[0].first;
    if (percent <= c.front().second) return c.front().first;
    if (percent >= c.back().second) return c.back().first;
    for (size_t i = 1; i < c.size(); ++i)
        if (percent <= c[i].second) {
            const double p0 = c[i - 1].second, p1 = c[i].second;
            if (p1 <= p0) return c[i].first;
            const double t = (percent - p0) / (p1 - p0);
            return std::exp(std::log(c[i - 1].first) + t * (std::log(c[i].first) - std::log(c[i - 1].first)));
        }
    return c.back().first;
}

namespace sediment {

namespace {
constexpr double g = 9.80665;
constexpr double rhoW = 1000.0;
constexpr double ftM = 0.3048;
constexpr double tonUS = 907.18474;   // kg per short ton
}  // namespace

double kinematicViscosity(double tempC) {
    const double t = std::clamp(tempC, 0.0, 40.0);
    return 1.79e-6 / (1.0 + 0.0337 * t + 0.000221 * t * t);
}

double fallVelocity(double d, double s, double nu) {
    if (!(d > 0)) return 0;
    const double dstar = d * std::cbrt((s - 1) * g / (nu * nu));
    return nu / d * (std::sqrt(10.36 * 10.36 + 1.049 * dstar * dstar * dstar) - 10.36);
}

double capacity(TransportFunction f, const Hydraulics& h, double d50, double d90, double s, double nu) {
    if (!(h.Q > 0) || !(h.R > 0) || !(h.S > 0) || !(h.B > 0) || !(d50 > 0)) return 0;
    const double theta = h.R * h.S / ((s - 1) * d50);
    switch (f) {
        case TransportFunction::MeyerPeterMuller: {
            const double nGrain = std::pow(std::max(d90, d50), 1.0 / 6.0) / 26.0;
            const double ratio = std::min(1.0, std::pow(nGrain / std::max(h.n, 1e-4), 1.5));
            const double ex = ratio * theta - 0.047;
            if (ex <= 0) return 0;
            const double qs = 8.0 * std::pow(ex, 1.5) * std::sqrt((s - 1) * g * d50 * d50 * d50);
            return qs * h.B;
        }
        case TransportFunction::EngelundHansen: {
            const double qs = 0.05 * h.V * h.V * std::sqrt(d50 / (g * (s - 1))) * std::pow(theta, 1.5);
            return qs * h.B;
        }
        case TransportFunction::Yang: {
            const double w = fallVelocity(d50, s, nu);
            if (!(w > 0)) return 0;
            const double ustar = std::sqrt(g * h.R * h.S);
            const double re = ustar * d50 / nu;
            const double vcr = re < 70 ? w * (2.5 / (std::log10(std::max(re, 1.2)) - 0.06) + 0.66) : 2.05 * w;
            const double x = (h.V - vcr) * h.S / w;
            if (x <= 0) return 0;
            const double a = std::log10(w * d50 / nu), b = std::log10(ustar / w);
            const double logC = 5.435 - 0.286 * a - 0.457 * b + (1.799 - 0.409 * a - 0.314 * b) * std::log10(x);
            const double ppm = std::pow(10.0, logC);
            return ppm * 1e-6 * rhoW * h.Q / (s * rhoW);
        }
    }
    return 0;
}

SedimentResult run(const Geometry& geomIn, const SedimentSetup& st, UnitSystem units, const Callbacks& cb) {
    SedimentResult res;
    res.units = units;
    res.function = transportFunctionName(st.data.function);
    const bool us = units == UnitSystem::USCustomary;
    const double L = us ? ftM : 1.0;                 // project length -> m
    const double Qf = us ? ftM * ftM * ftM : 1.0;    // project flow -> m3/s
    const double massUnit = us ? tonUS : 1000.0;     // kg per reported ton
    auto fail = [&](const char* code, std::vector<std::string> args = {}) {
        res.messages.push_back(makeMsg(Severity::Error, code, std::move(args)));
        res.status = RunStatus::Failed;
        return res;
    };
    Geometry geom = geomIn;
    const Reach* reach0 = geom.findReach(st.flow.river, st.flow.reach);
    if (!reach0 && geom.reaches.size() == 1) reach0 = &geom.reaches[0];
    if (!reach0) return fail("SED_NO_REACH", {st.flow.river + " / " + st.flow.reach});
    if (geom.reaches.size() != 1) res.messages.push_back(makeMsg(Severity::Warning, "SED_ONE_REACH", {reach0->label()}));
    if (st.flow.records.empty()) return fail("SED_NO_FLOW");
    const double d50mm = gradationDiameter(st.data.gradation, 50), d90mm = gradationDiameter(st.data.gradation, 90);
    if (!(d50mm > 0)) return fail("SED_NO_GRADATION");
    res.d50 = d50mm;
    res.d90 = d90mm;
    const double s = st.data.specificGravity, p = st.data.porosity;
    if (!(s > 1.01) || !(p >= 0 && p < 0.9)) return fail("SED_BAD_PROPERTIES");
    if (st.data.function == TransportFunction::Yang && d50mm > 2.0) res.messages.push_back(makeMsg(Severity::Warning, "SED_YANG_GRAVEL", {fmt(d50mm, 2)}));
    const double nu = kinematicViscosity(st.data.temperature);
    const double d50 = d50mm / 1000.0, d90 = (d90mm > 0 ? d90mm : d50mm) / 1000.0;
    const double rhoS = s * rhoW;

    // the reach being computed: work on a geometry that holds only it
    Reach* reach = nullptr;
    {
        Geometry one;
        one.title = geom.title;
        one.reaches.push_back(*reach0);
        geom = std::move(one);
        reach = &geom.reaches[0];
    }
    res.river = reach->river;
    res.reach = reach->reach;
    std::vector<size_t> xi;   // node indices of the cross sections
    for (size_t k = 0; k < reach->nodes.size(); ++k)
        if (reach->nodes[k].type == NodeType::CrossSection) xi.push_back(k);
    const size_t N = xi.size();
    if (N < 2) return fail("SED_NO_REACH", {reach->label()});

    // movable bed: points strictly between the bank stations
    struct Bed { std::vector<size_t> pts; double bEff = 0; double len = 0; double shift = 0; };
    std::vector<Bed> bed(N);
    std::vector<double> lenDn(N, 0);
    for (size_t i = 0; i < N; ++i) {
        const CrossSection& xs = reach->nodes[xi[i]];
        lenDn[i] = i + 1 < N ? std::max(xs.lenChannel, 0.0) : 0.0;
        const double bl = xs.bankLeft.value_or(xs.staElev.empty() ? 0 : xs.staElev.front().station);
        const double br = xs.bankRight.value_or(xs.staElev.empty() ? 0 : xs.staElev.back().station);
        const auto& se = xs.staElev;
        for (size_t k = 1; k + 1 < se.size(); ++k)
            if (se[k].station > bl && se[k].station < br) bed[i].pts.push_back(k);
        if (bed[i].pts.empty()) return fail("SED_NO_MOVABLE_BED", {xs.rs});
        const size_t a = bed[i].pts.front(), b = bed[i].pts.back();
        bed[i].bEff = (se[b].station - se[a].station) + 0.5 * (se[a].station - se[a - 1].station) + 0.5 * (se[b + 1].station - se[b].station);
    }
    for (size_t i = 0; i < N; ++i) bed[i].len = 0.5 * (i > 0 ? lenDn[i - 1] : 0.0) + 0.5 * lenDn[i];
    double dist = 0;
    res.distance.assign(N, 0);
    for (size_t i = N; i-- > 0;) {
        res.distance[i] = dist;
        if (i > 0) dist += lenDn[i - 1];
    }
    for (size_t i = 0; i < N; ++i) {
        res.rs.push_back(reach->nodes[xi[i]].rs);
        res.invert0.push_back(reach->nodes[xi[i]].channelInvert());
    }

    Plan plan;
    plan.title = "sediment";
    plan.regime = FlowRegime::Subcritical;
    SteadyFlow flow;
    flow.profileNames = {"Q"};
    FlowChange fc;
    fc.river = reach->river;
    fc.reach = reach->reach;
    fc.rs = reach->nodes[xi[0]].rs;
    fc.flows = {0};
    flow.changes.push_back(fc);

    // steps
    struct Step { double dtH; size_t rec; };
    std::vector<Step> steps;
    double total = 0;
    for (size_t r = 0; r < st.flow.records.size(); ++r) {
        const auto& rc = st.flow.records[r];
        if (!(rc.durationHours > 0) || !(rc.flow > 0)) return fail("SED_BAD_RECORD", {std::to_string(r + 1)});
        const double ch = rc.computeHours > 0 ? std::min(rc.computeHours, rc.durationHours) : rc.durationHours;
        const int n = std::max(1, int(std::ceil(rc.durationHours / ch - 1e-9)));
        for (int k = 0; k < n; ++k) steps.push_back({rc.durationHours / n, r});
        total += rc.durationHours;
    }
    if (steps.size() > 200000) return fail("SED_TOO_MANY_STEPS", {std::to_string(steps.size())});
    const double outEvery = st.outputHours > 0 ? st.outputHours : total;

    std::vector<double> lastWs(N, 0), lastLoad(N, 0);
    auto record = [&](double tH, double q) {
        res.hours.push_back(tH);
        res.flow.push_back(q);
        std::vector<double> inv(N), ch(N);
        for (size_t i = 0; i < N; ++i) {
            inv[i] = reach->nodes[xi[i]].channelInvert();
            ch[i] = bed[i].shift;
        }
        res.invert.push_back(inv);
        res.change.push_back(ch);
        res.ws.push_back(lastWs);
        res.load.push_back(lastLoad);
    };

    // hydraulics of the current bed for one flow record
    std::vector<Hydraulics> hy(N);
    auto hydraulics = [&](const QuasiRecord& rc) -> bool {
        flow.changes[0].flows[0] = rc.flow;
        auto& bc = flow.boundaryRef(reach->river, reach->reach, 0);
        if (std::isfinite(rc.dsStage)) {
            bc.downstream.type = BoundaryType::KnownWS;
            bc.downstream.knownWS = rc.dsStage;
        } else {
            bc.downstream.type = BoundaryType::NormalDepth;
            bc.downstream.slope = st.flow.dsSlope > 0 ? st.flow.dsSlope : 0.001;
        }
        const RunResult hr = steady::solve(geom, flow, plan, units, {});
        if (hr.status != RunStatus::Succeeded || hr.profiles.empty() || hr.profiles[0].xs.size() < N) {
            for (const auto& m : hr.messages) if (m.severity == Severity::Error) res.messages.push_back(m);
            return false;
        }
        for (size_t i = 0; i < N; ++i) {
            const CrossSection& xs = reach->nodes[xi[i]];
            const XsResult* xr = nullptr;
            for (const auto& x : hr.profiles[0].xs) if (x.rs == xs.rs) { xr = &x; break; }
            if (!xr) return false;
            const hyd::XsGeom gx = hyd::XsGeom::from(xs, units);
            const hyd::XsProps pr = hyd::properties(gx, xr->ws);
            Hydraulics& h = hy[i];
            const double aCh = pr.ch.area * L * L, pCh = pr.ch.perimeter * L;
            h.Q = std::max(xr->qCh, 0.0) * Qf;
            h.V = aCh > 0 ? h.Q / aCh : 0;
            h.R = pCh > 0 ? aCh / pCh : 0;
            h.S = std::max(xr->egSlope, 0.0);
            h.B = pr.ch.topWidth * L;
            const double k = us ? 1.486 : 1.0;
            const double Rp = pr.ch.perimeter > 0 ? pr.ch.area / pr.ch.perimeter : 0;
            h.n = pr.ch.conveyance > 0 ? k * pr.ch.area * std::pow(Rp, 2.0 / 3.0) / pr.ch.conveyance : 0.035;
            lastWs[i] = xr->ws;
        }
        return true;
    };
    // sediment inflow at the upstream section, m3/s of solids
    auto inflow = [&](const QuasiRecord& rc) {
        const double cap0 = capacity(st.data.function, hy[0], d50, d90, s, nu);
        switch (st.data.inflow) {
            case SedimentInflow::Equilibrium: return cap0;
            case SedimentInflow::ClearWater: return 0.0;
            default: break;
        }
        const auto& rt = st.data.rating;
        double tpd = 0;
        if (!rt.empty()) {
            if (rc.flow <= rt.front().first) tpd = rt.front().second * rc.flow / std::max(rt.front().first, 1e-12);
            else if (rc.flow >= rt.back().first) tpd = rt.back().second;
            else
                for (size_t j = 1; j < rt.size(); ++j)
                    if (rc.flow <= rt[j].first) {
                        const double t = (rc.flow - rt[j - 1].first) / std::max(rt[j].first - rt[j - 1].first, 1e-12);
                        tpd = rt[j - 1].second + t * (rt[j].second - rt[j - 1].second);
                        break;
                    }
        }
        return std::max(tpd, 0.0) * massUnit / 86400.0 / rhoS;
    };
    // Exner over all sections for a time step dt (s): bed changes dz (project
    // length) and the outflow of every section (m3/s of solids)
    std::vector<double> dzv(N), outv(N);
    auto exner = [&](double qin, double dt) {
        double in = qin;
        for (size_t i = 0; i < N; ++i) {
            const double cap = capacity(st.data.function, hy[i], d50, d90, s, nu);
            const double area = bed[i].bEff * L * bed[i].len * L;   // plan area of the control volume, m2
            double out = cap;
            if (area <= 0) out = in;
            else if (cap > in) {
                const double avail = std::max(bed[i].shift + st.data.maxErodibleDepth, 0.0) * L * area * (1 - p);  // solids, m3
                out = in + std::min(cap - in, avail / dt);
            }
            dzv[i] = area > 0 ? (in - out) * dt / ((1 - p) * area) / L : 0.0;
            outv[i] = out;
            in = out;
        }
    };

    double tH = 0, nextOut = 0;
    long subSteps = 0, wide = 0;
    if (!hydraulics(st.flow.records[0])) return fail("SED_HYDRAULICS_FAILED", {fmt(0.0, 2)});
    exner(inflow(st.flow.records[0]), 3600.0);
    for (size_t i = 0; i < N; ++i) lastLoad[i] = outv[i] * rhoS / massUnit * 86400.0;
    record(0, st.flow.records[0].flow);
    for (size_t k = 0; k < steps.size(); ++k) {
        const auto& rc = st.flow.records[steps[k].rec];
        double remaining = steps[k].dtH * 3600.0;
        int guard = 0;
        // Bed sub-steps: the hydraulics are recomputed whenever the bed would
        // change by more than 5 % of the water depth in the remaining time.
        while (remaining > 1e-6) {
            if (cb.cancelled && cb.cancelled()) {
                res.status = RunStatus::Cancelled;
                return res;
            }
            if (!hydraulics(rc)) return fail("SED_HYDRAULICS_FAILED", {fmt(tH, 2)});
            const double qin = inflow(rc);
            double dt = remaining;
            exner(qin, dt);
            double ratio = 0;
            for (size_t i = 0; i < N; ++i) {
                const double depth = std::max(lastWs[i] - reach->nodes[xi[i]].channelInvert(), 0.01);
                ratio = std::max(ratio, std::fabs(dzv[i]) / (0.05 * depth));
            }
            if (ratio > 1 && guard < 200) {
                dt = remaining / std::min(std::ceil(ratio), 50.0);
                exner(qin, dt);
                ++subSteps;
            } else if (ratio > 1) {
                ++wide;
            }
            ++guard;
            for (size_t i = 0; i < N; ++i) {
                CrossSection& xs = reach->nodes[xi[i]];
                double dz = dzv[i];
                if (bed[i].shift + dz < -st.data.maxErodibleDepth) dz = -st.data.maxErodibleDepth - bed[i].shift;
                for (size_t q : bed[i].pts) xs.staElev[q].elev += dz;
                bed[i].shift += dz;
                res.massChange += dz * L * (1 - p) * bed[i].bEff * L * bed[i].len * L * rhoS / massUnit;
                lastLoad[i] = outv[i] * rhoS / massUnit * 86400.0;
            }
            res.massIn += qin * dt * rhoS / massUnit;
            res.massOut += outv[N - 1] * dt * rhoS / massUnit;
            remaining -= dt;
            tH += dt / 3600.0;
        }
        if (tH + 1e-9 >= nextOut + outEvery || k + 1 == steps.size()) {
            record(tH, rc.flow);
            nextOut = tH;
        }
        if (cb.progress) cb.progress(int(k + 1), int(steps.size()));
    }
    if (subSteps > 0) res.messages.push_back(makeMsg(Severity::Info, "SED_SUBSTEPS", {std::to_string(subSteps)}));
    if (wide > 0) res.messages.push_back(makeMsg(Severity::Warning, "SED_STEP_LARGE", {std::to_string(wide)}));
    res.messages.push_back(makeMsg(Severity::Info, "SED_DONE",
                                   {res.function, fmt(d50mm, 3), std::to_string(steps.size()), fmt(total, 1), fmt(res.massIn, 1), fmt(res.massOut, 1),
                                    fmt(res.massChange, 1)}));
    res.status = RunStatus::Succeeded;
    return res;
}

}  // namespace sediment
}  // namespace deras

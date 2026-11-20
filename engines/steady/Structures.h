// De-RAS - engines/steady/Structures.h
// Hydraulics of bridges, culverts and inline structures for the steady
// solver (M4, Preview). Formulation after the public HEC-RAS Hydraulic
// Reference Manual (ch. 5–8) and FHWA HDS-5 (culvert inlet control),
// implemented independently:
//  - weir flow Q = C L H^1.5 over a crest profile, Villemonte submergence
//    reduction (1 - (Hd/Hu)^1.5)^0.385;
//  - bridge pressure flow: sluice gate Q = Cd A sqrt(2g (EG - Zbed - Z/2))
//    with Cd from Y3/Z, or orifice Q = C A sqrt(2g (EG - WSdn)) when the
//    downstream low chord is submerged;
//  - Yarnell pier loss H3 = 2K (K + 10w - 0.6)(a + 15a^4) V2^2/2g;
//  - culverts: FHWA inlet control (form 1, unsubmerged / submerged with the
//    3.5..4.0 transition) and outlet control with full-flow losses
//    (Ke + friction + Kx) hv above max(TW, (dc + D)/2);
//  - gates: sluice Q = C W B sqrt(2g H), orifice when the tailwater ratio
//    exceeds 0.67, weir over the sill when the opening is not reached.
#pragma once

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/Model.h"
#include "core/Units.h"

namespace deras::steady::st {

double defaultWeirCoef(UnitSystem u);          // 2.6 (US) / 1.44 (SI), broad-crested road or deck
double villemonte(double hu, double hd);       // submergence factor for weir flow (1 = free)

// Weir flow over a crest profile for upstream energy eg and tailwater tw.
// Stations listed in `blocked` (pairs a..b) carry no weir flow (gate openings).
double weirFlow(const std::vector<std::pair<double, double>>& crest, double eg, double tw, double coef,
                const std::vector<std::pair<double, double>>& blocked = {});

// Bridge opening under the low chord at the upstream face: net area below the
// low chord (piers removed), mean bed elevation and the maximum low chord.
struct Opening { double area = 0, meanBed = 0, maxLowChord = 0, minLowChord = 0; };
Opening bridgeOpening(const CrossSection& xs, const std::vector<DeckPoint>& deck, const std::vector<Pier>& piers, bool upstreamFace);

double sluiceCd(double y3OverZ);  // pressure-flow coefficient (FHWA chart, tabulated)

// FHWA inlet control coefficients for a chart/scale (form 1). Returns false
// for charts not in the De-RAS table; the closest shape's values are used.
struct InletCoef { double K, M, c, Y; bool mitered; };
bool inletCoefficients(int shape, int chart, int scale, InletCoef& out);

// Barrel geometry at depth y (circular uses rise as diameter).
void barrelSection(const CulvertGroup& c, double y, double& area, double& top, double& perim);
double barrelCriticalDepth(const CulvertGroup& c, double q, double g);

struct CulvertState {
    double egUp = 0;        // upstream energy elevation needed for the barrel flow
    double hwInlet = 0, hwOutlet = 0;  // inlet / outlet control energy elevations
    bool inletControls = true;
};
// Upstream energy for the flow q through ONE barrel, downstream water surface
// twWs and downstream velocity head hv2.
CulvertState culvertEnergy(const CulvertGroup& c, double q, double twWs, double hv2, UnitSystem u, double g);
// Flow through one barrel for upstream energy egUp (inverse of culvertEnergy).
double culvertFlow(const CulvertGroup& c, double egUp, double twWs, double hv2, UnitSystem u, double g);

// Flow through one gate opening (opening height b) for upstream energy egUp and tailwater twWs.
double gateFlow(const GateGroup& gt, double b, double egUp, double twWs, double weirCoef, double g);

// Bisection: x in [lo, hi] with f(x) = 0 for an increasing f; NaN when not bracketed.
template <class F> double bisect(F f, double lo, double hi, double tol = 1e-6, int maxIt = 200) {
    double flo = f(lo), fhi = f(hi);
    if (!(flo <= 0 && fhi >= 0)) return std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < maxIt && hi - lo > tol; ++i) {
        const double m = 0.5 * (lo + hi), fm = f(m);
        if (fm < 0) lo = m; else hi = m;
    }
    return 0.5 * (lo + hi);
}

}  // namespace deras::steady::st

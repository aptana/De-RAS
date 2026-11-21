// De-RAS - engines/design/Design.h
// Hydraulic design functions and other M8 tools (Preview). All functions use
// the project unit system unless stated otherwise.
//
// Uniform flow: Manning's equation in a trapezoidal channel
//   Q = (k/n) A R^(2/3) S^(1/2), k = 1 (SI) or 1.486 (US).
// Scour (FHWA HEC-18, 5th ed., 2012, public document):
//   - pier scour, CSU equation: ys/a = 2.0 K1 K2 K3 (y1/a)^0.35 Fr1^0.43,
//     K2 = (cos t + L/a sin t)^0.65, limited to 2.4 a (Fr <= 0.8) or 3.0 a;
//   - live-bed contraction scour (modified Laursen 1960):
//     y2/y1 = (Q2/Q1)^(6/7) (W1/W2)^k1, k1 from V*/w (0.59, 0.64, 0.69);
//   - clear-water contraction scour (Laursen 1963):
//     y2 = [Ku Q^2 / (Dm^(2/3) W^2)]^(3/7), Dm = 1.25 D50, Ku = 0.025 SI, 0.0077 US;
//   - abutment scour, Froehlich (1989) with the +1 ya safety term:
//     ys/ya = 2.27 K1 K2 (L'/ya)^0.43 Fr^0.61 + 1.
// Manning's n calibration: one factor applied to Manning's n (main channel
// or all), found by golden-section search to minimise the RMS difference
// between computed and observed water surfaces of one steady profile.
// Water quality: 1D advection-dispersion with first-order decay on the
// hydraulics of an unsteady run, implicit (upwind advection, central
// dispersion), conservative in the absence of decay.
// Unit conversion: hydraulic data of geometry, flow and plan files between SI
// and US Customary (map coordinates are kept).
#pragma once

#include <string>
#include <vector>

#include "core/Model.h"
#include "core/Results.h"

namespace deras::design {

// ---- uniform flow ----
struct Trapezoid {
    double bottom = 10, sideSlope = 2;   // bottom width, horizontal : vertical (both sides)
    double n = 0.03, slope = 0.001;
};
struct UniformResult {
    bool ok = false;
    double depth = 0, flow = 0, area = 0, perimeter = 0, radius = 0, topWidth = 0;
    double velocity = 0, froude = 0, criticalDepth = 0, shear = 0;   // shear: rho g R S (Pa or lb/ft2)
};
UniformResult uniformFromDepth(const Trapezoid& c, double depth, UnitSystem u);
UniformResult uniformFromFlow(const Trapezoid& c, double flow, UnitSystem u);   // normal depth

// ---- scour ----
enum class PierNose { Square = 0, Round = 1, Circular = 2, GroupOfCylinders = 3, Sharp = 4 };
double pierShapeK1(PierNose n);
struct PierScourInput {
    double a = 1;           // pier width
    double L = 5;           // pier length
    double angle = 0;       // attack angle, degrees
    PierNose nose = PierNose::Round;
    double K3 = 1.1;        // bed condition
    double y1 = 2, V1 = 1;  // flow depth and velocity directly upstream of the pier
};
struct PierScourResult { double K1 = 1, K2 = 1, froude = 0, scour = 0, limit = 0; bool limited = false; };
PierScourResult pierScour(const PierScourInput& in, UnitSystem u);

struct ContractionInput {
    bool liveBed = true;
    double y1 = 2;          // average depth in the upstream main channel
    double y0 = 2;          // existing depth in the contracted section before scour
    double Q1 = 100, Q2 = 100;  // flow in the upstream channel / in the contracted channel
    double W1 = 40, W2 = 30;    // bottom widths (upstream / contracted)
    double S1 = 0.001;      // energy slope of the upstream channel (live bed: shear velocity)
    double D50mm = 1;       // median bed grain size, mm
};
struct ContractionResult { double y2 = 0, scour = 0, k1 = 0, ustarOverW = 0; };
ContractionResult contractionScour(const ContractionInput& in, UnitSystem u);

struct AbutmentInput {
    double Lprime = 30;     // length of the active flow obstructed by the embankment
    double ya = 2;          // average depth of flow on the floodplain
    double Ve = 0.5;        // velocity of the obstructed flow (Qe / Ae)
    double K1 = 1.0;        // shape: vertical wall 1.0, wing wall 0.82, spill-through 0.55
    double angle = 90;      // embankment angle to the flow, degrees (K2 = (angle/90)^0.13)
};
struct AbutmentResult { double K2 = 1, froude = 0, scour = 0; };
AbutmentResult abutmentScour(const AbutmentInput& in, UnitSystem u);

// ---- Manning's n calibration ----
struct Observation { std::string river, reach, rs; double ws = 0; };
struct CalibrationResult {
    bool ok = false;
    std::string error;
    double factor = 1, rmsBefore = 0, rmsAfter = 0;
    int runs = 0;
    std::vector<double> computedBefore, computedAfter;   // per observation
};
// scope: 0 main channel n only, 1 all n values
CalibrationResult calibrateN(const Geometry& g, const SteadyFlow& f, const Plan& p, UnitSystem u, int profile,
                             const std::vector<Observation>& obs, int scope, double lo = 0.5, double hi = 2.0);
// Multiplies Manning's n of every cross section (scope as above).
void scaleManning(Geometry& g, double factor, int scope);

// ---- water quality ----
struct WqInput {
    double initial = 0;                                   // initial concentration (mg/L)
    std::vector<std::pair<double, double>> upstream;      // (hours, concentration) at the upstream end
    double dispersion = 10;                               // m2/s (ft2/s US)
    double decayPerDay = 0;                               // first-order decay rate, 1/day
};
struct WqResult {
    RunStatus status = RunStatus::NotRun;
    std::string error, river, reach;
    std::vector<std::string> rs;
    std::vector<double> distance, hours;
    std::vector<std::vector<double>> conc;   // [time][xs]
    double massIn = 0, massOut = 0, massStored0 = 0, massStored = 0, massDecayed = 0;   // g (per unit: concentration x volume)
    // Mass added (+) or removed (-) because the stored hydraulics do not close
    // continuity exactly between output times (see waterQuality). Balance:
    // stored - stored0 = in - out - decayed + correction.
    double massCorrection = 0;
};
// Uses the stage and flow time series of the first reach of an unsteady result.
WqResult waterQuality(const Geometry& g, const RunResult& r, const WqInput& in, UnitSystem u);

// ---- unit conversion ----
// Converts in place; toSI = true: US Customary -> SI.
void convertGeometry(Geometry& g, bool toSI);
void convertSteadyFlow(SteadyFlow& f, bool toSI);
void convertUnsteadyFlow(UnsteadyFlow& f, bool toSI);
void convertPlan(Plan& p, bool toSI);

}  // namespace deras::design

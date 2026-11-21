// De-RAS - engines/sediment/Sediment.h
// Quasi-unsteady sediment transport (M7, Preview).
//
// The flow series is a sequence of steady flows. At every computation step
// the steady standard-step solver gives the water surface at every cross
// section; the transport capacity of the main channel follows from one of
// three published transport functions, and the bed of the main channel is
// raised or lowered with the Exner equation (sediment continuity):
//   (1 - p) B L dz/dt = Qs,in - Qs,out
// over the control volume of each cross section (half the reach lengths
// upstream and downstream). Erosion is limited by the erodible depth below
// the initial invert. One grain size (d50 of the bed gradation) represents
// the bed; there is no bed sorting or armouring.
//
// Transport functions (SI internally; volume rate of solids):
//  - Meyer-Peter and Mueller (1948): qs* = 8 (theta' - 0.047)^1.5, theta' =
//    (n'/n)^1.5 theta, n' = d90^(1/6) / 26.
//  - Engelund and Hansen (1967): qs = 0.05 V^2 sqrt(d / (g (s-1))) theta^1.5.
//  - Yang (1973), sand: log Ct (ppm by weight) from the unit stream power VS.
#pragma once

#include <functional>

#include "core/SedimentModel.h"

namespace deras::sediment {

double kinematicViscosity(double tempC);                       // m2/s
double fallVelocity(double dMetres, double s, double nu);      // m/s (Soulsby 1997)

struct Hydraulics {   // main channel, SI
    double Q = 0, V = 0, R = 0, S = 0, B = 0, n = 0.035;
};
// Transport capacity of the main channel, m3/s of solids.
double capacity(TransportFunction f, const Hydraulics& h, double d50, double d90, double s, double nu);

struct Callbacks {
    std::function<bool()> cancelled;
    std::function<void(int done, int total)> progress;
};

SedimentResult run(const Geometry& geom, const SedimentSetup& setup, UnitSystem units, const Callbacks& cb = {});

}  // namespace deras::sediment

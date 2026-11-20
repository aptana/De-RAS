// De-RAS - engines/steady/SteadySolver.h
// 1D steady, gradually varied flow by the standard step method
// (subcritical regime). Formulation follows the public HEC-RAS Hydraulic
// Reference Manual, chapter 2 (energy equation, conveyance, losses,
// iteration procedure) and is implemented independently.
#pragma once

#include <functional>

#include "core/Model.h"
#include "core/Results.h"
#include "engines/common/XsHydraulics.h"

namespace deras::steady {

struct Callbacks {
    std::function<bool()> cancelled;                   // polled between cross sections
    std::function<void(int done, int total)> progress; // number of XS computed so far
};

// Runs every profile on every reach. `geom` and `flow` must have passed the
// Model Doctor; the solver still guards against invalid numbers and reports
// them as errors instead of producing results.
RunResult solve(const Geometry& geom, const SteadyFlow& flow, const Plan& plan, UnitSystem units, const Callbacks& cb);

// Flow for a cross section of a reach (nearest flow-change location at or
// upstream of it in reach order). Returns NaN when none applies.
double flowAt(const SteadyFlow& flow, const Reach& reach, size_t xsIndexInReach, int profile,
              const std::vector<const CrossSection*>& xsList);

// Fills the hydraulic columns of a result row (areas, flow split, velocities,
// energy, Froude) for water surface ws and discharge q.
void fillXsResult(XsResult& r, const hyd::XsGeom& g, double ws, double q, double grav);

}  // namespace deras::steady

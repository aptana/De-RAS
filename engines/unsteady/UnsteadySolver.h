// De-RAS - engines/unsteady/UnsteadySolver.h
// 1D unsteady flow (M5, Preview): Saint-Venant equations solved with the
// four-point implicit (Preissmann) box scheme and Newton iterations, as
// described in the public HEC-RAS Hydraulic Reference Manual (ch. 2,
// "Unsteady Flow Hydraulics"); implemented independently.
//
// Simplifications of this version (stated in the help and the CHANGELOG):
//  - one flow path per reach (main-channel length, total conveyance and the
//    velocity coefficient alpha used as momentum coefficient); the HEC-RAS
//    channel/floodplain split is not applied;
//  - ineffective areas give no storage;
//  - junctions force equal water surfaces and flow continuity;
//  - bridges, culverts and inline structures are not computed (Model
//    Doctor stops such runs).
// M6 (Preview): storage areas (level pool, implicit in their own stage),
// lateral structures (weir along the reach to a storage area, a 2D flow
// area or out of the system, Villemonte submergence, optional breach),
// storage-area connections, pump stations and 2D flow areas (see
// Flow2D.h). Exchange flows are implicit in the reach stages and in the
// receiving area's own stage, and iterated with the other unknowns inside
// each time step.
// Each reach is reduced to two linear relations between its end stages
// (solved with a banded LU); the end stages of all reaches are then solved
// together with the boundary and junction equations.
#pragma once

#include <functional>

#include "core/Model.h"
#include "core/Results.h"
#include "io/GeoTiff.h"

namespace deras::unsteady {

struct Callbacks {
    std::function<bool()> cancelled;
    std::function<void(int done, int total)> progress;  // time steps
};

struct Inputs {
    const std::vector<io::Raster>* terrains = nullptr;  // needed for 2D flow areas (and storage areas without a volume table)
};

RunResult solve(const Geometry& geom, const UnsteadyFlow& flow, const Plan& plan, UnitSystem units, const Callbacks& cb,
                const Inputs& in = {});

// Weir flow from side u to side d (negative when d is higher). zd NaN: free outfall.
// Villemonte submergence for a tailwater above the crest.
double weirFlow(double zu, double zd, double crest, double coef, double length);

// Value of a hydrograph at tHours after the simulation start (linear
// interpolation, last value held after the end). NaN when it has no data.
double seriesValue(const UnsteadyBoundary& b, double tHours);
// Duration of the hydrograph in hours (0 when empty).
double seriesHours(const UnsteadyBoundary& b);

// Boundary of a reach end (upstream = true: first section). Null if none.
const UnsteadyBoundary* endBoundary(const UnsteadyFlow& f, const Reach& r, bool upstream);

}  // namespace deras::unsteady

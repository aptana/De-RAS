// De-RAS - engines/common/XsHydraulics.h
// Hydraulic properties of a 1D cross section at a given water surface:
// flow area, wetted perimeter, top width and conveyance per flow element
// (left overbank, main channel, right overbank), velocity weighting
// coefficient alpha, critical and normal water surface.
//
// Method (HEC-RAS Hydraulic Reference Manual, ch. 2, public document):
//  - Conveyance K = (k/n) A R^(2/3), k = 1.486 (US) or 1.0 (SI).
//  - Overbanks are subdivided at every Manning's n breakpoint; the
//    conveyances of the pieces are summed.
//  - The main channel is one element. When it has more than one n value a
//    composite n is used: nc = [sum(P_i n_i^1.5) / P]^(2/3).
//  - alpha = A_t^2 * sum(K_i^3 / A_i^2) / K_t^3 over the three elements.
//  - If the water surface is above an end point, vertical walls are
//    extended at that end (their wetted perimeter is included).
//  - Blocked obstructions raise the ground to the obstruction elevation
//    inside their station range (area removed, perimeter added). A levee
//    higher than the ground is added as a ground point.
//  - Ineffective flow areas carry no area, perimeter or conveyance while
//    the water surface is at or below their elevation (always, when
//    permanent). Water behind a levee is ignored until the water surface
//    is above the levee elevation.
#pragma once

#include <vector>

#include "core/Model.h"
#include "core/Units.h"

namespace deras::hyd {

struct Element {
    double area = 0, perimeter = 0, topWidth = 0, conveyance = 0;
};

struct XsProps {
    double ws = 0;
    Element lob, ch, rob;
    double area = 0, perimeter = 0, topWidth = 0, conveyance = 0;
    double alpha = 1.0;
    bool wallsExtended = false;
    double hydraulicRadius() const { return perimeter > 0 ? area / perimeter : 0; }
};

// Prepared cross-section geometry (validated, banks clamped to data range).
struct XsGeom {
    std::vector<StaElev> pts;
    std::vector<ManningBreak> n;
    double bankL = 0, bankR = 0;
    double minEl = 0, invertCh = 0;
    double leftTop = 0, rightTop = 0;  // end-point elevations
    UnitSystem units = UnitSystem::USCustomary;
    std::vector<StationBlock> ineffective;
    LeveePoint leveeLeft, leveeRight;

    // Station intervals that carry flow at water surface ws (sorted, disjoint).
    std::vector<std::pair<double, double>> activeIntervals(double ws) const;
    bool hasExclusions() const { return !ineffective.empty() || leveeLeft.present || leveeRight.present; }

    static XsGeom from(const CrossSection& xs, UnitSystem units);
    // Manning n at a station (value of the last breakpoint <= station).
    double nAt(double station) const;
};

XsProps properties(const XsGeom& g, double ws);

// Wetted area and perimeter of the ground polyline between stations a and b.
void wettedSegment(const std::vector<StaElev>& pts, double a, double b, double ws, double& area, double& perim,
                   double& top);

// Specific force (momentum function) Q^2 / (g A) + A * ybar over the
// active flow area, used by the mixed-regime solver.
double specificForce(const XsGeom& g, double ws, double q);

// Total energy H = WS + alpha V^2 / 2g for discharge q.
double totalEnergy(const XsGeom& g, double ws, double q);

// Critical water surface: minimum specific energy for discharge q
// (search over the full section, refined to `tol`). Returns NaN on failure.
double criticalWS(const XsGeom& g, double q, double tol);

// Water surface where K(ws) * sqrt(slope) = q (Manning normal depth).
double normalWS(const XsGeom& g, double q, double slope, double tol);

}  // namespace deras::hyd

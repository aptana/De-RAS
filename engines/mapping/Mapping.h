// De-RAS - engines/mapping/Mapping.h
// RAS Mapper computations without GIS libraries: cross-section cut lines,
// terrain sampling along a line, and steady inundation maps.
//
// Inundation (Preview): between two neighbouring cut lines of a reach the
// water surface is interpolated linearly by the distances to both lines
// (the HEC-RAS approach of a water surface between cross sections, without
// its TIN and connectivity checks). Depth = WSE - terrain where positive.
#pragma once

#include <string>
#include <vector>

#include "core/Model.h"
#include "core/Results.h"
#include "io/GeoTiff.h"

namespace deras::mapping {

struct Pt { double x = 0, y = 0; };
using Line = std::vector<Pt>;

double length(const Line& l);
Pt pointAt(const Line& l, double s, Pt* dir = nullptr);         // point at distance s along l
// Projection of p onto l: distance along l and signed offset (positive = right of the direction of l).
void project(const Line& l, Pt p, double& s, double& offset);
double distanceTo(const Line& l, Pt p);
bool intersect(const Line& a, const Line& b, double& sa, double& sb);  // first crossing, distances along a and b
bool insidePolygon(const Line& poly, Pt p);

Line centreline(const Reach& r);  // r.xy as points (upstream -> downstream)

struct CutLine {
    int nodeIndex = -1;       // index in Reach::nodes
    Line line;                // left -> right looking downstream
    double s = 0;             // where it crosses the centreline (distance from the upstream end)
    bool fromFile = false;    // true: XS GIS Cut Line; false: placed across the centreline
};
// Cut lines of every cross section of the reach. Sections without a cut line
// in the file are placed across the centreline, spaced by the main-channel
// lengths (scaled to the centreline length) and centred on the main channel.
std::vector<CutLine> cutLines(const Reach& r);
// Map point of a station of a cross section along its cut line (proportional).
Pt stationPoint(const CrossSection& xs, const Line& cut, double station);

// First non-NaN terrain value at (x, y).
double terrainAt(const std::vector<io::Raster>& terrains, double x, double y);
// Station-elevation points along a line every `step` (map units), thinned to
// at most `maxPoints` with the Douglas-Peucker rule. Empty when the line
// leaves the terrain.
std::vector<StaElev> sampleLine(const std::vector<io::Raster>& terrains, const Line& line, double step, size_t maxPoints = 500,
                                int* missing = nullptr);

struct MapResult {
    io::Raster depth, wse;
    long wetCells = 0;
    double maxDepth = 0;
    double wetArea = 0;       // map units squared
    int reachesMapped = 0;
};
// Inundation of one profile. The grid follows the first terrain (cell size,
// alignment, CRS) over the extent of all cut lines.
MapResult inundation(const Geometry& g, const ProfileResult& profile, const std::vector<io::Raster>& terrains,
                     std::string& error, long maxCells = 40'000'000);

// 2D flow area result maps (M6, Preview). variable: 0 max depth, 1 depth at
// output time `timeIndex`, 2 max velocity, 3 arrival time (hours).
// Depths use the terrain below each cell (subgrid): depth = cell water
// surface - terrain, on the terrain grid. Velocity and arrival time are
// one value per computation cell. Values go to `depth`; `wse` holds the
// water surface for the depth variables.
MapResult grid2dMap(const Grid2DResult& g, int variable, int timeIndex, const std::vector<io::Raster>& terrains, std::string& error,
                    long maxCells = 40'000'000);

}  // namespace deras::mapping

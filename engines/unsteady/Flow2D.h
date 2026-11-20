// De-RAS - engines/unsteady/Flow2D.h
// 2D flow area (M6, Preview): diffusion-wave equations on a regular
// Cartesian grid inside the area polygon, with subgrid terrain tables.
//
// Every cell keeps its terrain samples, so the stored volume V(z) and wet
// area A(z) follow the terrain inside the cell; every face keeps the terrain
// profile along it, so its conveyance K(z) follows the terrain across the
// face (the idea of the HEC-RAS subgrid approach, implemented independently
// on a square grid instead of the HEC-RAS polygon mesh).
//
// Time stepping is fully implicit: V(z) - V(z_old) = dt (sum of face flows
// + sources), face flow Q = K(z_up) sqrt(|dz| / L) towards the lower cell,
// linearised around the current iterate (Picard) and solved with the
// conjugate gradient method (Jacobi preconditioner).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/Model.h"
#include "core/Results.h"
#include "io/GeoTiff.h"

namespace deras::flow2d {

struct Cell {
    int c = 0, r = 0;            // grid column / row (row 0 at the top)
    double x = 0, y = 0;         // centre
    double zMin = 0;
    std::vector<double> se;      // terrain samples, sorted
    std::vector<double> pre;     // prefix sums of se
    double sampleArea = 0;       // plan area of one sample
    double volume(double z) const;
    double wetArea(double z) const;
    double level(double v) const;   // water surface holding volume v
};

struct Face {
    int a = -1, b = -1;          // a: left / upper cell, b: right / lower cell
    bool xFace = true;           // true: between horizontal neighbours
    std::vector<double> e;       // terrain along the face
    double stripWidth = 0;
    double zMin = 0;
    double conveyance(double z, double kOverN) const;
    double flowArea(double z) const;
};

// Cells cut by a boundary-condition line, with the terrain along the line.
struct BoundaryCells {
    std::vector<int> cell;
    std::vector<Face> edge;      // one per cell: the line inside the cell (a = cell)
    double length = 0;
};

class Area {
public:
    // Builds the grid. cellSize <= 0: about 1/60 of the longer polygon side.
    bool build(const StorageArea& sa, const std::vector<io::Raster>& terrains, UnitSystem units, std::string& error,
               long maxCells = 300000);

    const std::string& name() const { return name_; }
    size_t size() const { return cells_.size(); }
    const Cell& cell(size_t i) const { return cells_[i]; }
    double cellSize() const { return cs_; }
    double cellArea() const { return cs_ * cs_; }
    double kOverN() const { return kOverN_; }
    int lowestCell() const;
    // Cell containing a point (-1 outside); nearest active cell (-1 if none).
    int cellAt(double x, double y) const;
    int nearestCell(double x, double y) const;
    BoundaryCells boundaryCells(const std::vector<XY>& line, const std::vector<io::Raster>& terrains) const;

    // State
    std::vector<double> z, zOld;
    // Sources of the current iteration (volume per second into the cell) and
    // their derivative with respect to the cell's own water surface.
    std::vector<double> src, srcD;
    // Cells held at a stage (penalty), filled per iteration.
    std::vector<std::pair<int, double>> fixed;
    double lastSourceVolume = 0;   // volume added by the sources in the last iterate() (free cells)

    void setDry();
    void beginStep() { zOld = z; }
    // One linearised implicit update; returns the largest stage change. False if the solve fails.
    bool iterate(double dt, double& maxChange);
    // Face velocities of the current state, averaged per cell (magnitude).
    std::vector<double> cellVelocity() const;
    double totalVolume() const;

    // Result grid skeleton (no times yet).
    Grid2DResult skeleton() const;

private:
    std::string name_;
    double kOverN_ = 1 / 0.06;
    double x0_ = 0, y0_ = 0, cs_ = 1;
    int nx_ = 0, ny_ = 0;
    std::vector<int> index_;
    std::vector<Cell> cells_;
    std::vector<Face> faces_;
    std::vector<std::vector<int>> cellFaces_;
    int epsg_ = 0;
    double faceAlpha(const Face& f, double za, double zb) const;
};

}  // namespace deras::flow2d

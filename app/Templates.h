// De-RAS - app/Templates.h
// Generators for new-project templates. The "prismatic channel" template
// builds a real, editable geometry (trapezoidal sections at a constant bed
// slope); the example project is the same template labelled as example data.
#pragma once

#include <string>
#include <vector>

#include "core/Model.h"
#include "io/Project.h"

namespace deras::app {

struct PrismaticSpec {
    std::string river = "Sungai";
    std::string reach = "Hilir";
    int count = 11;               // number of cross sections
    double spacing = 100;         // distance between sections (channel)
    double bottomWidth = 10;
    double sideSlope = 2;         // H:V
    double bankHeight = 4;        // channel depth to the bank top
    double floodplainWidth = 30;  // each side, flat, at bank top
    double floodplainRise = 1;    // floodplain rises this much to the end points
    double bedSlope = 0.001;
    double downstreamInvert = 100;
    double nChannel = 0.03, nOverbank = 0.06;
    std::vector<std::string> profileNames{"Q2", "Q25", "Q100"};
    std::vector<double> flows{50, 120, 180};
    BoundaryType dnType = BoundaryType::NormalDepth;
    double dnSlope = 0.001;
    double dnKnownWS = 0;
};

// Adds a geometry, steady flow and plan built from `spec` to the project.
void addPrismaticChannel(io::Project& prj, const PrismaticSpec& spec, bool exampleLabel);

// Creates the trapezoidal cross section used by the template.
CrossSection trapezoidXs(const PrismaticSpec& s, double invert, const std::string& rs, double lenToNext);

}  // namespace deras::app

// De-RAS - core/SedimentModel.h
// Quasi-unsteady flow and sediment transport data (M7, Preview). Stored in
// the project's .deras.json (modules["sediment"]); HEC-RAS keeps these in
// .q## and .s## files, the formats of which De-RAS does not read or write.
#pragma once

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/Messages.h"
#include "core/Model.h"
#include "core/Results.h"

namespace deras {

// One flow record of a quasi-unsteady series: a steady flow held for
// `durationHours`, computed in steps of `computeHours`.
struct QuasiRecord {
    double durationHours = 24;
    double flow = 0;
    double computeHours = 6;
    double dsStage = std::numeric_limits<double>::quiet_NaN();  // NaN: normal depth with QuasiFlow::dsSlope
};

struct QuasiFlow {
    std::string river, reach;           // the reach the series applies to (one reach)
    std::vector<QuasiRecord> records;
    double dsSlope = 0.001;             // friction slope of the normal-depth downstream boundary
};

enum class TransportFunction { MeyerPeterMuller = 0, EngelundHansen = 1, Yang = 2 };
const char* transportFunctionName(TransportFunction f);
enum class SedimentInflow { Equilibrium = 0, ClearWater = 1, Rating = 2 };

struct SedimentData {
    TransportFunction function = TransportFunction::MeyerPeterMuller;
    double specificGravity = 2.65;
    double porosity = 0.4;
    double temperature = 25;            // water temperature, degrees C (viscosity, fall velocity)
    std::vector<std::pair<double, double>> gradation;  // bed material: (diameter mm, percent finer), increasing
    double maxErodibleDepth = 2;        // below the initial channel invert (project length unit)
    SedimentInflow inflow = SedimentInflow::Equilibrium;
    std::vector<std::pair<double, double>> rating;     // (flow, load in tonnes/day SI or tons/day US)
};

struct SedimentSetup {
    std::string geomFile = "g01";
    QuasiFlow flow;
    SedimentData data;
    double outputHours = 24;            // output interval
    bool empty() const { return flow.records.empty(); }
};

// Diameter (mm) at a percent finer from a gradation curve (log-linear interpolation); NaN if the curve is unusable.
double gradationDiameter(const std::vector<std::pair<double, double>>& curve, double percent);

struct SedimentResult {
    RunStatus status = RunStatus::NotRun;
    MessageList messages;
    std::string river, reach, function;
    double d50 = 0, d90 = 0;                        // mm
    std::vector<std::string> rs;                    // cross sections, upstream -> downstream
    std::vector<double> distance;                   // channel distance from the downstream end
    std::vector<double> invert0;                    // initial channel invert
    std::vector<double> hours;                      // output times
    std::vector<double> flow;                       // flow at each output time
    std::vector<std::vector<double>> invert, ws, load, change;  // [time][xs]; load in t/day (tons/day US)
    double massIn = 0, massOut = 0, massChange = 0; // tonnes (tons US)
    std::string startedUtc, solverVersion;
    UnitSystem units = UnitSystem::SI;
};

}  // namespace deras

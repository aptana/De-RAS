// De-RAS - core/Results.h
// Computed results of a steady 1D run plus the run metadata needed to
// reproduce it. Results are always tagged with their origin.
#pragma once

#include <string>
#include <vector>

#include "Messages.h"
#include "Units.h"

namespace deras {

enum class DataOrigin { Input, Computed, Imported, Example };
const char* originLabel(DataOrigin o, Language lang);

struct XsResult {
    std::string river, reach, rs;
    double q = 0, qLob = 0, qCh = 0, qRob = 0;
    double minChEl = 0;
    double ws = 0, critWs = 0, eg = 0;
    double egSlope = 0;          // friction slope at the section (Q/K)^2
    double velCh = 0, velTotal = 0;
    double area = 0, areaLob = 0, areaCh = 0, areaRob = 0;
    double topWidth = 0;
    double froudeCh = 0;
    double alpha = 1;
    double frictionLoss = 0, ceLoss = 0;   // losses between this XS and the next one downstream
    double lengthWeighted = 0;             // discharge-weighted reach length used
    int trials = 0;
    bool critical = false;                 // WS was set to critical depth
    bool supercritical = false;            // WS from the supercritical (downstream-marching) computation
    double channelDistance = 0;            // cumulative main-channel distance from the downstream end
    MessageList notes;
};

struct ProfileResult {
    std::string name;
    std::vector<XsResult> xs;  // per reach, upstream -> downstream, reaches in geometry order
};

enum class RunStatus { NotRun, Succeeded, Failed, Cancelled };
const char* runStatusLabel(RunStatus s, Language lang);

struct RunMetadata {
    std::string solverName = "deras-steady";
    std::string solverVersion;
    std::string method = "standard-step-subcritical";
    std::string startedUtc, finishedUtc;
    double elapsedSeconds = 0;
    std::string projectFile, planFile, geomFile, flowFile;
    std::string geomSha256, flowSha256, planSha256;
    UnitSystem units = UnitSystem::USCustomary;
    double gravity = 0, manningK = 0;
    double stdStepTol = 0, criticalTol = 0, maxErrorTol = 0;
    int maxTrials = 0;
    std::string frictionMethod;
    std::string host;  // OS name
};

// Unsteady output (M5): water surface and flow at every output time for the
// cross sections of profiles[0] (same order).
struct TimeSeriesResult {
    double startSeconds = 0;               // simulation start, seconds since 1970 (UTC, time zone of the data)
    std::vector<double> hours;             // output times after the start
    std::vector<std::vector<double>> ws, q;  // [time][cross section]
    bool empty() const { return hours.empty(); }
};

// M6: stage or flow of storage areas, lateral structures, SA connections
// and pumps at the times of `series.hours`.
struct ExtraSeries {
    std::string kind;       // "storage", "area2d", "lateral", "connection", "pump"
    std::string name;
    std::string quantity;   // "stage" or "flow"
    std::vector<double> values;
    double maxValue = 0;
};

// M6: 2D flow area results on the regular computation grid (Preview).
// Stored in <base>.<plan>.deras-2d.bin next to the project.
struct Grid2DResult {
    std::string area;
    double x0 = 0, y0 = 0, cell = 1;   // top-left corner and cell size (map units)
    int nx = 0, ny = 0;
    int epsg = 0;
    std::vector<int> index;            // nx*ny, row 0 at the top: active cell number or -1
    std::vector<float> minElev;        // lowest terrain point of each active cell
    std::vector<double> hours;         // output times
    std::vector<std::vector<float>> wse;  // [time][active cell]
    std::vector<float> maxWse, maxDepth, maxVel, arrivalHours;  // arrival NaN = never wet
    bool empty() const { return index.empty(); }
};

struct RunResult {
    RunStatus status = RunStatus::NotRun;
    DataOrigin origin = DataOrigin::Computed;
    RunMetadata meta;
    std::vector<ProfileResult> profiles;   // unsteady: "Max WS" first, then output times
    TimeSeriesResult series;
    std::vector<ExtraSeries> extra;        // M6, same times as series.hours
    std::vector<Grid2DResult> grids;       // M6, 2D flow areas
    MessageList messages;
    bool isUnsteady() const { return !series.empty(); }
};

}  // namespace deras

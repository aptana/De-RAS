// De-RAS - core/Model.h
// In-memory model of a HEC-RAS project as used by De-RAS (Stage 1 scope:
// 1D steady geometry, steady flow data and plans). All values are in the
// project unit system (see Units.h).
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Units.h"

namespace deras {

struct StaElev {
    double station = 0;  // horizontal station across the section (left to right)
    double elev = 0;     // ground elevation
};

struct ManningBreak {
    double station = 0;  // n applies from this station to the next breakpoint
    double n = 0;
};

// A station range with an elevation: ineffective flow area or blocked
// obstruction ("#XS Ineff=" / "#Block Obstruct=" triplets).
struct StationBlock {
    double staStart = 0, staEnd = 0, elev = 0;
    bool permanent = false;  // ineffective areas only ("Permanent Ineff=")
};

struct LeveePoint {
    bool present = false;
    double station = 0, elev = 0;
};

// HEC-RAS geometry node types ("Type RM Length L Ch R = <type>").
enum class NodeType { CrossSection = 1, Culvert = 2, Bridge = 3, MultipleOpening = 4,
                      InlineStructure = 5, LateralStructure = 6, Other = 0 };

const char* nodeTypeName(NodeType t);
double rsNumber(const std::string& rs);  // numeric value of a river station text (NaN if none)
int nextUid();  // process-wide unique id generator

// ---- hydraulic structures (M4) ----
// Deck / roadway / weir point: station with high chord (top of deck or road)
// and low chord (bottom of the deck; equal to the high chord for a road
// embankment without opening).
struct DeckPoint {
    double station = 0, high = 0, low = 0;
    bool operator==(const DeckPoint& o) const { return station == o.station && high == o.high && low == o.low; }
};
struct Pier {
    double skew = 0, upSta = 0, dnSta = 0;
    std::vector<double> upWidth, upElev, dnWidth, dnElev;  // width at elevation, bottom to top
    bool operator==(const Pier& o) const {
        return skew == o.skew && upSta == o.upSta && dnSta == o.dnSta && upWidth == o.upWidth && upElev == o.upElev &&
               dnWidth == o.dnWidth && dnElev == o.dnElev;
    }
};
// Identical culvert barrels (one "Culvert=" line per barrel in the file).
struct CulvertGroup {
    std::string name;
    int shape = 1;                 // 1 circular, 2 box (other HEC-RAS shapes are kept but not computed)
    double span = 0, rise = 0;     // box width / height; circular: rise = diameter
    double length = 0, n = 0.013;
    double ke = 0.5, kx = 1.0;     // entrance and exit loss coefficients
    int chart = 1, scale = 1;      // FHWA inlet control chart and scale
    double upInvert = 0, dnInvert = 0;
    std::vector<double> upStations, dnStations;  // barrel centre lines (one per barrel)
    bool operator==(const CulvertGroup& o) const {
        return name == o.name && shape == o.shape && span == o.span && rise == o.rise && length == o.length && n == o.n &&
               ke == o.ke && kx == o.kx && chart == o.chart && scale == o.scale && upInvert == o.upInvert &&
               dnInvert == o.dnInvert && upStations == o.upStations && dnStations == o.dnStations;
    }
};
// Gate group of an inline structure. `opening` is the gate opening height
// used by steady runs (De-RAS setting, stored in the project's .deras.json).
struct GateGroup {
    std::string name;
    double width = 0, height = 0, invert = 0;
    double coef = 0.6;             // sluice / orifice discharge coefficient
    int type = 0;                  // 0 sluice, 1 radial (both computed as sluice gates)
    double weirCoef = 0;           // weir coefficient over the gate sill when the gate is not submerged
    std::vector<double> stations;  // opening centre lines
    double opening = -1;           // < 0: fully open (= height)
    bool operator==(const GateGroup& o) const {
        return name == o.name && width == o.width && height == o.height && invert == o.invert && coef == o.coef &&
               type == o.type && weirCoef == o.weirCoef && stations == o.stations;
    }
};
// Breach of a lateral structure or storage area connection (M8). The crest
// is cut by a trapezoid that grows linearly to its final size during
// `formHours`, starting `startHours` after the simulation start. Stored in
// the project's .deras.json (HEC-RAS keeps breach data in the plan file, the
// format of which De-RAS does not write).
struct Breach {
    bool active = false;
    double station = 0;        // centre station along the weir crest
    double bottomElev = 0;     // final breach bottom elevation
    double bottomWidth = 0;    // final bottom width
    double sideSlope = 1;      // horizontal : vertical
    double startHours = 0, formHours = 1;
    bool operator==(const Breach& o) const {
        return active == o.active && station == o.station && bottomElev == o.bottomElev && bottomWidth == o.bottomWidth &&
               sideSlope == o.sideSlope && startHours == o.startHours && formHours == o.formHours;
    }
};
// Effective crest elevation of a weir point at time t (hours) with a breach.
double breachedCrest(const Breach& b, double station, double crest, double tHours);

// Lateral structure (node type 6, M6): a weir along the reach between two
// cross sections that spills into a storage area, a 2D flow area or out of
// the system. Stations of the crest run along the structure from its
// upstream end.
struct LateralData {
    int side = 1;                  // 0 left bank, 1 right bank (looking downstream)
    std::string toName;            // storage area or 2D flow area name; empty = out of the system
    Breach breach;                 // not part of the geometry file (see Breach)
    bool operator==(const LateralData& o) const { return side == o.side && toName == o.toName; }
};

struct StructureData {
    bool parsed = false;           // data understood by De-RAS (false: kept verbatim, not computed)
    double dist = 0;               // distance from the upstream cross section to the upstream face
    double width = 0;              // deck / road / weir width along the flow
    double weirCoef = 0;           // weir coefficient of deck, road or inline weir crest
    double skew = 0, maxSubmerge = 0.95;
    bool ogee = false;
    std::vector<DeckPoint> deckUp, deckDn;  // bridge deck or culvert road
    std::vector<Pier> piers;
    std::vector<CulvertGroup> culverts;
    std::vector<StaElev> weir;              // inline weir crest
    std::vector<GateGroup> gates;
    LateralData lateral;                    // lateral structures only
    bool operator==(const StructureData& o) const {
        return parsed == o.parsed && dist == o.dist && width == o.width && weirCoef == o.weirCoef && skew == o.skew &&
               maxSubmerge == o.maxSubmerge && ogee == o.ogee && deckUp == o.deckUp && deckDn == o.deckDn &&
               piers == o.piers && culverts == o.culverts && weirEq(o) && gates == o.gates && lateral == o.lateral;
    }
    bool weirEq(const StructureData& o) const {
        if (weir.size() != o.weir.size()) return false;
        for (size_t i = 0; i < weir.size(); ++i)
            if (weir[i].station != o.weir[i].station || weir[i].elev != o.weir[i].elev) return false;
        return true;
    }
};

struct CrossSection {
    int uid = 0;                    // stable identity inside one editing session (not written to file)
    std::string rs;                 // river station text as written in the file (e.g. "1000", "5.99*")
    NodeType type = NodeType::CrossSection;
    double lenLeft = 0, lenChannel = 0, lenRight = 0;  // downstream reach lengths
    std::vector<StaElev> staElev;
    std::vector<ManningBreak> manning;
    int manningFormat = 0;          // second value of '#Mann= N , f , 0' (preserved)
    std::optional<double> bankLeft, bankRight;
    double contraction = 0.1, expansion = 0.3;
    std::string description;
    // Read from the file and used by the solver (M2); not edited by the UI yet,
    // so the original lines are kept unchanged when the section is saved.
    std::vector<StationBlock> ineffective;
    int ineffectiveMode = 0;          // second value of '#XS Ineff= N , m'
    std::vector<StationBlock> obstructions;
    int obstructionMode = 0;          // second value of '#Block Obstruct= N , m'
    LeveePoint leveeLeft, leveeRight;
    // GIS cut line ('XS GIS Cut Line='), map coordinates, left to right looking
    // downstream. Empty: RAS Mapper places the section across the reach centreline.
    std::vector<std::pair<double, double>> cutLine;
    // Bridge (3), culvert (2) and inline structure (5) nodes.
    StructureData structure;
    // Stage-1 unsupported data that exists in the file (kept, but ignored by
    // the solver). Filled by the parser so Model Doctor can warn about it.
    std::vector<std::string> ignoredFeatures;

    bool sameData(const CrossSection& o) const;  // equality of everything written to file
    double rsValue() const;          // numeric part of rs (NaN if not numeric)
    double minElevation() const;
    double channelInvert() const;    // lowest ground point between the banks
};

struct Reach {
    int uid = 0;
    std::string river;
    std::string reach;
    std::vector<CrossSection> nodes;  // file order (normally upstream -> downstream)
    std::vector<std::pair<double, double>> xy;  // reach centreline (schematic or GIS)

    std::string label() const { return river + " / " + reach; }
    bool sameHeader(const Reach& o) const { return river == o.river && reach == o.reach && xy == o.xy; }
    std::vector<const CrossSection*> crossSections() const;  // only Type 1
};

// Junction between reaches ("Junct Name=" block). Lengths are the
// "Junc L&A=" lengths, in the order of the side that has several reaches
// (upstream reaches for a confluence, downstream reaches for a split).
struct Junction {
    std::string name;
    std::vector<std::pair<std::string, std::string>> up, dn;  // (river, reach)
    std::vector<double> lengths;
    // Lines of the block De-RAS does not interpret, kept in place when the
    // junction is rewritten: before the first "Up River,Reach=" and after the
    // last "Junc L&A=" line.
    std::vector<std::string> linesBefore, linesAfter;
    double lengthTo(const std::string& river, const std::string& reach) const;
    bool operator==(const Junction& o) const {
        return name == o.name && up == o.up && dn == o.dn && lengths == o.lengths && linesBefore == o.linesBefore &&
               linesAfter == o.linesAfter;
    }
};

// ---- storage areas and 2D flow areas (M6) ----
using XY = std::pair<double, double>;
struct StorageArea {
    std::string name;
    std::vector<XY> polygon;          // "Storage Area Surface Line=" (map coordinates)
    bool is2D = false;                // "Storage Area Is2D=" (2D flow area)
    // 1D storage: elevation-volume table, or a flat area above a minimum elevation.
    std::vector<std::pair<double, double>> volElev;  // (elevation, volume), elevation increasing
    double area = 0, minElev = 0;
    // 2D flow area: cell size of the regular computation grid and Manning's n.
    double cellSize = 0;
    double manningN = 0.06;
    std::vector<std::string> otherLines;  // block lines De-RAS does not interpret (kept in order)
    bool operator==(const StorageArea& o) const {
        return name == o.name && polygon == o.polygon && is2D == o.is2D && volElev == o.volElev && area == o.area &&
               minElev == o.minElev && cellSize == o.cellSize && manningN == o.manningN && otherLines == o.otherLines;
    }
};

// Weir connection between two storage / 2D areas ("Connection=" block).
struct SaConnection {
    std::string name;
    std::string upName, dnName;       // storage area or 2D area names
    std::vector<XY> line;             // map line of the connection
    double width = 10, coef = 1.44;
    std::vector<StaElev> weir;        // crest station-elevation
    Breach breach;                    // not part of the geometry file
    std::vector<std::string> otherLines;
    bool operator==(const SaConnection& o) const {
        if (weir.size() != o.weir.size()) return false;
        for (size_t i = 0; i < weir.size(); ++i)
            if (weir[i].station != o.weir[i].station || weir[i].elev != o.weir[i].elev) return false;
        return name == o.name && upName == o.upName && dnName == o.dnName && line == o.line && width == o.width && coef == o.coef &&
               otherLines == o.otherLines;
    }
};

// Boundary condition line of a 2D flow area ("BC Line Name=" block).
struct BcLine {
    std::string name, area;
    std::vector<XY> line;
    std::vector<std::string> otherLines;
    bool operator==(const BcLine& o) const { return name == o.name && area == o.area && line == o.line && otherLines == o.otherLines; }
};

// Pump station (M6). End points are a reach cross section, a storage area,
// a 2D flow area cell (map point) or, for the outlet, out of the system.
// Stored in the project's .deras.json (pump station blocks of HEC-RAS
// geometry files are kept but not read).
struct PumpEnd {
    int kind = 1;                     // 0 reach cross section, 1 storage / 2D area, 2 out of the system
    std::string area, river, reach, rs;
    XY point{0, 0};                   // 2D flow area location
};
struct PumpGroup {
    std::string name;
    int count = 1;
    double capacity = 1;              // flow per pump
    double onElev = 0, offElev = 0;   // start when the inlet water surface rises above onElev, stop below offElev
};
struct PumpStation {
    std::string name;
    PumpEnd from, to;
    std::vector<PumpGroup> groups;
};

struct Geometry {
    std::string title;
    std::string programVersion;
    std::vector<Reach> reaches;
    int junctionCount = 0;           // junctions present in the file
    std::vector<Junction> junctions;
    int storageAreaCount = 0;
    int area2DCount = 0;
    std::vector<StorageArea> storageAreas;   // storage areas and 2D flow areas
    std::vector<SaConnection> connections;
    std::vector<BcLine> bcLines;
    std::vector<PumpStation> pumps;   // from the project's .deras.json (not part of the .g## file)

    StorageArea* findArea(const std::string& name);
    const StorageArea* findArea(const std::string& name) const;

    Reach* findReach(const std::string& river, const std::string& reach);
    const Reach* findReach(const std::string& river, const std::string& reach) const;
};

// Steady flow boundary condition types; numeric values follow the HEC-RAS
// .f## "Up Type=" / "Dn Type=" codes.
enum class BoundaryType { None = 0, KnownWS = 1, CriticalDepth = 2, NormalDepth = 3, RatingCurve = 4 };

struct Boundary {
    BoundaryType type = BoundaryType::None;
    double knownWS = 0;
    double slope = 0;
    std::vector<std::pair<double, double>> rating;  // (stage, flow) pairs
};

struct ReachBoundary {
    std::string river, reach;
    int profile = 0;      // 0-based profile index
    Boundary upstream, downstream;
};

struct FlowChange {
    std::string river, reach, rs;
    std::vector<double> flows;  // one per profile
};

struct SteadyFlow {
    std::string title;
    std::string programVersion;
    std::vector<std::string> profileNames;
    std::vector<FlowChange> changes;
    std::vector<ReachBoundary> boundaries;

    int profileCount() const { return static_cast<int>(profileNames.size()); }
    const ReachBoundary* boundary(const std::string& river, const std::string& reach, int profile) const;
    ReachBoundary& boundaryRef(const std::string& river, const std::string& reach, int profile);
};

// ---- unsteady flow (M5) ----
// Boundary condition types of a .u## "Boundary Location=" block.
enum class UBoundaryType { None, FlowHydrograph, StageHydrograph, LateralInflow, UniformLateralInflow, NormalDepth, RatingCurve, Precipitation, Other };
const char* uBoundaryKey(UBoundaryType t);  // file key, e.g. "Flow Hydrograph="

struct UnsteadyBoundary {
    std::string river, reach, rs;     // location (rs empty = reach end)
    std::string rsEnd;                // downstream RS of a uniform lateral inflow
    std::string area;                 // storage area or 2D flow area (6th location field)
    std::string bcLine;               // 2D boundary condition line (8th location field)
    std::vector<std::string> location;  // all trimmed fields of "Boundary Location=" (kept on write)
    UBoundaryType type = UBoundaryType::None;
    std::string interval = "1HOUR";   // time step of the hydrograph values
    std::vector<double> values;       // hydrograph ordinates (flow or stage)
    double slope = 0;                 // normal depth friction slope
    std::vector<std::pair<double, double>> rating;  // (stage, flow)
    std::vector<std::string> otherLines;  // block lines De-RAS does not interpret, kept in order
    bool operator==(const UnsteadyBoundary& o) const {
        return river == o.river && reach == o.reach && rs == o.rs && rsEnd == o.rsEnd && area == o.area && bcLine == o.bcLine &&
               location == o.location && type == o.type &&
               interval == o.interval && values == o.values && slope == o.slope && rating == o.rating && otherLines == o.otherLines;
    }
};

struct InitialFlow {
    std::string river, reach, rs;
    double q = 0;
    bool operator==(const InitialFlow& o) const { return river == o.river && reach == o.reach && rs == o.rs && q == o.q; }
};

struct UnsteadyFlow {
    std::string title;
    std::string programVersion;
    std::vector<InitialFlow> initialFlows;
    std::vector<std::pair<std::string, double>> initialStorage;  // "Initial Storage Elev=" (area name, water surface)
    std::vector<UnsteadyBoundary> boundaries;
    bool operator==(const UnsteadyFlow& o) const {
        return title == o.title && programVersion == o.programVersion && initialFlows == o.initialFlows &&
               initialStorage == o.initialStorage && boundaries == o.boundaries;
    }
};

// Interval text of HEC-RAS ("1MIN", "15MIN", "1HOUR", "1DAY", "30SEC", "1WEEK") in seconds; 0 if not understood.
double intervalSeconds(const std::string& text);
// "01JAN2024,0000" or "01JAN2024,2400" -> seconds since 1 Jan 1970 UTC; NaN if not understood.
double rasDateTimeSeconds(const std::string& date, const std::string& time);
std::string rasDateText(double seconds);  // "01JAN2024"
std::string rasTimeText(double seconds);  // "0000"

enum class FlowRegime { Subcritical, Supercritical, Mixed };

enum class FrictionSlopeMethod { AverageConveyance = 1, AverageFrictionSlope = 2,
                                 GeometricMean = 3, HarmonicMean = 4 };

struct Plan {
    std::string title;
    std::string shortId;
    std::string programVersion;
    std::string geomFile;   // extension e.g. "g01"
    std::string flowFile;   // extension e.g. "f01" (steady) or "u01"
    FlowRegime regime = FlowRegime::Subcritical;
    double stdStepTol = 0.01;
    double criticalTol = 0.01;
    int maxTrials = 20;
    double maxErrorTol = 0.3;
    FrictionSlopeMethod frictionMethod = FrictionSlopeMethod::AverageConveyance;
    // unsteady settings (M5): "Simulation Date=start date,time,end date,time"
    std::string simStartDate, simStartTime, simEndDate, simEndTime;
    std::string compInterval = "1MIN", outputInterval = "1HOUR";
    double theta = 1.0;               // implicit weighting factor (0.6 .. 1)
    double unetZTol = 0.01;           // water surface tolerance of the Newton iterations
    int unetMaxIter = 20;
    bool isUnsteady() const { return !flowFile.empty() && flowFile[0] == 'u'; }
};

const char* regimeName(FlowRegime r, bool indonesian);

}  // namespace deras

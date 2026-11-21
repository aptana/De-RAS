// De-RAS - io/Project.h
// A HEC-RAS project opened by De-RAS: the .prj file plus its geometry,
// steady flow and plan files, and the De-RAS sidecar (<base>.deras.json)
// that HEC-RAS ignores. There is no De-RAS-specific project format.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "GeometryFile.h"
#include "ProjectFiles.h"
#include "core/Json.h"

namespace deras::io {

struct ProjectSettings {             // stored in <base>.deras.json
    std::string coordinateSystem;    // EPSG code or WKT (informative in Stage 1)
    std::string timeZone = "UTC+07:00";
    std::string notes;
    std::string targetRasVersion = "7.00";  // written to "Program Version="
    // RAS Mapper layers (M3): GeoTIFF terrains and georeferenced pictures
    // (PNG/JPG with a world file), relative to the project folder when inside it.
    std::vector<std::string> terrains;
    std::vector<std::string> pictures;
    // Gate openings of inline structures for steady runs (M4), keyed
    // "river|reach|rs|gate name"; HEC-RAS keeps these in the flow file, the
    // format of which De-RAS does not write yet.
    std::map<std::string, double> gateOpenings;
    // De-RAS data of later milestones, one JSON object per module (M6-M8):
    // "pumps" {geom ext: [...]}, "breaches" {key: {...}}, "sediment", "water_quality",
    // "design", "calibration", ...
    std::map<std::string, json::Value> modules;
};
std::string gateKey(const std::string& river, const std::string& reach, const std::string& rs, const std::string& gate);
// Copies the stored openings into the gate groups of a geometry (missing = fully open).
void applyGateOpenings(const ProjectSettings& s, Geometry& g);

// Pump stations and breaches (stored in modules["pumps"] / modules["breaches"]).
std::string lateralKey(const std::string& river, const std::string& reach, const std::string& rs);  // "lat|river|reach|rs"
std::string connectionKey(const std::string& name);                                               // "conn|name"
std::vector<PumpStation> pumpsFor(const ProjectSettings& s, const std::string& geomExt);
void setPumps(ProjectSettings& s, const std::string& geomExt, const std::vector<PumpStation>& pumps);
Breach breachFor(const ProjectSettings& s, const std::string& key);
void setBreach(ProjectSettings& s, const std::string& key, const Breach& b);
// Gate openings, pumps and breaches of the settings applied to a geometry before a run.
void applyGeometrySettings(const ProjectSettings& s, const std::string& geomExt, Geometry& g);

class Project {
public:
    // Opens <dir>/<base>.prj and all files it lists.
    static std::unique_ptr<Project> open(const fs::path& prjPath, MessageList& msgs);
    // Creates a new, empty project (files are written on save()).
    static std::unique_ptr<Project> create(const fs::path& dir, const std::string& baseName, const std::string& title,
                                           UnitSystem units, const ProjectSettings& settings);
    // Copies every file of the project into newDir/newBase.* (HEC-RAS "Save Project As").
    static bool duplicate(const fs::path& prjPath, const fs::path& newDir, const std::string& newBase, MessageList& msgs);

    bool save(MessageList& msgs);
    bool isModified() const;

    fs::path dir() const { return dir_; }
    std::string baseName() const { return base_; }
    fs::path prjPath() const { return dir_ / toPath(base_ + ".prj"); }
    fs::path pathFor(const std::string& ext) const { return dir_ / toPath(base_ + "." + ext); }
    fs::path sidecarPath() const { return dir_ / toPath(base_ + ".deras.json"); }

    ProjectFileData& info() { return prj_.data; }
    const ProjectFileData& info() const { return prj_.data; }
    UnitSystem units() const { return prj_.data.units; }
    ProjectSettings settings;

    // Files (keyed by extension e.g. "g01").
    std::map<std::string, GeometryFile> geometries;
    std::map<std::string, SteadyFlowFile> steadyFlows;
    std::map<std::string, UnsteadyFlowFile> unsteadyFlows;
    std::map<std::string, PlanFile> plans;

    // Adds new files with the lowest free number (HEC-RAS rule) and lists them in the .prj.
    std::string addGeometry(const std::string& title);
    std::string addSteadyFlow(const std::string& title);
    std::string addUnsteadyFlow(const std::string& title);
    std::string addPlan(const std::string& title, const std::string& shortId, const std::string& geom, const std::string& flow);
    std::string nextFreeExt(char prefix) const;

    // Autosave (M8): writes every modified file of the project (as it would
    // be saved) into `dir`, with "recovery.json" naming the project. The
    // project itself is not changed or marked saved.
    bool writeRecovery(const fs::path& dir, MessageList& msgs) const;
    // Loads the files of a recovery folder over the opened project; they show
    // as unsaved changes. Returns false when the folder is not for this project.
    bool applyRecovery(const fs::path& dir, MessageList& msgs);

private:
    fs::path dir_;
    std::string base_;
    ProjectFile prj_;
    bool sidecarDirty_ = false;
    bool loadSidecar(MessageList& msgs);
    bool saveSidecar(MessageList& msgs);
    std::string sidecarText() const;
    static void settingsFromJson(const json::Value& v, ProjectSettings& s);
    ProjectSettings savedSettings_;
};

bool sameSettings(const ProjectSettings& a, const ProjectSettings& b);

}  // namespace deras::io

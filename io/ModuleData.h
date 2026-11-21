// De-RAS - io/ModuleData.h
// Data of the M7/M8 modules kept in the project's .deras.json
// (ProjectSettings::modules) and their result files:
//  - "sediment": quasi-unsteady flow and sediment data (M7)
//  - "water_quality": water quality data of one unsteady plan (M8)
//  Result file of a sediment run: <base>.deras-sedimen.json
//  Result file of a water quality run: <base>.<plan>.deras-wq.json
#pragma once

#include <string>

#include "core/SedimentModel.h"
#include "engines/design/Design.h"
#include "io/Project.h"

namespace deras::io {

SedimentSetup sedimentFor(const ProjectSettings& s);
void setSediment(ProjectSettings& s, const SedimentSetup& setup);

fs::path sedimentResultPath(const fs::path& dir, const std::string& base);
bool writeSedimentResult(const fs::path& path, const SedimentResult& r, std::string& error);
bool readSedimentResult(const fs::path& path, SedimentResult& r, std::string& error);

struct WqSetup {
    std::string plan;          // unsteady plan whose hydraulics carry the constituent
    std::string constituent = "Tracer";
    design::WqInput input;
};
WqSetup waterQualityFor(const ProjectSettings& s);
void setWaterQuality(ProjectSettings& s, const WqSetup& w);
fs::path wqResultPath(const fs::path& dir, const std::string& base, const std::string& planExt);
bool writeWqResult(const fs::path& path, const design::WqResult& r, const std::string& meta, std::string& error);
bool readWqResult(const fs::path& path, design::WqResult& r, std::string& meta, std::string& error);

// Converts the De-RAS module data of the sidecar (gate openings, pumps,
// breaches, sediment, water quality) between unit systems.
void convertSettings(ProjectSettings& s, bool toSI);

}  // namespace deras::io

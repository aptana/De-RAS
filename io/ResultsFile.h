// De-RAS - io/ResultsFile.h
// Stage 1 result storage: <base>.<plan>.deras-results.json next to the
// project. HEC-RAS ignores this file. Writing results into the HEC-RAS
// .p##.hdf layout is planned for Stage 2 (see the feature matrix).
#pragma once

#include "core/Json.h"
#include "core/Results.h"
#include "RasText.h"

namespace deras::io {

fs::path resultsPathFor(const fs::path& projectDir, const std::string& base, const std::string& planExt);
// 2D flow area results (M6): binary file next to the results.
fs::path grid2dPathFor(const fs::path& projectDir, const std::string& base, const std::string& planExt);
bool writeGrids2D(const fs::path& path, const std::vector<Grid2DResult>& grids, std::string& error);
bool readGrids2D(const fs::path& path, std::vector<Grid2DResult>& grids, std::string& error);
fs::path logPathFor(const fs::path& projectDir, const std::string& base, const std::string& planExt);

json::Value messageToJson(const Message& m);
Message messageFromJson(const json::Value& v);

json::Value resultsToJson(const RunResult& r);
bool resultsFromJson(const json::Value& v, RunResult& r, std::string& error);

bool writeResults(const fs::path& path, const RunResult& r, std::string& error);
// Also reads the 2D grids when the matching .deras-2d.bin exists.
bool readResults(const fs::path& path, RunResult& r, std::string& error);

}  // namespace deras::io

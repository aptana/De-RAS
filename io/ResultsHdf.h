// De-RAS - io/ResultsHdf.h
// Steady results in an HDF5 file laid out like a HEC-RAS plan results file
// (.p##.hdf): /Results/Steady/Output/Output Blocks/Base Output/Steady
// Profiles/... with one row per profile and one column per cross section.
// The layout follows public descriptions of HEC-RAS 6.x result files; it
// has not yet been opened in HEC-RAS itself (test K3).
#pragma once

#include <string>

#include "core/Model.h"
#include "core/Results.h"
#include "RasText.h"

namespace deras::io {

fs::path hdfResultsPathFor(const fs::path& projectDir, const std::string& base, const std::string& planExt);

// True when the file at `path` does not exist or was written by De-RAS
// (De-RAS never overwrites a results file written by another program).
bool hdfWritableByDeras(const fs::path& path);

std::string steadyResultsHdf(const RunResult& r, const Plan& plan, UnitSystem units, const std::string& derasVersion);
bool writeSteadyHdf(const fs::path& path, const RunResult& r, const Plan& plan, UnitSystem units,
                    const std::string& derasVersion, std::string& error);

}  // namespace deras::io

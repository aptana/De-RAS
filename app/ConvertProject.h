// De-RAS - app/ConvertProject.h
// Project unit conversion SI <-> US Customary (M8, Preview). The project is
// first copied under a new base name (the original stays untouched), then
// the copy's geometry, flow and plan files and the De-RAS module data are
// converted and the .prj unit line is switched. Results of the copy are
// removed because they are in the old units. Terrains and GIS layers are not
// converted: they keep their own units and are listed in `notes`.
#pragma once

#include <string>
#include <vector>

#include "io/Project.h"

namespace deras::app {

bool convertProjectCopy(const io::fs::path& prjPath, const io::fs::path& newDir, const std::string& newBase, UnitSystem target,
                        MessageList& msgs, std::vector<std::string>& notes);

}  // namespace deras::app

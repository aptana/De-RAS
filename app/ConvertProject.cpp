// De-RAS - app/ConvertProject.cpp
#include "app/ConvertProject.h"

#include "engines/design/Design.h"
#include "io/ModuleData.h"
#include "io/RasText.h"

namespace deras::app {

bool convertProjectCopy(const io::fs::path& prjPath, const io::fs::path& newDir, const std::string& newBase, UnitSystem target,
                        MessageList& msgs, std::vector<std::string>& notes) {
    {
        auto src = io::Project::open(prjPath, msgs);
        if (!src) return false;
        if (src->units() == target) {
            msgs.push_back(makeMsg(Severity::Error, "UNITS_SAME", {}));
            return false;
        }
    }
    if (!io::Project::duplicate(prjPath, newDir, newBase, msgs)) return false;
    // results of the copy are in the old units
    std::error_code ec;
    for (const auto& e : io::fs::directory_iterator(newDir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string n = io::fromPath(e.path().filename());
        if (n.compare(0, newBase.size() + 1, newBase + ".") != 0) continue;
        for (const char* suf : {".deras-results.json", ".deras-2d.bin", ".deras-sedimen.json", ".deras-wq.json", ".hdf", ".log"})
            if (n.size() > std::string(suf).size() && n.compare(n.size() - std::string(suf).size(), std::string::npos, suf) == 0) {
                io::fs::remove(e.path(), ec);
                break;
            }
    }
    auto p = io::Project::open(newDir / io::toPath(newBase + ".prj"), msgs);
    if (!p) return false;
    const bool toSI = target == UnitSystem::SI;
    for (auto& [ext, g] : p->geometries) design::convertGeometry(g.model, toSI);
    for (auto& [ext, f] : p->steadyFlows) design::convertSteadyFlow(f.model, toSI);
    for (auto& [ext, f] : p->unsteadyFlows) design::convertUnsteadyFlow(f.model, toSI);
    for (auto& [ext, pl] : p->plans) design::convertPlan(pl.model, toSI);
    io::convertSettings(p->settings, toSI);
    p->info().units = target;
    for (const auto& t : p->settings.terrains) notes.push_back(t);
    for (const auto& t : p->settings.pictures) notes.push_back(t);
    return p->save(msgs);
}

}  // namespace deras::app

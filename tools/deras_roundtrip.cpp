// De-RAS - tools/deras_roundtrip.cpp
// deras-roundtrip: K1 compatibility check. Opens every HEC-RAS project
// (*.prj with "Proj Title=") under a folder, re-serialises each .prj, .g##,
// .f## and .p## file without edits, and compares the bytes with the
// original. Prints one line per file and a summary; exit code 1 if any
// file differs.
//
//   deras-roundtrip <folder-or-prj> [--edit-check]
//
// --edit-check additionally changes nothing semantically but forces
// regeneration of every cross section, then re-parses the output and
// verifies the parsed model is identical (lossless regeneration check).
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "io/Project.h"

using namespace deras;
namespace fs = std::filesystem;

static bool isRasPrj(const fs::path& p) {
    std::string bytes;
    if (!io::readFile(p, bytes)) return false;
    return bytes.rfind("Proj Title=", 0) == 0 || bytes.find("\nProj Title=") != std::string::npos;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "Usage: deras-roundtrip <folder-or-prj> [--edit-check]\n"); return 2; }
    bool editCheck = argc > 2 && std::string(argv[2]) == "--edit-check";
    fs::path root = io::toPath(argv[1]);
    std::vector<fs::path> prjs;
    std::error_code ec;
    if (fs::is_regular_file(root)) prjs.push_back(root);
    else
        for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file() && it->path().extension() == ".prj" && isRasPrj(it->path())) prjs.push_back(it->path());

    int files = 0, same = 0, regenOk = 0, regenFail = 0;
    for (const auto& prj : prjs) {
        MessageList msgs;
        auto p = io::Project::open(prj, msgs);
        if (!p) { std::printf("OPEN-FAIL %s\n", io::fromPath(prj).c_str()); continue; }
        auto check = [&](const fs::path& path, const std::string& out) {
            std::string orig;
            io::readFile(path, orig);
            ++files;
            bool eq = orig == out;
            if (eq) ++same;
            std::printf("%s %s\n", eq ? "IDENTICAL" : "DIFFERENT", io::fromPath(path).c_str());
        };
        for (auto& [ext, g] : p->geometries) {
            check(p->pathFor(ext), g.serialize());
            if (editCheck) {
                // force regeneration: bump and restore a value via a copy of each XS
                io::GeometryFile copy = g;
                for (auto& r : copy.model.reaches)
                    for (auto& n : r.nodes)
                        if (n.type == NodeType::CrossSection) n.description += " ";
                std::string regenerated = copy.serialize();
                io::GeometryFile back;
                MessageList m2;
                back.parse(regenerated, "regen", m2);
                bool ok = back.model.reaches.size() == g.model.reaches.size();
                for (size_t i = 0; ok && i < g.model.reaches.size(); ++i) {
                    const auto& a = g.model.reaches[i];
                    const auto& b = back.model.reaches[i];
                    ok = a.river == b.river && a.reach == b.reach && a.nodes.size() == b.nodes.size();
                    for (size_t k = 0; ok && k < a.nodes.size(); ++k) {
                        CrossSection x = b.nodes[k];
                        if (x.type == NodeType::CrossSection && !x.description.empty() && x.description.back() == ' ')
                            x.description.pop_back();
                        x.uid = a.nodes[k].uid;
                        ok = a.nodes[k].sameData(x);
                    }
                }
                std::printf("%s %s\n", ok ? "REGEN-LOSSLESS" : "REGEN-CHANGED", io::fromPath(p->pathFor(ext)).c_str());
                ok ? ++regenOk : ++regenFail;
            }
        }
        for (auto& [ext, f] : p->steadyFlows) check(p->pathFor(ext), f.serialize());
        for (auto& [ext, pl] : p->plans) check(p->pathFor(ext), pl.serialize());
        {
            io::ProjectFile pf;
            MessageList m;
            pf.load(prj, m);
            check(prj, pf.serialize());
        }
    }
    std::printf("SUMMARY projects=%zu files=%d identical=%d", prjs.size(), files, same);
    if (editCheck) std::printf(" regen_lossless=%d regen_changed=%d", regenOk, regenFail);
    std::printf("\n");
    return (same == files && regenFail == 0) ? 0 : 1;
}

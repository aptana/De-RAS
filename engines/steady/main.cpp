// De-RAS - engines/steady/main.cpp
// deras-steady: headless engine process for 1D steady plans.
//
//   deras-steady <project.prj> [--plan p01] [--lang id|en] [--cancel-file <path>]
//
// Output protocol on stdout (one line each, UTF-8):
//   PROGRESS <done> <total>
//   MSG <info|warning|error> <text>
//   STEP <preprocess|simulate|write>
//   STATUS <succeeded|failed|cancelled> <plan>
// Exit codes: 0 succeeded, 1 model invalid / failed, 3 cancelled, 2 usage.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "Version.h"
#include "app/RunService.h"
#include "io/RasText.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace deras;

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::string prj, plan, cancelFile;
    Language lang = Language::ID;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--plan" && i + 1 < argc) plan = argv[++i];
        else if (a == "--lang" && i + 1 < argc) lang = std::string(argv[++i]) == "en" ? Language::EN : Language::ID;
        else if (a == "--cancel-file" && i + 1 < argc) cancelFile = argv[++i];
        else if (a == "--version") { std::printf("deras-steady %s\n", DERAS_VERSION); return 0; }
        else if (a == "--help" || a == "-h") {
            std::printf("deras-steady %s\nUsage: deras-steady <project.prj> [--plan p01] [--lang id|en] [--cancel-file path]\n", DERAS_VERSION);
            return 0;
        } else prj = a;
    }
    if (prj.empty()) { std::fprintf(stderr, "Usage: deras-steady <project.prj> [--plan p01]\n"); return 2; }

    app::RunRequest req;
    req.prjPath = io::toPath(prj);
    req.planExt = plan;
    if (!cancelFile.empty()) {
        auto cf = io::toPath(cancelFile);
        req.cancelled = [cf]() { std::error_code ec; return std::filesystem::exists(cf, ec); };
    }
    int lastPct = -1;
    req.progress = [&](int done, int total) {
        int pct = total > 0 ? done * 100 / total : 100;
        if (pct != lastPct) { std::printf("PROGRESS %d %d\n", done, total); std::fflush(stdout); lastPct = pct; }
    };
    req.message = [&](const Message& m) {
        const char* s = m.severity == Severity::Error ? "error" : m.severity == Severity::Warning ? "warning" : "info";
        std::printf("MSG %s %s\n", s, m.text(lang).c_str());
        std::fflush(stdout);
    };
    req.step = [&](const std::string& s) { std::printf("STEP %s\n", s.c_str()); std::fflush(stdout); };
    std::string used;
    RunResult r = app::runSteadyPlan(req, used);
    const char* st = r.status == RunStatus::Succeeded ? "succeeded" : r.status == RunStatus::Cancelled ? "cancelled" : "failed";
    std::printf("STATUS %s %s\n", st, used.c_str());
    std::fflush(stdout);
    if (r.status == RunStatus::Succeeded) return 0;
    if (r.status == RunStatus::Cancelled) return 3;
    return 1;
}

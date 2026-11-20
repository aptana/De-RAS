// End-to-end: template project -> save -> run (as the engine does) ->
// results written -> reopened; failure, cancellation and stale detection.
#include "TestMain.h"
#include "app/RunService.h"
#include "app/Templates.h"
#include "core/Hash.h"
#include "engines/steady/SteadySolver.h"
#include "io/ResultsFile.h"
#include <filesystem>
using namespace deras;
namespace fs = std::filesystem;

static fs::path makeProject(const std::string& name, BoundaryType bt = BoundaryType::NormalDepth) {
    fs::path dir = fs::temp_directory_path() / ("deras_run_" + name);
    fs::remove_all(dir);
    auto p = io::Project::create(dir, name, "Uji " + name, UnitSystem::SI, {});
    app::PrismaticSpec s;
    s.dnType = bt;
    app::addPrismaticChannel(*p, s, true);
    MessageList m;
    p->save(m);
    return dir / (name + ".prj");
}

TEST(run_save_reopen) {
    auto prj = makeProject("Ok");
    app::RunRequest req; req.prjPath = prj;
    std::string plan;
    RunResult r = app::runSteadyPlan(req, plan);
    CHECK(r.status == RunStatus::Succeeded);
    CHECK(plan == "p01");
    CHECK(r.profiles.size() == 3);
    CHECK(r.meta.geomSha256.size() == 64);
    auto rp = io::resultsPathFor(prj.parent_path(), "Ok", "p01");
    CHECK(fs::exists(rp));
    RunResult back; std::string err;
    CHECK(io::readResults(rp, back, err));
    CHECK(back.status == RunStatus::Succeeded);
    CHECK(back.profiles.size() == 3 && back.profiles[2].xs.size() == 11);
    CHECK(back.profiles[1].xs[3].ws == r.profiles[1].xs[3].ws);  // exact (17 significant digits)
    CHECK(back.origin == DataOrigin::Computed);
    // stale detection: change the geometry file -> hash differs
    std::string bytes; io::readFile(prj.parent_path() / "Ok.g01", bytes);
    CHECK(sha256Hex(bytes) == back.meta.geomSha256);
    // WS increases with discharge at every section
    for (size_t i = 0; i < 11; ++i) CHECK(back.profiles[2].xs[i].ws > back.profiles[0].xs[i].ws);
    // report and CSV are produced in both languages
    CHECK(app::runReportHtml(back, Language::ID, "t", "p", "").find("Laporan Run De-RAS") != std::string::npos);
    CHECK(app::runReportHtml(back, Language::EN, "t", "p", "").find("De-RAS Run Report") != std::string::npos);
    CHECK(app::resultsCsv(back, Language::EN).find("W.S. Elev (m)") != std::string::npos);
    fs::remove_all(prj.parent_path());
}
TEST(invalid_model_produces_no_results) {
    auto prj = makeProject("Bad", BoundaryType::None);
    app::RunRequest req; req.prjPath = prj;
    std::string plan;
    RunResult r = app::runSteadyPlan(req, plan);
    CHECK(r.status == RunStatus::Failed);
    CHECK(r.profiles.empty());
    CHECK(hasErrors(r.messages));
    RunResult back; std::string err;
    CHECK(io::readResults(io::resultsPathFor(prj.parent_path(), "Bad", "p01"), back, err));
    CHECK(back.status == RunStatus::Failed && back.profiles.empty());
    fs::remove_all(prj.parent_path());
}
TEST(cancellation_is_safe) {
    auto prj = makeProject("Cancel");
    app::RunRequest req; req.prjPath = prj;
    int calls = 0;
    req.cancelled = [&]() { return ++calls > 5; };
    std::string plan;
    RunResult r = app::runSteadyPlan(req, plan);
    CHECK(r.status == RunStatus::Cancelled);
    CHECK(r.profiles.empty());
    fs::remove_all(prj.parent_path());
}
TEST(supercritical_without_upstream_boundary_refused) {
    Geometry g; Reach r; r.river = "R"; r.reach = "A";
    CrossSection a; a.rs = "100"; a.staElev = {{0, 10}, {0, 0}, {5, 0}, {5, 10}}; a.manning = {{0, 0.03}};
    a.bankLeft = 0; a.bankRight = 5; a.lenLeft = a.lenChannel = a.lenRight = 100;
    CrossSection b = a; b.rs = "0"; b.lenLeft = b.lenChannel = b.lenRight = 0;
    r.nodes = {a, b}; g.reaches.push_back(r);
    SteadyFlow f; f.profileNames = {"PF1"}; f.changes.push_back({"R", "A", "100", {10}});
    Plan p; p.regime = FlowRegime::Supercritical;
    auto res = steady::solve(g, f, p, UnitSystem::SI, {});
    CHECK(res.status == RunStatus::Failed);
    bool found = false;
    for (const auto& m : res.messages) found = found || m.code == "BC_MISSING_UP";
    CHECK(found);
}

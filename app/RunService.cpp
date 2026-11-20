// De-RAS - app/RunService.cpp
#include "RunService.h"

#include <chrono>
#include <ctime>
#include <sstream>

#include "Version.h"
#include "app/ModelDoctor.h"
#include "core/Hash.h"
#include "engines/steady/SteadySolver.h"
#include "engines/unsteady/UnsteadySolver.h"
#include "io/GeoTiff.h"
#include "io/Project.h"
#include "io/ResultsFile.h"
#include "io/ResultsHdf.h"

namespace deras::app {

std::string utcNowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return b;
}

std::string hostName() {
#ifdef _WIN32
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "other";
#endif
}

static const char* frictionName(FrictionSlopeMethod m) {
    switch (m) {
        case FrictionSlopeMethod::AverageConveyance: return "average-conveyance";
        case FrictionSlopeMethod::AverageFrictionSlope: return "average-friction-slope";
        case FrictionSlopeMethod::GeometricMean: return "geometric-mean";
        case FrictionSlopeMethod::HarmonicMean: return "harmonic-mean";
    }
    return "";
}

RunResult runSteadyPlan(const RunRequest& req, std::string& planExt) {
    RunResult res;
    res.status = RunStatus::Failed;
    res.meta.solverVersion = DERAS_VERSION;
    res.meta.startedUtc = utcNowIso();
    res.meta.host = hostName();
    const auto t0 = std::chrono::steady_clock::now();
    auto emit = [&](const Message& m) {
        res.messages.push_back(m);
        if (req.message) req.message(m);
    };

    auto step = [&](const char* name) { if (req.step) req.step(name); };
    step("preprocess");
    MessageList msgs;
    auto prj = io::Project::open(req.prjPath, msgs);
    for (const auto& m : msgs) emit(m);
    if (!prj) { emit(makeMsg(Severity::Error, "SOLV_FAILED")); return res; }
    planExt = req.planExt.empty() ? prj->info().currentPlan : req.planExt;
    auto pit = prj->plans.find(planExt);
    if (pit == prj->plans.end()) {
        emit(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {prj->baseName() + "." + planExt}));
        emit(makeMsg(Severity::Error, "SOLV_FAILED"));
        return res;
    }
    const Plan& plan = pit->second.model;
    auto git = prj->geometries.find(plan.geomFile);
    auto fit = prj->steadyFlows.find(plan.flowFile);
    Geometry* g = git != prj->geometries.end() ? &git->second.model : nullptr;
    if (g) io::applyGeometrySettings(prj->settings, plan.geomFile, *g);  // gate openings, pumps, breaches stored by De-RAS
    const SteadyFlow* f = fit != prj->steadyFlows.end() ? &fit->second.model : nullptr;

    auto& md = res.meta;
    md.projectFile = io::fromPath(req.prjPath.filename());
    md.planFile = prj->baseName() + "." + planExt;
    md.geomFile = prj->baseName() + "." + plan.geomFile;
    md.flowFile = prj->baseName() + "." + plan.flowFile;
    std::string bytes;
    if (io::readFile(prj->pathFor(plan.geomFile), bytes)) md.geomSha256 = sha256Hex(bytes);
    if (io::readFile(prj->pathFor(plan.flowFile), bytes)) md.flowSha256 = sha256Hex(bytes);
    if (io::readFile(prj->pathFor(planExt), bytes)) md.planSha256 = sha256Hex(bytes);
    const auto K = constantsFor(prj->units());
    md.units = prj->units();
    md.gravity = K.gravity;
    md.manningK = K.manningK;
    md.stdStepTol = plan.stdStepTol;
    md.criticalTol = plan.criticalTol;
    md.maxErrorTol = plan.maxErrorTol;
    md.maxTrials = plan.maxTrials;
    md.frictionMethod = frictionName(plan.frictionMethod);

    if (plan.isUnsteady()) {
        md.solverName = "deras-unsteady";
        auto uit = prj->unsteadyFlows.find(plan.flowFile);
        const UnsteadyFlow* uf = uit != prj->unsteadyFlows.end() ? &uit->second.model : nullptr;
        auto check = checkUnsteadyRun(plan, g, uf, prj->units());
        for (const auto& m : check) emit(m);
        if (hasErrors(check)) {
            emit(makeMsg(Severity::Error, "SOLV_MODEL_INVALID"));
            res.status = RunStatus::Failed;
        } else {
            step("simulate");
            unsteady::Callbacks cb;
            cb.cancelled = req.cancelled;
            cb.progress = req.progress;
            // terrains of RAS Mapper (2D areas and storage areas without a volume table)
            std::vector<io::Raster> terrains;
            bool needTerrain = false;
            for (const auto& a : g->storageAreas) needTerrain = needTerrain || a.is2D || (a.volElev.size() < 2 && !(a.area > 0));
            if (needTerrain)
                for (const auto& t : prj->settings.terrains) {
                    io::fs::path tp = io::toPath(t);
                    if (tp.is_relative()) tp = prj->dir() / tp;
                    io::Raster ras;
                    std::string terr;
                    if (io::readGeoTiff(tp, ras, terr)) terrains.push_back(std::move(ras));
                    else emit(makeMsg(Severity::Warning, "RUN_TERRAIN_UNREAD", {t, terr}));
                }
            unsteady::Inputs uin;
            uin.terrains = &terrains;
            RunResult r = unsteady::solve(*g, *uf, plan, prj->units(), cb, uin);
            for (const auto& m : r.messages) emit(m);
            res.status = r.status;
            res.meta.method = r.meta.method;
            if (r.status == RunStatus::Succeeded) {
                res.profiles = std::move(r.profiles);
                res.series = std::move(r.series);
                res.extra = std::move(r.extra);
                res.grids = std::move(r.grids);
            } else if (r.status == RunStatus::Failed) emit(makeMsg(Severity::Error, "SOLV_FAILED"));
        }
    } else {
    auto check = checkSteadyRun(plan, g, f, prj->units());
    for (const auto& m : check) emit(m);
    if (hasErrors(check)) {
        emit(makeMsg(Severity::Error, "SOLV_MODEL_INVALID"));
        res.status = RunStatus::Failed;
    } else {
        step("simulate");
        steady::Callbacks cb;
        cb.cancelled = req.cancelled;
        cb.progress = req.progress;
        RunResult r = steady::solve(*g, *f, plan, prj->units(), cb);
        for (const auto& m : r.messages) emit(m);
        res.status = r.status;
        res.meta.method = r.meta.method;
        if (r.status == RunStatus::Succeeded) res.profiles = std::move(r.profiles);
        else if (r.status == RunStatus::Failed) emit(makeMsg(Severity::Error, "SOLV_FAILED"));
    }
    }
    step("write");
    md.finishedUtc = utcNowIso();
    md.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::string err;
    if (res.status == RunStatus::Succeeded && res.isUnsteady()) {
        emit(makeMsg(Severity::Info, "RES_HDF_UNSTEADY_LATER"));
    } else if (res.status == RunStatus::Succeeded) {
        // results also in the HEC-RAS style .p##.hdf (never over a file written by another program)
        io::fs::path hp = io::hdfResultsPathFor(prj->dir(), prj->baseName(), planExt);
        if (!io::hdfWritableByDeras(hp)) {
            io::fs::path alt = prj->dir() / io::toPath(prj->baseName() + "." + planExt + ".deras.hdf");
            emit(makeMsg(Severity::Warning, "RES_HDF_KEPT", {io::fromPath(hp.filename()), io::fromPath(alt.filename())}));
            hp = alt;
        }
        std::string herr;
        if (io::writeSteadyHdf(hp, res, plan, prj->units(), DERAS_VERSION, herr))
            emit(makeMsg(Severity::Info, "RES_HDF_WRITTEN", {io::fromPath(hp.filename())}));
        else
            emit(makeMsg(Severity::Warning, "RES_HDF_FAILED", {herr}));
    }
    io::writeResults(io::resultsPathFor(prj->dir(), prj->baseName(), planExt), res, err);
    {
        const auto gp = io::grid2dPathFor(prj->dir(), prj->baseName(), planExt);
        std::error_code ec;
        if (!res.grids.empty()) {
            std::string gerr;
            if (!io::writeGrids2D(gp, res.grids, gerr)) emit(makeMsg(Severity::Warning, "RES_2D_FAILED", {gerr}));
        } else if (io::fs::exists(gp, ec)) {
            io::fs::remove(gp, ec);  // stale 2D results of an earlier run of this plan
        }
    }
    // plain-text log next to the results
    std::ostringstream log;
    log << "De-RAS " << DERAS_VERSION << " / " << md.solverName << "\n";
    log << "Plan: " << md.planFile << "  Status: " << runStatusLabel(res.status, Language::EN) << "\n";
    for (const auto& m : res.messages)
        log << severityLabel(m.severity, Language::EN) << ": " << m.text(Language::EN) << "\n";
    io::writeFileAtomic(io::logPathFor(prj->dir(), prj->baseName(), planExt), log.str(), err);
    return res;
}

static std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string o = "\"";
    for (char c : s) { if (c == '"') o += '"'; o += c; }
    return o + "\"";
}

std::string resultsCsv(const RunResult& r, Language lang) {
    const auto u = r.meta.units;
    const std::string L = lengthUnit(u), Q = flowUnit(u), A = areaUnit(u), V = velocityUnit(u);
    const bool id = lang == Language::ID;
    std::ostringstream o;
    o << (id ? "# Asal data: " : "# Data origin: ") << originLabel(r.origin, lang) << "\n";
    o << (id ? "Profil" : "Profile") << ",River,Reach,RS," << (id ? "Debit" : "Q Total") << " (" << Q << "),"
      << "Min Ch El (" << L << "),W.S. Elev (" << L << "),Crit W.S. (" << L << "),E.G. Elev (" << L << "),"
      << "E.G. Slope (" << L << "/" << L << "),Vel Chnl (" << V << "),Vel Total (" << V << "),"
      << "Flow Area (" << A << "),Top Width (" << L << "),Froude # Chl,Alpha,Q Left (" << Q << "),Q Channel (" << Q
      << "),Q Right (" << Q << ")," << (id ? "Kritis" : "Critical") << "\n";
    for (const auto& p : r.profiles)
        for (const auto& x : p.xs) {
            o << csvEscape(p.name) << "," << csvEscape(x.river) << "," << csvEscape(x.reach) << "," << csvEscape(x.rs) << ","
              << fmt(x.q, 3) << "," << fmt(x.minChEl, 3) << "," << fmt(x.ws, 3) << "," << fmt(x.critWs, 3) << ","
              << fmt(x.eg, 3) << "," << fmt(x.egSlope, 7) << "," << fmt(x.velCh, 3) << "," << fmt(x.velTotal, 3) << ","
              << fmt(x.area, 3) << "," << fmt(x.topWidth, 3) << "," << fmt(x.froudeCh, 3) << "," << fmt(x.alpha, 3) << ","
              << fmt(x.qLob, 3) << "," << fmt(x.qCh, 3) << "," << fmt(x.qRob, 3) << "," << (x.critical ? "1" : "0") << "\n";
        }
    return o.str();
}

static std::string html(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '&') o += "&amp;";
        else if (c == '"') o += "&quot;";
        else o += c;
    }
    return o;
}

std::string runReportHtml(const RunResult& r, Language lang, const std::string& projectTitle, const std::string& planTitle,
                          const std::string& logo) {
    const bool id = lang == Language::ID;
    const auto u = r.meta.units;
    std::ostringstream o;
    o << "<!DOCTYPE html><html lang=\"" << (id ? "id" : "en") << "\"><head><meta charset=\"utf-8\"><title>De-RAS "
      << (id ? "Laporan Run" : "Run Report") << "</title><style>"
      << "body{font-family:'Segoe UI Variable','Segoe UI',Inter,Arial,sans-serif;font-size:14px;color:#1A2433;margin:32px;}"
      << "h1{color:#01215F;font-size:22px;margin:0}h2{color:#01215F;font-size:17px;border-bottom:1px solid #DCE4F0;padding-bottom:4px;margin-top:28px}"
      << "table{border-collapse:collapse;font-size:12.5px}td,th{border:1px solid #DCE4F0;padding:4px 7px;text-align:right}"
      << "th{background:#F5F8FC;color:#01215F}td.l,th.l{text-align:left}.hdr{display:flex;gap:16px;align-items:center}"
      << ".muted{color:#5B6B82}.ok{color:#2E9E6B}.warn{color:#8a6400}.err{color:#D64545}.badge{display:inline-block;padding:2px 8px;border-radius:6px;background:#F5F8FC;border:1px solid #DCE4F0}"
      << "</style></head><body>";
    o << "<div class=\"hdr\">";
    if (!logo.empty()) o << "<img src=\"" << logo << "\" width=\"56\" height=\"56\" alt=\"De-RAS\">";
    o << "<div><h1>" << (id ? "Laporan Run De-RAS" : "De-RAS Run Report") << "</h1><div class=\"muted\">"
      << html(projectTitle) << " &middot; " << html(planTitle) << "</div></div></div>";
    const char* stCls = r.status == RunStatus::Succeeded ? "ok" : (r.status == RunStatus::Cancelled ? "warn" : "err");
    o << "<h2>" << (id ? "Ringkasan" : "Summary") << "</h2><table>";
    auto row = [&](const std::string& k, const std::string& v) { o << "<tr><th class=l>" << k << "</th><td class=l>" << v << "</td></tr>"; };
    row(id ? "Status" : "Status", std::string("<span class=") + stCls + ">" + runStatusLabel(r.status, lang) + "</span>");
    row(id ? "Asal data" : "Data origin", originLabel(r.origin, lang));
    row(id ? "Metode" : "Method", id ? "Standard step, aliran steady subkritis 1D" : "Standard step, 1D subcritical steady flow");
    row(id ? "Versi solver" : "Solver version", html(r.meta.solverName + " " + r.meta.solverVersion));
    row(id ? "Mulai / selesai (UTC)" : "Started / finished (UTC)", html(r.meta.startedUtc + " / " + r.meta.finishedUtc));
    row(id ? "Satuan" : "Units", u == UnitSystem::SI ? "SI (m, m3/s)" : "US Customary (ft, cfs)");
    row(id ? "Toleransi standard step" : "Standard step tolerance", fmt(r.meta.stdStepTol, 4) + " " + lengthUnit(u));
    row(id ? "Maks. iterasi" : "Max trials", std::to_string(r.meta.maxTrials));
    row(id ? "Metode kemiringan gesek" : "Friction slope method", html(r.meta.frictionMethod));
    row("g", fmt(r.meta.gravity, 3));
    row(id ? "File geometri (SHA-256)" : "Geometry file (SHA-256)", html(r.meta.geomFile) + "<br><span class=muted>" + r.meta.geomSha256 + "</span>");
    row(id ? "File aliran (SHA-256)" : "Flow file (SHA-256)", html(r.meta.flowFile) + "<br><span class=muted>" + r.meta.flowSha256 + "</span>");
    row(id ? "File plan (SHA-256)" : "Plan file (SHA-256)", html(r.meta.planFile) + "<br><span class=muted>" + r.meta.planSha256 + "</span>");
    o << "</table>";
    o << "<h2>" << (id ? "Status validasi" : "Validation status") << "</h2><p>"
      << (id ? "Metode ini lolos uji unit dan benchmark analitis De-RAS (lihat docs/validation). "
               "Kesetaraan numerik dengan HEC-RAS <b>belum divalidasi</b>; jangan anggap hasil ini sama dengan HEC-RAS."
             : "This method passes De-RAS unit tests and analytical benchmarks (see docs/validation). "
               "Numerical equivalence with HEC-RAS has <b>not been validated</b>; do not treat these results as identical to HEC-RAS.")
      << "</p>";
    int nw = 0, ne = 0;
    for (const auto& m : r.messages) { if (m.severity == Severity::Warning) ++nw; if (m.severity == Severity::Error) ++ne; }
    o << "<h2>" << (id ? "Peringatan dan kesalahan" : "Warnings and errors") << " <span class=badge>" << ne << " / " << nw << "</span></h2><ul>";
    for (const auto& m : r.messages) {
        if (m.severity == Severity::Info) continue;
        o << "<li class=" << (m.severity == Severity::Error ? "err" : "warn") << ">" << severityLabel(m.severity, lang) << ": " << html(m.text(lang)) << "</li>";
    }
    if (ne + nw == 0) o << "<li class=ok>" << (id ? "Tidak ada." : "None.") << "</li>";
    o << "</ul>";
    if (r.status == RunStatus::Succeeded) {
        const std::string L = lengthUnit(u), Q = flowUnit(u), V = velocityUnit(u), A = areaUnit(u);
        for (const auto& p : r.profiles) {
            o << "<h2>" << (id ? "Hasil profil " : "Results, profile ") << html(p.name) << "</h2><table><tr>"
              << "<th class=l>River / Reach</th><th>RS</th><th>Q (" << Q << ")</th><th>Min Ch El (" << L << ")</th><th>W.S. (" << L
              << ")</th><th>Crit W.S. (" << L << ")</th><th>E.G. (" << L << ")</th><th>E.G. Slope</th><th>Vel Chnl (" << V
              << ")</th><th>Area (" << A << ")</th><th>Top W. (" << L << ")</th><th>Froude</th></tr>";
            for (const auto& x : p.xs)
                o << "<tr><td class=l>" << html(x.river + " / " + x.reach) << "</td><td>" << html(x.rs) << (x.critical ? " *" : "") << "</td><td>"
                  << fmt(x.q, 2) << "</td><td>" << fmt(x.minChEl, 2) << "</td><td>" << fmt(x.ws, 3) << "</td><td>" << fmt(x.critWs, 3)
                  << "</td><td>" << fmt(x.eg, 3) << "</td><td>" << fmt(x.egSlope, 6) << "</td><td>" << fmt(x.velCh, 3) << "</td><td>"
                  << fmt(x.area, 2) << "</td><td>" << fmt(x.topWidth, 2) << "</td><td>" << fmt(x.froudeCh, 3) << "</td></tr>";
            o << "</table><p class=muted>* " << (id ? "muka air ditetapkan pada kedalaman kritis" : "water surface set to critical depth") << "</p>";
        }
    } else {
        o << "<p>" << (id ? "Run tidak berhasil, sehingga tidak ada tabel hasil." : "The run did not succeed, so there is no results table.") << "</p>";
    }
    o << "<p class=muted>" << (id ? "Dibuat oleh De-RAS " : "Generated by De-RAS ") << DERAS_VERSION
      << (id ? ". De-RAS tidak berafiliasi dengan HEC atau USACE." : ". De-RAS is not affiliated with HEC or USACE.") << "</p>";
    o << "</body></html>";
    return o.str();
}

}  // namespace deras::app

// De-RAS - io/ResultsFile.cpp
#include "ResultsFile.h"

#include <cmath>
#include <cstring>

namespace deras::io {

fs::path resultsPathFor(const fs::path& dir, const std::string& base, const std::string& planExt) {
    return dir / toPath(base + "." + planExt + ".deras-results.json");
}

fs::path grid2dPathFor(const fs::path& dir, const std::string& base, const std::string& planExt) {
    return dir / toPath(base + "." + planExt + ".deras-2d.bin");
}

namespace {
const char kGridMagic[8] = {'D', 'E', 'R', 'A', 'S', '2', 'D', '1'};
void putI(std::string& o, int64_t v) { o.append(reinterpret_cast<const char*>(&v), 8); }
void putD(std::string& o, double v) { o.append(reinterpret_cast<const char*>(&v), 8); }
void putS(std::string& o, const std::string& s) { putI(o, int64_t(s.size())); o += s; }
template <class T> void putV(std::string& o, const std::vector<T>& v) {
    putI(o, int64_t(v.size()));
    o.append(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(T));
}
struct Reader {
    const std::string& b;
    size_t p = 0;
    bool ok = true;
    bool need(size_t n) { if (p + n > b.size()) ok = false; return ok; }
    int64_t i() { int64_t v = 0; if (need(8)) { std::memcpy(&v, b.data() + p, 8); p += 8; } return v; }
    double d() { double v = 0; if (need(8)) { std::memcpy(&v, b.data() + p, 8); p += 8; } return v; }
    std::string s() { const int64_t n = i(); if (n < 0 || !need(size_t(n))) return {}; std::string r = b.substr(p, size_t(n)); p += size_t(n); return r; }
    template <class T> void v(std::vector<T>& out) {
        const int64_t n = i();
        if (n < 0 || !need(size_t(n) * sizeof(T))) { ok = false; return; }
        out.resize(size_t(n));
        std::memcpy(out.data(), b.data() + p, size_t(n) * sizeof(T));
        p += size_t(n) * sizeof(T);
    }
};
}  // namespace

// Little-endian: magic, grid count, then per grid: name, x0, y0, cell, nx, ny, epsg,
// index[int32], minElev, maxWse, maxDepth, maxVel, arrival [float32], hours [float64],
// then wse[float32] per time.
bool writeGrids2D(const fs::path& path, const std::vector<Grid2DResult>& grids, std::string& error) {
    std::string o(kGridMagic, 8);
    putI(o, int64_t(grids.size()));
    for (const auto& g : grids) {
        putS(o, g.area);
        putD(o, g.x0); putD(o, g.y0); putD(o, g.cell);
        putI(o, g.nx); putI(o, g.ny); putI(o, g.epsg);
        std::vector<int32_t> idx(g.index.begin(), g.index.end());
        putV(o, idx);
        putV(o, g.minElev); putV(o, g.maxWse); putV(o, g.maxDepth); putV(o, g.maxVel); putV(o, g.arrivalHours);
        putV(o, g.hours);
        putI(o, int64_t(g.wse.size()));
        for (const auto& w : g.wse) putV(o, w);
    }
    return writeFileAtomic(path, o, error);
}

bool readGrids2D(const fs::path& path, std::vector<Grid2DResult>& grids, std::string& error) {
    std::string bytes;
    if (!readFile(path, bytes)) { error = "cannot read " + fromPath(path); return false; }
    if (bytes.size() < 16 || std::memcmp(bytes.data(), kGridMagic, 8) != 0) { error = "not a De-RAS 2D results file"; return false; }
    Reader r{bytes, 8};
    const int64_t n = r.i();
    grids.clear();
    for (int64_t k = 0; k < n && r.ok; ++k) {
        Grid2DResult g;
        g.area = r.s();
        g.x0 = r.d(); g.y0 = r.d(); g.cell = r.d();
        g.nx = int(r.i()); g.ny = int(r.i()); g.epsg = int(r.i());
        std::vector<int32_t> idx;
        r.v(idx);
        g.index.assign(idx.begin(), idx.end());
        r.v(g.minElev); r.v(g.maxWse); r.v(g.maxDepth); r.v(g.maxVel); r.v(g.arrivalHours);
        r.v(g.hours);
        const int64_t nt = r.i();
        for (int64_t t = 0; t < nt && r.ok; ++t) { g.wse.emplace_back(); r.v(g.wse.back()); }
        grids.push_back(std::move(g));
    }
    if (!r.ok) { error = "truncated 2D results file"; grids.clear(); return false; }
    return true;
}

fs::path logPathFor(const fs::path& dir, const std::string& base, const std::string& planExt) {
    return dir / toPath(base + "." + planExt + ".deras-log.txt");
}

static const char* sevName(Severity s) {
    return s == Severity::Error ? "error" : s == Severity::Warning ? "warning" : "info";
}
static Severity sevFrom(const std::string& s) {
    return s == "error" ? Severity::Error : s == "warning" ? Severity::Warning : Severity::Info;
}
static const char* statusName(RunStatus s) {
    switch (s) {
        case RunStatus::Succeeded: return "succeeded";
        case RunStatus::Failed: return "failed";
        case RunStatus::Cancelled: return "cancelled";
        default: return "not-run";
    }
}
static RunStatus statusFrom(const std::string& s) {
    if (s == "succeeded") return RunStatus::Succeeded;
    if (s == "failed") return RunStatus::Failed;
    if (s == "cancelled") return RunStatus::Cancelled;
    return RunStatus::NotRun;
}

json::Value messageToJson(const Message& m) {
    json::Value v = json::Value::object();
    v["severity"] = sevName(m.severity);
    v["code"] = m.code;
    json::Value a = json::Value::array();
    for (const auto& s : m.args) a.push(s);
    v["args"] = a;
    v["location"] = m.location;
    v["text_id"] = m.text(Language::ID);
    v["text_en"] = m.text(Language::EN);
    return v;
}

Message messageFromJson(const json::Value& v) {
    Message m;
    m.severity = sevFrom(v.get("severity").str());
    m.code = v.get("code").str();
    for (const auto& a : v.get("args").items()) m.args.push_back(a.str());
    m.location = v.get("location").str();
    return m;
}

json::Value resultsToJson(const RunResult& r) {
    json::Value root = json::Value::object();
    root["format"] = "De-RAS steady results";
    root["format_version"] = 1;
    root["origin"] = r.origin == DataOrigin::Computed ? "computed-by-de-ras" : "other";
    root["status"] = statusName(r.status);
    const auto& m = r.meta;
    json::Value meta = json::Value::object();
    meta["solver"] = m.solverName;
    meta["solver_version"] = m.solverVersion;
    meta["method"] = m.method;
    meta["started_utc"] = m.startedUtc;
    meta["finished_utc"] = m.finishedUtc;
    meta["elapsed_s"] = m.elapsedSeconds;
    meta["project_file"] = m.projectFile;
    meta["plan_file"] = m.planFile;
    meta["geometry_file"] = m.geomFile;
    meta["flow_file"] = m.flowFile;
    meta["geometry_sha256"] = m.geomSha256;
    meta["flow_sha256"] = m.flowSha256;
    meta["plan_sha256"] = m.planSha256;
    meta["units"] = unitSystemName(m.units);
    meta["gravity"] = m.gravity;
    meta["manning_k"] = m.manningK;
    meta["std_step_tol"] = m.stdStepTol;
    meta["critical_tol"] = m.criticalTol;
    meta["max_error_tol"] = m.maxErrorTol;
    meta["max_trials"] = m.maxTrials;
    meta["friction_slope_method"] = m.frictionMethod;
    meta["host"] = m.host;
    root["metadata"] = meta;
    json::Value msgs = json::Value::array();
    for (const auto& x : r.messages) msgs.push(messageToJson(x));
    root["messages"] = msgs;
    json::Value profs = json::Value::array();
    for (const auto& p : r.profiles) {
        json::Value pv = json::Value::object();
        pv["name"] = p.name;
        json::Value xa = json::Value::array();
        for (const auto& x : p.xs) {
            json::Value xv = json::Value::object();
            xv["river"] = x.river;
            xv["reach"] = x.reach;
            xv["rs"] = x.rs;
            xv["q"] = x.q; xv["q_lob"] = x.qLob; xv["q_ch"] = x.qCh; xv["q_rob"] = x.qRob;
            xv["min_ch_el"] = x.minChEl; xv["ws"] = x.ws; xv["crit_ws"] = x.critWs; xv["eg"] = x.eg;
            xv["eg_slope"] = x.egSlope; xv["vel_ch"] = x.velCh; xv["vel_total"] = x.velTotal;
            xv["area"] = x.area; xv["area_lob"] = x.areaLob; xv["area_ch"] = x.areaCh; xv["area_rob"] = x.areaRob;
            xv["top_width"] = x.topWidth; xv["froude_ch"] = x.froudeCh; xv["alpha"] = x.alpha;
            xv["friction_loss"] = x.frictionLoss; xv["ce_loss"] = x.ceLoss; xv["length_weighted"] = x.lengthWeighted;
            xv["trials"] = x.trials; xv["critical"] = x.critical; xv["channel_distance"] = x.channelDistance;
            json::Value notes = json::Value::array();
            for (const auto& n : x.notes) notes.push(messageToJson(n));
            xv["notes"] = notes;
            xa.push(xv);
        }
        pv["cross_sections"] = xa;
        profs.push(pv);
    }
    root["profiles"] = profs;
    if (!r.series.empty()) {
        json::Value ts = json::Value::object();
        ts["start_seconds"] = r.series.startSeconds;
        json::Value h = json::Value::array();
        for (double x : r.series.hours) h.push(x);
        ts["hours"] = h;
        auto mat = [](const std::vector<std::vector<double>>& m) {
            json::Value a = json::Value::array();
            for (const auto& row : m) {
                json::Value rv = json::Value::array();
                for (double x : row) rv.push(std::round(x * 10000) / 10000);
                a.push(rv);
            }
            return a;
        };
        ts["ws"] = mat(r.series.ws);
        ts["q"] = mat(r.series.q);
        root["time_series"] = ts;
    }
    if (!r.extra.empty()) {
        json::Value ex = json::Value::array();
        for (const auto& e : r.extra) {
            json::Value ev = json::Value::object();
            ev["kind"] = e.kind;
            ev["name"] = e.name;
            ev["quantity"] = e.quantity;
            ev["max"] = std::round(e.maxValue * 10000) / 10000;
            json::Value vals = json::Value::array();
            for (double x : e.values) vals.push(std::round(x * 10000) / 10000);
            ev["values"] = vals;
            ex.push(ev);
        }
        root["extra_series"] = ex;
    }
    if (!r.grids.empty()) {
        json::Value gs = json::Value::array();
        for (const auto& g : r.grids) gs.push(g.area);
        root["areas_2d"] = gs;  // grids are in <base>.<plan>.deras-2d.bin
    }
    return root;
}

bool resultsFromJson(const json::Value& v, RunResult& r, std::string& error) {
    if (v.get("format").str() != "De-RAS steady results") { error = "not a De-RAS results file"; return false; }
    r = RunResult{};
    r.status = statusFrom(v.get("status").str());
    r.origin = v.get("origin").str() == "computed-by-de-ras" ? DataOrigin::Computed : DataOrigin::Imported;
    const auto& m = v.get("metadata");
    auto& md = r.meta;
    md.solverName = m.get("solver").str();
    md.solverVersion = m.get("solver_version").str();
    md.method = m.get("method").str();
    md.startedUtc = m.get("started_utc").str();
    md.finishedUtc = m.get("finished_utc").str();
    md.elapsedSeconds = m.get("elapsed_s").num();
    md.projectFile = m.get("project_file").str();
    md.planFile = m.get("plan_file").str();
    md.geomFile = m.get("geometry_file").str();
    md.flowFile = m.get("flow_file").str();
    md.geomSha256 = m.get("geometry_sha256").str();
    md.flowSha256 = m.get("flow_sha256").str();
    md.planSha256 = m.get("plan_sha256").str();
    md.units = m.get("units").str() == "SI" ? UnitSystem::SI : UnitSystem::USCustomary;
    md.gravity = m.get("gravity").num();
    md.manningK = m.get("manning_k").num();
    md.stdStepTol = m.get("std_step_tol").num();
    md.criticalTol = m.get("critical_tol").num();
    md.maxErrorTol = m.get("max_error_tol").num();
    md.maxTrials = static_cast<int>(m.get("max_trials").num());
    md.frictionMethod = m.get("friction_slope_method").str();
    md.host = m.get("host").str();
    for (const auto& x : v.get("messages").items()) r.messages.push_back(messageFromJson(x));
    for (const auto& pv : v.get("profiles").items()) {
        ProfileResult p;
        p.name = pv.get("name").str();
        for (const auto& xv : pv.get("cross_sections").items()) {
            XsResult x;
            x.river = xv.get("river").str();
            x.reach = xv.get("reach").str();
            x.rs = xv.get("rs").str();
            x.q = xv.get("q").num(); x.qLob = xv.get("q_lob").num(); x.qCh = xv.get("q_ch").num(); x.qRob = xv.get("q_rob").num();
            x.minChEl = xv.get("min_ch_el").num(); x.ws = xv.get("ws").num(); x.critWs = xv.get("crit_ws").num(); x.eg = xv.get("eg").num();
            x.egSlope = xv.get("eg_slope").num(); x.velCh = xv.get("vel_ch").num(); x.velTotal = xv.get("vel_total").num();
            x.area = xv.get("area").num(); x.areaLob = xv.get("area_lob").num(); x.areaCh = xv.get("area_ch").num(); x.areaRob = xv.get("area_rob").num();
            x.topWidth = xv.get("top_width").num(); x.froudeCh = xv.get("froude_ch").num(); x.alpha = xv.get("alpha").num();
            x.frictionLoss = xv.get("friction_loss").num(); x.ceLoss = xv.get("ce_loss").num(); x.lengthWeighted = xv.get("length_weighted").num();
            x.trials = static_cast<int>(xv.get("trials").num()); x.critical = xv.get("critical").boolean();
            x.channelDistance = xv.get("channel_distance").num();
            for (const auto& n : xv.get("notes").items()) x.notes.push_back(messageFromJson(n));
            p.xs.push_back(x);
        }
        r.profiles.push_back(p);
    }
    if (v.has("time_series")) {
        const auto& ts = v.get("time_series");
        r.series.startSeconds = ts.get("start_seconds").num();
        for (const auto& x : ts.get("hours").items()) r.series.hours.push_back(x.num());
        auto mat = [](const json::Value& a, std::vector<std::vector<double>>& m) {
            for (const auto& row : a.items()) {
                m.emplace_back();
                for (const auto& x : row.items()) m.back().push_back(x.num());
            }
        };
        mat(ts.get("ws"), r.series.ws);
        mat(ts.get("q"), r.series.q);
    }
    for (const auto& ev : v.get("extra_series").items()) {
        ExtraSeries e;
        e.kind = ev.get("kind").str();
        e.name = ev.get("name").str();
        e.quantity = ev.get("quantity").str();
        e.maxValue = ev.get("max").num();
        for (const auto& x : ev.get("values").items()) e.values.push_back(x.num());
        r.extra.push_back(std::move(e));
    }
    return true;
}

bool writeResults(const fs::path& path, const RunResult& r, std::string& error) {
    return writeFileAtomic(path, resultsToJson(r).dump(1) + "\n", error);
}

bool readResults(const fs::path& path, RunResult& r, std::string& error) {
    std::string bytes;
    if (!readFile(path, bytes)) { error = "cannot read " + fromPath(path); return false; }
    json::Value v;
    if (!json::parse(bytes, v, error)) return false;
    if (!resultsFromJson(v, r, error)) return false;
    if (v.has("areas_2d")) {
        std::string name = fromPath(path.filename());
        const std::string suffix = ".deras-results.json";
        if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            name = name.substr(0, name.size() - suffix.size()) + ".deras-2d.bin";
            std::string e2;
            readGrids2D(path.parent_path() / toPath(name), r.grids, e2);
        }
    }
    return true;
}

}  // namespace deras::io

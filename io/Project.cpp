// De-RAS - io/Project.cpp
#include "Project.h"

#include <cstdio>
#include <set>
#include <system_error>

namespace deras::io {

static std::string modulesText(const ProjectSettings& s) {
    std::string t;
    for (const auto& [k, v] : s.modules) t += k + "=" + v.dump(0) + ";";
    return t;
}

bool sameSettings(const ProjectSettings& a, const ProjectSettings& b) {
    return a.coordinateSystem == b.coordinateSystem && a.timeZone == b.timeZone && a.notes == b.notes &&
           a.targetRasVersion == b.targetRasVersion && a.terrains == b.terrains && a.pictures == b.pictures &&
           a.gateOpenings == b.gateOpenings && modulesText(a) == modulesText(b);
}

std::string gateKey(const std::string& river, const std::string& reach, const std::string& rs, const std::string& gate) {
    return river + "|" + reach + "|" + rs + "|" + gate;
}

void applyGateOpenings(const ProjectSettings& s, Geometry& g) {
    for (auto& r : g.reaches)
        for (auto& n : r.nodes)
            for (auto& gt : n.structure.gates) {
                auto it = s.gateOpenings.find(gateKey(r.river, r.reach, n.rs, gt.name));
                gt.opening = it != s.gateOpenings.end() ? it->second : -1;
            }
}

std::string lateralKey(const std::string& river, const std::string& reach, const std::string& rs) {
    return "lat|" + river + "|" + reach + "|" + rs;
}
std::string connectionKey(const std::string& name) { return "conn|" + name; }

static json::Value endJson(const PumpEnd& e) {
    json::Value v = json::Value::object();
    v["kind"] = e.kind;
    if (!e.area.empty()) v["area"] = e.area;
    if (!e.river.empty()) { v["river"] = e.river; v["reach"] = e.reach; v["rs"] = e.rs; }
    if (e.point.first != 0 || e.point.second != 0) { v["x"] = e.point.first; v["y"] = e.point.second; }
    return v;
}
static PumpEnd endFrom(const json::Value& v) {
    PumpEnd e;
    e.kind = int(v.get("kind").num(1));
    e.area = v.get("area").str();
    e.river = v.get("river").str();
    e.reach = v.get("reach").str();
    e.rs = v.get("rs").str();
    e.point = {v.get("x").num(0), v.get("y").num(0)};
    return e;
}

std::vector<PumpStation> pumpsFor(const ProjectSettings& s, const std::string& geomExt) {
    std::vector<PumpStation> out;
    auto it = s.modules.find("pumps");
    if (it == s.modules.end()) return out;
    for (const auto& p : it->second.get(geomExt).items()) {
        PumpStation ps;
        ps.name = p.get("name").str();
        ps.from = endFrom(p.get("from"));
        ps.to = endFrom(p.get("to"));
        for (const auto& g : p.get("groups").items()) {
            PumpGroup pg;
            pg.name = g.get("name").str();
            pg.count = int(g.get("count").num(1));
            pg.capacity = g.get("capacity").num(0);
            pg.onElev = g.get("on").num(0);
            pg.offElev = g.get("off").num(0);
            ps.groups.push_back(pg);
        }
        out.push_back(ps);
    }
    return out;
}

void setPumps(ProjectSettings& s, const std::string& geomExt, const std::vector<PumpStation>& pumps) {
    json::Value all = s.modules.count("pumps") ? s.modules["pumps"] : json::Value::object();
    json::Value arr = json::Value::array();
    for (const auto& p : pumps) {
        json::Value v = json::Value::object();
        v["name"] = p.name;
        v["from"] = endJson(p.from);
        v["to"] = endJson(p.to);
        json::Value gs = json::Value::array();
        for (const auto& g : p.groups) {
            json::Value gv = json::Value::object();
            gv["name"] = g.name;
            gv["count"] = g.count;
            gv["capacity"] = g.capacity;
            gv["on"] = g.onElev;
            gv["off"] = g.offElev;
            gs.push(gv);
        }
        v["groups"] = gs;
        arr.push(v);
    }
    // rebuild the object without the old entry (keeps the order of the others)
    json::Value out = json::Value::object();
    for (const auto& m : all.members()) if (m.first != geomExt) out[m.first] = m.second;
    if (!pumps.empty()) out[geomExt] = arr;
    if (out.size() == 0) s.modules.erase("pumps");
    else s.modules["pumps"] = out;
}

Breach breachFor(const ProjectSettings& s, const std::string& key) {
    Breach b;
    auto it = s.modules.find("breaches");
    if (it == s.modules.end() || !it->second.has(key)) return b;
    const auto& v = it->second.get(key);
    b.active = v.get("active").boolean(false);
    b.station = v.get("station").num(0);
    b.bottomElev = v.get("bottom_elev").num(0);
    b.bottomWidth = v.get("bottom_width").num(0);
    b.sideSlope = v.get("side_slope").num(1);
    b.startHours = v.get("start_hours").num(0);
    b.formHours = v.get("form_hours").num(1);
    return b;
}

void setBreach(ProjectSettings& s, const std::string& key, const Breach& b) {
    json::Value all = s.modules.count("breaches") ? s.modules["breaches"] : json::Value::object();
    json::Value out = json::Value::object();
    for (const auto& m : all.members()) if (m.first != key) out[m.first] = m.second;
    if (b.active) {
        json::Value v = json::Value::object();
        v["active"] = true;
        v["station"] = b.station;
        v["bottom_elev"] = b.bottomElev;
        v["bottom_width"] = b.bottomWidth;
        v["side_slope"] = b.sideSlope;
        v["start_hours"] = b.startHours;
        v["form_hours"] = b.formHours;
        out[key] = v;
    }
    if (out.size() == 0) s.modules.erase("breaches");
    else s.modules["breaches"] = out;
}

void applyGeometrySettings(const ProjectSettings& s, const std::string& geomExt, Geometry& g) {
    applyGateOpenings(s, g);
    g.pumps = pumpsFor(s, geomExt);
    for (auto& r : g.reaches)
        for (auto& n : r.nodes)
            if (n.type == NodeType::LateralStructure) n.structure.lateral.breach = breachFor(s, lateralKey(r.river, r.reach, n.rs));
    for (auto& c : g.connections) c.breach = breachFor(s, connectionKey(c.name));
}

std::unique_ptr<Project> Project::open(const fs::path& prjPath, MessageList& msgs) {
    auto p = std::make_unique<Project>();
    p->dir_ = prjPath.parent_path();
    std::string fname = fromPath(prjPath.filename());
    p->base_ = fname.size() > 4 ? fname.substr(0, fname.size() - 4) : fname;
    if (!p->prj_.load(prjPath, msgs)) return nullptr;
    auto& d = p->prj_.data;
    for (const auto& g : d.geomFiles) {
        GeometryFile gf;
        if (!fs::exists(p->pathFor(g))) { msgs.push_back(makeMsg(Severity::Warning, "PRJ_MISSING_FILE", {p->base_ + "." + g})); continue; }
        if (gf.load(p->pathFor(g), msgs)) p->geometries.emplace(g, std::move(gf));
    }
    for (const auto& f : d.flowFiles) {
        SteadyFlowFile ff;
        if (!fs::exists(p->pathFor(f))) { msgs.push_back(makeMsg(Severity::Warning, "PRJ_MISSING_FILE", {p->base_ + "." + f})); continue; }
        if (ff.load(p->pathFor(f), msgs)) p->steadyFlows.emplace(f, std::move(ff));
    }
    for (const auto& u : d.unsteadyFiles) {
        UnsteadyFlowFile uf;
        if (!fs::exists(p->pathFor(u))) { msgs.push_back(makeMsg(Severity::Warning, "PRJ_MISSING_FILE", {p->base_ + "." + u})); continue; }
        if (uf.load(p->pathFor(u), msgs)) p->unsteadyFlows.emplace(u, std::move(uf));
    }
    for (const auto& pl : d.planFiles) {
        PlanFile pf;
        if (!fs::exists(p->pathFor(pl))) { msgs.push_back(makeMsg(Severity::Warning, "PRJ_MISSING_FILE", {p->base_ + "." + pl})); continue; }
        if (pf.load(p->pathFor(pl), msgs)) p->plans.emplace(pl, std::move(pf));
    }
    p->loadSidecar(msgs);
    p->savedSettings_ = p->settings;
    return p;
}

std::unique_ptr<Project> Project::create(const fs::path& dir, const std::string& baseName, const std::string& title,
                                         UnitSystem units, const ProjectSettings& settings) {
    auto p = std::make_unique<Project>();
    p->dir_ = dir;
    p->base_ = baseName;
    p->prj_ = ProjectFile::createNew(title, units);
    p->settings = settings;
    p->sidecarDirty_ = true;
    return p;
}

std::string Project::nextFreeExt(char prefix) const {
    std::set<std::string> used;
    const auto& d = prj_.data;
    auto add = [&](const std::vector<std::string>& v) { for (const auto& e : v) used.insert(e.substr(1)); };
    if (prefix == 'g') add(d.geomFiles);
    else if (prefix == 'p') add(d.planFiles);
    else if (prefix == 'f') add(d.flowFiles);
    else if (prefix == 'u') add(d.unsteadyFiles);
    for (int n = 1; n <= 99; ++n) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "%02d", n);
        std::string ext = std::string(1, prefix) + buf;
        if (used.count(buf)) continue;
        std::error_code ec;
        if (fs::exists(pathFor(ext), ec)) continue;  // never overwrite a stray file
        return ext;
    }
    return {};
}

std::string Project::addGeometry(const std::string& title) {
    std::string ext = nextFreeExt('g');
    if (ext.empty()) return {};
    geometries.emplace(ext, GeometryFile::createNew(title, settings.targetRasVersion));
    prj_.data.geomFiles.push_back(ext);
    return ext;
}

std::string Project::addSteadyFlow(const std::string& title) {
    std::string ext = nextFreeExt('f');
    if (ext.empty()) return {};
    steadyFlows.emplace(ext, SteadyFlowFile::createNew(title, settings.targetRasVersion));
    prj_.data.flowFiles.push_back(ext);
    return ext;
}

std::string Project::addUnsteadyFlow(const std::string& title) {
    std::string ext = nextFreeExt('u');
    if (ext.empty()) return {};
    unsteadyFlows.emplace(ext, UnsteadyFlowFile::createNew(title, settings.targetRasVersion));
    prj_.data.unsteadyFiles.push_back(ext);
    return ext;
}

std::string Project::addPlan(const std::string& title, const std::string& shortId, const std::string& geom,
                             const std::string& flow) {
    std::string ext = nextFreeExt('p');
    if (ext.empty()) return {};
    Plan pl;
    pl.title = title;
    pl.shortId = shortId;
    pl.programVersion = settings.targetRasVersion;
    pl.geomFile = geom;
    pl.flowFile = flow;
    const auto k = constantsFor(units());
    pl.stdStepTol = k.defaultStdStepTol;
    pl.criticalTol = k.defaultCriticalTol;
    pl.maxErrorTol = k.defaultMaxErrorTol;
    plans.emplace(ext, PlanFile::createNew(pl, units()));
    prj_.data.planFiles.push_back(ext);
    if (prj_.data.currentPlan.empty()) prj_.data.currentPlan = ext;
    return ext;
}

bool Project::isModified() const {
    if (prj_.isModified() || sidecarDirty_ || !sameSettings(settings, savedSettings_)) return true;
    for (const auto& [k, g] : geometries) if (g.isModified()) return true;
    for (const auto& [k, f] : steadyFlows) if (f.isModified()) return true;
    for (const auto& [k, f] : unsteadyFlows) if (f.isModified()) return true;
    for (const auto& [k, p] : plans) if (p.isModified()) return true;
    return false;
}

bool Project::save(MessageList& msgs) {
    std::error_code ec;
    fs::create_directories(dir_, ec);
    bool ok = true;
    for (auto& [ext, g] : geometries) if (g.isModified()) ok = g.save(pathFor(ext), msgs) && ok;
    for (auto& [ext, f] : steadyFlows) if (f.isModified()) ok = f.save(pathFor(ext), msgs) && ok;
    for (auto& [ext, f] : unsteadyFlows) if (f.isModified()) ok = f.save(pathFor(ext), msgs) && ok;
    for (auto& [ext, p] : plans) if (p.isModified()) ok = p.save(pathFor(ext), msgs) && ok;
    if (prj_.isModified()) ok = prj_.save(prjPath(), msgs) && ok;
    if (sidecarDirty_ || !sameSettings(settings, savedSettings_)) ok = saveSidecar(msgs) && ok;
    return ok;
}

bool Project::loadSidecar(MessageList& msgs) {
    std::string bytes;
    if (!readFile(sidecarPath(), bytes)) return false;  // optional file
    json::Value v;
    std::string err;
    if (!json::parse(bytes, v, err)) {
        msgs.push_back(makeMsg(Severity::Warning, "PARSE_BAD_NUMBER", {err, base_ + ".deras.json", "-"}));
        return false;
    }
    settingsFromJson(v, settings);
    return true;
}

void Project::settingsFromJson(const json::Value& v, ProjectSettings& settings) {
    settings.coordinateSystem = v.get("coordinate_system").str();
    if (v.has("time_zone")) settings.timeZone = v.get("time_zone").str();
    settings.notes = v.get("notes").str();
    if (v.has("target_ras_version")) settings.targetRasVersion = v.get("target_ras_version").str();
    settings.terrains.clear();
    settings.pictures.clear();
    for (const auto& t : v.get("terrains").items()) if (!t.str().empty()) settings.terrains.push_back(t.str());
    for (const auto& t : v.get("pictures").items()) if (!t.str().empty()) settings.pictures.push_back(t.str());
    settings.gateOpenings.clear();
    for (const auto& m : v.get("gate_openings").members())
        if (m.second.isNumber()) settings.gateOpenings[m.first] = m.second.num();
    settings.modules.clear();
    for (const auto& m : v.get("modules").members()) settings.modules[m.first] = m.second;
}

std::string Project::sidecarText() const {
    json::Value v = json::Value::object();
    v["format"] = "De-RAS sidecar";
    v["format_version"] = 1;
    v["note"] = "De-RAS settings for this HEC-RAS project. HEC-RAS ignores this file.";
    v["coordinate_system"] = settings.coordinateSystem;
    v["time_zone"] = settings.timeZone;
    v["target_ras_version"] = settings.targetRasVersion;
    v["notes"] = settings.notes;
    if (!settings.terrains.empty() || !settings.pictures.empty()) {
        json::Value t = json::Value::array(), pc = json::Value::array();
        for (const auto& s : settings.terrains) t.push(json::Value(s));
        for (const auto& s : settings.pictures) pc.push(json::Value(s));
        v["terrains"] = t;
        v["pictures"] = pc;
    }
    if (!settings.gateOpenings.empty()) {
        json::Value o = json::Value::object();
        for (const auto& [k, x] : settings.gateOpenings) o[k] = x;
        v["gate_openings"] = o;
    }
    if (!settings.modules.empty()) {
        json::Value o = json::Value::object();
        for (const auto& [k, x] : settings.modules) o[k] = x;
        v["modules"] = o;
    }
    return v.dump(2) + "\n";
}

bool Project::saveSidecar(MessageList& msgs) {
    std::string err;
    if (!writeFileAtomic(sidecarPath(), sidecarText(), err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    sidecarDirty_ = false;
    savedSettings_ = settings;
    return true;
}

bool Project::writeRecovery(const fs::path& dir, MessageList& msgs) const {
    std::error_code ec;
    fs::create_directories(dir, ec);
    bool ok = true;
    std::string err;
    json::Value files = json::Value::array();
    auto put = [&](const std::string& name, const std::string& text) {
        if (!writeFileAtomic(dir / toPath(name), text, err)) {
            msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
            ok = false;
        }
        files.push(json::Value(name));
    };
    for (const auto& [ext, g] : geometries) if (g.isModified()) put(base_ + "." + ext, g.serialize());
    for (const auto& [ext, f] : steadyFlows) if (f.isModified()) put(base_ + "." + ext, f.serialize());
    for (const auto& [ext, f] : unsteadyFlows) if (f.isModified()) put(base_ + "." + ext, f.serialize());
    for (const auto& [ext, p] : plans) if (p.isModified()) put(base_ + "." + ext, p.serialize());
    if (prj_.isModified()) put(base_ + ".prj", prj_.serialize());
    if (sidecarDirty_ || !sameSettings(settings, savedSettings_)) put(base_ + ".deras.json", sidecarText());
    json::Value r = json::Value::object();
    r["format"] = "deras-recovery";
    r["project"] = fromPath(prjPath());
    r["base"] = base_;
    r["files"] = files;
    if (!writeFileAtomic(dir / "recovery.json", r.dump(2), err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        ok = false;
    }
    return ok;
}

namespace {
// Recovered models get the uids of the matching reaches and nodes of the open
// file, so saving merges them into the original text like a normal edit.
void adoptUids(Geometry& rec, const Geometry& cur) {
    for (auto& r : rec.reaches) {
        const Reach* c = cur.findReach(r.river, r.reach);
        if (!c) continue;
        r.uid = c->uid;
        for (auto& n : r.nodes)
            for (const auto& cn : c->nodes)
                if (cn.rs == n.rs && cn.type == n.type) { n.uid = cn.uid; break; }
    }
}
}  // namespace

bool Project::applyRecovery(const fs::path& dir, MessageList& msgs) {
    std::string bytes, err;
    json::Value r;
    if (!readFile(dir / "recovery.json", bytes) || !json::parse(bytes, r, err) || r.get("format").str() != "deras-recovery") return false;
    if (r.get("base").str() != base_) return false;
    for (const auto& f : r.get("files").items()) {
        const std::string name = f.str();
        const fs::path path = dir / toPath(name);
        if (name.size() <= base_.size() + 1) continue;
        const std::string ext = name.substr(base_.size() + 1);
        if (ext == "prj") {
            ProjectFile t;
            if (t.load(path, msgs)) prj_.data = t.data;
        } else if (ext == "deras.json") {
            json::Value v;
            if (readFile(path, bytes) && json::parse(bytes, v, err)) settingsFromJson(v, settings);
        } else if (ext.size() == 3 && ext[0] == 'g') {
            GeometryFile t;
            if (!t.load(path, msgs)) continue;
            auto it = geometries.find(ext);
            if (it == geometries.end()) {
                GeometryFile n = GeometryFile::createNew(t.model.title, t.model.programVersion);
                n.model = t.model;
                geometries.emplace(ext, std::move(n));
            } else {
                adoptUids(t.model, it->second.model);
                it->second.model = t.model;
            }
        } else if (ext.size() == 3 && ext[0] == 'f') {
            SteadyFlowFile t;
            if (!t.load(path, msgs)) continue;
            auto it = steadyFlows.find(ext);
            if (it == steadyFlows.end()) {
                SteadyFlowFile n = SteadyFlowFile::createNew(t.model.title, t.model.programVersion);
                n.model = t.model;
                steadyFlows.emplace(ext, std::move(n));
            } else it->second.model = t.model;
        } else if (ext.size() == 3 && ext[0] == 'u') {
            UnsteadyFlowFile t;
            if (!t.load(path, msgs)) continue;
            auto it = unsteadyFlows.find(ext);
            if (it == unsteadyFlows.end()) {
                UnsteadyFlowFile n = UnsteadyFlowFile::createNew(t.model.title, t.model.programVersion);
                n.model = t.model;
                unsteadyFlows.emplace(ext, std::move(n));
            } else it->second.model = t.model;
        } else if (ext.size() == 3 && ext[0] == 'p') {
            PlanFile t;
            if (!t.load(path, msgs)) continue;
            auto it = plans.find(ext);
            if (it == plans.end()) plans.emplace(ext, PlanFile::createNew(t.model, units()));
            else it->second.model = t.model;
        }
    }
    return true;
}

bool Project::duplicate(const fs::path& prjPath, const fs::path& newDir, const std::string& newBase, MessageList& msgs) {
    fs::path dir = prjPath.parent_path();
    std::string fname = fromPath(prjPath.filename());
    std::string base = fname.substr(0, fname.size() - 4);
    std::error_code ec;
    fs::create_directories(newDir, ec);
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string n = fromPath(e.path().filename());
        if (n.size() <= base.size() + 1 || n.compare(0, base.size() + 1, base + ".") != 0) continue;
        fs::path target = newDir / toPath(newBase + n.substr(base.size()));
        if (fs::exists(target)) {
            msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {fromPath(target) + " (exists)"}));
            return false;
        }
        fs::copy_file(e.path(), target, ec);
        if (ec) {
            msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {fromPath(target)}));
            return false;
        }
    }
    return true;
}

}  // namespace deras::io

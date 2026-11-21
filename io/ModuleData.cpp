// De-RAS - io/ModuleData.cpp
#include "io/ModuleData.h"

#include <algorithm>
#include <cmath>

#include "io/RasText.h"
#include "io/ResultsFile.h"

namespace deras::io {

namespace {
json::Value pairs(const std::vector<std::pair<double, double>>& v) {
    json::Value a = json::Value::array();
    for (const auto& p : v) {
        json::Value r = json::Value::array();
        r.push(p.first);
        r.push(p.second);
        a.push(r);
    }
    return a;
}
std::vector<std::pair<double, double>> pairsFrom(const json::Value& v) {
    std::vector<std::pair<double, double>> out;
    for (const auto& r : v.items())
        if (r.size() >= 2) out.emplace_back(r.at(0).num(), r.at(1).num());
    return out;
}
json::Value numbers(const std::vector<double>& v) {
    json::Value a = json::Value::array();
    for (double x : v) a.push(std::isfinite(x) ? json::Value(x) : json::Value());
    return a;
}
std::vector<double> numbersFrom(const json::Value& v) {
    std::vector<double> out;
    for (const auto& x : v.items()) out.push_back(x.isNumber() ? x.num() : std::nan(""));
    return out;
}
json::Value matrix(const std::vector<std::vector<double>>& m) {
    json::Value a = json::Value::array();
    for (const auto& r : m) a.push(numbers(r));
    return a;
}
std::vector<std::vector<double>> matrixFrom(const json::Value& v) {
    std::vector<std::vector<double>> out;
    for (const auto& r : v.items()) out.push_back(numbersFrom(r));
    return out;
}
}  // namespace

SedimentSetup sedimentFor(const ProjectSettings& s) {
    SedimentSetup st;
    auto it = s.modules.find("sediment");
    if (it == s.modules.end()) return st;
    const json::Value& v = it->second;
    st.geomFile = v.get("geometry").str().empty() ? st.geomFile : v.get("geometry").str();
    st.outputHours = v.get("output_hours").num(st.outputHours);
    const json::Value& f = v.get("quasi_unsteady");
    st.flow.river = f.get("river").str();
    st.flow.reach = f.get("reach").str();
    st.flow.dsSlope = f.get("ds_slope").num(st.flow.dsSlope);
    for (const auto& r : f.get("records").items()) {
        QuasiRecord q;
        q.durationHours = r.get("duration_hours").num(q.durationHours);
        q.flow = r.get("flow").num(0);
        q.computeHours = r.get("compute_hours").num(q.computeHours);
        if (r.get("ds_stage").isNumber()) q.dsStage = r.get("ds_stage").num();
        st.flow.records.push_back(q);
    }
    const json::Value& d = v.get("sediment");
    st.data.function = static_cast<TransportFunction>(std::clamp(int(d.get("function").num(0)), 0, 2));
    st.data.specificGravity = d.get("specific_gravity").num(st.data.specificGravity);
    st.data.porosity = d.get("porosity").num(st.data.porosity);
    st.data.temperature = d.get("temperature_c").num(st.data.temperature);
    st.data.gradation = pairsFrom(d.get("gradation_mm_percent_finer"));
    st.data.maxErodibleDepth = d.get("max_erodible_depth").num(st.data.maxErodibleDepth);
    st.data.inflow = static_cast<SedimentInflow>(std::clamp(int(d.get("inflow").num(0)), 0, 2));
    st.data.rating = pairsFrom(d.get("rating_flow_load"));
    return st;
}

void setSediment(ProjectSettings& s, const SedimentSetup& st) {
    if (st.empty() && st.data.gradation.empty()) {
        s.modules.erase("sediment");
        return;
    }
    json::Value v = json::Value::object();
    v["geometry"] = st.geomFile;
    v["output_hours"] = st.outputHours;
    json::Value f = json::Value::object();
    f["river"] = st.flow.river;
    f["reach"] = st.flow.reach;
    f["ds_slope"] = st.flow.dsSlope;
    json::Value recs = json::Value::array();
    for (const auto& q : st.flow.records) {
        json::Value r = json::Value::object();
        r["duration_hours"] = q.durationHours;
        r["flow"] = q.flow;
        r["compute_hours"] = q.computeHours;
        r["ds_stage"] = std::isfinite(q.dsStage) ? json::Value(q.dsStage) : json::Value();
        recs.push(r);
    }
    f["records"] = recs;
    v["quasi_unsteady"] = f;
    json::Value d = json::Value::object();
    d["function"] = int(st.data.function);
    d["specific_gravity"] = st.data.specificGravity;
    d["porosity"] = st.data.porosity;
    d["temperature_c"] = st.data.temperature;
    d["gradation_mm_percent_finer"] = pairs(st.data.gradation);
    d["max_erodible_depth"] = st.data.maxErodibleDepth;
    d["inflow"] = int(st.data.inflow);
    d["rating_flow_load"] = pairs(st.data.rating);
    v["sediment"] = d;
    s.modules["sediment"] = v;
}

fs::path sedimentResultPath(const fs::path& dir, const std::string& base) { return dir / toPath(base + ".deras-sedimen.json"); }

bool writeSedimentResult(const fs::path& path, const SedimentResult& r, std::string& error) {
    json::Value v = json::Value::object();
    v["format"] = "deras-sediment-results";
    v["format_version"] = 1;
    v["status"] = r.status == RunStatus::Succeeded ? "succeeded" : r.status == RunStatus::Cancelled ? "cancelled" : r.status == RunStatus::Failed ? "failed" : "not-run";
    v["solver_version"] = r.solverVersion;
    v["started_utc"] = r.startedUtc;
    v["units"] = r.units == UnitSystem::SI ? "SI" : "US";
    v["river"] = r.river;
    v["reach"] = r.reach;
    v["function"] = r.function;
    v["d50_mm"] = r.d50;
    v["d90_mm"] = r.d90;
    json::Value rs = json::Value::array();
    for (const auto& x : r.rs) rs.push(x);
    v["rs"] = rs;
    v["distance"] = numbers(r.distance);
    v["invert0"] = numbers(r.invert0);
    v["hours"] = numbers(r.hours);
    v["flow"] = numbers(r.flow);
    v["invert"] = matrix(r.invert);
    v["ws"] = matrix(r.ws);
    v["load_t_per_day"] = matrix(r.load);
    v["bed_change"] = matrix(r.change);
    v["mass_in"] = r.massIn;
    v["mass_out"] = r.massOut;
    v["mass_change"] = r.massChange;
    json::Value ms = json::Value::array();
    for (const auto& m : r.messages) ms.push(messageToJson(m));
    v["messages"] = ms;
    return writeFileAtomic(path, v.dump(1), error);
}

bool readSedimentResult(const fs::path& path, SedimentResult& r, std::string& error) {
    std::string text;
    if (!readFile(path, text)) { error = "read"; return false; }
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.get("format").str() != "deras-sediment-results") { error = "format"; return false; }
    const std::string st = v.get("status").str();
    r.status = st == "succeeded" ? RunStatus::Succeeded : st == "failed" ? RunStatus::Failed : st == "cancelled" ? RunStatus::Cancelled : RunStatus::NotRun;
    r.solverVersion = v.get("solver_version").str();
    r.startedUtc = v.get("started_utc").str();
    r.units = v.get("units").str() == "US" ? UnitSystem::USCustomary : UnitSystem::SI;
    r.river = v.get("river").str();
    r.reach = v.get("reach").str();
    r.function = v.get("function").str();
    r.d50 = v.get("d50_mm").num();
    r.d90 = v.get("d90_mm").num();
    r.rs.clear();
    for (const auto& x : v.get("rs").items()) r.rs.push_back(x.str());
    r.distance = numbersFrom(v.get("distance"));
    r.invert0 = numbersFrom(v.get("invert0"));
    r.hours = numbersFrom(v.get("hours"));
    r.flow = numbersFrom(v.get("flow"));
    r.invert = matrixFrom(v.get("invert"));
    r.ws = matrixFrom(v.get("ws"));
    r.load = matrixFrom(v.get("load_t_per_day"));
    r.change = matrixFrom(v.get("bed_change"));
    r.massIn = v.get("mass_in").num();
    r.massOut = v.get("mass_out").num();
    r.massChange = v.get("mass_change").num();
    r.messages.clear();
    for (const auto& m : v.get("messages").items()) r.messages.push_back(messageFromJson(m));
    return true;
}

// ---------------- water quality ----------------
WqSetup waterQualityFor(const ProjectSettings& s) {
    WqSetup w;
    w.input.upstream = {{0, 10}};
    auto it = s.modules.find("water_quality");
    if (it == s.modules.end()) return w;
    const json::Value& v = it->second;
    w.plan = v.get("plan").str();
    if (!v.get("constituent").str().empty()) w.constituent = v.get("constituent").str();
    w.input.initial = v.get("initial_mg_l").num(0);
    w.input.dispersion = v.get("dispersion").num(10);
    w.input.decayPerDay = v.get("decay_per_day").num(0);
    w.input.upstream = pairsFrom(v.get("upstream_hours_mg_l"));
    return w;
}

void setWaterQuality(ProjectSettings& s, const WqSetup& w) {
    json::Value v = json::Value::object();
    v["plan"] = w.plan;
    v["constituent"] = w.constituent;
    v["initial_mg_l"] = w.input.initial;
    v["dispersion"] = w.input.dispersion;
    v["decay_per_day"] = w.input.decayPerDay;
    v["upstream_hours_mg_l"] = pairs(w.input.upstream);
    s.modules["water_quality"] = v;
}

fs::path wqResultPath(const fs::path& dir, const std::string& base, const std::string& planExt) {
    return dir / toPath(base + "." + planExt + ".deras-wq.json");
}

bool writeWqResult(const fs::path& path, const design::WqResult& r, const std::string& meta, std::string& error) {
    json::Value v = json::Value::object();
    v["format"] = "deras-water-quality-results";
    v["format_version"] = 1;
    v["status"] = r.status == RunStatus::Succeeded ? "succeeded" : r.status == RunStatus::Failed ? "failed" : "not-run";
    v["error"] = r.error;
    v["meta"] = meta;
    v["river"] = r.river;
    v["reach"] = r.reach;
    json::Value rs = json::Value::array();
    for (const auto& x : r.rs) rs.push(x);
    v["rs"] = rs;
    v["distance"] = numbers(r.distance);
    v["hours"] = numbers(r.hours);
    v["conc_mg_l"] = matrix(r.conc);
    v["mass_in"] = r.massIn;
    v["mass_out"] = r.massOut;
    v["mass_stored0"] = r.massStored0;
    v["mass_stored"] = r.massStored;
    v["mass_decayed"] = r.massDecayed;
    v["mass_continuity_correction"] = r.massCorrection;
    return writeFileAtomic(path, v.dump(1), error);
}

bool readWqResult(const fs::path& path, design::WqResult& r, std::string& meta, std::string& error) {
    std::string text;
    if (!readFile(path, text)) { error = "read"; return false; }
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.get("format").str() != "deras-water-quality-results") { error = "format"; return false; }
    const std::string st = v.get("status").str();
    r.status = st == "succeeded" ? RunStatus::Succeeded : st == "failed" ? RunStatus::Failed : RunStatus::NotRun;
    r.error = v.get("error").str();
    meta = v.get("meta").str();
    r.river = v.get("river").str();
    r.reach = v.get("reach").str();
    r.rs.clear();
    for (const auto& x : v.get("rs").items()) r.rs.push_back(x.str());
    r.distance = numbersFrom(v.get("distance"));
    r.hours = numbersFrom(v.get("hours"));
    r.conc = matrixFrom(v.get("conc_mg_l"));
    r.massIn = v.get("mass_in").num();
    r.massOut = v.get("mass_out").num();
    r.massStored0 = v.get("mass_stored0").num();
    r.massStored = v.get("mass_stored").num();
    r.massDecayed = v.get("mass_decayed").num();
    r.massCorrection = v.get("mass_continuity_correction").num();
    return true;
}

// ---------------- unit conversion of the module data ----------------
void convertSettings(ProjectSettings& s, bool toSI) {
    const double L = toSI ? 0.3048 : 1 / 0.3048;
    const double Q = L * L * L;
    const double T = toSI ? 0.90718474 : 1 / 0.90718474;   // US short ton <-> metric tonne
    for (auto& [k, v] : s.gateOpenings) v *= L;
    // pumps
    if (s.modules.count("pumps")) {
        std::vector<std::string> geoms;
        for (const auto& m : s.modules["pumps"].members()) geoms.push_back(m.first);
        for (const auto& ge : geoms) {
            auto ps = pumpsFor(s, ge);
            for (auto& p : ps) {
                for (auto& g : p.groups) {
                    g.capacity *= Q;
                    g.onElev *= L;
                    g.offElev *= L;
                }
            }
            setPumps(s, ge, ps);
        }
    }
    // breaches
    if (s.modules.count("breaches")) {
        std::vector<std::string> keys;
        for (const auto& m : s.modules["breaches"].members()) keys.push_back(m.first);
        for (const auto& k : keys) {
            Breach b = breachFor(s, k);
            b.station *= L;
            b.bottomElev *= L;
            b.bottomWidth *= L;
            setBreach(s, k, b);
        }
    }
    // sediment
    if (s.modules.count("sediment")) {
        SedimentSetup st = sedimentFor(s);
        for (auto& r : st.flow.records) {
            r.flow *= Q;
            if (std::isfinite(r.dsStage)) r.dsStage *= L;
        }
        st.data.maxErodibleDepth *= L;
        for (auto& r : st.data.rating) {
            r.first *= Q;
            r.second *= T;
        }
        setSediment(s, st);
    }
    // water quality (concentrations in mg/L in both systems)
    if (s.modules.count("water_quality")) {
        WqSetup w = waterQualityFor(s);
        w.input.dispersion *= L * L;
        setWaterQuality(s, w);
    }
}

}  // namespace deras::io

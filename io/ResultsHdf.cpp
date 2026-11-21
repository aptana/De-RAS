// De-RAS - io/ResultsHdf.cpp
#include "ResultsHdf.h"

#include "HdfWriter.h"

namespace deras::io {

namespace {
const char* kMarker = "Written by De-RAS";
}

fs::path hdfResultsPathFor(const fs::path& projectDir, const std::string& base, const std::string& planExt) {
    return projectDir / toPath(base + "." + planExt + ".hdf");
}

bool hdfWritableByDeras(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;
    std::string bytes;
    if (!readFile(path, bytes)) return false;
    return bytes.find(kMarker) != std::string::npos;
}

std::string steadyResultsHdf(const RunResult& r, const Plan& plan, UnitSystem units, const std::string& derasVersion) {
    hdf::Group root;
    root.attr("File Type", "HEC-RAS Results");
    root.attr("File Version", "De-RAS " + derasVersion);
    root.attr("Units System", units == UnitSystem::SI ? "SI Units" : "US Customary");
    root.attr("Writer", std::string(kMarker) + " " + derasVersion);

    auto& pi = root.group("Plan Data/Plan Information");
    pi.attr("Plan Title", plan.title);
    pi.attr("Plan ShortID", plan.shortId);
    pi.attr("Geometry File", plan.geomFile);
    pi.attr("Flow File", plan.flowFile);
    pi.attr("Flow Regime", plan.regime == FlowRegime::Subcritical ? "Subcritical" : plan.regime == FlowRegime::Supercritical ? "Supercritical" : "Mixed");
    pi.attr("Computation Method", r.meta.method);

    auto& sum = root.group("Results/Steady/Summary");
    sum.attr("Solution", r.status == RunStatus::Succeeded ? "Steady Finished Successfully" : "Steady Failed");
    sum.attr("Run Time Window", r.meta.startedUtc + " to " + r.meta.finishedUtc);
    sum.attr("Computation Time (s)", r.meta.elapsedSeconds);

    const size_t np = r.profiles.size();
    const size_t nx = np ? r.profiles[0].xs.size() : 0;
    std::vector<std::string> names, rivers, reaches, stations, xsOnly;
    for (const auto& p : r.profiles) names.push_back(p.name);
    if (np)
        for (const auto& x : r.profiles[0].xs) {
            rivers.push_back(x.river);
            reaches.push_back(x.reach);
            stations.push_back(x.rs);
            xsOnly.push_back(x.river + " " + x.reach + " " + x.rs);
        }
    auto& gi = root.group("Results/Steady/Output/Geometry Info");
    gi.addStrings("Cross Section Only", xsOnly);
    gi.addStrings("River Names", rivers, 16);
    gi.addStrings("Reach Names", reaches, 16);
    gi.addStrings("River Stations", stations, 8);

    const std::string base = "Results/Steady/Output/Output Blocks/Base Output/Steady Profiles";
    auto& sp = root.group(base);
    sp.addStrings("Profile Names", names);
    auto& xs = root.group(base + "/Cross Sections");
    auto& add = root.group(base + "/Cross Sections/Additional Variables");
    auto table = [&](hdf::Group& g, const std::string& name, const std::string& unit, auto get) {
        std::vector<double> v;
        v.reserve(np * nx);
        for (const auto& p : r.profiles)
            for (size_t i = 0; i < nx; ++i) v.push_back(i < p.xs.size() ? get(p.xs[i]) : 0.0);
        auto& d = g.addFloat32(name, {np, nx}, v);
        if (!unit.empty()) d.attrs.push_back({"Units", hdf::Type::String, {}, {}, {unit}, 0});
    };
    const std::string L = lengthUnit(units), Q = flowUnit(units), V = velocityUnit(units), A = areaUnit(units);
    table(xs, "Water Surface", L, [](const XsResult& x) { return x.ws; });
    table(xs, "Flow", Q, [](const XsResult& x) { return x.q; });
    table(xs, "Energy Grade", L, [](const XsResult& x) { return x.eg; });
    table(add, "Critical Water Surface", L, [](const XsResult& x) { return x.critWs; });
    table(add, "Minimum Channel Elevation", L, [](const XsResult& x) { return x.minChEl; });
    table(add, "Energy Grade Slope", L + "/" + L, [](const XsResult& x) { return x.egSlope; });
    table(add, "Velocity Channel", V, [](const XsResult& x) { return x.velCh; });
    table(add, "Velocity Total", V, [](const XsResult& x) { return x.velTotal; });
    table(add, "Area Flow Total", A, [](const XsResult& x) { return x.area; });
    table(add, "Top Width Total", L, [](const XsResult& x) { return x.topWidth; });
    table(add, "Froude Number Channel", "", [](const XsResult& x) { return x.froudeCh; });
    table(add, "Flow Left Overbank", Q, [](const XsResult& x) { return x.qLob; });
    table(add, "Flow Channel", Q, [](const XsResult& x) { return x.qCh; });
    table(add, "Flow Right Overbank", Q, [](const XsResult& x) { return x.qRob; });
    table(add, "Friction Loss", L, [](const XsResult& x) { return x.frictionLoss; });
    table(add, "Contraction Expansion Loss", L, [](const XsResult& x) { return x.ceLoss; });
    table(add, "Alpha", "", [](const XsResult& x) { return x.alpha; });
    return hdf::build(root);
}

bool writeSteadyHdf(const fs::path& path, const RunResult& r, const Plan& plan, UnitSystem units,
                    const std::string& derasVersion, std::string& error) {
    return writeFileAtomic(path, steadyResultsHdf(r, plan, units, derasVersion), error);
}

}  // namespace deras::io

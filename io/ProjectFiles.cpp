// De-RAS - io/ProjectFiles.cpp
#include "ProjectFiles.h"

#include <algorithm>
#include <cmath>

namespace deras::io {

// ====================== KeyedText ======================
int KeyedText::indexOf(const std::string& key) const {
    for (size_t i = 0; i < doc.lines.size(); ++i)
        if (startsWith(doc.lines[i], key)) return static_cast<int>(i);
    return -1;
}

std::optional<std::string> KeyedText::get(const std::string& key) const {
    int i = indexOf(key);
    if (i < 0) return std::nullopt;
    return doc.lines[static_cast<size_t>(i)].substr(key.size());
}

bool KeyedText::hasLine(const std::string& exact) const {
    for (const auto& l : doc.lines)
        if (rtrim(l) == exact) return true;
    return false;
}

void KeyedText::set(const std::string& key, const std::string& value, const std::vector<std::string>& anchors) {
    int i = indexOf(key);
    if (i >= 0) { doc.lines[static_cast<size_t>(i)] = key + value; return; }
    int pos = -1;
    for (const auto& a : anchors)
        for (size_t k = 0; k < doc.lines.size(); ++k)
            if (startsWith(doc.lines[k], a) || rtrim(doc.lines[k]) == a) pos = std::max(pos, static_cast<int>(k));
    if (pos < 0) doc.lines.push_back(key + value);
    else doc.lines.insert(doc.lines.begin() + pos + 1, key + value);
}

void KeyedText::removeKey(const std::string& key) {
    doc.lines.erase(std::remove_if(doc.lines.begin(), doc.lines.end(),
                                   [&](const std::string& l) { return startsWith(l, key); }),
                    doc.lines.end());
}

// ====================== .prj ======================
bool ProjectFile::load(const fs::path& path, MessageList& msgs) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {fromPath(path)}));
        return false;
    }
    return parse(bytes, fromPath(path.filename()), msgs);
}

bool ProjectFile::parse(const std::string& bytes, const std::string& src, MessageList& msgs) {
    text_.doc = TextDoc::fromString(bytes);
    isNew_ = false;
    data = ProjectFileData{};
    bool isRas = false;
    bool inDesc = false;
    std::string desc;
    for (const auto& l : text_.doc.lines) {
        if (inDesc) {
            if (startsWith(l, "END DESCRIPTION")) { inDesc = false; continue; }
            if (!desc.empty()) desc += "\n";
            desc += l;
            continue;
        }
        if (startsWith(l, "BEGIN DESCRIPTION")) { inDesc = true; continue; }
        if (auto v = valueOf(l, "Proj Title=")) { data.title = *v; isRas = true; }
        else if (auto v2 = valueOf(l, "Current Plan=")) data.currentPlan = trim(*v2);
        else if (rtrim(l) == "SI Units") data.units = UnitSystem::SI;
        else if (rtrim(l) == "English Units") data.units = UnitSystem::USCustomary;
        else if (auto g = valueOf(l, "Geom File=")) data.geomFiles.push_back(trim(*g));
        else if (auto f = valueOf(l, "Flow File=")) data.flowFiles.push_back(trim(*f));
        else if (auto u = valueOf(l, "Unsteady File=")) data.unsteadyFiles.push_back(trim(*u));
        else if (auto p = valueOf(l, "Plan File=")) data.planFiles.push_back(trim(*p));
    }
    data.description = desc;
    original_ = data;
    if (!isRas) {
        msgs.push_back(makeMsg(Severity::Error, "PRJ_NOT_RAS", {src}));
        return false;
    }
    return true;
}

ProjectFile ProjectFile::createNew(const std::string& title, UnitSystem units) {
    ProjectFile p;
    p.isNew_ = true;
    p.text_.doc.eol = "\r\n";
    p.text_.doc.finalEol = true;
    p.text_.doc.lines = {
        "Proj Title=" + title,
        "Default Exp/Contr=0.3,0.1",
        units == UnitSystem::SI ? "SI Units" : "English Units",
        "Y Axis Title=Elevation",
        "X Axis Title(PF)=Main Channel Distance",
        "X Axis Title(XS)=Station",
        "BEGIN DESCRIPTION:",
        "",
        "END DESCRIPTION:",
        "DSS Start Date=",
        "DSS Start Time=",
        "DSS End Date=",
        "DSS End Time=",
        "DSS Export Filename=",
        "DSS Export Rating Curves= 0 ",
        "DSS Export Rating Curve Sorted= 0 ",
        "DSS Export Volume Flow Curves= 0 ",
        "DXF Filename=",
        "DXF OffsetX= 0 ",
        "DXF OffsetY= 0 ",
        "DXF ScaleX= 1 ",
        "DXF ScaleY= 10 ",
        "GIS Export Profiles= 0 ",
    };
    p.data.title = title;
    p.data.units = units;
    p.original_ = p.data;
    return p;
}

static bool sameProject(const ProjectFileData& a, const ProjectFileData& b) {
    return a.title == b.title && a.currentPlan == b.currentPlan && a.units == b.units && a.geomFiles == b.geomFiles &&
           a.flowFiles == b.flowFiles && a.unsteadyFiles == b.unsteadyFiles && a.planFiles == b.planFiles &&
           a.description == b.description;
}

bool ProjectFile::isModified() const { return isNew_ || !sameProject(data, original_); }

std::string ProjectFile::serialize() const {
    if (!isModified()) return text_.doc.toString();
    KeyedText t = text_;
    t.set("Proj Title=", data.title);
    if (!data.currentPlan.empty()) t.set("Current Plan=", data.currentPlan, {"Proj Title="});
    else t.removeKey("Current Plan=");
    // units line
    auto& L = t.doc.lines;
    bool unitsFound = false;
    for (auto& l : L)
        if (rtrim(l) == "SI Units" || rtrim(l) == "English Units") {
            l = data.units == UnitSystem::SI ? "SI Units" : "English Units";
            unitsFound = true;
        }
    if (!unitsFound) {
        int at = t.indexOf("Default Exp/Contr=");
        if (at < 0) at = t.indexOf("Current Plan=");
        if (at < 0) at = t.indexOf("Proj Title=");
        L.insert(L.begin() + at + 1, data.units == UnitSystem::SI ? "SI Units" : "English Units");
    }
    // file lists: rebuild only if they changed
    if (data.geomFiles != original_.geomFiles || data.flowFiles != original_.flowFiles ||
        data.unsteadyFiles != original_.unsteadyFiles || data.planFiles != original_.planFiles || isNew_) {
        int first = -1;
        for (size_t i = 0; i < L.size(); ++i)
            if (startsWith(L[i], "Geom File=") || startsWith(L[i], "Flow File=") || startsWith(L[i], "Plan File=") ||
                startsWith(L[i], "Unsteady File=")) { first = static_cast<int>(i); break; }
        L.erase(std::remove_if(L.begin(), L.end(), [](const std::string& l) {
                    return startsWith(l, "Geom File=") || startsWith(l, "Flow File=") || startsWith(l, "Plan File=") ||
                           startsWith(l, "Unsteady File=");
                }), L.end());
        if (first < 0) {
            for (size_t i = 0; i < L.size(); ++i)
                if (rtrim(L[i]) == "SI Units" || rtrim(L[i]) == "English Units") first = static_cast<int>(i) + 1;
            if (first < 0) first = static_cast<int>(L.size());
        }
        std::vector<std::string> fl;
        for (const auto& g : data.geomFiles) fl.push_back("Geom File=" + g);
        for (const auto& f : data.flowFiles) fl.push_back("Flow File=" + f);
        for (const auto& p : data.planFiles) fl.push_back("Plan File=" + p);
        for (const auto& u : data.unsteadyFiles) fl.push_back("Unsteady File=" + u);
        L.insert(L.begin() + std::min<long>(first, static_cast<long>(L.size())), fl.begin(), fl.end());
    }
    if (data.description != original_.description) {
        int b = t.indexOf("BEGIN DESCRIPTION:");
        int e = t.indexOf("END DESCRIPTION:");
        std::vector<std::string> body = split(data.description, '\n');
        if (b >= 0 && e > b) {
            L.erase(L.begin() + b + 1, L.begin() + e);
            L.insert(L.begin() + b + 1, body.begin(), body.end());
        }
    }
    return t.doc.toString();
}

bool ProjectFile::save(const fs::path& path, MessageList& msgs) {
    std::string err;
    std::string bytes = serialize();
    if (!writeFileAtomic(path, bytes, err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    ProjectFileData keep = data;
    MessageList ig;
    parse(bytes, fromPath(path.filename()), ig);
    data = keep;
    original_ = keep;
    return true;
}

// ====================== .f## ======================
static bool sameBoundary(const Boundary& a, const Boundary& b) {
    return a.type == b.type && a.knownWS == b.knownWS && a.slope == b.slope && a.rating == b.rating;
}

bool sameFlow(const SteadyFlow& a, const SteadyFlow& b) {
    if (a.title != b.title || a.programVersion != b.programVersion || a.profileNames != b.profileNames) return false;
    if (a.changes.size() != b.changes.size() || a.boundaries.size() != b.boundaries.size()) return false;
    for (size_t i = 0; i < a.changes.size(); ++i) {
        const auto& x = a.changes[i];
        const auto& y = b.changes[i];
        if (x.river != y.river || x.reach != y.reach || x.rs != y.rs || x.flows != y.flows) return false;
    }
    for (size_t i = 0; i < a.boundaries.size(); ++i) {
        const auto& x = a.boundaries[i];
        const auto& y = b.boundaries[i];
        if (x.river != y.river || x.reach != y.reach || x.profile != y.profile || !sameBoundary(x.upstream, y.upstream) ||
            !sameBoundary(x.downstream, y.downstream)) return false;
    }
    return true;
}

bool SteadyFlowFile::load(const fs::path& path, MessageList& msgs) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {fromPath(path)}));
        return false;
    }
    return parse(bytes, fromPath(path.filename()), msgs);
}

static std::vector<std::string> rrParts(const std::string& v) {
    auto p = split(v, ',');
    for (auto& s : p) s = trim(s);
    return p;
}

bool SteadyFlowFile::parse(const std::string& bytes, const std::string& src, MessageList& msgs) {
    doc_ = TextDoc::fromString(bytes);
    originalBytes_ = bytes;
    isNew_ = false;
    model = SteadyFlow{};
    extra_.clear();
    int nprof = 0;
    ReachBoundary* curB = nullptr;
    const auto& L = doc_.lines;
    for (size_t i = 0; i < L.size(); ++i) {
        const std::string& l = L[i];
        if (auto v = valueOf(l, "Flow Title=")) model.title = *v;
        else if (auto v2 = valueOf(l, "Program Version=")) model.programVersion = *v2;
        else if (auto v3 = valueOf(l, "Number of Profiles=")) nprof = static_cast<int>(parseNumber(*v3).value_or(0));
        else if (auto v4 = valueOf(l, "Profile Names=")) {
            model.profileNames.clear();
            for (auto& s : split(*v4, ',')) model.profileNames.push_back(s);
        } else if (auto v5 = valueOf(l, "River Rch & RM=")) {
            auto p = rrParts(*v5);
            FlowChange fc;
            fc.river = p.size() > 0 ? p[0] : "";
            fc.reach = p.size() > 1 ? p[1] : "";
            fc.rs = p.size() > 2 ? p[2] : "";
            while (static_cast<int>(fc.flows.size()) < nprof && i + 1 < L.size() && L[i + 1].find('=') == std::string::npos) {
                ++i;
                for (const auto& f : fixedFields(L[i], 8)) {
                    if (static_cast<int>(fc.flows.size()) >= nprof) break;
                    auto n = parseNumber(f);
                    if (!n) msgs.push_back(makeMsg(Severity::Error, "PARSE_BAD_NUMBER", {f, src, std::to_string(i + 1)}));
                    fc.flows.push_back(n.value_or(0));
                }
            }
            if (static_cast<int>(fc.flows.size()) != nprof)
                msgs.push_back(makeMsg(Severity::Error, "PARSE_COUNT_MISMATCH",
                                       {"River Rch & RM", std::to_string(nprof), std::to_string(fc.flows.size()), src}));
            model.changes.push_back(fc);
        } else if (auto v6 = valueOf(l, "Boundary for River Rch & Prof#=")) {
            auto p = rrParts(*v6);
            int prof = p.size() > 2 ? static_cast<int>(parseNumber(p[2]).value_or(1)) - 1 : 0;
            curB = &model.boundaryRef(p.size() > 0 ? p[0] : "", p.size() > 1 ? p[1] : "", prof);
        } else if (curB && (startsWith(l, "Up ") || startsWith(l, "Dn "))) {
            Boundary& b = startsWith(l, "Up ") ? curB->upstream : curB->downstream;
            std::string rest = l.substr(3);
            if (auto t = valueOf(rest, "Type=")) b.type = static_cast<BoundaryType>(static_cast<int>(parseNumber(*t).value_or(0)));
            else if (auto s = valueOf(rest, "Slope=")) b.slope = parseNumber(*s).value_or(0);
            else if (auto k = valueOf(rest, "Known WS=")) b.knownWS = parseNumber(*k).value_or(0);
            else if (auto r = valueOf(rest, "Rating Curve=")) {
                int n = static_cast<int>(parseNumber(split(*r, ',')[0]).value_or(0));
                std::vector<double> vals;
                while (static_cast<int>(vals.size()) < 2 * n && i + 1 < L.size() && L[i + 1].find('=') == std::string::npos) {
                    ++i;
                    for (const auto& f : fixedFields(L[i], 8)) vals.push_back(parseNumber(f).value_or(0));
                }
                for (size_t k2 = 0; k2 + 1 < vals.size(); k2 += 2) b.rating.emplace_back(vals[k2], vals[k2 + 1]);
            } else {
                extra_.push_back(l);
            }
        } else {
            extra_.push_back(l);
        }
    }
    if (static_cast<int>(model.profileNames.size()) < nprof)
        for (int k = static_cast<int>(model.profileNames.size()); k < nprof; ++k) model.profileNames.push_back("PF " + std::to_string(k + 1));
    model.profileNames.resize(static_cast<size_t>(nprof));
    return true;
}

SteadyFlowFile SteadyFlowFile::createNew(const std::string& title, const std::string& programVersion) {
    SteadyFlowFile f;
    f.isNew_ = true;
    f.model.title = title;
    f.model.programVersion = programVersion;
    f.doc_.eol = "\r\n";
    f.doc_.finalEol = true;
    return f;
}

static void boundaryLines(const char* side, const Boundary& b, std::vector<std::string>& out) {
    out.push_back(std::string(side) + " Type= " + std::to_string(static_cast<int>(b.type)) + " ");
    if (b.type == BoundaryType::KnownWS) out.push_back(std::string(side) + " Known WS=" + formatPlain(b.knownWS));
    if (b.type == BoundaryType::NormalDepth) out.push_back(std::string(side) + " Slope=" + formatPlain(b.slope));
    if (b.type == BoundaryType::RatingCurve) {
        out.push_back(std::string(side) + " Rating Curve= " + std::to_string(b.rating.size()) + " ");
        std::vector<double> v;
        for (const auto& p : b.rating) { v.push_back(p.first); v.push_back(p.second); }
        for (auto& l : fixedLines(v, 8, 10)) out.push_back(l);
    }
}

std::string SteadyFlowFile::generate() const {
    std::vector<std::string> out;
    out.push_back("Flow Title=" + model.title);
    out.push_back("Program Version=" + model.programVersion);
    out.push_back("Number of Profiles= " + std::to_string(model.profileCount()) + " ");
    std::string names;
    for (size_t i = 0; i < model.profileNames.size(); ++i) names += (i ? "," : "") + model.profileNames[i];
    out.push_back("Profile Names=" + names);
    for (const auto& c : model.changes) {
        out.push_back("River Rch & RM=" + padRight(c.river, 16) + "," + padRight(c.reach, 16) + "," + c.rs);
        for (auto& l : fixedLines(c.flows, 8, 10)) out.push_back(l);
    }
    auto bs = model.boundaries;
    std::stable_sort(bs.begin(), bs.end(), [](const ReachBoundary& a, const ReachBoundary& b) {
        if (a.river != b.river) return a.river < b.river;
        if (a.reach != b.reach) return a.reach < b.reach;
        return a.profile < b.profile;
    });
    for (const auto& b : bs) {
        if (b.profile < 0 || b.profile >= model.profileCount()) continue;
        out.push_back("Boundary for River Rch & Prof#=" + padRight(b.river, 16) + "," + padRight(b.reach, 16) + ", " +
                      std::to_string(b.profile + 1) + " ");
        boundaryLines("Up", b.upstream, out);
        boundaryLines("Dn", b.downstream, out);
    }
    if (isNew_ && extra_.empty()) {
        for (const char* l : {"DSS Import StartDate=", "DSS Import StartTime=", "DSS Import EndDate=", "DSS Import EndTime=",
                              "DSS Import GetInterval= 0 ", "DSS Import Interval=", "DSS Import GetPeak= 0 ",
                              "DSS Import FillOption= 0 "})
            out.push_back(l);
    }
    for (const auto& e : extra_) out.push_back(e);
    TextDoc d = doc_;
    if (isNew_) { d.eol = "\r\n"; d.finalEol = true; }
    d.lines = out;
    return d.toString();
}

bool SteadyFlowFile::isModified() const {
    if (isNew_) return true;
    SteadyFlowFile tmp;
    MessageList ig;
    tmp.parse(originalBytes_, "", ig);
    return !sameFlow(model, tmp.model);
}

std::string SteadyFlowFile::serialize() const {
    if (!isModified()) return originalBytes_;
    return generate();
}

bool SteadyFlowFile::save(const fs::path& path, MessageList& msgs) {
    std::string err;
    std::string bytes = serialize();
    if (!writeFileAtomic(path, bytes, err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    SteadyFlow keep = model;
    MessageList ig;
    parse(bytes, fromPath(path.filename()), ig);
    model = keep;
    return true;
}

// ====================== .u## ======================
bool UnsteadyFlowFile::load(const fs::path& path, MessageList& msgs) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {fromPath(path)}));
        return false;
    }
    return parse(bytes, fromPath(path.filename()), msgs);
}

namespace {
const UBoundaryType kHydroTypes[] = {UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::LateralInflow,
                                     UBoundaryType::UniformLateralInflow, UBoundaryType::Precipitation};
bool isDataLine(const std::string& l) { return l.find('=') == std::string::npos; }
}  // namespace

bool UnsteadyFlowFile::parse(const std::string& bytes, const std::string& src, MessageList& msgs) {
    doc_ = TextDoc::fromString(bytes);
    originalBytes_ = bytes;
    isNew_ = false;
    model = UnsteadyFlow{};
    header_.clear();
    initialAt_ = -1;
    rawHeader_.clear();
    rawBlocks_.clear();
    const auto& L = doc_.lines;
    UnsteadyBoundary* b = nullptr;
    size_t blockStart = L.size();
    auto closeBlock = [&](size_t end) {
        if (blockStart < end) rawBlocks_.emplace_back(L.begin() + long(blockStart), L.begin() + long(end));
    };
    auto readValues = [&](size_t& i, int n, std::vector<double>& out) {
        out.clear();
        while (static_cast<int>(out.size()) < n && i + 1 < L.size() && isDataLine(L[i + 1])) {
            ++i;
            for (const auto& f : fixedFields(L[i], 8)) {
                if (static_cast<int>(out.size()) >= n) break;
                auto v = parseNumber(f);
                if (!v) msgs.push_back(makeMsg(Severity::Error, "PARSE_BAD_NUMBER", {f, src, std::to_string(i + 1)}));
                out.push_back(v.value_or(0));
            }
        }
        if (static_cast<int>(out.size()) != n)
            msgs.push_back(makeMsg(Severity::Error, "PARSE_COUNT_MISMATCH", {"Hydrograph", std::to_string(n), std::to_string(out.size()), src}));
    };
    for (size_t i = 0; i < L.size(); ++i) {
        const std::string& l = L[i];
        if (auto loc = valueOf(l, "Boundary Location=")) {
            if (!b) rawHeader_.assign(L.begin(), L.begin() + long(i));
            closeBlock(i);
            blockStart = i;
            model.boundaries.emplace_back();
            b = &model.boundaries.back();
            b->location = rrParts(*loc);
            b->river = b->location.size() > 0 ? b->location[0] : "";
            b->reach = b->location.size() > 1 ? b->location[1] : "";
            b->rs = b->location.size() > 2 ? b->location[2] : "";
            b->rsEnd = b->location.size() > 3 ? b->location[3] : "";
            b->area = b->location.size() > 5 ? b->location[5] : "";
            b->bcLine = b->location.size() > 7 ? b->location[7] : "";
            b->interval.clear();
            continue;
        }
        if (!b) {
            if (auto v = valueOf(l, "Flow Title=")) model.title = *v;
            else if (auto v2 = valueOf(l, "Program Version=")) model.programVersion = *v2;
            else if (auto v3 = valueOf(l, "Initial Flow Loc=")) {
                auto p = rrParts(*v3);
                InitialFlow f;
                f.river = p.size() > 0 ? p[0] : "";
                f.reach = p.size() > 1 ? p[1] : "";
                f.rs = p.size() > 2 ? p[2] : "";
                f.q = p.size() > 3 ? parseNumber(p[3]).value_or(0) : 0;
                model.initialFlows.push_back(f);
                if (initialAt_ < 0) initialAt_ = static_cast<int>(header_.size());
            } else if (auto v4 = valueOf(l, "Initial Storage Elev=")) {
                auto p = split(*v4, ',');
                model.initialStorage.emplace_back(trim(p.size() > 0 ? p[0] : ""), p.size() > 1 ? parseNumber(p[1]).value_or(0) : 0);
                if (initialAt_ < 0) initialAt_ = static_cast<int>(header_.size());
            } else header_.push_back(l);
            continue;
        }
        bool known = false;
        if (b->type == UBoundaryType::None) {
            for (UBoundaryType t : kHydroTypes)
                if (auto v = valueOf(l, uBoundaryKey(t))) {
                    b->type = t;
                    readValues(i, static_cast<int>(parseNumber(split(*v, ',')[0]).value_or(0)), b->values);
                    known = true;
                    break;
                }
            if (!known) {
                if (auto v = valueOf(l, "Friction Slope=")) {
                    b->type = UBoundaryType::NormalDepth;
                    auto p = split(*v, ',');
                    b->slope = parseNumber(p[0]).value_or(0);
                    if (p.size() > 1) b->otherLines.push_back("#FS2=" + p[1]);  // second field kept (meaning not interpreted)
                    known = true;
                } else if (auto r = valueOf(l, "Rating Curve=")) {
                    b->type = UBoundaryType::RatingCurve;
                    std::vector<double> v2;
                    readValues(i, 2 * static_cast<int>(parseNumber(split(*r, ',')[0]).value_or(0)), v2);
                    for (size_t k = 0; k + 1 < v2.size(); k += 2) b->rating.emplace_back(v2[k], v2[k + 1]);
                    known = true;
                }
            }
        }
        if (known) continue;
        if (auto iv = valueOf(l, "Interval=")) { b->interval = trim(*iv); continue; }
        b->otherLines.push_back(l);
    }
    if (!b) rawHeader_ = L;
    closeBlock(L.size());
    for (auto& x : model.boundaries) {
        if (x.type == UBoundaryType::None && !x.otherLines.empty()) x.type = UBoundaryType::Other;
        if (x.interval.empty()) x.interval = x.type == UBoundaryType::NormalDepth || x.type == UBoundaryType::RatingCurve ? "" : "1HOUR";
    }
    original_ = model;
    return true;
}

UnsteadyFlowFile UnsteadyFlowFile::createNew(const std::string& title, const std::string& programVersion) {
    UnsteadyFlowFile f;
    f.isNew_ = true;
    f.model.title = title;
    f.model.programVersion = programVersion;
    f.header_ = {"Use Restart= 0 "};
    f.initialAt_ = 1;
    f.doc_.eol = "\r\n";
    f.doc_.finalEol = true;
    return f;
}

std::string UnsteadyFlowFile::generate() const {
    std::vector<std::string> out;
    const bool headerSame = !isNew_ && model.title == original_.title && model.programVersion == original_.programVersion &&
                            model.initialFlows == original_.initialFlows && model.initialStorage == original_.initialStorage;
    if (headerSame) out = rawHeader_;
    else {
    out.push_back("Flow Title=" + model.title);
    out.push_back("Program Version=" + model.programVersion);
    std::vector<std::string> init;
    for (const auto& f : model.initialFlows)
        init.push_back("Initial Flow Loc=" + padRight(f.river, 16) + "," + padRight(f.reach, 16) + "," + padRight(f.rs, 8) + "," + formatPlain(f.q));
    for (const auto& st : model.initialStorage) init.push_back("Initial Storage Elev=" + padRight(st.first, 16) + "," + formatPlain(st.second));
    const int at = initialAt_ < 0 ? static_cast<int>(header_.size()) : std::min(initialAt_, static_cast<int>(header_.size()));
    for (int k = 0; k < static_cast<int>(header_.size()); ++k) {
        if (k == at) out.insert(out.end(), init.begin(), init.end());
        out.push_back(header_[size_t(k)]);
    }
    if (at >= static_cast<int>(header_.size())) out.insert(out.end(), init.begin(), init.end());
    }
    for (size_t bi = 0; bi < model.boundaries.size(); ++bi) {
        const auto& b = model.boundaries[bi];
        if (!isNew_ && bi < original_.boundaries.size() && bi < rawBlocks_.size() && b == original_.boundaries[bi]) {
            out.insert(out.end(), rawBlocks_[bi].begin(), rawBlocks_[bi].end());  // unchanged block: original lines
            continue;
        }
        std::vector<std::string> loc = b.location;
        const bool areaLoc = !b.area.empty() || !b.bcLine.empty();
        if (loc.size() < (areaLoc ? 9u : 8u)) loc.resize(areaLoc ? 9 : 8);
        loc[0] = b.river;
        loc[1] = b.reach;
        loc[2] = b.rs;
        loc[3] = b.rsEnd;
        loc[5] = b.area;
        loc[7] = b.bcLine;
        const size_t w[] = {16, 16, 8, 8, 16, 16, 16, 32, 32};
        std::string ll = "Boundary Location=";
        for (size_t k = 0; k < loc.size(); ++k) ll += (k ? "," : "") + padRight(loc[k], k < 9 ? w[k] : 16);
        out.push_back(ll);
        const bool hydro = b.type == UBoundaryType::FlowHydrograph || b.type == UBoundaryType::StageHydrograph ||
                           b.type == UBoundaryType::LateralInflow || b.type == UBoundaryType::UniformLateralInflow ||
                           b.type == UBoundaryType::Precipitation;
        if (hydro || !b.interval.empty()) out.push_back("Interval=" + b.interval);
        bool fresh = b.otherLines.empty();
        std::string fs2 = "0";
        if (hydro) {
            out.push_back(std::string(uBoundaryKey(b.type)) + " " + std::to_string(b.values.size()) + " ");
            for (auto& l : fixedLines(b.values, 8, 10)) out.push_back(l);
        } else if (b.type == UBoundaryType::NormalDepth) {
            for (const auto& o : b.otherLines) if (startsWith(o, "#FS2=")) fs2 = o.substr(5);
            std::string sl = formatPlain(b.slope);
            if (!sl.empty() && sl[0] == '.') sl = "0" + sl;
            out.push_back("Friction Slope=" + sl + "," + fs2);
            fresh = true;
            for (const auto& o : b.otherLines) if (!startsWith(o, "#FS2=")) fresh = false;
        } else if (b.type == UBoundaryType::RatingCurve) {
            out.push_back("Rating Curve= " + std::to_string(b.rating.size()) + " ");
            std::vector<double> v;
            for (const auto& p : b.rating) { v.push_back(p.first); v.push_back(p.second); }
            for (auto& l : fixedLines(v, 8, 10)) out.push_back(l);
        }
        for (const auto& o : b.otherLines) if (!startsWith(o, "#FS2=")) out.push_back(o);
        if (fresh && hydro) {
            if (b.type == UBoundaryType::StageHydrograph || b.type == UBoundaryType::FlowHydrograph) out.push_back("Stage Hydrograph TW Check=0");
            for (const char* l : {"DSS Path=", "Use DSS=False", "Use Fixed Start Time=False", "Fixed Start Date/Time=,", "Is Critical Boundary=False", "Critical Boundary Flow="})
                out.push_back(l);
        }
    }
    TextDoc d = doc_;
    if (isNew_) { d.eol = "\r\n"; d.finalEol = true; }
    d.lines = out;
    return d.toString();
}

bool UnsteadyFlowFile::isModified() const {
    if (isNew_) return true;
    UnsteadyFlowFile tmp;
    MessageList ig;
    tmp.parse(originalBytes_, "", ig);
    return !(model == tmp.model);
}

std::string UnsteadyFlowFile::serialize() const {
    if (!isModified()) return originalBytes_;
    return generate();
}

bool UnsteadyFlowFile::save(const fs::path& path, MessageList& msgs) {
    std::string err;
    std::string bytes = serialize();
    if (!writeFileAtomic(path, bytes, err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    UnsteadyFlow keep = model;
    MessageList ig;
    parse(bytes, fromPath(path.filename()), ig);
    model = keep;
    return true;
}

// ====================== .p## ======================
bool samePlan(const Plan& a, const Plan& b) {
    return a.title == b.title && a.shortId == b.shortId && a.programVersion == b.programVersion && a.geomFile == b.geomFile &&
           a.flowFile == b.flowFile && a.regime == b.regime && a.stdStepTol == b.stdStepTol && a.criticalTol == b.criticalTol &&
           a.maxTrials == b.maxTrials && a.maxErrorTol == b.maxErrorTol && a.frictionMethod == b.frictionMethod &&
           a.simStartDate == b.simStartDate && a.simStartTime == b.simStartTime && a.simEndDate == b.simEndDate &&
           a.simEndTime == b.simEndTime && a.compInterval == b.compInterval && a.outputInterval == b.outputInterval &&
           a.theta == b.theta && a.unetZTol == b.unetZTol && a.unetMaxIter == b.unetMaxIter;
}

bool PlanFile::load(const fs::path& path, MessageList& msgs) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {fromPath(path)}));
        return false;
    }
    return parse(bytes, fromPath(path.filename()), msgs);
}

bool PlanFile::parse(const std::string& bytes, const std::string&, MessageList&) {
    text_.doc = TextDoc::fromString(bytes);
    isNew_ = false;
    model = Plan{};
    for (const auto& l : text_.doc.lines) {
        const std::string t = rtrim(l);
        if (auto v = valueOf(l, "Plan Title=")) model.title = *v;
        else if (auto v2 = valueOf(l, "Short Identifier=")) model.shortId = rtrim(*v2);
        else if (auto v3 = valueOf(l, "Program Version=")) model.programVersion = *v3;
        else if (auto g = valueOf(l, "Geom File=")) model.geomFile = trim(*g);
        else if (auto f = valueOf(l, "Flow File=")) model.flowFile = trim(*f);
        else if (t == "Subcritical Flow") model.regime = FlowRegime::Subcritical;
        else if (t == "Supercritical Flow") model.regime = FlowRegime::Supercritical;
        else if (t == "Mixed Flow") model.regime = FlowRegime::Mixed;
        else if (auto s = valueOf(l, "Std Step Tol=")) model.stdStepTol = parseNumber(*s).value_or(model.stdStepTol);
        else if (auto c = valueOf(l, "Critical Tol=")) model.criticalTol = parseNumber(*c).value_or(model.criticalTol);
        else if (auto n = valueOf(l, "Num of Std Step Trials=")) model.maxTrials = static_cast<int>(parseNumber(*n).value_or(20));
        else if (auto m = valueOf(l, "Max Error Tol=")) model.maxErrorTol = parseNumber(*m).value_or(model.maxErrorTol);
        else if (auto sd = valueOf(l, "Simulation Date=")) {
            auto p = split(*sd, ',');
            p.resize(4);
            model.simStartDate = trim(p[0]);
            model.simStartTime = trim(p[1]);
            model.simEndDate = trim(p[2]);
            model.simEndTime = trim(p[3]);
        } else if (auto ci = valueOf(l, "Computation Interval=")) model.compInterval = trim(*ci);
        else if (auto oi = valueOf(l, "Output Interval=")) model.outputInterval = trim(*oi);
        else if (auto th = valueOf(l, "UNET Theta=")) model.theta = parseNumber(*th).value_or(1);
        else if (auto zt = valueOf(l, "UNET ZTol=")) model.unetZTol = parseNumber(*zt).value_or(model.unetZTol);
        else if (auto mi = valueOf(l, "UNET MaxIter=")) model.unetMaxIter = static_cast<int>(parseNumber(*mi).value_or(20));
        else if (auto fr = valueOf(l, "Friction Slope Method=")) {
            int k = static_cast<int>(parseNumber(*fr).value_or(1));
            if (k >= 1 && k <= 4) model.frictionMethod = static_cast<FrictionSlopeMethod>(k);
        }
    }
    original_ = model;
    return true;
}

PlanFile PlanFile::createNew(const Plan& p, UnitSystem units) {
    PlanFile f;
    f.isNew_ = true;
    f.model = p;
    const auto k = constantsFor(units);
    if (f.model.stdStepTol <= 0) f.model.stdStepTol = k.defaultStdStepTol;
    f.text_.doc.eol = "\r\n";
    f.text_.doc.finalEol = true;
    f.text_.doc.lines = {
        "Plan Title=" + p.title,
        "Program Version=" + p.programVersion,
        "Short Identifier=" + p.shortId,
        "Simulation Date=,,,",
        "Geom File=" + p.geomFile,
        "Flow File=" + p.flowFile,
        "Subcritical Flow",
        "K Sum by GR= 0 ",
        "Std Step Tol= " + formatPlain(f.model.stdStepTol) + " ",
        "Critical Tol= " + formatPlain(f.model.criticalTol) + " ",
        "Num of Std Step Trials= " + std::to_string(f.model.maxTrials) + " ",
        "Max Error Tol= " + formatPlain(f.model.maxErrorTol) + " ",
        "Flow Tol Ratio= 0.001 ",
        "Split Flow NTrial= 30 ",
        "Split Flow Tol= 0.02 ",
        "Split Flow Ratio= 0.02 ",
        "Log Output Level= 0 ",
        "Friction Slope Method= " + std::to_string(static_cast<int>(f.model.frictionMethod)) + " ",
        "Unsteady Friction Slope Method= 2 ",
        "Unsteady Bridges Friction Slope Method= 1 ",
        "Parabolic Critical Depth",
        "Global Vel Dist= 0 , 0 , 0 ",
        "Global Log Level= 0 ",
        "CheckData=True",
        "Encroach Param=-1 ,0,0, 0 ",
        "Computation Level Output = False",
        "Run HTab= 0 ",
        "Run UNet= 0 ",
        "Run Sediment= 0 ",
        "Run PostProcess= 0 ",
        "Run WQNet= 0 ",
    };
    f.original_ = f.model;
    return f;
}

bool PlanFile::isModified() const { return isNew_ || !samePlan(model, original_); }

void PlanFile::unsteadyLines(KeyedText& t) const {
    const Plan& o = original_;
    const Plan& m = model;
    if (isNew_ || m.simStartDate != o.simStartDate || m.simStartTime != o.simStartTime || m.simEndDate != o.simEndDate || m.simEndTime != o.simEndTime)
        t.set("Simulation Date=", m.simStartDate + "," + m.simStartTime + "," + m.simEndDate + "," + m.simEndTime, {"Short Identifier="});
    if (!m.isUnsteady()) return;
    if (isNew_ || m.compInterval != o.compInterval || !t.get("Computation Interval=")) t.set("Computation Interval=", m.compInterval, {"Flow File="});
    if (isNew_ || m.outputInterval != o.outputInterval || !t.get("Output Interval=")) t.set("Output Interval=", m.outputInterval, {"Computation Interval="});
    if (isNew_ || m.theta != o.theta || !t.get("UNET Theta=")) t.set("UNET Theta=", " " + formatPlain(m.theta) + " ", {"Run WQNet=", "Run PostProcess="});
    if (isNew_ || m.unetZTol != o.unetZTol || !t.get("UNET ZTol=")) t.set("UNET ZTol=", " " + formatPlain(m.unetZTol) + " ", {"UNET Theta="});
    if (isNew_ || m.unetMaxIter != o.unetMaxIter || !t.get("UNET MaxIter=")) t.set("UNET MaxIter=", " " + std::to_string(m.unetMaxIter) + " ", {"UNET ZTol="});
    if (isNew_) {
        for (auto& l : t.doc.lines) if (startsWith(l, "Run UNet=")) l = "Run UNet= 1 ";
        for (auto& l : t.doc.lines) if (startsWith(l, "Run PostProcess=")) l = "Run PostProcess= 1 ";
    }
}

std::string PlanFile::serialize() const {
    if (!isModified()) return text_.doc.toString();
    KeyedText t = text_;
    if (!isNew_) {
        t.set("Plan Title=", model.title);
        t.set("Program Version=", model.programVersion, {"Plan Title="});
        t.set("Short Identifier=", model.shortId, {"Program Version="});
        t.set("Geom File=", model.geomFile, {"Short Identifier=", "Simulation Date="});
        t.set("Flow File=", model.flowFile, {"Geom File="});
        auto& L = t.doc.lines;
        const char* regimeLine = model.regime == FlowRegime::Subcritical ? "Subcritical Flow"
                                 : model.regime == FlowRegime::Supercritical ? "Supercritical Flow" : "Mixed Flow";
        bool found = false;
        for (auto& l : L) {
            std::string r = rtrim(l);
            if (r == "Subcritical Flow" || r == "Supercritical Flow" || r == "Mixed Flow") { l = regimeLine; found = true; }
        }
        if (!found) {
            int at = t.indexOf("Flow File=");
            L.insert(L.begin() + at + 1, regimeLine);
        }
        t.set("Std Step Tol=", " " + formatPlain(model.stdStepTol) + " ", {"K Sum by GR=", "Subcritical Flow", "Mixed Flow", "Supercritical Flow"});
        t.set("Critical Tol=", " " + formatPlain(model.criticalTol) + " ", {"Std Step Tol="});
        t.set("Num of Std Step Trials=", " " + std::to_string(model.maxTrials) + " ", {"Critical Tol="});
        t.set("Max Error Tol=", " " + formatPlain(model.maxErrorTol) + " ", {"Num of Std Step Trials="});
        t.set("Friction Slope Method=", " " + std::to_string(static_cast<int>(model.frictionMethod)) + " ", {"Log Output Level="});
    }
    if (model.isUnsteady() || isNew_) unsteadyLines(t);
    return t.doc.toString();
}

bool PlanFile::save(const fs::path& path, MessageList& msgs) {
    std::string err;
    std::string bytes = serialize();
    if (!writeFileAtomic(path, bytes, err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    Plan keep = model;
    MessageList ig;
    parse(bytes, fromPath(path.filename()), ig);
    model = keep;
    original_ = keep;
    return true;
}

}  // namespace deras::io

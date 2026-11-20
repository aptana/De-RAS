// De-RAS - io/GeometryFile.cpp
#include "GeometryFile.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <set>

namespace deras::io {

namespace {

const char* kTypeKey = "Type RM Length L Ch R =";
const char* kReachKey = "River Reach=";

// Lines that start a top-level block which De-RAS does not interpret.
bool isOtherBlockStart(const std::string& l) {
    static const char* keys[] = {"Junct Name=", "Storage Area=", "Connection=", "BreakLine Name=", "BC Line Name=",
                                 "Refinement Region Name=", "LCMann Time=", "LCMann Region",
                                 "Chan Stop Cuts=", "Use User Specified Reach Order=", "GIS Ratio Cuts To Invert=",
                                 "GIS Limit At Bridges=", "Composite Channel Slope=", "Pipe Conduit Name=",
                                 "Pipe Node Name=", "Pipe Network"};
    for (const char* k : keys)
        if (startsWith(l, k)) return true;
    return false;
}

bool isBlank(const std::string& l) { return trim(l).empty(); }

// A "key line" carries '=' or is a description marker; anything else in a
// block is a data line belonging to the preceding key line.
bool isKeyLine(const std::string& l) {
    return l.find('=') != std::string::npos || startsWith(l, "BEGIN ") || startsWith(l, "END ");
}

struct Item {
    std::vector<std::string> lines;
    std::string key() const {
        if (lines.empty()) return "";
        const std::string& l = lines[0];
        if (startsWith(l, "BEGIN DESCRIPTION")) return "BEGIN DESCRIPTION:";
        size_t p = l.find('=');
        return p == std::string::npos ? trim(l) : l.substr(0, p + 1);
    }
};

std::vector<Item> toItems(const std::vector<std::string>& lines) {
    std::vector<Item> items;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        if (startsWith(l, "BEGIN DESCRIPTION")) {
            Item it;
            while (i < lines.size()) {
                it.lines.push_back(lines[i]);
                if (startsWith(lines[i], "END DESCRIPTION")) break;
                ++i;
            }
            items.push_back(it);
            continue;
        }
        if (isBlank(l) || isKeyLine(l) || items.empty()) {
            items.push_back(Item{{l}});
        } else {
            items.back().lines.push_back(l);
        }
    }
    return items;
}

std::vector<std::string> fromItems(const std::vector<Item>& items) {
    std::vector<std::string> out;
    for (const auto& it : items) out.insert(out.end(), it.lines.begin(), it.lines.end());
    return out;
}

int findItem(const std::vector<Item>& items, const std::string& key) {
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].key() == key) return static_cast<int>(i);
    return -1;
}

// Replace item with `key`, or insert after the first existing key in `after`.
void setItem(std::vector<Item>& items, const std::string& key, const std::vector<std::string>& lines,
             const std::vector<std::string>& after) {
    int idx = findItem(items, key);
    if (idx >= 0) { items[static_cast<size_t>(idx)].lines = lines; return; }
    int pos = -1;
    for (const auto& a : after) {
        int j = findItem(items, a);
        if (j >= 0) pos = std::max(pos, j);
    }
    // insert before trailing blank lines if no anchor found
    size_t at;
    if (pos >= 0) at = static_cast<size_t>(pos) + 1;
    else {
        at = items.size();
        while (at > 0 && items[at - 1].lines.size() == 1 && isBlank(items[at - 1].lines[0])) --at;
    }
    items.insert(items.begin() + static_cast<long>(at), Item{lines});
}

void removeItem(std::vector<Item>& items, const std::string& key) {
    int idx = findItem(items, key);
    if (idx >= 0) items.erase(items.begin() + idx);
}

std::vector<double> readFixed(const std::vector<std::string>& data, int width, int count, bool& ok) {
    std::vector<double> v;
    ok = true;
    for (const auto& l : data) {
        for (const auto& f : fixedFields(l, width)) {
            if (static_cast<int>(v.size()) >= count) break;
            auto n = parseNumber(f);
            if (!n) { ok = false; v.push_back(0); } else v.push_back(*n);
        }
    }
    if (static_cast<int>(v.size()) != count) ok = false;
    return v;
}

int leadingInt(const std::string& s) {
    auto parts = split(s, ',');
    auto n = parseNumber(parts.empty() ? s : parts[0]);
    return n ? static_cast<int>(std::lround(*n)) : 0;
}

}  // namespace


// ---------------- hydraulic structures (M4) ----------------
// Bridge / culvert / inline structure lines De-RAS understands. Every line of
// the node that is not listed here (BR Coef, WSPro, BC Design, HTab, ...) is
// kept verbatim. `used` marks the understood lines; `err` names the first
// block that could not be read (the structure is then not computed).
static bool parseStructureLines(const std::vector<std::string>& L, NodeType type, StructureData& sd, std::vector<bool>& used,
                                std::string& err) {
    sd = StructureData{};
    used.assign(L.size(), false);
    auto numbers = [&](size_t& i, int count, int width, int perLine, std::vector<double>& out) {
        out.clear();
        const int nl = linesFor(count, perLine);
        if (i + nl > L.size()) return false;
        std::vector<std::string> data(L.begin() + long(i), L.begin() + long(i + nl));
        bool ok = true;
        out = readFixed(data, width, count, ok);
        for (int k = 0; k < nl; ++k) used[i + k] = true;
        i += nl;
        return ok && int(out.size()) == count;
    };
    auto num = [](const std::string& t, double def) { return parseNumber(trim(t)).value_or(def); };
    bool any = false;
    for (size_t i = 0; i < L.size(); ++i) {
        const std::string& l = L[i];
        if (startsWith(l, "Bridge Culvert-")) { used[i] = true; continue; }
        if (startsWith(l, "Deck Dist Width WeirC")) {
            used[i] = true;
            if (i + 1 >= L.size()) { err = "Deck"; return false; }
            auto f = fixedFields(L[i + 1], 8);
            used[i + 1] = true;
            auto fld = [&](size_t k, double def) { return k < f.size() ? num(f[k], def) : def; };
            sd.dist = fld(0, 0);
            sd.width = fld(1, 0);
            sd.weirCoef = fld(2, 0);
            sd.skew = fld(3, 0);
            const int nUp = int(fld(4, 0)), nDn = int(fld(5, 0));
            sd.maxSubmerge = fld(8, 0.95);
            sd.ogee = fld(9, 0) != 0;
            size_t j = i + 2;
            std::vector<double> st, hi, lo;
            for (int side = 0; side < 2; ++side) {
                const int n = side == 0 ? nUp : nDn;
                if (!numbers(j, n, 8, 10, st) || !numbers(j, n, 8, 10, hi) || !numbers(j, n, 8, 10, lo)) { err = "Deck"; return false; }
                auto& v = side == 0 ? sd.deckUp : sd.deckDn;
                for (int k = 0; k < n; ++k) v.push_back({st[size_t(k)], hi[size_t(k)], lo[size_t(k)]});
            }
            i = j - 1;
            any = true;
            continue;
        }
        if (startsWith(l, "Pier Skew, UpSta & Num, DnSta & Num=")) {
            used[i] = true;
            auto f = split(l.substr(l.find('=') + 1), ',');
            auto fld = [&](size_t k) { return k < f.size() ? num(f[k], 0) : 0.0; };
            Pier p;
            p.skew = fld(0);
            p.upSta = fld(1);
            const int nUp = int(fld(2));
            p.dnSta = fld(3);
            const int nDn = int(fld(4));
            size_t j = i + 1;
            if (!numbers(j, nUp, 8, 10, p.upWidth) || !numbers(j, nUp, 8, 10, p.upElev) || !numbers(j, nDn, 8, 10, p.dnWidth) ||
                !numbers(j, nDn, 8, 10, p.dnElev)) { err = "Pier"; return false; }
            sd.piers.push_back(p);
            i = j - 1;
            continue;
        }
        if (startsWith(l, "Culvert=")) {
            used[i] = true;
            auto f = split(l.substr(8), ',');
            auto fld = [&](size_t k, double def) { return k < f.size() ? num(f[k], def) : def; };
            CulvertGroup c;
            c.shape = int(fld(0, 1));
            c.span = fld(1, 0);
            c.rise = fld(2, 0);
            c.length = fld(3, 0);
            c.n = fld(4, 0.013);
            c.ke = fld(5, 0.5);
            c.kx = fld(6, 1);
            c.chart = int(fld(7, 1));
            c.scale = int(fld(8, 1));
            c.upInvert = fld(9, 0);
            const double us = fld(10, 0);
            c.dnInvert = fld(11, 0);
            const double ds = fld(12, us);
            c.name = f.size() > 13 ? trim(f[13]) : std::string();
            if (c.shape == 1 && c.span == 0) c.span = c.rise;
            // consecutive lines with the same barrel data form one group
            if (!sd.culverts.empty()) {
                CulvertGroup& g = sd.culverts.back();
                CulvertGroup t = c;
                t.upStations = g.upStations;
                t.dnStations = g.dnStations;
                if (t == g) { g.upStations.push_back(us); g.dnStations.push_back(ds); continue; }
            }
            c.upStations = {us};
            c.dnStations = {ds};
            sd.culverts.push_back(c);
            any = true;
            continue;
        }
        if (startsWith(l, "IW Dist,WD,Coef,Skew,MaxSub,Min_El,Is_Ogee,SpillHt,DesHd=")) {
            used[i] = true;
            auto f = split(l.substr(l.find('=') + 1), ',');
            auto fld = [&](size_t k, double def) { return k < f.size() ? num(f[k], def) : def; };
            sd.dist = fld(0, 0);
            sd.width = fld(1, 0);
            sd.weirCoef = fld(2, 0);
            sd.skew = fld(3, 0);
            sd.maxSubmerge = fld(4, 0.95);
            sd.ogee = fld(6, 0) != 0;
            any = true;
            continue;
        }
        if (startsWith(l, "#Inline Weir SE=")) {
            used[i] = true;
            const int n = leadingInt(l.substr(16));
            size_t j = i + 1;
            std::vector<double> v;
            if (!numbers(j, 2 * n, 8, 10, v)) { err = "Inline Weir SE"; return false; }
            for (int k = 0; k < n; ++k) sd.weir.push_back({v[size_t(2 * k)], v[size_t(2 * k + 1)]});
            i = j - 1;
            continue;
        }
        if (startsWith(l, "IW Gate Name")) {
            used[i] = true;
            if (i + 1 >= L.size()) { err = "IW Gate"; return false; }
            auto f = split(L[i + 1], ',');
            used[i + 1] = true;
            auto fld = [&](size_t k, double def) { return k < f.size() ? num(f[k], def) : def; };
            GateGroup g;
            g.name = f.empty() ? std::string() : trim(f[0]);
            g.width = fld(1, 0);
            g.height = fld(2, 0);
            g.invert = fld(3, 0);
            g.coef = fld(4, 0.6);
            g.type = int(fld(8, 0));
            g.weirCoef = fld(9, 0);
            const int n = int(fld(13, 1));
            size_t j = i + 2;
            if (!numbers(j, n, 8, 10, g.stations)) { err = "IW Gate"; return false; }
            sd.gates.push_back(g);
            i = j - 1;
            continue;
        }
    }
    if (type == NodeType::LateralStructure) {
        for (size_t i = 0; i < L.size(); ++i) {
            const std::string& l = L[i];
            auto val = [&](const char* k) { return trim(l.substr(std::string(k).size())); };
            if (startsWith(l, "Lateral Weir Pos=")) { used[i] = true; sd.lateral.side = int(num(val("Lateral Weir Pos="), 1)) == 0 ? 0 : 1; }
            else if (startsWith(l, "Lateral Weir Distance=")) { used[i] = true; sd.dist = num(val("Lateral Weir Distance="), 0); }
            else if (startsWith(l, "Lateral Weir WD=")) { used[i] = true; sd.width = num(val("Lateral Weir WD="), 0); }
            else if (startsWith(l, "Lateral Weir Coef=")) { used[i] = true; sd.weirCoef = num(val("Lateral Weir Coef="), 0); }
            else if (startsWith(l, "Lateral Weir End=")) {
                used[i] = true;
                auto f = split(val("Lateral Weir End="), ',');
                sd.lateral.toName = f.size() > 3 ? trim(f[3]) : std::string();
            } else if (startsWith(l, "Lateral Weir SE=")) {
                used[i] = true;
                const int n = leadingInt(l.substr(16));
                size_t j = i + 1;
                std::vector<double> v;
                if (!numbers(j, 2 * n, 8, 10, v)) { err = "Lateral Weir SE"; return false; }
                for (int k = 0; k < n; ++k) sd.weir.push_back({v[size_t(2 * k)], v[size_t(2 * k + 1)]});
                i = j - 1;
            }
        }
        if (sd.weir.size() < 2) { err = err.empty() ? "Lateral Weir SE" : err; return false; }
        sd.parsed = true;
        return true;
    }
    if (type == NodeType::Bridge && sd.deckUp.size() < 2) { err = err.empty() ? "Deck" : err; return false; }
    if (type == NodeType::Culvert && (sd.culverts.empty() || sd.deckUp.size() < 2)) { err = err.empty() ? "Culvert" : err; return false; }
    if (type == NodeType::InlineStructure && sd.weir.size() < 2) { err = err.empty() ? "Inline Weir SE" : err; return false; }
    sd.parsed = any || type == NodeType::InlineStructure;
    return sd.parsed;
}

static std::string f8(double v) { return formatFixed(v, 8); }

static std::vector<std::string> structureLines(NodeType type, const StructureData& sd) {
    std::vector<std::string> out;
    auto arr = [&](const std::vector<double>& v) { for (auto& l : fixedLines(v, 8, 10)) out.push_back(l); };
    if (type == NodeType::Bridge || type == NodeType::Culvert) {
        out.push_back("Bridge Culvert-1,0,-1,-1, 0");
        out.push_back("Deck Dist Width WeirC Skew NumUp NumDn MinLoCord MaxHiCord MaxSubmerge Is_Ogee");
        out.push_back(f8(sd.dist) + f8(sd.width) + f8(sd.weirCoef) + f8(sd.skew) + f8(double(sd.deckUp.size())) +
                      f8(double(sd.deckDn.size())) + std::string(16, ' ') + f8(sd.maxSubmerge) + f8(sd.ogee ? -1 : 0));
        for (const auto* d : {&sd.deckUp, &sd.deckDn}) {
            std::vector<double> st, hi, lo;
            for (const auto& p : *d) { st.push_back(p.station); hi.push_back(p.high); lo.push_back(p.low); }
            arr(st);
            arr(hi);
            arr(lo);
        }
        for (const auto& p : sd.piers) {
            out.push_back("Pier Skew, UpSta & Num, DnSta & Num=" + formatPlain(p.skew) + "," + formatPlain(p.upSta) + "," +
                          std::to_string(p.upWidth.size()) + "," + formatPlain(p.dnSta) + "," + std::to_string(p.dnWidth.size()));
            arr(p.upWidth);
            arr(p.upElev);
            arr(p.dnWidth);
            arr(p.dnElev);
        }
        for (const auto& c : sd.culverts)
            for (size_t k = 0; k < c.upStations.size(); ++k)
                out.push_back("Culvert=" + std::to_string(c.shape) + "," + formatPlain(c.span) + "," + formatPlain(c.rise) + "," +
                              formatPlain(c.length) + "," + formatPlain(c.n) + "," + formatPlain(c.ke) + "," + formatPlain(c.kx) + "," +
                              std::to_string(c.chart) + "," + std::to_string(c.scale) + "," + formatPlain(c.upInvert) + "," +
                              formatPlain(c.upStations[k]) + "," + formatPlain(c.dnInvert) + "," +
                              formatPlain(k < c.dnStations.size() ? c.dnStations[k] : c.upStations[k]) + "," + c.name);
    } else if (type == NodeType::LateralStructure) {
        out.push_back("Lateral Weir Pos= " + std::to_string(sd.lateral.side));
        out.push_back("Lateral Weir End=" + std::string(16, ' ') + "," + std::string(16, ' ') + "," + std::string(8, ' ') + "," +
                      padRight(sd.lateral.toName, 16));
        out.push_back("Lateral Weir Distance=" + formatPlain(sd.dist));
        out.push_back("Lateral Weir WD=" + formatPlain(sd.width));
        out.push_back("Lateral Weir Coef=" + formatPlain(sd.weirCoef));
        out.push_back("Lateral Weir SE= " + std::to_string(sd.weir.size()));
        std::vector<double> v;
        for (const auto& p : sd.weir) { v.push_back(p.station); v.push_back(p.elev); }
        arr(v);
    } else if (type == NodeType::InlineStructure) {
        out.push_back("IW Dist,WD,Coef,Skew,MaxSub,Min_El,Is_Ogee,SpillHt,DesHd=" + formatPlain(sd.dist) + "," + formatPlain(sd.width) +
                      "," + formatPlain(sd.weirCoef) + "," + formatPlain(sd.skew) + "," + formatPlain(sd.maxSubmerge) + ",," +
                      (sd.ogee ? "-1" : "0") + ",,");
        out.push_back("#Inline Weir SE= " + std::to_string(sd.weir.size()));
        std::vector<double> v;
        for (const auto& p : sd.weir) { v.push_back(p.station); v.push_back(p.elev); }
        arr(v);
        for (const auto& g : sd.gates) {
            out.push_back("IW Gate Name Wd,H,InvEl,GCoef,Exp_T,Exp_O,Exp_H,Type,WCoef,Is_Ogee,SpillHt,DesHd,#Openings");
            out.push_back(padRight(g.name, 16) + "," + formatPlain(g.width) + "," + formatPlain(g.height) + "," + formatPlain(g.invert) +
                          "," + formatPlain(g.coef) + ",0,0,0," + std::to_string(g.type) + "," + formatPlain(g.weirCoef) + ",0,,," +
                          std::to_string(g.stations.size()));
            arr(g.stations);
        }
    }
    return out;
}

// Replace the understood structure lines of a node, keep everything else.
static void replaceStructureLines(std::vector<std::string>& lines, NodeType type, const StructureData& sd) {
    StructureData tmp;
    std::vector<bool> used;
    std::string err;
    parseStructureLines(lines, type, tmp, used, err);
    size_t at = lines.size();
    for (size_t i = 0; i < lines.size(); ++i) if (used[i]) { at = i; break; }
    if (at == lines.size()) {  // after the last key line of the header part
        at = 1;
        for (size_t i = 0; i < lines.size(); ++i)
            if (startsWith(lines[i], "Node Last Edited Time=") || startsWith(lines[i], "END DESCRIPTION")) at = i + 1;
    }
    std::vector<std::string> out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i == at) for (auto& l : structureLines(type, sd)) out.push_back(l);
        if (!used[i]) out.push_back(lines[i]);
    }
    if (at >= lines.size()) for (auto& l : structureLines(type, sd)) out.push_back(l);
    // keep the trailing blank line last
    for (size_t i = 0; i + 1 < out.size(); ++i)
        if (isBlank(out[i]) && !isBlank(out.back())) { out.erase(out.begin() + long(i)); out.push_back(""); break; }
    lines = out;
}

// ---------------- storage areas, 2D areas, connections, BC lines (M6) ----------------
// Block formats follow HEC-RAS geometry files as far as known; they are not
// verified against HEC-RAS yet. Lines De-RAS does not interpret are kept.
static std::vector<XY> readXY(const Item& it, size_t keyLen) {
    std::vector<XY> pts;
    const int n = leadingInt(it.lines[0].substr(keyLen));
    std::vector<std::string> data(it.lines.begin() + 1, it.lines.end());
    bool ok = true;
    auto v = readFixed(data, 16, 2 * n, ok);
    for (size_t k = 0; k + 1 < v.size(); k += 2) pts.emplace_back(v[k], v[k + 1]);
    return pts;
}
static std::vector<std::string> xyLines(const std::string& key, const std::vector<XY>& pts) {
    std::vector<std::string> out{key + " " + std::to_string(pts.size()) + " "};
    std::vector<double> v;
    for (const auto& p : pts) { v.push_back(p.first); v.push_back(p.second); }
    for (auto& l : fixedLines(v, 16, 4)) out.push_back(l);
    return out;
}
static std::vector<std::string> pairLines(const std::string& key, const std::vector<std::pair<double, double>>& pts) {
    std::vector<std::string> out{key + " " + std::to_string(pts.size()) + " "};
    std::vector<double> v;
    for (const auto& p : pts) { v.push_back(p.first); v.push_back(p.second); }
    for (auto& l : fixedLines(v, 8, 10)) out.push_back(l);
    return out;
}
static std::vector<std::pair<double, double>> readPairs(const Item& it, size_t keyLen) {
    std::vector<std::pair<double, double>> out;
    const int n = leadingInt(it.lines[0].substr(keyLen));
    std::vector<std::string> data(it.lines.begin() + 1, it.lines.end());
    bool ok = true;
    auto v = readFixed(data, 8, 2 * n, ok);
    for (size_t k = 0; k + 1 < v.size(); k += 2) out.emplace_back(v[k], v[k + 1]);
    return out;
}
static XY centroid(const std::vector<XY>& p) {
    if (p.empty()) return {0, 0};
    double x = 0, y = 0;
    for (const auto& q : p) { x += q.first; y += q.second; }
    return {x / double(p.size()), y / double(p.size())};
}
static double fieldNum(const std::string& v, size_t k, double def) {
    auto f = split(v, ',');
    return k < f.size() ? parseNumber(trim(f[k])).value_or(def) : def;
}

static const char* kAreaKeys[] = {"Storage Area=", "Storage Area Surface Line=", "Storage Area Is2D=", "Storage Area Area=",
                                  "Storage Area Min Elev=", "Storage Area Point Generation Data=", "Storage Area Vol Elev=",
                                  "Storage Area Mannings="};
static StorageArea parseAreaBlock(const std::vector<std::string>& L) {
    StorageArea a;
    for (const auto& it : toItems(L)) {
        const std::string k = it.key();
        const std::string v = it.lines[0].substr(std::min(k.size(), it.lines[0].size()));
        if (k == "Storage Area=") a.name = trim(split(v, ',').empty() ? v : split(v, ',')[0]);
        else if (k == "Storage Area Surface Line=") a.polygon = readXY(it, k.size());
        else if (k == "Storage Area Is2D=") a.is2D = trim(v) != "0" && !trim(v).empty();
        else if (k == "Storage Area Area=") a.area = parseNumber(trim(v)).value_or(0);
        else if (k == "Storage Area Min Elev=") a.minElev = parseNumber(trim(v)).value_or(0);
        else if (k == "Storage Area Point Generation Data=") a.cellSize = fieldNum(v, 2, 0);
        else if (k == "Storage Area Vol Elev=") a.volElev = readPairs(it, k.size());
        else if (k == "Storage Area Mannings=") a.manningN = parseNumber(trim(v)).value_or(0.06);
        else a.otherLines.insert(a.otherLines.end(), it.lines.begin(), it.lines.end());
    }
    while (!a.otherLines.empty() && isBlank(a.otherLines.back())) a.otherLines.pop_back();
    return a;
}
static std::vector<std::vector<std::string>> areaItemLines(const StorageArea& a) {
    const XY c = centroid(a.polygon);
    std::vector<std::vector<std::string>> it;
    it.push_back({"Storage Area=" + padRight(a.name, 16) + "," + formatPlain(c.first) + "," + formatPlain(c.second)});
    it.push_back(xyLines("Storage Area Surface Line=", a.polygon));
    it.push_back({"Storage Area Is2D=" + std::string(a.is2D ? "-1" : "0")});
    it.push_back({"Storage Area Area=" + (a.area > 0 ? formatPlain(a.area) : std::string())});
    it.push_back({"Storage Area Min Elev=" + (a.area > 0 || !a.volElev.empty() ? formatPlain(a.minElev) : std::string())});
    it.push_back({"Storage Area Point Generation Data=,," + (a.cellSize > 0 ? formatPlain(a.cellSize) + "," + formatPlain(a.cellSize) : std::string(","))});
    it.push_back(pairLines("Storage Area Vol Elev=", a.volElev));
    it.push_back({"Storage Area Mannings=" + formatPlain(a.manningN)});
    return it;
}
static std::vector<std::string> areaLines(const StorageArea& a, const std::vector<std::string>* orig) {
    const auto fresh = areaItemLines(a);
    if (!orig) {
        std::vector<std::string> out;
        for (size_t i = 0; i < fresh.size(); ++i) {
            out.insert(out.end(), fresh[i].begin(), fresh[i].end());
            if (i == 1) out.insert(out.end(), a.otherLines.begin(), a.otherLines.end());
        }
        out.push_back("");
        return out;
    }
    auto items = toItems(*orig);
    std::vector<std::string> prev{"Storage Area="};
    for (size_t i = 0; i < fresh.size(); ++i) {
        setItem(items, kAreaKeys[i], fresh[i], prev);
        prev.push_back(kAreaKeys[i]);
    }
    auto out = fromItems(items);
    if (out.empty() || !isBlank(out.back())) out.push_back("");
    return out;
}

static const char* kConnKeys[] = {"Connection=", "Connection Line=", "Connection Up SA=", "Connection Dn SA=", "Conn Weir WD=",
                                  "Conn Weir Coef=", "Conn Weir SE="};
static SaConnection parseConnBlock(const std::vector<std::string>& L) {
    SaConnection c;
    for (const auto& it : toItems(L)) {
        const std::string k = it.key();
        const std::string v = it.lines[0].substr(std::min(k.size(), it.lines[0].size()));
        if (k == "Connection=") c.name = trim(split(v, ',').empty() ? v : split(v, ',')[0]);
        else if (k == "Connection Line=") c.line = readXY(it, k.size());
        else if (k == "Connection Up SA=") c.upName = trim(v);
        else if (k == "Connection Dn SA=") c.dnName = trim(v);
        else if (k == "Conn Weir WD=") c.width = parseNumber(trim(v)).value_or(10);
        else if (k == "Conn Weir Coef=") c.coef = parseNumber(trim(v)).value_or(1.44);
        else if (k == "Conn Weir SE=") for (const auto& p : readPairs(it, k.size())) c.weir.push_back({p.first, p.second});
        else c.otherLines.insert(c.otherLines.end(), it.lines.begin(), it.lines.end());
    }
    while (!c.otherLines.empty() && isBlank(c.otherLines.back())) c.otherLines.pop_back();
    return c;
}
static std::vector<std::string> connLines(const SaConnection& c, const std::vector<std::string>* orig) {
    const XY m = centroid(c.line);
    std::vector<std::pair<double, double>> se;
    for (const auto& p : c.weir) se.emplace_back(p.station, p.elev);
    std::vector<std::vector<std::string>> fresh{
        {"Connection=" + padRight(c.name, 16) + "," + formatPlain(m.first) + "," + formatPlain(m.second)},
        xyLines("Connection Line=", c.line),
        {"Connection Up SA=" + c.upName},
        {"Connection Dn SA=" + c.dnName},
        {"Conn Weir WD=" + formatPlain(c.width)},
        {"Conn Weir Coef=" + formatPlain(c.coef)},
        pairLines("Conn Weir SE=", se)};
    std::vector<std::string> out;
    if (!orig) {
        for (size_t i = 0; i < fresh.size(); ++i) {
            out.insert(out.end(), fresh[i].begin(), fresh[i].end());
            if (i == 1) out.insert(out.end(), c.otherLines.begin(), c.otherLines.end());
        }
    } else {
        auto items = toItems(*orig);
        std::vector<std::string> prev{"Connection="};
        for (size_t i = 0; i < fresh.size(); ++i) { setItem(items, kConnKeys[i], fresh[i], prev); prev.push_back(kConnKeys[i]); }
        out = fromItems(items);
    }
    if (out.empty() || !isBlank(out.back())) out.push_back("");
    return out;
}

static BcLine parseBcBlock(const std::vector<std::string>& L) {
    BcLine b;
    for (const auto& it : toItems(L)) {
        const std::string k = it.key();
        const std::string v = it.lines[0].substr(std::min(k.size(), it.lines[0].size()));
        if (k == "BC Line Name=") b.name = trim(v);
        else if (k == "BC Line Storage Area=") b.area = trim(v);
        else if (k == "BC Line Arc=") b.line = readXY(it, k.size());
        else b.otherLines.insert(b.otherLines.end(), it.lines.begin(), it.lines.end());
    }
    while (!b.otherLines.empty() && isBlank(b.otherLines.back())) b.otherLines.pop_back();
    return b;
}
static std::vector<std::string> bcLines(const BcLine& b, const std::vector<std::string>* orig) {
    std::vector<std::vector<std::string>> fresh{{"BC Line Name=" + b.name}, {"BC Line Storage Area=" + b.area}, xyLines("BC Line Arc=", b.line)};
    static const char* keys[] = {"BC Line Name=", "BC Line Storage Area=", "BC Line Arc="};
    std::vector<std::string> out;
    if (!orig) {
        for (auto& f : fresh) out.insert(out.end(), f.begin(), f.end());
        out.insert(out.end(), b.otherLines.begin(), b.otherLines.end());
    } else {
        auto items = toItems(*orig);
        std::vector<std::string> prev{"BC Line Name="};
        for (size_t i = 0; i < fresh.size(); ++i) { setItem(items, keys[i], fresh[i], prev); prev.push_back(keys[i]); }
        out = fromItems(items);
    }
    if (out.empty() || !isBlank(out.back())) out.push_back("");
    return out;
}

std::string rasTimestampNow() {
    static const char* mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s/%02d/%04d %02d:%02d:%02d", mon[tmv.tm_mon], tmv.tm_mday, tmv.tm_year + 1900,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

bool GeometryFile::load(const fs::path& path, MessageList& msgs) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_OPEN_FAILED", {fromPath(path)}));
        return false;
    }
    return parse(bytes, fromPath(path.filename()), msgs);
}

bool GeometryFile::parse(const std::string& bytes, const std::string& sourceName, MessageList& msgs) {
    doc_ = TextDoc::fromString(bytes);
    isNew_ = false;
    parseSegments(msgs, sourceName);
    original_ = model;
    return true;
}

GeometryFile GeometryFile::createNew(const std::string& title, const std::string& programVersion) {
    GeometryFile g;
    g.isNew_ = true;
    g.doc_.eol = "\r\n";
    g.doc_.finalEol = true;
    Segment h;
    h.kind = Kind::Header;
    h.lines = {"Geom Title=" + title, "Program Version=" + programVersion, ""};
    g.segs_.push_back(h);
    g.model.title = title;
    g.model.programVersion = programVersion;
    g.original_ = g.model;
    return g;
}

void GeometryFile::parseSegments(MessageList& msgs, const std::string& src) {
    segs_.clear();
    model = Geometry{};
    const auto& L = doc_.lines;
    Segment cur;
    cur.kind = Kind::Header;
    bool inDescription = false;
    bool blockClosed = false;  // a blank line ended the current Reach/Node block

    auto flush = [&]() {
        if (!cur.lines.empty() || cur.kind == Kind::Header) segs_.push_back(cur);
        cur = Segment{};
    };

    for (size_t i = 0; i < L.size(); ++i) {
        const std::string& l = L[i];
        if (inDescription) {
            cur.lines.push_back(l);
            if (startsWith(l, "END DESCRIPTION")) inDescription = false;
            continue;
        }
        bool startReach = startsWith(l, kReachKey);
        bool startNode = startsWith(l, kTypeKey);
        bool startOther = isOtherBlockStart(l);
        if (startReach || startNode || startOther) {
            flush();
            cur.kind = startReach ? Kind::Reach : (startNode ? Kind::Node : Kind::Raw);
            blockClosed = false;
            cur.lines.push_back(l);
            continue;
        }
        if ((cur.kind == Kind::Reach || cur.kind == Kind::Node) && blockClosed && !isBlank(l)) {
            flush();
            cur.kind = Kind::Raw;
            blockClosed = false;
        }
        if (startsWith(l, "BEGIN DESCRIPTION")) inDescription = true;
        cur.lines.push_back(l);
        if (isBlank(l) && (cur.kind == Kind::Reach || cur.kind == Kind::Node)) blockClosed = true;
    }
    flush();

    // Interpret segments.
    Reach* curReach = nullptr;
    int lineNo = 1;
    for (auto& s : segs_) {
        const int segLine = lineNo;
        lineNo += static_cast<int>(s.lines.size());
        if (s.kind == Kind::Header) {
            for (const auto& l : s.lines) {
                if (auto v = valueOf(l, "Geom Title=")) model.title = *v;
                else if (auto v2 = valueOf(l, "Program Version=")) model.programVersion = *v2;
            }
        } else if (s.kind == Kind::Raw) {
            if (!s.lines.empty() && startsWith(s.lines[0], "Junct Name=")) {
                Junction j;
                j.name = trim(s.lines[0].substr(11));
                bool seenRefs = false;
                for (size_t li = 1; li < s.lines.size(); ++li) {
                    const std::string& l = s.lines[li];
                    const bool ref = startsWith(l, "Up River,Reach=") || startsWith(l, "Dn River,Reach=") || startsWith(l, "Junc L&A=");
                    if (ref) seenRefs = true;
                    else (seenRefs ? j.linesAfter : j.linesBefore).push_back(l);
                }
                for (const auto& l : s.lines) {
                    auto pair = [&](const char* key) {
                        auto parts = split(valueOf(l, key).value_or(""), ',');
                        return std::make_pair(trim(parts.size() > 0 ? parts[0] : ""), trim(parts.size() > 1 ? parts[1] : ""));
                    };
                    if (startsWith(l, "Up River,Reach=")) j.up.push_back(pair("Up River,Reach="));
                    else if (startsWith(l, "Dn River,Reach=")) j.dn.push_back(pair("Dn River,Reach="));
                    else if (startsWith(l, "Junc L&A=")) {
                        auto parts = split(l.substr(9), ',');
                        j.lengths.push_back(parts.empty() ? 0 : parseNumber(parts[0]).value_or(0));
                    }
                }
                model.junctions.push_back(j);
            }
            if (!s.lines.empty() && startsWith(s.lines[0], "Storage Area=")) model.storageAreas.push_back(parseAreaBlock(s.lines));
            if (!s.lines.empty() && startsWith(s.lines[0], "Connection=")) model.connections.push_back(parseConnBlock(s.lines));
            if (!s.lines.empty() && startsWith(s.lines[0], "BC Line Name=")) model.bcLines.push_back(parseBcBlock(s.lines));
            for (const auto& l : s.lines) {
                if (startsWith(l, "Junct Name=")) model.junctionCount++;
                if (startsWith(l, "Storage Area=")) model.storageAreaCount++;
                if (startsWith(l, "Storage Area Is2D=") && trim(l.substr(18)) != "0") model.area2DCount++;
            }
        } else if (s.kind == Kind::Reach) {
            Reach r;
            r.uid = nextUid();
            s.uid = r.uid;
            auto v = valueOf(s.lines[0], kReachKey).value_or("");
            auto parts = split(v, ',');
            r.river = trim(parts.size() > 0 ? parts[0] : "");
            r.reach = trim(parts.size() > 1 ? parts[1] : "");
            auto items = toItems(s.lines);
            int xyIdx = findItem(items, "Reach XY=");
            if (xyIdx >= 0) {
                const auto& it = items[static_cast<size_t>(xyIdx)];
                int n = leadingInt(it.lines[0].substr(9));
                bool ok = true;
                std::vector<std::string> data(it.lines.begin() + 1, it.lines.end());
                auto vals = readFixed(data, 16, 2 * n, ok);
                if (!ok) msgs.push_back(makeMsg(Severity::Warning, "PARSE_COUNT_MISMATCH",
                                                {"Reach XY", std::to_string(2 * n), std::to_string(vals.size()), src}, r.label()));
                for (size_t k = 0; k + 1 < vals.size(); k += 2) r.xy.emplace_back(vals[k], vals[k + 1]);
            }
            model.reaches.push_back(r);
            curReach = &model.reaches.back();
        } else if (s.kind == Kind::Node) {
            CrossSection xs;
            xs.uid = nextUid();
            s.uid = xs.uid;
            auto tv = valueOf(s.lines[0], kTypeKey).value_or("");
            auto tp = split(tv, ',');
            int type = tp.size() > 0 ? static_cast<int>(parseNumber(tp[0]).value_or(0)) : 0;
            xs.type = (type >= 1 && type <= 6) ? static_cast<NodeType>(type) : NodeType::Other;
            xs.rs = tp.size() > 1 ? trim(tp[1]) : "";
            xs.lenLeft = tp.size() > 2 ? parseNumber(tp[2]).value_or(0) : 0;
            xs.lenChannel = tp.size() > 3 ? parseNumber(tp[3]).value_or(0) : 0;
            xs.lenRight = tp.size() > 4 ? parseNumber(tp[4]).value_or(0) : 0;
            const std::string loc = (curReach ? curReach->label() : std::string("?")) + " RS " + xs.rs;

            auto items = toItems(s.lines);
            std::vector<bool> permanent;
            for (const auto& it : items) {
                const std::string key = it.key();
                std::vector<std::string> data(it.lines.begin() + 1, it.lines.end());
                if (key == "BEGIN DESCRIPTION:") {
                    std::string d;
                    for (size_t k = 1; k + 1 < it.lines.size(); ++k) {
                        if (k > 1) d += "\n";
                        d += it.lines[k];
                    }
                    xs.description = d;
                } else if (key == "#Sta/Elev=") {
                    int n = leadingInt(it.lines[0].substr(10));
                    bool ok = true;
                    auto vals = readFixed(data, 8, 2 * n, ok);
                    if (!ok) msgs.push_back(makeMsg(Severity::Error, "PARSE_COUNT_MISMATCH",
                                                    {"#Sta/Elev", std::to_string(2 * n), std::to_string(vals.size()), src}, loc));
                    for (size_t k = 0; k + 1 < vals.size(); k += 2) xs.staElev.push_back({vals[k], vals[k + 1]});
                } else if (key == "#Mann=") {
                    auto parts = split(it.lines[0].substr(6), ',');
                    int n = parts.size() > 0 ? static_cast<int>(parseNumber(parts[0]).value_or(0)) : 0;
                    xs.manningFormat = parts.size() > 1 ? static_cast<int>(parseNumber(parts[1]).value_or(0)) : 0;
                    bool ok = true;
                    auto vals = readFixed(data, 8, 3 * n, ok);
                    if (!ok) msgs.push_back(makeMsg(Severity::Error, "PARSE_COUNT_MISMATCH",
                                                    {"#Mann", std::to_string(3 * n), std::to_string(vals.size()), src}, loc));
                    for (size_t k = 0; k + 2 < vals.size() + 0 && k + 1 < vals.size(); k += 3) xs.manning.push_back({vals[k], vals[k + 1]});
                } else if (key == "Bank Sta=") {
                    auto parts = split(it.lines[0].substr(9), ',');
                    if (parts.size() >= 2) {
                        auto a = parseNumber(parts[0]);
                        auto b = parseNumber(parts[1]);
                        if (a) xs.bankLeft = *a;
                        if (b) xs.bankRight = *b;
                    }
                } else if (key == "Exp/Cntr=") {
                    auto parts = split(it.lines[0].substr(9), ',');
                    if (parts.size() >= 2) {
                        xs.expansion = parseNumber(parts[0]).value_or(0.3);
                        xs.contraction = parseNumber(parts[1]).value_or(0.1);
                    }
                } else if (key == "#XS Ineff=" || key == "#Block Obstruct=") {
                    const bool ineff = key == "#XS Ineff=";
                    auto parts = split(it.lines[0].substr(key.size()), ',');
                    int n = parts.size() > 0 ? static_cast<int>(parseNumber(parts[0]).value_or(0)) : 0;
                    int mode = parts.size() > 1 ? static_cast<int>(parseNumber(parts[1]).value_or(0)) : 0;
                    bool ok = true;
                    auto vals = readFixed(data, 8, 3 * n, ok);
                    if (!ok) msgs.push_back(makeMsg(Severity::Warning, "PARSE_COUNT_MISMATCH",
                                                    {key.substr(0, key.size() - 1), std::to_string(3 * n), std::to_string(vals.size()), src}, loc));
                    std::vector<StationBlock> blocks;
                    for (size_t k = 0; k + 2 < vals.size(); k += 3) blocks.push_back({vals[k], vals[k + 1], vals[k + 2], false});
                    if (ineff) { xs.ineffective = blocks; xs.ineffectiveMode = mode; }
                    else { xs.obstructions = blocks; xs.obstructionMode = mode; }
                } else if (key == "Permanent Ineff=") {
                    for (const auto& l : data)
                        for (const auto& f : fixedFields(l, 8)) {
                            std::string t = trim(f);
                            if (!t.empty()) permanent.push_back(t == "T" || t == "t" || t == "-1");
                        }
                } else if (key == "Levee=") {
                    auto parts = split(it.lines[0].substr(6), ',');
                    auto num = [&](size_t i) { return i < parts.size() ? parseNumber(parts[i]) : std::nullopt; };
                    if (auto a = num(1), b = num(2); a && b) xs.leveeLeft = {true, *a, *b};
                    if (auto a = num(4), b = num(5); a && b) xs.leveeRight = {true, *a, *b};
                } else if (key == "XS GIS Cut Line=") {
                    int n = leadingInt(it.lines[0].substr(16));
                    bool ok = true;
                    auto vals = readFixed(data, 16, 2 * n, ok);
                    if (!ok) msgs.push_back(makeMsg(Severity::Warning, "PARSE_COUNT_MISMATCH",
                                                    {"XS GIS Cut Line", std::to_string(2 * n), std::to_string(vals.size()), src}, loc));
                    for (size_t k = 0; k + 1 < vals.size(); k += 2) xs.cutLine.emplace_back(vals[k], vals[k + 1]);
                } else if (key == "Vertical n Elevations=" || key == "#Vert n=") {
                    xs.ignoredFeatures.push_back("Vertical Manning's n");
                }
            }
            for (size_t k = 0; k < xs.ineffective.size() && k < permanent.size(); ++k) xs.ineffective[k].permanent = permanent[k];
            if (xs.type == NodeType::Bridge || xs.type == NodeType::Culvert || xs.type == NodeType::InlineStructure ||
                xs.type == NodeType::LateralStructure) {
                std::vector<bool> used;
                std::string serr;
                if (!parseStructureLines(s.lines, xs.type, xs.structure, used, serr))
                    msgs.push_back(makeMsg(Severity::Warning, "GEOM_STRUCTURE_UNREAD", {nodeTypeName(xs.type), serr.empty() ? "?" : serr}, loc));
            }
            if (xs.manningFormat != 0 && xs.manningFormat != -1)
                xs.ignoredFeatures.push_back("Manning's n format " + std::to_string(xs.manningFormat));
            if (curReach) curReach->nodes.push_back(xs);
            if (xs.type != NodeType::CrossSection && !xs.structure.parsed)
                msgs.push_back(makeMsg(Severity::Info, "GEOM_NODE_UNSUPPORTED",
                                       {std::to_string(static_cast<int>(xs.type)), nodeTypeName(xs.type)}, loc));
            (void)segLine;
        }
    }
}

std::vector<std::string> GeometryFile::reachHeaderLines(const Reach& r) {
    std::vector<std::string> out;
    out.push_back(std::string(kReachKey) + padRight(r.river, 16) + "," + padRight(r.reach, 16));
    out.push_back("Reach XY= " + std::to_string(r.xy.size()) + " ");
    std::vector<double> vals;
    for (const auto& p : r.xy) { vals.push_back(p.first); vals.push_back(p.second); }
    for (auto& l : fixedLines(vals, 16, 4)) out.push_back(l);
    double tx = 0, ty = 0;
    if (!r.xy.empty()) {
        const auto& m = r.xy[r.xy.size() / 2];
        tx = m.first;
        ty = m.second;
    }
    out.push_back("Rch Text X Y=" + formatPlain(tx) + "," + formatPlain(ty));
    out.push_back("Reverse River Text= 0 ");
    out.push_back("");
    return out;
}

static std::vector<std::string> typeLine(const CrossSection& xs) {
    if (xs.type != NodeType::CrossSection && xs.lenLeft == 0 && xs.lenChannel == 0 && xs.lenRight == 0)
        return {std::string(kTypeKey) + " " + std::to_string(static_cast<int>(xs.type)) + " ," + padRight(xs.rs, 8) + ",,,"};
    return {std::string(kTypeKey) + " " + std::to_string(static_cast<int>(xs.type)) + " ," + padRight(xs.rs, 8) + "," +
            formatPlain(xs.lenLeft) + "," + formatPlain(xs.lenChannel) + "," + formatPlain(xs.lenRight)};
}

static std::vector<std::string> staElevLines(const CrossSection& xs) {
    std::vector<std::string> out{"#Sta/Elev= " + std::to_string(xs.staElev.size()) + " "};
    std::vector<double> v;
    for (const auto& p : xs.staElev) { v.push_back(p.station); v.push_back(p.elev); }
    for (auto& l : fixedLines(v, 8, 10)) out.push_back(l);
    return out;
}

static std::vector<std::string> manningLines(const CrossSection& xs) {
    std::string f = xs.manningFormat == 0 ? " 0 " : (xs.manningFormat < 0 ? std::to_string(xs.manningFormat) + " "
                                                                           : " " + std::to_string(xs.manningFormat) + " ");
    std::vector<std::string> out{"#Mann= " + std::to_string(xs.manning.size()) + " ," + f + ", 0 "};
    std::vector<double> v;
    for (const auto& m : xs.manning) { v.push_back(m.station); v.push_back(m.n); v.push_back(0); }
    for (auto& l : fixedLines(v, 8, 9)) out.push_back(l);
    return out;
}

static bool sameBlocks(const std::vector<StationBlock>& a, const std::vector<StationBlock>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].staStart != b[i].staStart || a[i].staEnd != b[i].staEnd || a[i].elev != b[i].elev || a[i].permanent != b[i].permanent)
            return false;
    return true;
}

static std::vector<std::string> blockLines(const char* key, const std::vector<StationBlock>& v, int mode) {
    std::vector<std::string> out{std::string(key) + " " + std::to_string(v.size()) + " ," + (mode < 0 ? std::to_string(mode) : " " + std::to_string(mode)) + " "};
    std::vector<double> vals;
    for (const auto& b : v) { vals.push_back(b.staStart); vals.push_back(b.staEnd); vals.push_back(b.elev); }
    for (auto& l : fixedLines(vals, 8, 9)) out.push_back(l);
    return out;
}

static std::vector<std::string> permanentLines(const std::vector<StationBlock>& v) {
    std::vector<std::string> out{"Permanent Ineff="};
    std::string line;
    for (size_t i = 0; i < v.size(); ++i) {
        line += std::string(7, ' ') + (v[i].permanent ? "T" : "F");
        if ((i + 1) % 10 == 0) { out.push_back(line); line.clear(); }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

static std::string leveeLine(const CrossSection& xs) {
    auto side = [](const LeveePoint& p) { return p.present ? "-1," + formatPlain(p.station) + "," + formatPlain(p.elev) : std::string("0,,"); };
    return "Levee=" + side(xs.leveeLeft) + "," + side(xs.leveeRight) + ",,,,";
}

static bool sameLevees(const CrossSection& a, const CrossSection& b) {
    auto eq = [](const LeveePoint& x, const LeveePoint& y) { return x.present == y.present && (!x.present || (x.station == y.station && x.elev == y.elev)); };
    return eq(a.leveeLeft, b.leveeLeft) && eq(a.leveeRight, b.leveeRight);
}

// Ineffective areas, obstructions and levees: rewritten only when they changed.
static void updateExtraItems(std::vector<Item>& items, const CrossSection& xs, const CrossSection* orig) {
    if (!orig || !sameBlocks(xs.ineffective, orig->ineffective) || xs.ineffectiveMode != orig->ineffectiveMode) {
        if (xs.ineffective.empty()) { removeItem(items, "#XS Ineff="); removeItem(items, "Permanent Ineff="); }
        else {
            setItem(items, "#XS Ineff=", blockLines("#XS Ineff=", xs.ineffective, xs.ineffectiveMode), {"Bank Sta="});
            setItem(items, "Permanent Ineff=", permanentLines(xs.ineffective), {"#XS Ineff="});
        }
    }
    if (!orig || !sameBlocks(xs.obstructions, orig->obstructions) || xs.obstructionMode != orig->obstructionMode) {
        if (xs.obstructions.empty()) removeItem(items, "#Block Obstruct=");
        else setItem(items, "#Block Obstruct=", blockLines("#Block Obstruct=", xs.obstructions, xs.obstructionMode),
                     {"Permanent Ineff=", "#XS Ineff=", "Bank Sta="});
    }
    if (!orig || !sameLevees(xs, *orig)) {
        if (!xs.leveeLeft.present && !xs.leveeRight.present) removeItem(items, "Levee=");
        else setItem(items, "Levee=", {leveeLine(xs)}, {"#Block Obstruct=", "Permanent Ineff=", "#XS Ineff=", "Bank Sta="});
    }
}

static std::vector<std::string> cutLineLines(const CrossSection& xs) {
    std::vector<std::string> out{"XS GIS Cut Line=" + std::to_string(xs.cutLine.size())};
    std::vector<double> v;
    for (const auto& p : xs.cutLine) { v.push_back(p.first); v.push_back(p.second); }
    for (auto& l : fixedLines(v, 16, 4)) out.push_back(l);
    return out;
}

static std::vector<std::string> descriptionLines(const std::string& d) {
    std::vector<std::string> out{"BEGIN DESCRIPTION:"};
    if (!d.empty())
        for (auto& l : split(d, '\n')) out.push_back(l);
    out.push_back("END DESCRIPTION:");
    return out;
}

std::vector<std::string> GeometryFile::crossSectionLines(const CrossSection& xs, const std::string& editedTime) {
    std::vector<std::string> out = typeLine(xs);
    if (xs.type != NodeType::CrossSection) {
        if (!xs.description.empty())
            for (auto& l : descriptionLines(xs.description)) out.push_back(l);
        out.push_back("Node Last Edited Time=" + editedTime);
        for (auto& l : structureLines(xs.type, xs.structure)) out.push_back(l);
        out.push_back("");
        return out;
    }
    if (!xs.description.empty())
        for (auto& l : descriptionLines(xs.description)) out.push_back(l);
    if (!xs.cutLine.empty())
        for (auto& l : cutLineLines(xs)) out.push_back(l);
    out.push_back("Node Last Edited Time=" + editedTime);
    for (auto& l : staElevLines(xs)) out.push_back(l);
    for (auto& l : manningLines(xs)) out.push_back(l);
    if (xs.bankLeft && xs.bankRight) out.push_back("Bank Sta=" + formatPlain(*xs.bankLeft) + "," + formatPlain(*xs.bankRight));
    if (!xs.ineffective.empty()) {
        for (auto& l : blockLines("#XS Ineff=", xs.ineffective, xs.ineffectiveMode)) out.push_back(l);
        for (auto& l : permanentLines(xs.ineffective)) out.push_back(l);
    }
    if (!xs.obstructions.empty())
        for (auto& l : blockLines("#Block Obstruct=", xs.obstructions, xs.obstructionMode)) out.push_back(l);
    if (xs.leveeLeft.present || xs.leveeRight.present) out.push_back(leveeLine(xs));
    out.push_back("XS Rating Curve= 0 ,0");
    out.push_back("Exp/Cntr=" + formatPlain(xs.expansion) + "," + formatPlain(xs.contraction));
    out.push_back("");
    return out;
}

void GeometryFile::updateNodeLines(std::vector<std::string>& lines, const CrossSection& xs, const CrossSection* orig,
                                   const std::string& editedTime) {
    auto items = toItems(lines);
    items[0].lines = typeLine(xs);
    if (!xs.description.empty()) setItem(items, "BEGIN DESCRIPTION:", descriptionLines(xs.description), {kTypeKey});
    else removeItem(items, "BEGIN DESCRIPTION:");
    if (!orig || xs.cutLine != orig->cutLine) {
        if (xs.cutLine.empty()) removeItem(items, "XS GIS Cut Line=");
        else setItem(items, "XS GIS Cut Line=", cutLineLines(xs), {kTypeKey, "BEGIN DESCRIPTION:"});
    }
    setItem(items, "Node Last Edited Time=", {"Node Last Edited Time=" + editedTime}, {kTypeKey, "BEGIN DESCRIPTION:", "XS GIS Cut Line="});
    if (xs.type == NodeType::CrossSection) {
        setItem(items, "#Sta/Elev=", staElevLines(xs), {"Node Last Edited Time="});
        setItem(items, "#Mann=", manningLines(xs), {"#Sta/Elev="});
        if (xs.bankLeft && xs.bankRight)
            setItem(items, "Bank Sta=", {"Bank Sta=" + formatPlain(*xs.bankLeft) + "," + formatPlain(*xs.bankRight)}, {"#Mann="});
        setItem(items, "Exp/Cntr=", {"Exp/Cntr=" + formatPlain(xs.expansion) + "," + formatPlain(xs.contraction)},
                {"XS HTab Horizontal Distribution=", "XS HTab Starting El and Incr=", "XS Rating Curve=", "Bank Sta="});
        updateExtraItems(items, xs, orig);
    }
    lines = fromItems(items);
    if (xs.type != NodeType::CrossSection && xs.structure.parsed && (!orig || !(xs.structure == orig->structure)))
        replaceStructureLines(lines, xs.type, xs.structure);
    if (lines.empty() || !isBlank(lines.back())) lines.push_back("");
}

void GeometryFile::updateReachLines(std::vector<std::string>& lines, const Reach& r) {
    auto fresh = reachHeaderLines(r);
    auto items = toItems(lines);
    auto freshItems = toItems(fresh);
    items[0].lines = freshItems[0].lines;
    setItem(items, "Reach XY=", freshItems[1].lines, {kReachKey});
    lines = fromItems(items);
}

bool GeometryFile::isModified() const {
    if (isNew_) return true;
    if (model.title != original_.title || model.programVersion != original_.programVersion) return true;
    if (model.reaches.size() != original_.reaches.size()) return true;
    if (model.junctions != original_.junctions) return true;
    if (model.storageAreas != original_.storageAreas || model.connections != original_.connections || model.bcLines != original_.bcLines)
        return true;
    for (size_t i = 0; i < model.reaches.size(); ++i) {
        const auto& a = model.reaches[i];
        const auto& b = original_.reaches[i];
        if (a.uid != b.uid || !a.sameHeader(b) || a.nodes.size() != b.nodes.size()) return true;
        for (size_t k = 0; k < a.nodes.size(); ++k)
            if (a.nodes[k].uid != b.nodes[k].uid || !a.nodes[k].sameData(b.nodes[k])) return true;
    }
    return false;
}

std::string GeometryFile::serialize() const {
    if (!isNew_ && !isModified()) return doc_.toString();

    const std::string now = rasTimestampNow();
    std::map<int, const Segment*> nodeSegByUid;
    for (const auto& s : segs_)
        if (s.kind == Kind::Node) nodeSegByUid[s.uid] = &s;
    std::map<int, const CrossSection*> origNode;
    std::map<int, const Reach*> origReach;
    for (const auto& r : original_.reaches) {
        origReach[r.uid] = &r;
        for (const auto& n : r.nodes) origNode[n.uid] = &n;
    }
    auto emitReach = [&](const Reach& r, const Segment* seg, std::vector<std::string>& out) {
        if (seg) {
            std::vector<std::string> lines = seg->lines;
            auto it = origReach.find(r.uid);
            if (it == origReach.end() || !it->second->sameHeader(r)) updateReachLines(lines, r);
            out.insert(out.end(), lines.begin(), lines.end());
        } else {
            auto lines = reachHeaderLines(r);
            out.insert(out.end(), lines.begin(), lines.end());
        }
        for (const auto& n : r.nodes) {
            auto sit = nodeSegByUid.find(n.uid);
            if (sit != nodeSegByUid.end()) {
                std::vector<std::string> lines = sit->second->lines;
                auto oit = origNode.find(n.uid);
                if (oit == origNode.end() || !oit->second->sameData(n))
                    updateNodeLines(lines, n, oit == origNode.end() ? nullptr : oit->second, now);
                out.insert(out.end(), lines.begin(), lines.end());
            } else {
                auto lines = crossSectionLines(n, now);
                out.insert(out.end(), lines.begin(), lines.end());
            }
        }
    };

    std::set<int> modelReachUids;
    for (const auto& r : model.reaches) modelReachUids.insert(r.uid);
    std::set<int> emitted;
    // Position after which brand-new reaches are inserted: after the last
    // reach/node segment of the original file.
    int lastReachSeg = -1;
    for (size_t i = 0; i < segs_.size(); ++i)
        if (segs_[i].kind == Kind::Reach || segs_[i].kind == Kind::Node) lastReachSeg = static_cast<int>(i);

    std::vector<std::string> out;
    auto emitNewReaches = [&]() {
        for (const auto& r : model.reaches)
            if (!emitted.count(r.uid) && !origReach.count(r.uid)) { emitReach(r, nullptr, out); emitted.insert(r.uid); }
    };
    bool newEmitted = false;
    const bool junctionsChanged = model.junctions != original_.junctions;
    bool junctionsEmitted = false;
    auto emitJunctions = [&](std::vector<std::string>& o) {
        for (const auto& j : model.junctions) {
            o.push_back("Junct Name=" + padRight(j.name, 16));
            std::vector<std::string> before = j.linesBefore;
            if (before.empty()) {
                before.push_back("Junct Desc=, 0 , 0 ,-1 ,0");
                // drawing position: first point of the first downstream reach
                double x = 0, y = 0;
                for (const auto& d : j.dn)
                    if (const Reach* r = model.findReach(d.first, d.second); r && !r->xy.empty()) { x = r->xy.front().first; y = r->xy.front().second; break; }
                before.push_back("Junct X Y & Text X Y=" + formatPlain(x) + "," + formatPlain(y) + "," + formatPlain(x) + "," + formatPlain(y));
            }
            o.insert(o.end(), before.begin(), before.end());
            for (const auto& u : j.up) o.push_back("Up River,Reach=" + padRight(u.first, 16) + "," + padRight(u.second, 16));
            for (const auto& d : j.dn) o.push_back("Dn River,Reach=" + padRight(d.first, 16) + "," + padRight(d.second, 16));
            for (double L : j.lengths) o.push_back("Junc L&A=" + formatPlain(L) + ",0");
            std::vector<std::string> after = j.linesAfter;
            if (after.empty() || !isBlank(after.back())) after.push_back("");
            o.insert(o.end(), after.begin(), after.end());
        }
    };
    // Storage / 2D areas, connections and BC lines: when a kind changed, all
    // of its blocks are written where the first one of that kind was.
    std::map<std::string, const std::vector<std::string>*> origArea, origConn, origBc;
    for (const auto& sg : segs_) {
        if (sg.kind != Kind::Raw || sg.lines.empty()) continue;
        if (startsWith(sg.lines[0], "Storage Area=")) origArea[parseAreaBlock(sg.lines).name] = &sg.lines;
        else if (startsWith(sg.lines[0], "Connection=")) origConn[parseConnBlock(sg.lines).name] = &sg.lines;
        else if (startsWith(sg.lines[0], "BC Line Name=")) origBc[parseBcBlock(sg.lines).name] = &sg.lines;
    }
    const bool areasChanged = model.storageAreas != original_.storageAreas;
    const bool connChanged = model.connections != original_.connections;
    const bool bcChanged = model.bcLines != original_.bcLines;
    bool areasEmitted = false, connEmitted = false, bcEmitted = false;
    auto emitAreas = [&]() {
        for (const auto& a : model.storageAreas) {
            auto it = origArea.find(a.name);
            const std::vector<std::string>* o = it == origArea.end() ? nullptr : it->second;
            bool same = false;
            for (const auto& oa : original_.storageAreas) if (oa == a) same = true;
            auto lines = same && o ? *o : areaLines(a, o);
            out.insert(out.end(), lines.begin(), lines.end());
        }
        areasEmitted = true;
    };
    auto emitConns = [&]() {
        for (const auto& c : model.connections) {
            auto it = origConn.find(c.name);
            const std::vector<std::string>* o = it == origConn.end() ? nullptr : it->second;
            bool same = false;
            for (const auto& oc : original_.connections) if (oc == c) same = true;
            auto lines = same && o ? *o : connLines(c, o);
            out.insert(out.end(), lines.begin(), lines.end());
        }
        connEmitted = true;
    };
    auto emitBcs = [&]() {
        for (const auto& b : model.bcLines) {
            auto it = origBc.find(b.name);
            const std::vector<std::string>* o = it == origBc.end() ? nullptr : it->second;
            bool same = false;
            for (const auto& ob : original_.bcLines) if (ob == b) same = true;
            auto lines = same && o ? *o : bcLines(b, o);
            out.insert(out.end(), lines.begin(), lines.end());
        }
        bcEmitted = true;
    };
    for (size_t i = 0; i < segs_.size(); ++i) {
        const auto& s = segs_[i];
        if (s.kind == Kind::Raw && !s.lines.empty()) {
            const std::string& l0 = s.lines[0];
            if (areasChanged && startsWith(l0, "Storage Area=")) { if (!areasEmitted) emitAreas(); if (static_cast<int>(i) == lastReachSeg) { emitNewReaches(); newEmitted = true; } continue; }
            if (connChanged && startsWith(l0, "Connection=")) { if (!connEmitted) emitConns(); continue; }
            if (bcChanged && startsWith(l0, "BC Line Name=")) { if (!bcEmitted) emitBcs(); continue; }
        }
        if (s.kind == Kind::Header) {
            auto lines = s.lines;
            bool hasTitle = false, hasVer = false;
            for (auto& l : lines) {
                if (startsWith(l, "Geom Title=")) { l = "Geom Title=" + model.title; hasTitle = true; }
                else if (startsWith(l, "Program Version=")) { l = "Program Version=" + model.programVersion; hasVer = true; }
            }
            if (!hasVer) lines.insert(lines.begin() + (hasTitle ? 1 : 0), "Program Version=" + model.programVersion);
            if (!hasTitle) lines.insert(lines.begin(), "Geom Title=" + model.title);
            out.insert(out.end(), lines.begin(), lines.end());
        } else if (s.kind == Kind::Raw) {
            const bool isJunction = !s.lines.empty() && startsWith(s.lines[0], "Junct Name=");
            if (isJunction && junctionsChanged) {
                if (!junctionsEmitted) { emitJunctions(out); junctionsEmitted = true; }
            } else {
                out.insert(out.end(), s.lines.begin(), s.lines.end());
            }
        } else if (s.kind == Kind::Reach) {
            for (const auto& r : model.reaches)
                if (r.uid == s.uid && !emitted.count(r.uid)) { emitReach(r, &s, out); emitted.insert(r.uid); }
        }
        // Node segments are emitted together with their reach.
        if (static_cast<int>(i) == lastReachSeg) { emitNewReaches(); newEmitted = true; }
    }
    if (!newEmitted) emitNewReaches();
    if (junctionsChanged && !junctionsEmitted) emitJunctions(out);
    if (areasChanged && !areasEmitted) emitAreas();
    if (connChanged && !connEmitted) emitConns();
    if (bcChanged && !bcEmitted) emitBcs();

    TextDoc d = doc_;
    if (isNew_) { d.eol = "\r\n"; d.finalEol = true; }
    d.lines = out;
    return d.toString();
}

bool GeometryFile::save(const fs::path& path, MessageList& msgs) {
    std::string err;
    const std::string bytes = serialize();
    if (!writeFileAtomic(path, bytes, err)) {
        msgs.push_back(makeMsg(Severity::Error, "FILE_WRITE_FAILED", {err}));
        return false;
    }
    // After saving, the written text becomes the new baseline.
    MessageList ignore;
    Geometry keep = model;
    parse(bytes, fromPath(path.filename()), ignore);
    // keep uids stable for the UI: map by position (structure is identical)
    for (size_t i = 0; i < model.reaches.size() && i < keep.reaches.size(); ++i) {
        model.reaches[i].uid = keep.reaches[i].uid;
        for (size_t k = 0; k < model.reaches[i].nodes.size() && k < keep.reaches[i].nodes.size(); ++k)
            model.reaches[i].nodes[k].uid = keep.reaches[i].nodes[k].uid;
    }
    for (auto& s : segs_) (void)s;
    // re-sync segment uids with the kept model uids
    size_t ri = 0;
    size_t ni = 0;
    for (auto& s : segs_) {
        if (s.kind == Kind::Reach) {
            if (ri < model.reaches.size()) s.uid = model.reaches[ri].uid;
            ++ri;
            ni = 0;
        } else if (s.kind == Kind::Node) {
            if (ri >= 1 && ri - 1 < model.reaches.size() && ni < model.reaches[ri - 1].nodes.size())
                s.uid = model.reaches[ri - 1].nodes[ni].uid;
            ++ni;
        }
    }
    original_ = model;
    return true;
}

}  // namespace deras::io

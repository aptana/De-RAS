// De-RAS - core/Model.cpp
#include "Model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <limits>

namespace deras {

const char* nodeTypeName(NodeType t) {
    switch (t) {
        case NodeType::CrossSection: return "Cross Section";
        case NodeType::Culvert: return "Culvert";
        case NodeType::Bridge: return "Bridge";
        case NodeType::MultipleOpening: return "Multiple Opening";
        case NodeType::InlineStructure: return "Inline Structure";
        case NodeType::LateralStructure: return "Lateral Structure";
        default: return "Other";
    }
}

int nextUid() {
    static int counter = 0;
    return ++counter;
}

bool CrossSection::sameData(const CrossSection& o) const {
    auto eqSE = [](const std::vector<StaElev>& a, const std::vector<StaElev>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].station != b[i].station || a[i].elev != b[i].elev) return false;
        return true;
    };
    auto eqMN = [](const std::vector<ManningBreak>& a, const std::vector<ManningBreak>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].station != b[i].station || a[i].n != b[i].n) return false;
        return true;
    };
    auto eqBlocks = [](const std::vector<StationBlock>& a, const std::vector<StationBlock>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].staStart != b[i].staStart || a[i].staEnd != b[i].staEnd || a[i].elev != b[i].elev ||
                a[i].permanent != b[i].permanent) return false;
        return true;
    };
    auto eqLevee = [](const LeveePoint& a, const LeveePoint& b) {
        return a.present == b.present && (!a.present || (a.station == b.station && a.elev == b.elev));
    };
    return rs == o.rs && type == o.type && lenLeft == o.lenLeft && lenChannel == o.lenChannel &&
           lenRight == o.lenRight && eqSE(staElev, o.staElev) && eqMN(manning, o.manning) &&
           manningFormat == o.manningFormat && bankLeft == o.bankLeft && bankRight == o.bankRight &&
           contraction == o.contraction && expansion == o.expansion && description == o.description &&
           eqBlocks(ineffective, o.ineffective) && ineffectiveMode == o.ineffectiveMode &&
           eqBlocks(obstructions, o.obstructions) && obstructionMode == o.obstructionMode &&
           eqLevee(leveeLeft, o.leveeLeft) && eqLevee(leveeRight, o.leveeRight) && cutLine == o.cutLine && structure == o.structure;
}

double Junction::lengthTo(const std::string& river, const std::string& reach) const {
    const auto& side = up.size() > 1 ? up : dn;
    for (size_t i = 0; i < side.size(); ++i)
        if (side[i].first == river && side[i].second == reach) return i < lengths.size() ? lengths[i] : 0;
    return lengths.empty() ? 0 : lengths.front();
}

double CrossSection::rsValue() const { return rsNumber(rs); }

double rsNumber(const std::string& rs) {
    std::string s;
    for (char c : rs)
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') s += c;
    if (s.empty()) return std::numeric_limits<double>::quiet_NaN();
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return std::numeric_limits<double>::quiet_NaN();
    return v;
}

double CrossSection::minElevation() const {
    double m = std::numeric_limits<double>::infinity();
    for (const auto& p : staElev) m = std::min(m, p.elev);
    return m;
}

double CrossSection::channelInvert() const {
    if (!bankLeft || !bankRight) return minElevation();
    double m = std::numeric_limits<double>::infinity();
    for (const auto& p : staElev)
        if (p.station >= *bankLeft - 1e-9 && p.station <= *bankRight + 1e-9) m = std::min(m, p.elev);
    return std::isfinite(m) ? m : minElevation();
}

std::vector<const CrossSection*> Reach::crossSections() const {
    std::vector<const CrossSection*> out;
    for (const auto& n : nodes)
        if (n.type == NodeType::CrossSection) out.push_back(&n);
    return out;
}

Reach* Geometry::findReach(const std::string& river, const std::string& reach) {
    for (auto& r : reaches)
        if (r.river == river && r.reach == reach) return &r;
    return nullptr;
}
const Reach* Geometry::findReach(const std::string& river, const std::string& reach) const {
    for (const auto& r : reaches)
        if (r.river == river && r.reach == reach) return &r;
    return nullptr;
}

double breachedCrest(const Breach& b, double station, double crest, double tHours) {
    if (!b.active || tHours <= b.startHours) return crest;
    const double f = b.formHours > 0 ? std::min(1.0, (tHours - b.startHours) / b.formHours) : 1.0;
    // the trapezoid grows from the top of the crest: bottom drops and widens linearly
    const double top = crest;
    const double bottom = top - f * (top - b.bottomElev);
    if (bottom >= crest) return crest;
    const double halfB = 0.5 * f * b.bottomWidth;
    const double d = std::fabs(station - b.station);
    double z = d <= halfB ? bottom : bottom + (d - halfB) / std::max(b.sideSlope, 1e-6);
    return std::min(crest, z);
}

StorageArea* Geometry::findArea(const std::string& name) {
    for (auto& a : storageAreas)
        if (a.name == name) return &a;
    return nullptr;
}
const StorageArea* Geometry::findArea(const std::string& name) const {
    for (const auto& a : storageAreas)
        if (a.name == name) return &a;
    return nullptr;
}

const ReachBoundary* SteadyFlow::boundary(const std::string& river, const std::string& reach, int profile) const {
    for (const auto& b : boundaries)
        if (b.river == river && b.reach == reach && b.profile == profile) return &b;
    return nullptr;
}

ReachBoundary& SteadyFlow::boundaryRef(const std::string& river, const std::string& reach, int profile) {
    for (auto& b : boundaries)
        if (b.river == river && b.reach == reach && b.profile == profile) return b;
    ReachBoundary nb;
    nb.river = river;
    nb.reach = reach;
    nb.profile = profile;
    boundaries.push_back(nb);
    return boundaries.back();
}

const char* regimeName(FlowRegime r, bool id) {
    switch (r) {
        case FlowRegime::Subcritical: return id ? "Subkritis" : "Subcritical";
        case FlowRegime::Supercritical: return id ? "Superkritis" : "Supercritical";
        case FlowRegime::Mixed: return id ? "Campuran" : "Mixed";
    }
    return "";
}

const char* uBoundaryKey(UBoundaryType t) {
    switch (t) {
        case UBoundaryType::FlowHydrograph: return "Flow Hydrograph=";
        case UBoundaryType::StageHydrograph: return "Stage Hydrograph=";
        case UBoundaryType::LateralInflow: return "Lateral Inflow Hydrograph=";
        case UBoundaryType::UniformLateralInflow: return "Uniform Lateral Inflow Hydrograph=";
        case UBoundaryType::NormalDepth: return "Friction Slope=";
        case UBoundaryType::RatingCurve: return "Rating Curve=";
        case UBoundaryType::Precipitation: return "Precipitation Hydrograph=";
        default: return "";
    }
}

double intervalSeconds(const std::string& text) {
    std::string t;
    for (char c : text) if (c != ' ') t += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    size_t i = 0;
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i;
    if (i == 0) return 0;
    const double n = std::stod(t.substr(0, i));
    const std::string u = t.substr(i);
    if (u == "SEC" || u == "SECOND") return n;
    if (u == "MIN" || u == "MINUTE") return n * 60;
    if (u == "HOUR" || u == "HR") return n * 3600;
    if (u == "DAY") return n * 86400;
    if (u == "WEEK") return n * 7 * 86400;
    return 0;
}

namespace {
const char* kMon[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
long long daysFromCivil(long long y, unsigned m, unsigned d) {  // H. Hinnant's algorithm
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}
void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += m <= 2;
}
}  // namespace

double rasDateTimeSeconds(const std::string& date, const std::string& time) {
    std::string d;
    for (char c : date) if (c != ' ') d += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (d.size() < 8) return std::nan("");
    size_t i = 0;
    while (i < d.size() && std::isdigit(static_cast<unsigned char>(d[i]))) ++i;
    if (i == 0 || i > 2 || d.size() < i + 4) return std::nan("");
    const int day = std::stoi(d.substr(0, i));
    const std::string mon = d.substr(i, 3);
    int m = -1;
    for (int k = 0; k < 12; ++k) if (mon == kMon[k]) m = k + 1;
    const std::string ys = d.substr(i + 3);
    if (m < 0 || ys.empty() || ys.find_first_not_of("0123456789") != std::string::npos) return std::nan("");
    long long y = std::stoll(ys);
    if (ys.size() <= 2) y += y < 50 ? 2000 : 1900;
    std::string t;
    for (char c : time) if (std::isdigit(static_cast<unsigned char>(c))) t += c;
    if (t.empty()) t = "0000";
    while (t.size() < 4) t = "0" + t;
    const int hh = std::stoi(t.substr(0, 2)), mm = std::stoi(t.substr(2, 2));
    if (day < 1 || day > 31 || hh > 24 || mm > 59) return std::nan("");
    return static_cast<double>(daysFromCivil(y, unsigned(m), unsigned(day))) * 86400 + hh * 3600 + mm * 60;
}

std::string rasDateText(double s) {
    const long long days = static_cast<long long>(std::floor(s / 86400));
    long long y;
    unsigned m, d;
    civilFromDays(days, y, m, d);
    char b[32];
    std::snprintf(b, sizeof b, "%02u%s%04lld", d, kMon[m - 1], y);
    return b;
}

std::string rasTimeText(double s) {
    const long long sec = static_cast<long long>(std::llround(s - std::floor(s / 86400) * 86400));
    char b[16];
    std::snprintf(b, sizeof b, "%02lld%02lld", sec / 3600, (sec % 3600) / 60);
    return b;
}

}  // namespace deras

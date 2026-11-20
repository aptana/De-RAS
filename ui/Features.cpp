// De-RAS - ui/Features.cpp
#include "Features.h"

#include <cstring>

#include "Common.h"

namespace deras::ui {

namespace {
const LockedFeature kLocked[] = {
    // editors
    // analyses
    // GIS
    // results
    // geometry palette
    // drawing tools
    {"draw-breakline", "Breakline", "Breakline", "NA"},
    {"draw-region", "Region n Manning", "Manning's n region", "NA"},
    // cross-section extras and regimes
    {"nvertical", "n vertikal", "Vertical n variation", "LATER"},
};
}  // namespace

const LockedFeature* lockedFeature(const QString& id) {
    const QByteArray k = id.toLatin1();
    for (const auto& f : kLocked)
        if (std::strcmp(f.id, k.constData()) == 0) return &f;
    return nullptr;
}

bool isLocked(const QString& id) { return lockedFeature(id) != nullptr; }

QString milestoneTitle(const char* m) {
    const QString s = QString::fromLatin1(m);
    if (s == "M2") return tr2("Steady lengkap", "Complete steady flow");
    if (s == "M3") return tr2("Mapper dan GIS", "Mapper and GIS");
    if (s == "M4") return tr2("Struktur hidraulika", "Hydraulic structures");
    if (s == "M5") return tr2("Unsteady 1D", "1D unsteady");
    if (s == "M6") return tr2("2D, storage area, struktur lateral, pompa", "2D, storage areas, lateral structures, pumps");
    if (s == "M7") return tr2("Sedimen", "Sediment");
    if (s == "M8") return tr2("Sisa fitur", "Remaining features");
    return s;
}

QString lockedTooltip(const QString& id) {
    const LockedFeature* f = lockedFeature(id);
    if (!f) return {};
    if (std::strcmp(f->milestone, "LATER") == 0)
        return tr2("Belum tersedia di versi ini (dijadwalkan di M8 tetapi belum masuk build 0.2.3)", "Not available in this version (planned for M8 but not in build 0.2.3)");
    if (std::strcmp(f->milestone, "NA") == 0)
        return tr2("Belum tersedia di versi ini (tidak dijadwalkan dalam M1–M8)", "Not available in this version (not scheduled in M1–M8)");
    return tr2("Belum tersedia — dijadwalkan di %1 (%2)", "Not available yet — planned for %1 (%2)")
        .arg(QString::fromLatin1(f->milestone), milestoneTitle(f->milestone));
}

}  // namespace deras::ui

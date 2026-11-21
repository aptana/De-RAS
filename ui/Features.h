// De-RAS - ui/Features.h
// One list of the features that are not available in this build, with the
// milestone planned for them (M2–M8 of the 0.2.x builds; "LATER" = planned
// but not in the current build, "NA" = not scheduled).
// Toolbar buttons, palette entries, menu items and the feature matrix all
// read their lock state and tooltip from here, so the app never shows a
// control for a feature that does not work.
#pragma once

#include <QString>

namespace deras::ui {

struct LockedFeature {
    const char* id;
    const char* nameId;
    const char* nameEn;
    const char* milestone;  // "M2" … "M8"
};

const LockedFeature* lockedFeature(const QString& id);   // nullptr = available
bool isLocked(const QString& id);
QString lockedTooltip(const QString& id);                 // "Belum tersedia — dijadwalkan di M5 (…)"
QString milestoneTitle(const char* milestone);            // "Aliran unsteady 1D" …

}  // namespace deras::ui

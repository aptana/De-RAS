// De-RAS - ui/DoctorSheet.h
// The Model Doctor pop-up shown when a run is blocked (review R4 of build
// 0.2.0). It lists every error itself: what is wrong, where (plan, reach, RS)
// and how to fix it, with quick fixes where one is safe, instead of only
// pointing at the Messages drawer.
#pragma once

#include <QString>

#include "core/Messages.h"

class QWidget;

namespace deras::ui {

struct DoctorChoice {
    enum Kind { Close, OpenDrawer, FixRegime, OpenLocation, OpenView } kind = Close;
    QString location;  // OpenLocation: "River/Reach RS 1000"
    QString view;      // OpenView: view id ("analysis", "steady", "geometry")
};

// planLabel names the plan, e.g. "p01 · LONGBIA-PlanSteadyQ".
DoctorChoice doctorSheet(QWidget* win, const MessageList& msgs, const QString& planLabel);

// "1 error", "2 errors" / "1 kesalahan" (Indonesian has no plural form).
QString countErrors(int n);
QString countWarnings(int n);

}  // namespace deras::ui

// De-RAS - ui/DoctorSheet.cpp
#include "DoctorSheet.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common.h"
#include "Theme.h"
#include "Widgets.h"

namespace deras::ui {

QString countErrors(int n) { return tr2("%1 kesalahan", n == 1 ? "%1 error" : "%1 errors").arg(n); }
QString countWarnings(int n) { return tr2("%1 peringatan", n == 1 ? "%1 warning" : "%1 warnings").arg(n); }

namespace {
// Quick fix offered for a message code: {button text, choice}.
struct QuickFix { QString text; DoctorChoice choice; };

QList<QuickFix> quickFixes(const Message& m) {
    QList<QuickFix> f;
    auto view = [](const QString& id) { DoctorChoice c; c.kind = DoctorChoice::OpenView; c.view = id; return c; };
    if (m.code == "MD_NO_GEOMETRY" || m.code == "MD_NO_FLOW" || m.code == "PLAN_NONE") {
        f << QuickFix{tr2("Buka Analisis Steady", "Open Steady Analysis"), view("analysis")};
    } else if (m.code.rfind("FLOW_", 0) == 0 || m.code.rfind("BC_", 0) == 0) {
        f << QuickFix{tr2("Buka Aliran Steady", "Open Steady Flow"), view("steady")};
    } else if (m.code == "MD_NO_REACH" || m.code.rfind("MD_JUNCTION_", 0) == 0) {
        f << QuickFix{tr2("Buka Data Geometri", "Open Geometric Data"), view("geometry")};
    }
    return f;
}
}  // namespace

DoctorChoice doctorSheet(QWidget* win, const MessageList& msgs, const QString& planLabel) {
    int ne = 0, nw = 0;
    for (const auto& m : msgs) {
        if (m.severity == Severity::Error) ++ne;
        if (m.severity == Severity::Warning) ++nw;
    }
    Sheet sh(win, QStringLiteral("Model Doctor"), "sim.doctor", 640);
    const Theme& th = Theme::current();
    auto* lead = new QLabel(ne > 0 ? tr2("Plan %1 belum bisa dihitung. Perbaiki %2 di bawah ini dulu.", "Plan %1 cannot be computed yet. Fix the %2 below first.").arg(planLabel, countErrors(ne))
                                   : tr2("Plan %1 lolos pemeriksaan.", "Plan %1 passed the checks.").arg(planLabel));
    lead->setWordWrap(true);
    sh.body()->addWidget(lead);
    if (nw > 0) sh.body()->addWidget(makeMuted(tr2("Ada juga %1. Peringatan tidak menghalangi hitungan; daftarnya ada di panel Pesan.", "There are also %1. Warnings do not block the computation; they are listed in the Messages drawer.").arg(countWarnings(nw))));

    DoctorChoice result;
    int shown = 0;
    for (const auto& m : msgs) {
        if (m.severity != Severity::Error) continue;
        if (++shown > 12) break;
        auto* box = new QFrame;
        box->setObjectName("DoctorItem");
        box->setStyleSheet(QStringLiteral("QFrame#DoctorItem { border: 1px solid %1; border-radius: 10px; background: %2; }")
                               .arg(cssColor(withAlpha(th.error, 0.45)), cssColor(withAlpha(th.error, th.dark ? 0.08 : 0.06))));
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(14, 10, 14, 12);
        v->setSpacing(5);
        auto* what = new QLabel(QStringLiteral("⛔  ") + QString::fromStdString(m.body(currentLanguage())));
        what->setWordWrap(true);
        what->setObjectName("Strong");
        what->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(th.text)));
        v->addWidget(what);
        const QString where = m.location.empty() ? tr2("Plan %1", "Plan %1").arg(planLabel) : qs(m.location);
        auto* loc = new QLabel(tr2("Lokasi: %1", "Where: %1").arg(where));
        loc->setObjectName("Muted");
        loc->setWordWrap(true);
        v->addWidget(loc);
        const std::string hint = fixHint(m.code, currentLanguage());
        if (!hint.empty()) {
            auto* how = new QLabel(tr2("Cara memperbaiki: %1", "How to fix: %1").arg(QString::fromStdString(hint)));
            how->setWordWrap(true);
            v->addWidget(how);
        }
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        for (const QuickFix& q : quickFixes(m)) {
            auto* b = new QPushButton(q.text);
            if (q.choice.kind == DoctorChoice::FixRegime) b->setObjectName("Primary");
            const DoctorChoice c = q.choice;
            QObject::connect(b, &QPushButton::clicked, &sh, [&sh, &result, c]() { result = c; sh.done(2); });
            row->addWidget(b);
        }
        if (!m.location.empty() && m.location.find(" RS ") != std::string::npos) {
            auto* b = new QPushButton(tr2("Buka penampang", "Open cross section"));
            const QString l = qs(m.location);
            QObject::connect(b, &QPushButton::clicked, &sh, [&sh, &result, l]() { result.kind = DoctorChoice::OpenLocation; result.location = l; sh.done(2); });
            row->addWidget(b);
        }
        if (row->count() > 0) {
            row->addStretch();
            v->addLayout(row);
        } else {
            delete row;
        }
        sh.body()->addWidget(box);
    }
    if (ne > 12) sh.body()->addWidget(makeMuted(tr2("%1 kesalahan lain ada di panel Pesan.", "%1 more errors are in the Messages drawer.").arg(ne - 12)));
    sh.addButton(tr2("Lihat di panel Pesan", "Show in Messages drawer"), 1);
    sh.addButton(tr2("Tutup", "Close"), 0, true);
    const int r = sh.exec();
    if (r == 1) result.kind = DoctorChoice::OpenDrawer;
    else if (r == 0) result.kind = DoctorChoice::Close;
    return result;
}

}  // namespace deras::ui

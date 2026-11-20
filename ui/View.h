// De-RAS - ui/View.h
// A view of the single main window (Beranda, Data Geometri, Editor
// Penampang, Data Aliran Steady, Analisis Steady, Hasil, Opsi). The main
// window shows one view at a time; the view bar above it shows the
// breadcrumb, the title, the ⓘ topic and the view's main actions on the
// right (always in the same place).
#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

#include "ProjectSession.h"

namespace deras::ui {

class View : public QWidget {
    Q_OBJECT
public:
    View(ProjectSession* s, QWidget* parent = nullptr) : QWidget(parent), s_(s) {}
    virtual QString viewId() const = 0;
    virtual QString title() const = 0;
    // Breadcrumb parents as (label, view id); the title is appended by the view bar.
    virtual QList<QPair<QString, QString>> crumbs() const;
    virtual QString helpTopic() const { return QStringLiteral("start"); }
    // "preview" -> "Pratinjau — belum divalidasi" badge, "" -> none
    virtual QString status() const { return {}; }
    // Main actions shown at the right of the view bar (created once, owned by the view bar afterwards).
    QWidget* actions();
    // Called every time the view becomes visible (refresh from the session).
    virtual void activated() {}
    // Called when the project is opened, closed or the plan changes.
    virtual void reload() {}
    // Optional: the toolbar button to highlight while this view is shown.
    virtual QString toolbarId() const { return viewId(); }
signals:
    void navigate(const QString& viewId);
    void headerChanged();   // title/crumbs/status changed
    void requestRun();      // F5 from inside a view
    void requestDoctor();   // F7 from inside a view
    void requestSave();     // Ctrl+S / Simpan from inside a view
    void requestLocked(const QString& featureId);  // a locked feature was clicked
protected:
    virtual QWidget* createActions() { return nullptr; }
    ProjectSession* s_;
private:
    QWidget* actions_ = nullptr;
    bool actionsMade_ = false;
};

}  // namespace deras::ui

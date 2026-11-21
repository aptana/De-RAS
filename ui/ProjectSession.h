// De-RAS - ui/ProjectSession.h
// The open project shared by every view, plus change/selection signals so
// the schematic, tables and plots stay synchronised.
//
// As in the HEC-RAS main window, the current plan decides which geometry
// and flow file it uses; the Home card can additionally pick another
// geometry or flow file "for editing" without changing the plan.
#pragma once

#include <QObject>
#include <QString>
#include <map>
#include <memory>
#include <optional>

#include "core/Results.h"
#include "io/Project.h"

namespace deras::ui {

class ProjectSession : public QObject {
    Q_OBJECT
public:
    explicit ProjectSession(QObject* parent = nullptr);

    io::Project* project() const { return prj_.get(); }
    bool isOpen() const { return prj_ != nullptr; }
    void setProject(std::unique_ptr<io::Project> p);
    void close();
    bool isExample() const;
    // Copies the bundled example (standard test project DerasUji1km) into dir
    // unless it is already there; returns the .prj path, or empty on failure.
    // A file "versi-contoh.txt" records the De-RAS version that wrote the copy.
    static QString installExample(const QString& dir);  // project created from the "Contoh" template
    // Version recorded in an installed example ("" when unknown: written before 0.2.3).
    static QString exampleVersion(const QString& dir);
    // Moves an installed example to "<dir>-lama-<date time>" (nothing is
    // deleted) and installs a fresh copy; returns the .prj path, empty on
    // failure. `movedTo` receives the folder the old copy went to.
    static QString restoreExample(const QString& dir, QString* movedTo = nullptr);

    QString prjPath() const;
    QString title() const;

    // Current plan and the files it references (may be null).
    QString currentPlanExt() const;
    void setCurrentPlan(const QString& ext);  // also resets the editing overrides
    io::PlanFile* currentPlan() const;
    io::GeometryFile* currentGeometry() const;   // geometry being edited (plan's, unless overridden)
    io::SteadyFlowFile* currentFlow() const;     // steady flow being edited (plan's, unless overridden)
    QString currentGeometryExt() const;
    QString currentFlowExt() const;
    void setEditGeometry(const QString& ext);
    void setEditFlow(const QString& ext);
    QString unsteadyTitle(const QString& ext) const;  // "Flow Title=" of a .u## file
    // Unsteady flow file being edited (M5): the plan's when it is unsteady, else the override or the first one.
    QString currentUnsteadyExt() const;
    io::UnsteadyFlowFile* currentUnsteady() const;
    void setEditUnsteady(const QString& ext);

    bool save(MessageList& msgs);
    // Model Doctor for the current plan (empty plan -> one error message).
    MessageList doctor() const;
    void markEdited() { emit modelEdited(); }

    // Results of a plan read from disk (cached until resultsUpdated / project change).
    const RunResult* results(const QString& planExt);
    QString resultsError(const QString& planExt) const;

    // Cross-section lookup from a message location ("River / Reach RS 1000").
    bool findLocation(const QString& location, int& reachUid, int& xsUid) const;

    // selection
    void selectCrossSection(int reachUid, int xsUid) { selReach_ = reachUid; selXs_ = xsUid; emit crossSectionSelected(reachUid, xsUid); }
    int selectedReach() const { return selReach_; }
    int selectedXs() const { return selXs_; }

signals:
    void projectChanged();      // opened / closed / plan switched
    void modelEdited();         // any data edited
    void saved();
    void crossSectionSelected(int reachUid, int xsUid);
    void resultsUpdated(const QString& planExt);
    void logMessage(const QString& text);

private:
    std::unique_ptr<io::Project> prj_;
    QString editGeom_, editFlow_, editUnsteady_;
    std::map<QString, RunResult> results_;
    std::map<QString, QString> resultsErr_;
    int selReach_ = 0, selXs_ = 0;
};

}  // namespace deras::ui

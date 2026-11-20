// De-RAS - ui/MainWindow.h
// The single main window of De-RAS 0.2.0 (docs/CATATAN_VERSI.md, M1):
//   header (logo, name, version, menu, project chip)
//   grouped labelled toolbar capsule (PROYEK / EDITOR / JALANKAN / GIS / HASIL & TABEL)
//   view bar (back, breadcrumb, title, status badge, ⓘ, main actions on the right)
//   one view at a time (Beranda, Data Geometri, Editor Penampang, Aliran Steady,
//   Analisis Steady, Hasil, Opsi)
//   Messages drawer (closed by default) and a thin status bar.
// Only the Compute window and the Help window are separate windows; every
// other input is an in-window sheet.
#pragma once

#include <QMainWindow>
#include <QMap>
#include <QTimer>

#include "ProjectSession.h"

class QAction;
class QLabel;
class QListWidget;
class QMenu;
class QMenuBar;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QHBoxLayout;

namespace deras::ui {

class View;
class HomeView;
class GeometryView;
class XsEditorView;
class SteadyFlowView;
class SteadyAnalysisView;
class UnsteadyFlowView;
class UnsteadyAnalysisView;
class SedimentView;
class DesignView;
class WaterQualityView;
class ResultsView;
class MapperView;
class OptionsView;
class ComputeWindow;
class ToolbarCapsule;
class IconButton;
class Segmented;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ProjectSession* session, QWidget* parent = nullptr);
    ~MainWindow() override;
    void openProject(const QString& prjPath);
    void go(const QString& viewId, bool record = true);  // switch view (history kept)
    QString currentViewId() const;
    // Navigation history (Back = Alt+Left, Forward = Alt+Right); public for tests.
    void back();
    void forward();
    bool canGoBack() const;
    bool canGoForward() const;
    QStringList backHistory() const;
    bool isRunning() const;
    // Autosave (M8): offers to restore unsaved changes left by a crash (called once after start).
    void checkRecovery();
signals:
    // main() rebuilds the window (language / theme / font size change)
    void rebuildRequested();
protected:
    void closeEvent(QCloseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;  // mouse back/forward buttons
    // Mouse back/forward buttons anywhere in this window: child widgets such as
    // tables and the canvas used to swallow them (review R3).
    bool eventFilter(QObject* o, QEvent* e) override;
private:
    QWidget* buildHeader();
    void buildMenus(QMenuBar* mb);
    QWidget* buildToolbar();
    QWidget* buildViewBar();
    QWidget* buildDrawer();
    QWidget* buildStatusBar();
    void addView(View* v);
    void toolbarTriggered(const QString& id);
    void updateViewBar();
    void updateHeader();
    void updateEnabled();
    void updateDirty();
    void updateDoctor();
    void showLocked(const QString& featureId);
    bool canVisit(const QString& id) const;
    bool sheetOpen() const;
    void addLog(const QString& line, bool error = false);
    void openDrawer(int page);
    // project actions
    bool maybeSave();
    bool saveProject();
    void newProject();
    void openDialog();
    void duplicateProject();
    void openExample(bool restore = false);
    void closeProject();
    void editDescription();
    void runModelDoctor(bool openDrawer);
    void runCurrentPlan();
    void fillRecentMenu();

    ProjectSession* s_;
    QStackedWidget* stack_;
    QMap<QString, View*> views_;
    HomeView* home_;
    GeometryView* geom_;
    MapperView* mapper_;
    XsEditorView* xs_;
    SteadyFlowView* steady_;
    SteadyAnalysisView* analysis_;
    UnsteadyFlowView* unsteady_;
    UnsteadyAnalysisView* uanalysis_;
    SedimentView *quasi_, *sedData_, *sedRun_;
    DesignView* design_;
    WaterQualityView *wqData_, *wqRun_;
    ResultsView* results_;
    OptionsView* options_;
    ComputeWindow* compute_;
    ToolbarCapsule* toolbar_;
    // header
    QLabel* projectChip_;
    QLabel* exampleBadge_;
    QMenu* recentMenu_ = nullptr;
    QList<QAction*> needProject_;
    // view bar
    QWidget* viewBar_;
    IconButton* back_;
    QHBoxLayout* crumbs_;
    QLabel* viewTitle_;
    QLabel* viewStatus_;
    QHBoxLayout* viewInfo_;
    QHBoxLayout* actionsHost_;
    QWidget* currentActions_ = nullptr;
    // drawer
    QWidget* drawer_;
    Segmented* drawerTabs_;
    QStackedWidget* drawerPages_;
    QListWidget* log_;
    QListWidget* validation_;
    // status bar
    QLabel* statusText_;
    QPushButton* doctorBtn_;
    QPushButton* runLabel_;
    QProgressBar* runBar_;
    QLabel* unitsLabel_;
    QLabel* crsLabel_;
    QPushButton* msgBtn_;
    QTimer doctorTimer_, statusClear_, autosave_;
    int autosaveTicks_ = 0;
    void autosaveTick();
    void clearRecovery();
    // navigation history
    QStringList history_, future_;
};

}  // namespace deras::ui

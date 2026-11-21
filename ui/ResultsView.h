// De-RAS - ui/ResultsView.h
// Hasil: one view with a left list of result pages (cross section, profile,
// general profile, detailed table, profile table, messages, rating curve,
// 3D, and for unsteady plans hydrograph and time series). Every page
// shows where the numbers came from and whether they are stale.
#pragma once

#include "View.h"
#include "core/Results.h"

class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;
class QTableWidget;

namespace deras::ui {
class Perspective3D;

class PlotWidget;

class ResultsView : public View {
    Q_OBJECT
public:
    explicit ResultsView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("results"); }
    QString title() const override;
    QString helpTopic() const override;
    QString toolbarId() const override { return QStringLiteral("res-") + page_; }
    void reload() override;
    void activated() override;
    // "xs", "profile", "general", "detail", "table", "messages"
    void showPage(const QString& page);
    QString page() const { return page_; }
    bool hasResults() const { return result_ && result_->status == RunStatus::Succeeded; }
    static bool isPageLocked(const QString& page, QString* featureId = nullptr);
public slots:
    void exportCsv();
    void exportReport();
protected:
    QWidget* createActions() override;
private:
    void fillSelectors();
    void refreshPage();
    void fillXs();
    void fillProfile();
    void fillGeneral();
    void fillRating();
    void fill3d();
    void fillHydrograph();
    void fillTimeSeries();
    int seriesIndex() const;
    const ExtraSeries* extraSeries() const;  // storage area / lateral / connection / pump chosen in the reach box  // index of the selected cross section in the unsteady time series
    void fillDetail();
    void fillTable();
    void fillMessages();
    void checkStale();
    const XsResult* currentXs() const;
    const ProfileResult* currentProfile() const;

    const RunResult* result_ = nullptr;
    QString planExt_, page_ = QStringLiteral("xs");
    QListWidget* nav_;
    QLabel* origin_;
    QLabel* stale_;
    QComboBox *profile_, *reach_, *xs_, *variable_;
    QWidget *xsBox_, *reachBox_, *varBox_;
    QStackedWidget* pages_;
    PlotWidget *xsPlot_, *profilePlot_, *generalPlot_, *ratingPlot_, *hydroStage_ = nullptr, *hydroFlow_ = nullptr;
    QTableWidget* ts_ = nullptr;
    Perspective3D* p3d_ = nullptr;
    QTableWidget *detail_, *table_;
    QListWidget* msgs_;
    bool loading_ = false;
};

}  // namespace deras::ui

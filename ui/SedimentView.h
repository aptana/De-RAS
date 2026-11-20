// De-RAS - ui/SedimentView.h
// Sediment module (M7, Preview). One class, three views of the main window:
//   "quasi"        Data Quasi-unsteady: reach, flow series, downstream boundary
//   "sediment"     Data Sedimen: transport function, bed material, inflow load
//   "run-sediment" Analisis Sedimen: run, bed profile over time, table, CSV
// The data is stored in the project's .deras.json (modules["sediment"]);
// results go to <base>.deras-sedimen.json.
#pragma once

#include <QPointer>

#include "View.h"
#include "core/SedimentModel.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;

namespace deras::ui {

class NumTable;
class PlotWidget;

class SedimentView : public View {
    Q_OBJECT
public:
    enum class Page { Quasi, Data, Run };
    SedimentView(ProjectSession* s, Page page, QWidget* parent = nullptr);
    QString viewId() const override;
    QString title() const override;
    QString helpTopic() const override;
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override;
protected:
    QWidget* createActions() override;
private:
    void buildQuasi();
    void buildData();
    void buildRun();
    void load();
    void commit();
    void fillReaches();
    void plotFlow();
    void runSediment();
    void loadResult();
    void showResult();
    void exportCsv();
    QString lenU() const;
    QString flowU() const;
    QString loadU() const;

    Page page_;
    bool loading_ = false;
    SedimentSetup st_;
    // quasi
    QComboBox *geom_ = nullptr, *reach_ = nullptr;
    NumTable* records_ = nullptr;
    QDoubleSpinBox* dsSlope_ = nullptr;
    PlotWidget* flowPlot_ = nullptr;
    QLabel* quasiNote_ = nullptr;
    // data
    QComboBox *function_ = nullptr, *inflow_ = nullptr;
    QDoubleSpinBox *gs_ = nullptr, *porosity_ = nullptr, *temp_ = nullptr, *maxDepth_ = nullptr;
    NumTable *gradation_ = nullptr, *rating_ = nullptr;
    QLabel* gradNote_ = nullptr;
    QWidget* ratingBox_ = nullptr;
    // run
    QDoubleSpinBox* outHours_ = nullptr;
    QPushButton* runBtn_ = nullptr;
    QLabel *runInfo_ = nullptr, *summary_ = nullptr, *timeLabel_ = nullptr;
    QSlider* time_ = nullptr;
    PlotWidget *bedPlot_ = nullptr, *changePlot_ = nullptr;
    QTableWidget* table_ = nullptr;
    SedimentResult res_;
    bool hasRes_ = false;
    bool running_ = false;
};

}  // namespace deras::ui

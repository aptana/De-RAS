// De-RAS - ui/WaterQualityView.h
// Water quality module (M8, Preview). One class, two views of the main window:
//   "wq"      Data Kualitas Air: unsteady plan, constituent, dispersion,
//             decay, upstream concentration series
//   "run-wq"  Analisis Kualitas Air: run on the stored unsteady results of
//             the plan, concentration along the reach and over time, CSV
// Data: modules["water_quality"] of the .deras.json; results:
// <base>.<plan>.deras-wq.json.
#pragma once

#include "View.h"
#include "io/ModuleData.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTableWidget;

namespace deras::ui {

class NumTable;
class PlotWidget;

class WaterQualityView : public View {
    Q_OBJECT
public:
    enum class Page { Data, Run };
    WaterQualityView(ProjectSession* s, Page page, QWidget* parent = nullptr);
    QString viewId() const override { return page_ == Page::Data ? QStringLiteral("wq") : QStringLiteral("run-wq"); }
    QString title() const override;
    QString helpTopic() const override { return page_ == Page::Data ? QStringLiteral("wq.data") : QStringLiteral("wq.run"); }
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override { reload(); }
protected:
    QWidget* createActions() override;
private:
    void buildData();
    void buildRun();
    void load();
    void commit();
    void plotInflow();
    void runWq();
    void loadResult();
    void showResult();
    void exportCsv();
    QString lenU() const;

    Page page_;
    bool loading_ = false;
    io::WqSetup w_;
    // data
    QComboBox* plan_ = nullptr;
    QLineEdit* name_ = nullptr;
    QDoubleSpinBox *initial_ = nullptr, *disp_ = nullptr, *decay_ = nullptr;
    NumTable* upstream_ = nullptr;
    PlotWidget* inflowPlot_ = nullptr;
    QLabel* planNote_ = nullptr;
    // run
    QPushButton* runBtn_ = nullptr;
    QLabel *runInfo_ = nullptr, *summary_ = nullptr, *timeLabel_ = nullptr;
    QSlider* time_ = nullptr;
    PlotWidget *alongPlot_ = nullptr, *timePlot_ = nullptr;
    QTableWidget* table_ = nullptr;
    design::WqResult res_;
    std::string resMeta_;
    bool hasRes_ = false, running_ = false;
};

}  // namespace deras::ui

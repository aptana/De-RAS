// De-RAS - ui/SteadyAnalysisView.h
// Analisis Steady (HEC-RAS "Steady Flow Analysis"): plan, the geometry and
// flow files it uses, flow regime, computation options and an always-visible
// Model Doctor card. "Hitung" (F5) opens the separate Compute window.
#pragma once

#include <QTimer>

#include "View.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;

namespace deras::ui {

class Segmented;

class SteadyAnalysisView : public View {
    Q_OBJECT
public:
    explicit SteadyAnalysisView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("analysis"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.analysis"); }
    void reload() override;
    void activated() override;
    void refreshDoctor();
protected:
    QWidget* createActions() override;
private:
    void commit();
    void newPlan();
    QComboBox *plan_, *geom_, *flow_, *friction_;
    QLineEdit *title_, *shortId_;
    Segmented* regime_;
    QDoubleSpinBox *tol_, *ctol_, *maxErr_;
    QSpinBox* trials_;
    QLabel *output_, *units_, *doctorHead_;
    QListWidget* doctor_;
    QTimer debounce_;
    bool loading_ = false;
};

}  // namespace deras::ui

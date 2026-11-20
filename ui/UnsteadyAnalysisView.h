// De-RAS - ui/UnsteadyAnalysisView.h
// Analisis Unsteady (HEC-RAS "Unsteady Flow Analysis", M5 Preview): plan,
// geometry and unsteady flow files, simulation window, computation and
// output intervals, implicit weighting and the Model Doctor card.
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

class UnsteadyAnalysisView : public View {
    Q_OBJECT
public:
    explicit UnsteadyAnalysisView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("run-unsteady"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.run-unsteady"); }
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override;
    void refreshDoctor();
protected:
    QWidget* createActions() override;
private:
    void commit();
    void newPlan();
    QComboBox *plan_, *geom_, *flow_, *comp_, *out_;
    QLineEdit *title_, *shortId_, *startDate_, *startTime_, *endDate_, *endTime_;
    QDoubleSpinBox *theta_, *ztol_;
    QSpinBox* maxIter_;
    QLabel *note_, *output_, *doctorHead_;
    QListWidget* doctor_;
    QTimer debounce_;
    bool loading_ = false;
};

}  // namespace deras::ui

// De-RAS - ui/UnsteadyFlowView.h
// Data Aliran Unsteady (HEC-RAS "Unsteady Flow Data", M5 Preview): the
// boundary condition locations of the .u## file (reach ends and lateral
// inflows), the hydrograph / normal depth / rating data of the selected one
// and the initial flows.
#pragma once

#include "View.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QTableWidget;

namespace deras::ui {

class NumTable;
class PlotWidget;

class UnsteadyFlowView : public View {
    Q_OBJECT
public:
    explicit UnsteadyFlowView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("unsteady"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.unsteady"); }
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override;
protected:
    QWidget* createActions() override;
private:
    void fillList();
    void showBoundary();
    void commitBoundary();
    void fillInitial();
    void commitInitial();
    void completeEnds();
    void addLateral();
    void addAreaBoundary();
    void removeBoundary();
    void newFile();
    UnsteadyBoundary* current();
    QComboBox *file_, *type_, *interval_;
    QListWidget* list_;
    QLabel *where_, *note_;
    QStackedWidget* stack_;
    NumTable *values_, *rating_;
    QLineEdit* slope_;
    PlotWidget* plot_;
    QTableWidget* initial_;
    bool loading_ = false;
};

}  // namespace deras::ui

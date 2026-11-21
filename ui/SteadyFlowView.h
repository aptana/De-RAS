// De-RAS - ui/SteadyFlowView.h
// Data Aliran Steady: profiles, discharge per flow-change location and the
// boundary conditions (edited in a sheet), all inside the main window.
#pragma once

#include "View.h"

class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

namespace deras::ui {

class SteadyFlowView : public View {
    Q_OBJECT
public:
    explicit SteadyFlowView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("steady"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.steady"); }
    void reload() override;
    void activated() override;
protected:
    QWidget* createActions() override;
private:
    io::SteadyFlowFile* flow();
    void fillFiles();
    void fillTables();
    void fillBoundarySummary();
    void commitFlows();
    void setProfileCount(int n);
    void profileNamesSheet();
    void addLocation();
    void removeLocation();
    void boundarySheet();
    void importCsv();
    void exportCsv();
    void newFlowFile();

    QComboBox* file_;
    QSpinBox* nProf_;
    QTableWidget* flows_;
    QLabel* status_;
    QVBoxLayout* bcBody_;
    QLabel* profiles_;
    bool loading_ = false;
};

QString boundaryTypeName(BoundaryType t);

}  // namespace deras::ui

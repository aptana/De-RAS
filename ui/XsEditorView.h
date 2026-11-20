// De-RAS - ui/XsEditorView.h
// Editor Penampang (sub-view of Data Geometri): reach and river-station
// navigation, the station–elevation table, the cross-section plot (with the
// computed water surface of the current plan when results exist) and the
// "Data penampang" card (lengths, banks, Manning's n, coefficients).
#pragma once

#include "View.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace deras::ui {

class NumTable;
class PlotWidget;
class Segmented;

class XsEditorView : public View {
    Q_OBJECT
public:
    explicit XsEditorView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("xs"); }
    QString title() const override;
    QList<QPair<QString, QString>> crumbs() const override;
    QString helpTopic() const override { return QStringLiteral("view.xs"); }
    QString toolbarId() const override { return QStringLiteral("geometry"); }
    void reload() override;
    void activated() override;
protected:
    QWidget* createActions() override;
private:
    Reach* reach();
    CrossSection* xs();
    void fillReaches();
    void fillStations();
    void showXs();
    void commitFields();
    void commitTable();
    void updatePlot();
    void step(int delta);
    void addXs(bool copy);
    void deleteXs();
    void importSurvey();
    void manningSheet();
    void extraSheet(int kind);  // 0 ineffective areas, 1 blocked obstructions, 2 levees
    void updateExtraChips();
    QDoubleSpinBox* spin(double lo, double hi, int dec);

    QComboBox* reach_;
    QComboBox* station_;
    QPushButton *prev_, *next_;
    QStackedWidget* stack_;
    QLabel* nodeInfo_;
    NumTable* table_;
    QLabel* tableStatus_;
    PlotWidget* plot_;
    QLabel* plotNote_;
    QLineEdit* rs_;
    QLineEdit* desc_;
    QDoubleSpinBox *lenL_, *lenC_, *lenR_, *bankL_, *bankR_, *nL_, *nC_, *nR_, *contr_, *expan_;
    QLabel* nVaries_;
    QWidget* nRow_;
    QLabel* units_;
    QPushButton* extra_[3] = {nullptr, nullptr, nullptr};
    bool loading_ = false;
    int curReach_ = 0, curXs_ = 0;
};

}  // namespace deras::ui

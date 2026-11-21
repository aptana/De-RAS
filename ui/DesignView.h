// De-RAS - ui/DesignView.h
// Hydraulic Design (M8, Preview), view "design" of the main window. Three
// pages chosen with a segmented control:
//   Aliran seragam   uniform flow / normal depth of a trapezoidal channel
//   Gerusan          pier, contraction and abutment scour (FHWA HEC-18),
//                    with values taken from a steady result
//   Kalibrasi n      one factor on Manning's n fitted to observed water
//                    surfaces, optionally applied to the geometry
// Observations of the calibration are kept in modules["calibration"].
#pragma once

#include "View.h"
#include "engines/design/Design.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace deras::ui {

class PlotWidget;
class Segmented;

class DesignView : public View {
    Q_OBJECT
public:
    explicit DesignView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("design"); }
    QString title() const override;
    QString helpTopic() const override;
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override { reload(); }
    void showPage(int page);
private:
    QWidget* buildUniform();
    QWidget* buildScour();
    QWidget* buildCalibration();
    void computeUniform();
    void computeScour();
    void fillSourcePlans();
    void fillSourceProfiles();
    void fillFromResult();
    void fillCalibrationPlans();
    void fillObservations();
    void saveObservations();
    void runCalibration();
    void applyFactor();
    void plotCalibration();
    QString lenU() const;
    QString flowU() const;
    QString velU() const;
    UnitSystem units() const;

    Segmented* pageSel_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    bool loading_ = false;
    // uniform
    QDoubleSpinBox *ub_ = nullptr, *uz_ = nullptr, *un_ = nullptr, *us_ = nullptr, *uval_ = nullptr;
    Segmented* umode_ = nullptr;
    QLabel *uvalLabel_ = nullptr, *uout_ = nullptr;
    PlotWidget* uplot_ = nullptr;
    // scour
    QComboBox *srcPlan_ = nullptr, *srcProfile_ = nullptr, *srcXs_ = nullptr;
    QLabel* srcNote_ = nullptr;
    QDoubleSpinBox *pa_ = nullptr, *pl_ = nullptr, *pang_ = nullptr, *py_ = nullptr, *pv_ = nullptr;
    QComboBox *pnose_ = nullptr, *pk3_ = nullptr;
    QLabel* pout_ = nullptr;
    Segmented* cmode_ = nullptr;
    QDoubleSpinBox *cy1_ = nullptr, *cy0_ = nullptr, *cq1_ = nullptr, *cq2_ = nullptr, *cw1_ = nullptr, *cw2_ = nullptr, *cs1_ = nullptr, *cd50_ = nullptr;
    QLabel* cout_ = nullptr;
    QDoubleSpinBox *al_ = nullptr, *aya_ = nullptr, *ave_ = nullptr, *aang_ = nullptr;
    QComboBox* ak1_ = nullptr;
    QLabel *aout_ = nullptr, *total_ = nullptr;
    // calibration
    QComboBox *kplan_ = nullptr, *kprofile_ = nullptr, *kscope_ = nullptr;
    QDoubleSpinBox *klo_ = nullptr, *khi_ = nullptr;
    QPushButton *krun_ = nullptr, *kapply_ = nullptr;
    QTableWidget* kobs_ = nullptr;
    QLabel* kout_ = nullptr;
    PlotWidget* kplot_ = nullptr;
    design::CalibrationResult kres_;
    std::vector<design::Observation> kresObs_;
    std::string kresGeom_;
    bool kHas_ = false, kRunning_ = false, kApplied_ = false;
};

}  // namespace deras::ui

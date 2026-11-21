// De-RAS - ui/DesignView.cpp
#include "DesignView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

#include "Common.h"
#include "Help.h"
#include "PlotWidget.h"
#include "Theme.h"
#include "Widgets.h"
#include "io/Project.h"

namespace deras::ui {

namespace {
QDoubleSpinBox* spin(double lo, double hi, int dec, double step, double val) {
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi);
    b->setDecimals(dec);
    b->setSingleStep(step);
    b->setLocale(QLocale::c());
    b->setMaximumWidth(150);
    b->setValue(val);
    return b;
}
QString num(double v, int d) { return std::isfinite(v) ? QString::number(v, 'f', d) : QStringLiteral("—"); }
QWidget* scrollOf(QWidget* inner) {
    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(inner);
    return sc;
}
QLabel* outLabel() {
    auto* l = new QLabel;
    l->setObjectName("Mono");
    l->setFont(monoFont());
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setWordWrap(true);
    return l;
}
QString row(const QString& name, const QString& value, const QString& unit = {}) {
    return QStringLiteral("<tr><td style='padding-right:14px'>%1</td><td align='right'><b>%2</b></td><td style='padding-left:6px'>%3</td></tr>").arg(name.toHtmlEscaped(), value, unit.toHtmlEscaped());
}
// cumulative main-channel distance of every cross section of a reach, from the downstream end
std::map<std::string, double> distances(const Reach& r) {
    std::map<std::string, double> d;
    double acc = 0;
    for (auto it = r.nodes.rbegin(); it != r.nodes.rend(); ++it) {
        if (it->type != NodeType::CrossSection) continue;
        d[it->rs] = acc;
        acc += it->lenChannel;
    }
    return d;
}
}  // namespace

DesignView::DesignView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* top = new QHBoxLayout;
    pageSel_ = new Segmented;
    pageSel_->addOption(tr2("Aliran seragam", "Uniform flow"), 0);
    pageSel_->addOption(tr2("Gerusan (HEC-18)", "Scour (HEC-18)"), 1);
    pageSel_->addOption(tr2("Kalibrasi n Manning", "Manning's n calibration"), 2);
    pageSel_->setValue(0);
    top->addWidget(pageSel_);
    top->addStretch();
    root->addLayout(top);
    pages_ = new QStackedWidget;
    pages_->addWidget(buildUniform());
    pages_->addWidget(buildScour());
    pages_->addWidget(buildCalibration());
    root->addWidget(pages_, 1);
    connect(pageSel_, &Segmented::changed, this, [this](int v) { showPage(v); });
    connect(s_, &ProjectSession::projectChanged, this, [this]() { if (isVisible()) reload(); });
    connect(s_, &ProjectSession::resultsUpdated, this, [this]() { if (isVisible()) reload(); });
    computeUniform();
    computeScour();
}

QString DesignView::title() const { return tr2("Desain Hidraulik", "Hydraulic Design"); }
QString DesignView::helpTopic() const {
    const int p = pages_ ? pages_->currentIndex() : 0;
    return p == 0 ? QStringLiteral("design.uniform") : p == 1 ? QStringLiteral("design.scour") : QStringLiteral("design.calibration");
}
void DesignView::showPage(int page) {
    page = std::clamp(page, 0, 2);
    pages_->setCurrentIndex(page);
    if (pageSel_->value() != page) pageSel_->setValue(page);
    emit headerChanged();
}

UnitSystem DesignView::units() const { auto* p = s_->project(); return p ? p->units() : UnitSystem::SI; }
QString DesignView::lenU() const { return units() == UnitSystem::USCustomary ? QStringLiteral("ft") : QStringLiteral("m"); }
QString DesignView::flowU() const { return units() == UnitSystem::USCustomary ? QStringLiteral("cfs") : QStringLiteral("m³/s"); }
QString DesignView::velU() const { return units() == UnitSystem::USCustomary ? QStringLiteral("ft/s") : QStringLiteral("m/s"); }

void DesignView::reload() {
    loading_ = true;
    const QString L = QStringLiteral(" ") + lenU();
    for (auto* b : {ub_, pa_, pl_, py_, cy1_, cy0_, cw1_, cw2_, al_, aya_}) b->setSuffix(L);
    for (auto* b : {pv_, ave_}) b->setSuffix(QStringLiteral(" ") + velU());
    for (auto* b : {cq1_, cq2_}) b->setSuffix(QStringLiteral(" ") + flowU());
    uval_->setSuffix(QStringLiteral(" ") + (umode_->value() == 0 ? lenU() : flowU()));
    fillSourcePlans();
    fillCalibrationPlans();
    loading_ = false;
    computeUniform();
    computeScour();
}

// ============================ uniform flow ============================
QWidget* DesignView::buildUniform() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    auto* c1 = new Card(tr2("Saluran trapesium", "Trapezoidal channel"), "design.uniform");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    ub_ = spin(0, 10000, 3, 0.5, 10);
    uz_ = spin(0, 100, 3, 0.25, 2);
    un_ = spin(0.005, 0.5, 4, 0.001, 0.03);
    us_ = spin(0.000001, 1, 6, 0.0001, 0.001);
    umode_ = new Segmented;
    umode_->addOption(tr2("Debit dari kedalaman", "Flow from depth"), 0);
    umode_->addOption(tr2("Kedalaman normal dari debit", "Normal depth from flow"), 1);
    umode_->setValue(0);
    uval_ = spin(0.001, 1e7, 3, 0.1, 2);
    uvalLabel_ = new QLabel;
    int r = 0;
    g->addWidget(labelWithInfo(tr2("Lebar dasar b", "Bottom width b"), "design.uniform"), r, 0);
    g->addWidget(ub_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Kemiringan talud z (H:V)", "Side slope z (H:V)"), "design.uniform"), r, 0);
    g->addWidget(uz_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("n Manning", "Manning's n"), "design.uniform"), r, 0);
    g->addWidget(un_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Kemiringan dasar S", "Bed slope S"), "design.uniform"), r, 0);
    g->addWidget(us_, r++, 1, Qt::AlignLeft);
    g->addWidget(umode_, r++, 0, 1, 2, Qt::AlignLeft);
    g->addWidget(uvalLabel_, r, 0);
    g->addWidget(uval_, r++, 1, Qt::AlignLeft);
    g->setColumnStretch(1, 1);
    c1->body()->addLayout(g);
    uout_ = outLabel();
    c1->body()->addWidget(uout_);
    c1->body()->addWidget(makeMuted(tr2("Persamaan Manning untuk aliran seragam; kedalaman kritis dari Q²T/(gA³) = 1. Satuan mengikuti proyek (SI bila belum ada proyek).",
                                        "Manning's equation for uniform flow; critical depth from Q²T/(gA³) = 1. Units follow the project (SI when no project is open).")));
    c1->body()->addStretch();
    h->addWidget(scrollOf(c1), 2);
    auto* c2 = new Card(tr2("Penampang", "Cross section"), "design.uniform");
    uplot_ = new PlotWidget;
    c2->body()->addWidget(uplot_, 1);
    h->addWidget(c2, 3);
    for (auto* b : {ub_, uz_, un_, us_, uval_}) connect(b, &QDoubleSpinBox::valueChanged, this, &DesignView::computeUniform);
    connect(umode_, &Segmented::changed, this, [this](int) {
        uval_->setSuffix(QStringLiteral(" ") + (umode_->value() == 0 ? lenU() : flowU()));
        computeUniform();
    });
    return w;
}

void DesignView::computeUniform() {
    if (!uout_) return;
    const UnitSystem u = units();
    design::Trapezoid c{ub_->value(), uz_->value(), un_->value(), us_->value()};
    const bool fromDepth = umode_->value() == 0;
    uvalLabel_->setText(fromDepth ? tr2("Kedalaman y", "Depth y") : tr2("Debit Q", "Flow Q"));
    const auto r = fromDepth ? design::uniformFromDepth(c, uval_->value(), u) : design::uniformFromFlow(c, uval_->value(), u);
    if (!r.ok) {
        uout_->setText(tr2("Tidak bisa dihitung: periksa lebar dasar, n dan kemiringan.", "Cannot compute: check the bottom width, n and slope."));
        uplot_->clear();
        return;
    }
    const QString L = lenU();
    const QString shearU = u == UnitSystem::USCustomary ? QStringLiteral("lb/ft²") : QStringLiteral("Pa");
    const QString regime = r.froude < 0.95 ? tr2("subkritis", "subcritical") : r.froude > 1.05 ? tr2("superkritis", "supercritical") : tr2("dekat kritis", "near critical");
    QString t = QStringLiteral("<table>");
    t += row(tr2("Kedalaman normal y", "Normal depth y"), num(r.depth, 3), L);
    t += row(tr2("Debit Q", "Flow Q"), num(r.flow, 3), flowU());
    t += row(tr2("Luas A", "Area A"), num(r.area, 3), L + QStringLiteral("²"));
    t += row(tr2("Keliling basah P", "Wetted perimeter P"), num(r.perimeter, 3), L);
    t += row(tr2("Jari-jari hidraulik R", "Hydraulic radius R"), num(r.radius, 3), L);
    t += row(tr2("Lebar muka air T", "Top width T"), num(r.topWidth, 3), L);
    t += row(tr2("Kecepatan V", "Velocity V"), num(r.velocity, 3), velU());
    t += row(tr2("Bilangan Froude", "Froude number"), num(r.froude, 3), regime);
    t += row(tr2("Kedalaman kritis yc", "Critical depth yc"), num(r.criticalDepth, 3), L);
    t += row(tr2("Tegangan geser dasar τ = γRS", "Bed shear τ = γRS"), num(r.shear, 2), shearU);
    t += QStringLiteral("</table>");
    uout_->setText(t);
    const Theme& th = Theme::current();
    const double b = c.bottom, z = c.sideSlope, y = r.depth;
    const double H = std::max(y * 1.35, y + 0.3);
    QVector<QPointF> ground{{-b / 2 - z * H, H}, {-b / 2, 0}, {b / 2, 0}, {b / 2 + z * H, H}};
    QVector<QPointF> water{{-b / 2 - z * y, y}, {b / 2 + z * y, y}};
    QVector<QPointF> crit{{-b / 2 - z * r.criticalDepth, r.criticalDepth}, {b / 2 + z * r.criticalDepth, r.criticalDepth}};
    PlotSeries sg{tr2("Dasar", "Channel"), ground, th.ground, 2.2};
    PlotSeries sw{tr2("Muka air normal", "Normal water surface"), water, th.water, 2.0};
    sw.fillBelowSeries = 0;
    sw.fillColor = withAlpha(th.water, 0.25);
    PlotSeries sc{tr2("Kedalaman kritis", "Critical depth"), crit, th.warning, 1.5, Qt::DashLine};
    uplot_->setTitles(QString(), tr2("Jarak dari as (%1)", "Distance from centre line (%1)").arg(L), tr2("Tinggi dari dasar (%1)", "Height above bed (%1)").arg(L));
    uplot_->setSeries({sg, sw, sc});
}

// ============================ scour ============================
QWidget* DesignView::buildScour() {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 6, 0);
    v->setSpacing(12);
    auto* src = new Card(tr2("Ambil nilai dari hasil hitungan", "Take values from computed results"), "design.scour");
    auto* sh = new QHBoxLayout;
    sh->setSpacing(10);
    srcPlan_ = new QComboBox;
    srcProfile_ = new QComboBox;
    srcXs_ = new QComboBox;
    srcXs_->setMinimumWidth(200);
    auto* fill = makeButton(tr2("Isi dari hasil", "Fill from results"), false, "import");
    sh->addWidget(new QLabel(QStringLiteral("Plan")));
    sh->addWidget(srcPlan_);
    sh->addWidget(new QLabel(tr2("Profil", "Profile")));
    sh->addWidget(srcProfile_);
    sh->addWidget(new QLabel(tr2("Penampang", "Cross section")));
    sh->addWidget(srcXs_, 1);
    sh->addWidget(fill);
    src->body()->addLayout(sh);
    srcNote_ = makeMuted(tr2("Mengisi y1 = kedalaman maksimum alur utama (muka air − dasar terendah), V1 = kecepatan alur utama, y0 = kedalaman yang sama, Q1 = debit alur utama, Q2 = debit total, S1 = kemiringan energi; untuk abutmen Ve = debit/luas bantaran kiri. Nilai lain diisi manual.",
                             "Fills y1 = maximum main-channel depth (water surface − lowest bed), V1 = main-channel velocity, y0 = the same depth, Q1 = main-channel flow, Q2 = total flow, S1 = energy slope; for the abutment Ve = left overbank flow/area. Other values are entered by hand."));
    src->body()->addWidget(srcNote_);
    v->addWidget(src);
    auto* grid = new QHBoxLayout;
    grid->setSpacing(12);
    // pier
    auto* cp = new Card(tr2("Gerusan pilar (CSU)", "Pier scour (CSU)"), "design.scour");
    auto* gp = new QGridLayout;
    gp->setHorizontalSpacing(8);
    gp->setVerticalSpacing(8);
    pa_ = spin(0.01, 100, 3, 0.1, 1.5);
    pl_ = spin(0.01, 1000, 3, 0.5, 6);
    pang_ = spin(0, 90, 1, 5, 0);
    pnose_ = new QComboBox;
    pnose_->addItem(tr2("Persegi", "Square nose"), int(design::PierNose::Square));
    pnose_->addItem(tr2("Bulat", "Round nose"), int(design::PierNose::Round));
    pnose_->addItem(tr2("Silinder", "Circular cylinder"), int(design::PierNose::Circular));
    pnose_->addItem(tr2("Kelompok silinder", "Group of cylinders"), int(design::PierNose::GroupOfCylinders));
    pnose_->addItem(tr2("Runcing", "Sharp nose"), int(design::PierNose::Sharp));
    pnose_->setCurrentIndex(1);
    pk3_ = new QComboBox;
    pk3_->addItem(tr2("Air jernih / dasar rata / bukit pasir kecil (1,1)", "Clear water / plane bed / small dunes (1.1)"), 1.1);
    pk3_->addItem(tr2("Bukit pasir sedang (1,2)", "Medium dunes (1.2)"), 1.2);
    pk3_->addItem(tr2("Bukit pasir besar (1,3)", "Large dunes (1.3)"), 1.3);
    py_ = spin(0.01, 1000, 3, 0.1, 3);
    pv_ = spin(0, 100, 3, 0.1, 2);
    int r = 0;
    auto addRow = [](QGridLayout* g, int& r, const QString& t, QWidget* w) {
        g->addWidget(labelWithInfo(t, "design.scour"), r, 0);
        g->addWidget(w, r++, 1, Qt::AlignLeft);
    };
    addRow(gp, r, tr2("Lebar pilar a", "Pier width a"), pa_);
    addRow(gp, r, tr2("Panjang pilar L", "Pier length L"), pl_);
    addRow(gp, r, tr2("Sudut serang (°)", "Attack angle (°)"), pang_);
    addRow(gp, r, tr2("Bentuk ujung (K1)", "Nose shape (K1)"), pnose_);
    addRow(gp, r, tr2("Kondisi dasar (K3)", "Bed condition (K3)"), pk3_);
    addRow(gp, r, tr2("Kedalaman hulu y1", "Upstream depth y1"), py_);
    addRow(gp, r, tr2("Kecepatan hulu V1", "Upstream velocity V1"), pv_);
    cp->body()->addLayout(gp);
    pout_ = outLabel();
    cp->body()->addWidget(pout_);
    cp->body()->addStretch();
    grid->addWidget(cp, 1);
    // contraction
    auto* cc = new Card(tr2("Gerusan kontraksi", "Contraction scour"), "design.scour");
    cmode_ = new Segmented;
    cmode_->addOption(tr2("Dasar hidup", "Live bed"), 1);
    cmode_->addOption(tr2("Air jernih", "Clear water"), 0);
    cmode_->setValue(1);
    cc->body()->addWidget(cmode_);
    auto* gc = new QGridLayout;
    gc->setHorizontalSpacing(8);
    gc->setVerticalSpacing(8);
    cy1_ = spin(0.01, 1000, 3, 0.1, 2);
    cy0_ = spin(0, 1000, 3, 0.1, 2);
    cq1_ = spin(0, 1e7, 2, 10, 100);
    cq2_ = spin(0, 1e7, 2, 10, 100);
    cw1_ = spin(0.01, 1e5, 2, 1, 40);
    cw2_ = spin(0.01, 1e5, 2, 1, 30);
    cs1_ = spin(0.000001, 1, 6, 0.0001, 0.001);
    cd50_ = spin(0.01, 2000, 3, 0.1, 1);
    cd50_->setSuffix(QStringLiteral(" mm"));
    r = 0;
    addRow(gc, r, tr2("Kedalaman hulu y1", "Upstream depth y1"), cy1_);
    addRow(gc, r, tr2("Kedalaman di kontraksi y0", "Depth in the contraction y0"), cy0_);
    addRow(gc, r, tr2("Debit alur hulu Q1", "Upstream channel flow Q1"), cq1_);
    addRow(gc, r, tr2("Debit di kontraksi Q2", "Flow in the contraction Q2"), cq2_);
    addRow(gc, r, tr2("Lebar dasar hulu W1", "Upstream bottom width W1"), cw1_);
    addRow(gc, r, tr2("Lebar dasar kontraksi W2", "Contracted bottom width W2"), cw2_);
    addRow(gc, r, tr2("Kemiringan energi S1", "Energy slope S1"), cs1_);
    addRow(gc, r, QStringLiteral("D50"), cd50_);
    cc->body()->addLayout(gc);
    cout_ = outLabel();
    cc->body()->addWidget(cout_);
    cc->body()->addStretch();
    grid->addWidget(cc, 1);
    // abutment
    auto* ca = new Card(tr2("Gerusan abutmen (Froehlich)", "Abutment scour (Froehlich)"), "design.scour");
    auto* ga = new QGridLayout;
    ga->setHorizontalSpacing(8);
    ga->setVerticalSpacing(8);
    al_ = spin(0, 1e5, 2, 1, 30);
    aya_ = spin(0.01, 1000, 3, 0.1, 2);
    ave_ = spin(0, 100, 3, 0.1, 0.5);
    aang_ = spin(1, 179, 1, 5, 90);
    ak1_ = new QComboBox;
    ak1_->addItem(tr2("Dinding tegak (1,00)", "Vertical wall (1.00)"), 1.0);
    ak1_->addItem(tr2("Dinding sayap (0,82)", "Wing wall (0.82)"), 0.82);
    ak1_->addItem(tr2("Spill-through (0,55)", "Spill-through (0.55)"), 0.55);
    r = 0;
    addRow(ga, r, tr2("Panjang terhalang L'", "Obstructed length L'"), al_);
    addRow(ga, r, tr2("Kedalaman bantaran ya", "Floodplain depth ya"), aya_);
    addRow(ga, r, tr2("Kecepatan terhalang Ve", "Obstructed velocity Ve"), ave_);
    addRow(ga, r, tr2("Bentuk abutmen (K1)", "Abutment shape (K1)"), ak1_);
    addRow(ga, r, tr2("Sudut tanggul (°)", "Embankment angle (°)"), aang_);
    ca->body()->addLayout(ga);
    aout_ = outLabel();
    ca->body()->addWidget(aout_);
    ca->body()->addStretch();
    grid->addWidget(ca, 1);
    v->addLayout(grid);
    total_ = makeMuted(QString());
    v->addWidget(total_);
    v->addStretch();
    for (auto* b : {pa_, pl_, pang_, py_, pv_, cy1_, cy0_, cq1_, cq2_, cw1_, cw2_, cs1_, cd50_, al_, aya_, ave_, aang_})
        connect(b, &QDoubleSpinBox::valueChanged, this, &DesignView::computeScour);
    for (auto* c : {pnose_, pk3_, ak1_}) connect(c, &QComboBox::currentIndexChanged, this, &DesignView::computeScour);
    connect(cmode_, &Segmented::changed, this, [this](int) { computeScour(); });
    connect(srcPlan_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) fillSourceProfiles(); });
    connect(srcProfile_, &QComboBox::currentIndexChanged, this, [this]() {
        if (loading_) return;
        // keep the selected cross section when the profile changes
        const QString keep = srcXs_->currentText();
        fillSourceProfiles();
        const int i = srcXs_->findText(keep);
        if (i >= 0) srcXs_->setCurrentIndex(i);
    });
    connect(fill, &QPushButton::clicked, this, &DesignView::fillFromResult);
    return scrollOf(w);
}

void DesignView::computeScour() {
    if (!pout_) return;
    const UnitSystem u = units();
    const QString L = lenU();
    design::PierScourInput pi;
    pi.a = pa_->value();
    pi.L = pl_->value();
    pi.angle = pang_->value();
    pi.nose = static_cast<design::PierNose>(pnose_->currentData().toInt());
    pi.K3 = pk3_->currentData().toDouble();
    pi.y1 = py_->value();
    pi.V1 = pv_->value();
    const auto p = design::pierScour(pi, u);
    QString t = QStringLiteral("<table>");
    t += row(QStringLiteral("K1 / K2 / K3"), QStringLiteral("%1 / %2 / %3").arg(num(p.K1, 2), num(p.K2, 2), num(pi.K3, 2)));
    t += row(tr2("Froude Fr1", "Froude Fr1"), num(p.froude, 3));
    t += row(tr2("Batas atas", "Upper limit"), num(p.limit, 3), L);
    t += row(tr2("Gerusan pilar ys", "Pier scour ys"), num(p.scour, 3), L);
    t += QStringLiteral("</table>");
    if (p.limited) t += tr2("<br>Nilai dibatasi oleh batas atas HEC-18 untuk pilar searah aliran (2,4a untuk Fr ≤ 0,8; 3,0a di atasnya).", "<br>Limited by the HEC-18 upper limit for aligned piers (2.4a for Fr ≤ 0.8; 3.0a above).");
    if (pi.angle > 5 && pi.L / pi.a > 12) t += tr2("<br>L/a > 12: K2 dihitung dengan L/a = 12.", "<br>L/a > 12: K2 is computed with L/a = 12.");
    pout_->setText(t);
    design::ContractionInput ci;
    ci.liveBed = cmode_->value() == 1;
    ci.y1 = cy1_->value();
    ci.y0 = cy0_->value();
    ci.Q1 = cq1_->value();
    ci.Q2 = cq2_->value();
    ci.W1 = cw1_->value();
    ci.W2 = cw2_->value();
    ci.S1 = cs1_->value();
    ci.D50mm = cd50_->value();
    const auto c = design::contractionScour(ci, u);
    t = QStringLiteral("<table>");
    if (ci.liveBed) {
        t += row(QStringLiteral("V*/ω"), num(c.ustarOverW, 3));
        t += row(QStringLiteral("k1"), num(c.k1, 2));
    }
    t += row(tr2("Kedalaman setelah gerusan y2", "Depth after scour y2"), num(c.y2, 3), L);
    t += row(tr2("Gerusan kontraksi ys = y2 − y0", "Contraction scour ys = y2 − y0"), num(std::max(0.0, c.scour), 3), L);
    t += QStringLiteral("</table>");
    if (c.scour < 0) t += tr2("<br>y2 < y0: tidak ada gerusan kontraksi.", "<br>y2 < y0: no contraction scour.");
    cout_->setText(t);
    design::AbutmentInput ai;
    ai.Lprime = al_->value();
    ai.ya = aya_->value();
    ai.Ve = ave_->value();
    ai.K1 = ak1_->currentData().toDouble();
    ai.angle = aang_->value();
    const auto a = design::abutmentScour(ai, u);
    t = QStringLiteral("<table>");
    t += row(QStringLiteral("K2"), num(a.K2, 3));
    t += row(tr2("Froude Fr", "Froude Fr"), num(a.froude, 3));
    t += row(tr2("Gerusan abutmen ys", "Abutment scour ys"), num(a.scour, 3), L);
    t += QStringLiteral("</table>");
    if (ai.ya > 0 && ai.Lprime / ai.ya > 25) t += tr2("<br>L'/ya > 25: HEC-18 menyarankan persamaan HIRE.", "<br>L'/ya > 25: HEC-18 recommends the HIRE equation.");
    aout_->setText(t);
    total_->setText(tr2("Total di pilar (gerusan pilar + kontraksi) = %1 %2. Status Pratinjau: rumus HEC-18 edisi ke-5, belum dibandingkan dengan Hydraulic Design HEC-RAS. Keputusan desain tetap tanggung jawab perencana.",
                        "Total at the pier (pier + contraction scour) = %1 %2. Preview status: HEC-18 5th edition equations, not compared with the HEC-RAS Hydraulic Design yet. Design decisions remain the engineer's responsibility.")
                        .arg(num(p.scour + std::max(0.0, c.scour), 3), L));
}

void DesignView::fillSourcePlans() {
    auto* p = s_->project();
    const QString keep = srcPlan_->currentData().toString();
    const bool was = loading_;
    loading_ = true;
    srcPlan_->clear();
    if (p)
        for (const auto& [ext, pf] : p->plans)
            if (s_->results(qs(ext))) srcPlan_->addItem(QStringLiteral("%1 · %2").arg(qs(ext), qs(pf.model.title)), qs(ext));
    if (srcPlan_->count() == 0) srcPlan_->addItem(tr2("(belum ada hasil)", "(no results yet)"), QString());
    int i = srcPlan_->findData(keep);
    if (i < 0) i = srcPlan_->findData(s_->currentPlanExt());
    srcPlan_->setCurrentIndex(std::max(0, i));
    loading_ = was;
    fillSourceProfiles();
}

void DesignView::fillSourceProfiles() {
    const QString plan = srcPlan_->currentData().toString();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    const bool was = loading_;
    loading_ = true;
    const int keepProfile = srcProfile_->currentIndex();
    srcProfile_->clear();
    if (r)
        for (const auto& pr : r->profiles) srcProfile_->addItem(qs(pr.name));
    srcProfile_->setCurrentIndex(std::clamp(keepProfile, 0, std::max(0, srcProfile_->count() - 1)));
    srcXs_->clear();
    const int k = srcProfile_->currentIndex();
    if (r && k >= 0 && size_t(k) < r->profiles.size())
        for (size_t i = 0; i < r->profiles[size_t(k)].xs.size(); ++i) {
            const auto& x = r->profiles[size_t(k)].xs[i];
            srcXs_->addItem(QStringLiteral("%1 / %2  RS %3").arg(qs(x.river), qs(x.reach), qs(x.rs)), int(i));
        }
    loading_ = was;
}

void DesignView::fillFromResult() {
    const QString plan = srcPlan_->currentData().toString();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    const int k = srcProfile_->currentIndex();
    const int i = srcXs_->currentData().toInt();
    if (!r || k < 0 || size_t(k) >= r->profiles.size() || i < 0 || size_t(i) >= r->profiles[size_t(k)].xs.size()) {
        showToast(this, tr2("Pilih plan yang sudah punya hasil.", "Choose a plan that has results."), "info");
        return;
    }
    const auto& x = r->profiles[size_t(k)].xs[size_t(i)];
    const double depth = x.ws - x.minChEl;
    py_->setValue(depth);
    pv_->setValue(x.velCh);
    cy1_->setValue(depth);
    cy0_->setValue(depth);
    cq1_->setValue(x.qCh);
    cq2_->setValue(x.q);
    if (x.egSlope > 0) cs1_->setValue(x.egSlope);
    if (x.areaLob > 0 && x.qLob > 0) ave_->setValue(x.qLob / x.areaLob);
    computeScour();
    showToast(this, tr2("Diisi dari %1, profil %2, RS %3.", "Filled from %1, profile %2, RS %3.").arg(plan, srcProfile_->currentText(), qs(x.rs)));
}

// ============================ Manning's n calibration ============================
QWidget* DesignView::buildCalibration() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    auto* c1 = new Card(tr2("Pengamatan muka air", "Observed water surfaces"), "design.calibration");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(8);
    kplan_ = new QComboBox;
    kprofile_ = new QComboBox;
    kscope_ = new QComboBox;
    kscope_->addItem(tr2("n alur utama saja", "Main channel n only"), 0);
    kscope_->addItem(tr2("Semua n (alur dan bantaran)", "All n (channel and overbanks)"), 1);
    klo_ = spin(0.1, 1, 2, 0.05, 0.5);
    khi_ = spin(1, 10, 2, 0.1, 2);
    int r = 0;
    g->addWidget(labelWithInfo(tr2("Plan steady", "Steady plan"), "design.calibration"), r, 0);
    g->addWidget(kplan_, r++, 1);
    g->addWidget(labelWithInfo(tr2("Profil", "Profile"), "design.calibration"), r, 0);
    g->addWidget(kprofile_, r++, 1);
    g->addWidget(labelWithInfo(tr2("n yang diubah", "n changed"), "design.calibration"), r, 0);
    g->addWidget(kscope_, r++, 1);
    auto* range = new QHBoxLayout;
    range->addWidget(klo_);
    range->addWidget(new QLabel(QStringLiteral("–")));
    range->addWidget(khi_);
    range->addStretch();
    g->addWidget(labelWithInfo(tr2("Rentang faktor", "Factor range"), "design.calibration"), r, 0);
    g->addLayout(range, r++, 1);
    g->setColumnStretch(1, 1);
    c1->body()->addLayout(g);
    kobs_ = new QTableWidget(0, 4);
    kobs_->verticalHeader()->hide();
    kobs_->setAlternatingRowColors(true);
    kobs_->setMinimumHeight(240);
    c1->body()->addWidget(kobs_, 1);
    c1->body()->addWidget(makeMuted(tr2("Isi muka air terukur di kolom terakhir untuk penampang yang punya data (kosong = tidak dipakai). Minimal satu pengamatan.",
                                        "Enter the observed water surface in the last column for the sections that have data (empty = not used). At least one observation.")));
    auto* bh = new QHBoxLayout;
    krun_ = makeButton(tr2("Kalibrasi", "Calibrate"), true, "play");
    kapply_ = makeButton(tr2("Terapkan ke geometri", "Apply to geometry"), false, "check");
    kapply_->setEnabled(false);
    bh->addWidget(krun_);
    bh->addWidget(kapply_);
    bh->addStretch();
    c1->body()->addLayout(bh);
    h->addWidget(c1, 2);
    auto* c2 = new Card(tr2("Hasil kalibrasi", "Calibration result"), "design.calibration");
    kout_ = outLabel();
    kout_->setText(tr2("Belum dikalibrasi.", "Not calibrated yet."));
    c2->body()->addWidget(kout_);
    kplot_ = new PlotWidget;
    kplot_->setReverseX(true);
    kplot_->setEmptyText(tr2("Isi pengamatan lalu klik Kalibrasi.", "Enter observations, then click Calibrate."));
    c2->body()->addWidget(kplot_, 1);
    h->addWidget(c2, 3);
    connect(kplan_, &QComboBox::currentIndexChanged, this, [this]() {
        if (loading_) return;
        kHas_ = false;
        fillObservations();
        saveObservations();
    });
    connect(kprofile_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) { fillObservations(); saveObservations(); } });
    connect(kscope_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) saveObservations(); });
    connect(kobs_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) { if (!loading_ && it && it->column() == 3) saveObservations(); });
    connect(krun_, &QPushButton::clicked, this, &DesignView::runCalibration);
    connect(kapply_, &QPushButton::clicked, this, &DesignView::applyFactor);
    return w;
}

void DesignView::fillCalibrationPlans() {
    auto* p = s_->project();
    const bool was = loading_;
    loading_ = true;
    kplan_->clear();
    std::string savedPlan;
    int savedScope = 0;
    if (p) {
        auto it = p->settings.modules.find("calibration");
        if (it != p->settings.modules.end()) {
            savedPlan = it->second.get("plan").str();
            savedScope = int(it->second.get("scope").num(0));
            klo_->setValue(it->second.get("factor_min").num(0.5));
            khi_->setValue(it->second.get("factor_max").num(2));
        }
        for (const auto& [ext, pf] : p->plans)
            if (!pf.model.isUnsteady() && p->steadyFlows.count(pf.model.flowFile)) kplan_->addItem(QStringLiteral("%1 · %2").arg(qs(ext), qs(pf.model.title)), qs(ext));
    }
    int i = kplan_->findData(qs(savedPlan));
    if (i < 0) i = kplan_->findData(s_->currentPlanExt());
    kplan_->setCurrentIndex(std::max(0, i));
    kscope_->setCurrentIndex(kscope_->findData(savedScope));
    loading_ = was;
    fillObservations();
}

void DesignView::fillObservations() {
    auto* p = s_->project();
    const bool was = loading_;
    loading_ = true;
    const QString plan = kplan_->currentData().toString();
    const io::PlanFile* pf = p && p->plans.count(ss(plan)) ? &p->plans.at(ss(plan)) : nullptr;
    // profiles of the flow file
    const int keepProfile = kprofile_->currentIndex();
    kprofile_->clear();
    std::map<std::string, double> saved;   // "river|reach|rs" -> observed ws
    int savedProfile = keepProfile;
    if (p) {
        auto it = p->settings.modules.find("calibration");
        if (it != p->settings.modules.end() && it->second.get("plan").str() == ss(plan)) {
            savedProfile = int(it->second.get("profile").num(0));
            for (const auto& o : it->second.get("observations").items())
                saved[o.get("river").str() + "|" + o.get("reach").str() + "|" + o.get("rs").str()] = o.get("ws").num();
        }
    }
    if (pf && p->steadyFlows.count(pf->model.flowFile))
        for (const auto& n : p->steadyFlows.at(pf->model.flowFile).model.profileNames) kprofile_->addItem(qs(n));
    kprofile_->setCurrentIndex(std::clamp(savedProfile, 0, std::max(0, kprofile_->count() - 1)));
    const RunResult* res = plan.isEmpty() ? nullptr : s_->results(plan);
    std::map<std::string, double> computed;
    const int k = kprofile_->currentIndex();
    if (res && !res->isUnsteady() && k >= 0 && size_t(k) < res->profiles.size())
        for (const auto& x : res->profiles[size_t(k)].xs) computed[x.river + "|" + x.reach + "|" + x.rs] = x.ws;
    kobs_->clear();
    kobs_->setColumnCount(4);
    kobs_->setHorizontalHeaderLabels({QStringLiteral("River / Reach"), QStringLiteral("RS"), tr2("Muka air hitungan (%1)", "Computed WS (%1)").arg(lenU()),
                                      tr2("Muka air terukur (%1)", "Observed WS (%1)").arg(lenU())});
    kobs_->setRowCount(0);
    if (pf && p->geometries.count(pf->model.geomFile)) {
        for (const auto& reach : p->geometries.at(pf->model.geomFile).model.reaches)
            for (const auto& x : reach.nodes) {
                if (x.type != NodeType::CrossSection) continue;
                const int r = kobs_->rowCount();
                kobs_->insertRow(r);
                auto* a = new QTableWidgetItem(QStringLiteral("%1 / %2").arg(qs(reach.river), qs(reach.reach)));
                a->setData(Qt::UserRole, QStringList{qs(reach.river), qs(reach.reach), qs(x.rs)});
                a->setFlags(a->flags() & ~Qt::ItemIsEditable);
                kobs_->setItem(r, 0, a);
                auto* b = new QTableWidgetItem(qs(x.rs));
                b->setFlags(b->flags() & ~Qt::ItemIsEditable);
                kobs_->setItem(r, 1, b);
                const std::string key = reach.river + "|" + reach.reach + "|" + x.rs;
                auto* c = new QTableWidgetItem(computed.count(key) ? num(computed[key], 3) : QStringLiteral("—"));
                c->setFlags(c->flags() & ~Qt::ItemIsEditable);
                c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                kobs_->setItem(r, 2, c);
                auto* d = new QTableWidgetItem(saved.count(key) ? QString::number(saved[key], 'f', 3) : QString());
                d->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                kobs_->setItem(r, 3, d);
            }
    }
    kobs_->resizeColumnsToContents();
    kobs_->horizontalHeader()->setStretchLastSection(true);
    loading_ = was;
    plotCalibration();
}

void DesignView::saveObservations() {
    auto* p = s_->project();
    if (!p || loading_) return;
    json::Value v = json::Value::object();
    v["plan"] = ss(kplan_->currentData().toString());
    v["profile"] = kprofile_->currentIndex();
    v["scope"] = kscope_->currentData().toInt();
    v["factor_min"] = klo_->value();
    v["factor_max"] = khi_->value();
    json::Value obs = json::Value::array();
    for (int r = 0; r < kobs_->rowCount(); ++r) {
        auto* it = kobs_->item(r, 3);
        if (!it || it->text().trimmed().isEmpty()) continue;
        bool ok = false;
        const double ws = QString(it->text()).replace(',', '.').toDouble(&ok);
        if (!ok) continue;
        const QStringList k = kobs_->item(r, 0)->data(Qt::UserRole).toStringList();
        json::Value o = json::Value::object();
        o["river"] = ss(k[0]);
        o["reach"] = ss(k[1]);
        o["rs"] = ss(k[2]);
        o["ws"] = ws;
        obs.push(o);
    }
    v["observations"] = obs;
    auto old = p->settings.modules.find("calibration");
    if (old != p->settings.modules.end() && old->second.dump(0) == v.dump(0)) return;
    p->settings.modules["calibration"] = v;
    s_->markEdited();
}

void DesignView::runCalibration() {
    auto* p = s_->project();
    if (!p || kRunning_) return;
    saveObservations();
    const std::string plan = ss(kplan_->currentData().toString());
    if (!p->plans.count(plan)) {
        sheetInfo(this, tr2("Belum ada plan steady", "No steady plan"), tr2("Kalibrasi n memakai plan aliran steady. Buat atau pilih plan steady dulu.", "The n calibration uses a steady flow plan. Create or choose a steady plan first."));
        return;
    }
    const Plan pl = p->plans.at(plan).model;
    if (!p->geometries.count(pl.geomFile) || !p->steadyFlows.count(pl.flowFile)) return;
    std::vector<design::Observation> obs;
    for (int r = 0; r < kobs_->rowCount(); ++r) {
        auto* it = kobs_->item(r, 3);
        if (!it || it->text().trimmed().isEmpty()) continue;
        bool ok = false;
        const double ws = QString(it->text()).replace(',', '.').toDouble(&ok);
        if (!ok) {
            sheetInfo(this, tr2("Nilai tidak valid", "Invalid value"), tr2("Baris %1: '%2' bukan angka.", "Row %1: '%2' is not a number.").arg(r + 1).arg(it->text()));
            return;
        }
        const QStringList k = kobs_->item(r, 0)->data(Qt::UserRole).toStringList();
        obs.push_back({ss(k[0]), ss(k[1]), ss(k[2]), ws});
    }
    if (obs.empty()) {
        sheetInfo(this, tr2("Belum ada pengamatan", "No observations"), tr2("Isi muka air terukur minimal di satu penampang.", "Enter an observed water surface at one cross section at least."));
        return;
    }
    Geometry g = p->geometries.at(pl.geomFile).model;
    io::applyGeometrySettings(p->settings, pl.geomFile, g);
    const SteadyFlow f = p->steadyFlows.at(pl.flowFile).model;
    const UnitSystem u = p->units();
    const int profile = std::max(0, kprofile_->currentIndex());
    const int scope = kscope_->currentData().toInt();
    const double lo = klo_->value(), hi = std::max(khi_->value(), lo + 0.01);
    kRunning_ = true;
    krun_->setEnabled(false);
    kapply_->setEnabled(false);
    kout_->setText(tr2("Menghitung… (beberapa hitungan steady)", "Computing… (several steady runs)"));
    auto* out = new design::CalibrationResult;
    QThread* th = QThread::create([=]() { *out = design::calibrateN(g, f, pl, u, profile, obs, scope, lo, hi); });
    QPointer<DesignView> self(this);
    const std::string geomExt = pl.geomFile;
    connect(th, &QThread::finished, this, [self, th, out, obs, geomExt]() {
        th->deleteLater();
        std::unique_ptr<design::CalibrationResult> r(out);
        if (!self) return;
        self->kRunning_ = false;
        self->krun_->setEnabled(true);
        self->kres_ = *r;
        self->kresObs_ = obs;
        self->kresGeom_ = geomExt;
        self->kHas_ = true;
        self->kApplied_ = false;
        self->kapply_->setEnabled(r->ok && std::fabs(r->factor - 1) > 1e-4);
        self->plotCalibration();
    });
    th->start();
}

void DesignView::plotCalibration() {
    if (!kplot_) return;
    const Theme& th = Theme::current();
    auto* p = s_->project();
    if (!kHas_ || !p) {
        kplot_->clear();
        kout_->setText(tr2("Belum dikalibrasi.", "Not calibrated yet."));
        return;
    }
    if (!kres_.ok) {
        kplot_->clear();
        kout_->setText(tr2("Kalibrasi gagal: %1", "Calibration failed: %1").arg(qs(kres_.error)));
        return;
    }
    const QString L = lenU();
    QString t = QStringLiteral("<table>");
    t += row(tr2("Faktor n terbaik", "Best n factor"), num(kres_.factor, 4));
    t += row(tr2("RMS selisih sebelum", "RMS difference before"), num(kres_.rmsBefore, 3), L);
    t += row(tr2("RMS selisih sesudah", "RMS difference after"), num(kres_.rmsAfter, 3), L);
    t += row(tr2("Jumlah hitungan steady", "Steady runs"), QString::number(kres_.runs));
    t += QStringLiteral("</table>");
    if (kres_.factor <= klo_->value() + 1e-3 || kres_.factor >= khi_->value() - 1e-3)
        t += tr2("<br>Faktor menyentuh batas rentang: perlebar rentang atau periksa data.", "<br>The factor reached the end of the range: widen the range or check the data.");
    if (kApplied_) t += tr2("<br>Sudah diterapkan ke %1. Simpan proyek dan hitung ulang.", "<br>Applied to %1. Save the project and compute again.").arg(qs(kresGeom_));
    kout_->setText(t);
    // along the channel: distance of every observation in its reach
    std::map<std::string, std::map<std::string, double>> dist;
    if (p->geometries.count(kresGeom_))
        for (const auto& r : p->geometries.at(kresGeom_).model.reaches) dist[r.river + "|" + r.reach] = distances(r);
    struct Pt { double x, obs, before, after; };
    std::vector<Pt> pts;
    for (size_t i = 0; i < kresObs_.size(); ++i) {
        const auto& o = kresObs_[i];
        auto& d = dist[o.river + "|" + o.reach];
        const double x = d.count(o.rs) ? d[o.rs] : double(i);
        pts.push_back({x, o.ws, i < kres_.computedBefore.size() ? kres_.computedBefore[i] : NAN, i < kres_.computedAfter.size() ? kres_.computedAfter[i] : NAN});
    }
    std::sort(pts.begin(), pts.end(), [](const Pt& a, const Pt& b) { return a.x < b.x; });
    QVector<QPointF> so, sb, sa;
    for (const auto& q : pts) {
        so << QPointF(q.x, q.obs);
        if (std::isfinite(q.before)) sb << QPointF(q.x, q.before);
        if (std::isfinite(q.after)) sa << QPointF(q.x, q.after);
    }
    PlotSeries po{tr2("Terukur", "Observed"), so, th.error, 1.0, Qt::DotLine};
    po.markers = true;
    PlotSeries pb{tr2("Hitungan, n awal", "Computed, original n"), sb, th.text3, 1.6, Qt::DashLine};
    pb.markers = true;
    PlotSeries pa{tr2("Hitungan, n × %1", "Computed, n × %1").arg(num(kres_.factor, 3)), sa, th.water, 2.2};
    pa.markers = true;
    kplot_->setTitles(QString(), tr2("Jarak dari hilir (%1)", "Distance from downstream (%1)").arg(L), tr2("Muka air (%1)", "Water surface (%1)").arg(L));
    kplot_->setSeries({pb, pa, po});
}

void DesignView::applyFactor() {
    auto* p = s_->project();
    if (!p || !kHas_ || !kres_.ok || kApplied_ || !p->geometries.count(kresGeom_)) return;
    if (!sheetConfirm(this, tr2("Terapkan faktor n", "Apply the n factor"),
                      tr2("Semua n Manning %1 di geometri %2 dikalikan %3. Perubahan bisa dibatalkan dengan menutup proyek tanpa menyimpan.",
                          "Every %1 Manning's n of geometry %2 is multiplied by %3. You can undo it by closing the project without saving.")
                          .arg(kscope_->currentData().toInt() == 0 ? tr2("alur utama", "main channel") : tr2("(alur dan bantaran)", "(channel and overbank)"), qs(kresGeom_), num(kres_.factor, 4)),
                      tr2("Terapkan", "Apply")))
        return;
    design::scaleManning(p->geometries.at(kresGeom_).model, kres_.factor, kscope_->currentData().toInt());
    kApplied_ = true;
    kapply_->setEnabled(false);
    s_->markEdited();
    plotCalibration();
    showToast(this, tr2("n dikalikan %1 di %2.", "n multiplied by %1 in %2.").arg(num(kres_.factor, 4), qs(kresGeom_)));
}

}  // namespace deras::ui

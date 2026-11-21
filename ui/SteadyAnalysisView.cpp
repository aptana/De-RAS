// De-RAS - ui/SteadyAnalysisView.cpp
#include "SteadyAnalysisView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common.h"
#include "DoctorSheet.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "Widgets.h"

namespace deras::ui {

SteadyAnalysisView::SteadyAnalysisView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* left = new QVBoxLayout;
    left->setSpacing(12);

    // ---- plan card ----
    auto* pc = new Card(tr2("Plan", "Plan"), "plan.editor");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    plan_ = new QComboBox;
    plan_->setFont(monoFont());
    plan_->setAccessibleName(tr2("Plan", "Plan"));
    auto* newPlan = new IconButton("plus", tr2("Plan baru…", "New plan…"));
    title_ = new QLineEdit;
    shortId_ = new QLineEdit;
    shortId_->setMaxLength(24);
    geom_ = new QComboBox;
    geom_->setFont(monoFont());
    flow_ = new QComboBox;
    flow_->setFont(monoFont());
    int r = 0;
    auto* planRow = new QHBoxLayout;
    planRow->addWidget(plan_, 1);
    planRow->addWidget(newPlan);
    g->addWidget(labelWithInfo(tr2("Plan aktif", "Current plan"), "plan.editor"), r, 0);
    g->addLayout(planRow, r++, 1);
    g->addWidget(labelWithInfo(tr2("Judul plan", "Plan title"), "plan.title"), r, 0);
    g->addWidget(title_, r++, 1);
    g->addWidget(labelWithInfo(tr2("ID singkat", "Short ID"), "plan.shortid"), r, 0);
    g->addWidget(shortId_, r++, 1);
    g->addWidget(labelWithInfo(tr2("File geometri", "Geometry file"), "plan.files"), r, 0);
    g->addWidget(geom_, r++, 1);
    g->addWidget(labelWithInfo(tr2("File aliran steady", "Steady flow file"), "plan.files"), r, 0);
    g->addWidget(flow_, r++, 1);
    regime_ = new Segmented;
    regime_->addOption(tr2("Subkritis", "Subcritical"), 0);
    regime_->addOption(tr2("Superkritis", "Supercritical"), 1, true,
                       tr2("Dihitung dari hulu ke hilir; butuh batas hulu. Status: Pratinjau.", "Computed from upstream to downstream; needs an upstream boundary. Status: Preview."));
    regime_->addOption(tr2("Campuran", "Mixed"), 2, true,
                       tr2("Subkritis dan superkritis, loncatan hidraulik dari gaya spesifik; butuh batas hulu dan hilir. Status: Pratinjau.",
                           "Subcritical and supercritical with hydraulic jumps located by specific force; needs upstream and downstream boundaries. Status: Preview."));
    g->addWidget(labelWithInfo(tr2("Rezim aliran", "Flow regime"), "plan.regime"), r, 0);
    g->addWidget(regime_, r++, 1, Qt::AlignLeft);
    output_ = new QLabel;
    output_->setObjectName("Mono");
    output_->setWordWrap(true);
    output_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    g->addWidget(labelWithInfo(tr2("File hasil", "Results file"), "plan.output"), r, 0, Qt::AlignTop);
    g->addWidget(output_, r++, 1);
    g->setColumnStretch(1, 1);
    pc->body()->addLayout(g);
    left->addWidget(pc);

    // ---- options card ----
    auto* oc = new Card(tr2("Opsi komputasi", "Computation options"), "plan.tolerance");
    auto mk = [this](double lo, double hi, int dec) {
        auto* sp = new QDoubleSpinBox;
        sp->setRange(lo, hi);
        sp->setDecimals(dec);
        sp->setLocale(QLocale::c());
        sp->setKeyboardTracking(false);
        sp->setFont(monoFont());
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sp->setAlignment(Qt::AlignRight);
        connect(sp, &QDoubleSpinBox::valueChanged, this, [this]() { commit(); });
        return sp;
    };
    tol_ = mk(0.00001, 1, 5);
    ctol_ = mk(0.00001, 1, 5);
    maxErr_ = mk(0.001, 10, 3);
    trials_ = new QSpinBox;
    trials_->setRange(3, 200);
    trials_->setFont(monoFont());
    trials_->setAlignment(Qt::AlignRight);
    friction_ = new QComboBox;
    friction_->addItem(tr2("Rata-rata konveyansi (bawaan)", "Average conveyance (default)"), 1);
    friction_->addItem(tr2("Rata-rata kemiringan gesek", "Average friction slope"), 2);
    friction_->addItem(tr2("Rata-rata geometrik", "Geometric mean"), 3);
    friction_->addItem(tr2("Rata-rata harmonik", "Harmonic mean"), 4);
    auto* og = new QGridLayout;
    og->setHorizontalSpacing(10);
    og->setVerticalSpacing(10);
    r = 0;
    og->addWidget(labelWithInfo(tr2("Toleransi standard step", "Standard step tolerance"), "plan.tolerance"), r, 0);
    og->addWidget(tol_, r, 1);
    og->addWidget(labelWithInfo(tr2("Toleransi kedalaman kritis", "Critical depth tolerance"), "plan.tolerance"), r, 2);
    og->addWidget(ctol_, r++, 3);
    og->addWidget(labelWithInfo(tr2("Maks. iterasi", "Max. trials"), "plan.trials"), r, 0);
    og->addWidget(trials_, r, 1);
    og->addWidget(labelWithInfo(tr2("Toleransi selisih maks.", "Max. error tolerance"), "plan.maxerror"), r, 2);
    og->addWidget(maxErr_, r++, 3);
    og->addWidget(labelWithInfo(tr2("Metode kemiringan gesek", "Friction slope method"), "plan.friction"), r, 0);
    og->addWidget(friction_, r++, 1, 1, 3);
    og->setColumnStretch(1, 1);
    og->setColumnStretch(3, 1);
    oc->body()->addLayout(og);
    units_ = makeMuted(QString());
    oc->body()->addWidget(units_);
    left->addWidget(oc);
    left->addStretch();
    root->addLayout(left, 3);

    // ---- Model Doctor card ----
    auto* dc = new Card(QStringLiteral("Model Doctor"), "sim.doctor");
    dc->setMinimumWidth(340);
    doctorHead_ = new QLabel;
    doctorHead_->setObjectName("Strong");
    doctorHead_->setWordWrap(true);
    dc->body()->addWidget(doctorHead_);
    doctor_ = new QListWidget;
    doctor_->setWordWrap(true);
    doctor_->setObjectName("NavList");
    doctor_->setAccessibleName(QStringLiteral("Model Doctor"));
    dc->body()->addWidget(doctor_, 1);
    dc->body()->addWidget(makeMuted(tr2("Diperiksa otomatis setiap kali data berubah. Klik ganda pesan yang menyebut penampang untuk membukanya.",
                                        "Checked automatically after every change. Double-click a message with a location to open its cross section.")));
    root->addWidget(dc, 2);

    debounce_.setSingleShot(true);
    debounce_.setInterval(500);
    connect(&debounce_, &QTimer::timeout, this, &SteadyAnalysisView::refreshDoctor);
    connect(s_, &ProjectSession::modelEdited, this, [this]() { if (isVisible()) debounce_.start(); });
    connect(plan_, &QComboBox::activated, this, [this]() { s_->setCurrentPlan(plan_->currentData().toString()); });
    connect(newPlan, &QToolButton::clicked, this, &SteadyAnalysisView::newPlan);
    for (auto* le : {title_, shortId_}) connect(le, &QLineEdit::editingFinished, this, [this]() { commit(); });
    for (auto* cb : {geom_, flow_, friction_}) connect(cb, &QComboBox::activated, this, [this]() { commit(); });
    connect(trials_, &QSpinBox::valueChanged, this, [this]() { commit(); });
    connect(regime_, &Segmented::changed, this, [this]() { commit(); });
    connect(doctor_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        int rr = 0, xx = 0;
        if (s_->findLocation(it->data(Qt::UserRole).toString(), rr, xx)) {
            s_->selectCrossSection(rr, xx);
            emit navigate("xs");
        }
    });
    reload();
}

QString SteadyAnalysisView::title() const { return tr2("Analisis Steady", "Steady Flow Analysis"); }

QWidget* SteadyAnalysisView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* doc = makeButton(QStringLiteral("Model Doctor (F7)"), false, "doctor");
    auto* run = makeButton(tr2("Hitung (F5)", "Compute (F5)"), true, "run");
    h->addWidget(doc);
    h->addWidget(run);
    connect(doc, &QPushButton::clicked, this, [this]() { refreshDoctor(); emit requestDoctor(); });
    connect(run, &QPushButton::clicked, this, &View::requestRun);
    return w;
}

void SteadyAnalysisView::activated() {
    reload();
}

void SteadyAnalysisView::reload() {
    loading_ = true;
    plan_->clear();
    geom_->clear();
    flow_->clear();
    auto* p = s_->project();
    if (p) {
        for (auto& [e, pl] : p->plans) plan_->addItem(qs(e) + QStringLiteral(" · ") + qs(pl.model.title), qs(e));
        for (auto& [e, gg] : p->geometries) geom_->addItem(qs(e) + QStringLiteral(" · ") + qs(gg.model.title), qs(e));
        for (auto& [e, f] : p->steadyFlows) flow_->addItem(qs(e) + QStringLiteral(" · ") + qs(f.model.title), qs(e));
        plan_->setCurrentIndex(plan_->findData(s_->currentPlanExt()));
        units_->setText(tr2("Toleransi dalam satuan proyek (%1). Nilai bawaan sama dengan HEC-RAS.", "Tolerances in project units (%1). Defaults match HEC-RAS.").arg(qs(lengthUnit(p->units()))));
    }
    auto* pl = s_->currentPlan();
    for (QWidget* w : std::initializer_list<QWidget*>{title_, shortId_, geom_, flow_, regime_, tol_, ctol_, maxErr_, trials_, friction_}) w->setEnabled(pl != nullptr);
    if (pl) {
        const Plan& m = pl->model;
        title_->setText(qs(m.title));
        shortId_->setText(qs(m.shortId));
        geom_->setCurrentIndex(geom_->findData(qs(m.geomFile)));
        flow_->setCurrentIndex(flow_->findData(qs(m.flowFile)));
        regime_->setValue(m.regime == FlowRegime::Subcritical ? 0 : m.regime == FlowRegime::Supercritical ? 1 : 2);
        tol_->setValue(m.stdStepTol);
        ctol_->setValue(m.criticalTol);
        maxErr_->setValue(m.maxErrorTol);
        trials_->setValue(m.maxTrials);
        friction_->setCurrentIndex(friction_->findData(static_cast<int>(m.frictionMethod)));
        const QString base = qs(p->baseName());
        output_->setText(QStringLiteral("%1.%2.deras-results.json\n").arg(base, s_->currentPlanExt()) +
                         tr2("%1.%2.hdf (susunan HEC-RAS, Pratinjau)", "%1.%2.hdf (HEC-RAS layout, Preview)").arg(base, s_->currentPlanExt()));
        if (m.isUnsteady()) {
            units_->setText(tr2("Plan ini memakai aliran unsteady (.u##). Ubah dan hitung di Analisis Unsteady (menu Jalankan).", "This plan uses unsteady flow data (.u##). Edit and compute it in Unsteady Flow Analysis (Run menu)."));
            flow_->setEnabled(false);
        }
    } else {
        title_->clear();
        shortId_->clear();
        output_->setText(p ? tr2("Belum ada plan. Buat plan dengan tombol + di sebelah daftar plan.", "No plan yet. Create one with the + button next to the plan list.") : QString());
    }
    loading_ = false;
    refreshDoctor();
}

void SteadyAnalysisView::commit() {
    if (loading_) return;
    auto* pl = s_->currentPlan();
    if (!pl) return;
    Plan& m = pl->model;
    const std::string oldGeom = m.geomFile, oldFlow = m.flowFile;
    m.title = ss(title_->text());
    m.shortId = ss(shortId_->text());
    if (geom_->currentIndex() >= 0) m.geomFile = ss(geom_->currentData().toString());
    if (flow_->currentIndex() >= 0 && !m.isUnsteady()) m.flowFile = ss(flow_->currentData().toString());
    m.stdStepTol = tol_->value();
    m.criticalTol = ctol_->value();
    m.maxErrorTol = maxErr_->value();
    m.maxTrials = trials_->value();
    m.frictionMethod = static_cast<FrictionSlopeMethod>(friction_->currentData().toInt());
    m.regime = regime_->value() == 1 ? FlowRegime::Supercritical : regime_->value() == 2 ? FlowRegime::Mixed : FlowRegime::Subcritical;
    s_->markEdited();
    if (m.geomFile != oldGeom || m.flowFile != oldFlow) {
        // the editors follow the plan's files again
        s_->setEditGeometry(QString());
        s_->setEditFlow(QString());
    }
}

void SteadyAnalysisView::refreshDoctor() {
    doctor_->clear();
    if (!s_->isOpen()) {
        doctorHead_->setText(tr2("Belum ada proyek.", "No project open."));
        return;
    }
    const MessageList msgs = s_->doctor();
    int e = 0, w = 0;
    for (const auto& m : msgs) {
        if (m.severity == Severity::Error) ++e;
        if (m.severity == Severity::Warning) ++w;
        if (m.code == "MD_OK") continue;
        auto* it = new QListWidgetItem(severityPrefix(m.severity) + QStringLiteral("  ") + msgText(m) +
                                       (m.location.empty() ? QString() : QStringLiteral("\n    ") + qs(m.location)));
        it->setData(Qt::UserRole, qs(m.location));
        it->setForeground(m.severity == Severity::Error ? Theme::current().error : m.severity == Severity::Warning ? Theme::current().warning : Theme::current().text2);
        doctor_->addItem(it);
    }
    if (doctor_->count() == 0) doctor_->addItem(tr2("Tidak ada temuan.", "No findings."));
    doctorHead_->setText(e ? tr2("⛔ %1, %2. Perbaiki kesalahan dulu sebelum menghitung.", "⛔ %1, %2. Fix the errors before computing.").arg(countErrors(e), countWarnings(w))
                           : tr2("✓ Siap dihitung · %1", "✓ Ready to compute · %1").arg(countWarnings(w)));
    doctorHead_->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(e ? Theme::current().error : Theme::current().success)));
}

void SteadyAnalysisView::newPlan() {
    auto* p = s_->project();
    if (!p) return;
    if (p->geometries.empty() || p->steadyFlows.empty()) {
        sheetInfo(this, tr2("Plan baru", "New plan"), tr2("Plan butuh satu file geometri dan satu file aliran steady.", "A plan needs one geometry file and one steady flow file."));
        return;
    }
    QString t = tr2("Plan steady", "Steady plan");
    if (!sheetGetText(this, tr2("Plan baru", "New plan"), tr2("Judul plan:", "Plan title:"), t, 0, "plan.title",
                      tr2("Plan baru memakai file geometri dan aliran yang sedang dibuka. Bisa diganti di kartu Plan.", "The new plan uses the geometry and flow files currently open; change them in the Plan card.")))
        return;
    t = t.trimmed();
    if (t.isEmpty()) return;
    const std::string e = p->addPlan(ss(t), ss(t.left(12)), ss(s_->currentGeometryExt()), ss(s_->currentFlowExt()));
    if (e.empty()) return;
    s_->markEdited();
    s_->setCurrentPlan(qs(e));
}

}  // namespace deras::ui

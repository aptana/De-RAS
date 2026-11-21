// De-RAS - ui/UnsteadyAnalysisView.cpp
#include "UnsteadyAnalysisView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common.h"
#include "DoctorSheet.h"
#include "Help.h"
#include "Icons.h"
#include "Theme.h"
#include "Widgets.h"

namespace deras::ui {

namespace {
const char* kComp[] = {"1SEC", "2SEC", "5SEC", "10SEC", "15SEC", "20SEC", "30SEC", "1MIN", "2MIN", "3MIN", "5MIN", "10MIN", "15MIN", "20MIN", "30MIN", "1HOUR"};
const char* kOut[] = {"1MIN", "2MIN", "5MIN", "10MIN", "15MIN", "20MIN", "30MIN", "1HOUR", "2HOUR", "3HOUR", "6HOUR", "12HOUR", "1DAY"};
}  // namespace

UnsteadyAnalysisView::UnsteadyAnalysisView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    // The left column scrolls so the cards keep their natural height on small windows.
    auto* leftHost = new QWidget;
    auto* left = new QVBoxLayout(leftHost);
    left->setContentsMargins(0, 0, 6, 0);
    left->setSpacing(12);

    auto* pc = new Card(tr2("Plan unsteady", "Unsteady plan"), "plan.editor");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    plan_ = new QComboBox;
    plan_->setFont(monoFont());
    auto* newPlan = new IconButton("plus", tr2("Plan unsteady baru…", "New unsteady plan…"));
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
    g->addWidget(labelWithInfo(tr2("File aliran unsteady", "Unsteady flow file"), "plan.files"), r, 0);
    g->addWidget(flow_, r++, 1);
    output_ = new QLabel;
    output_->setObjectName("Mono");
    output_->setWordWrap(true);
    g->addWidget(labelWithInfo(tr2("File hasil", "Results file"), "plan.output"), r, 0, Qt::AlignTop);
    g->addWidget(output_, r++, 1);
    g->setColumnStretch(1, 1);
    pc->body()->addLayout(g);
    note_ = makeMuted(QString());
    pc->body()->addWidget(note_);
    left->addWidget(pc);

    auto* tc = new Card(tr2("Waktu simulasi dan opsi", "Simulation window and options"), "unsteady.time");
    auto* tg = new QGridLayout;
    tg->setHorizontalSpacing(10);
    tg->setVerticalSpacing(10);
    auto mkLine = [](const QString& ph, int w) {
        auto* e = new QLineEdit;
        e->setPlaceholderText(ph);
        e->setFont(monoFont());
        e->setMaximumWidth(w);
        return e;
    };
    startDate_ = mkLine("01JAN2024", 130);
    startTime_ = mkLine("0000", 70);
    endDate_ = mkLine("02JAN2024", 130);
    endTime_ = mkLine("0000", 70);
    comp_ = new QComboBox;
    out_ = new QComboBox;
    for (const char* c : kComp) comp_->addItem(QString::fromLatin1(c), QString::fromLatin1(c));
    for (const char* c : kOut) out_->addItem(QString::fromLatin1(c), QString::fromLatin1(c));
    theta_ = new QDoubleSpinBox;
    theta_->setRange(0.5, 1.0);
    theta_->setDecimals(2);
    theta_->setSingleStep(0.05);
    theta_->setLocale(QLocale::c());
    ztol_ = new QDoubleSpinBox;
    ztol_->setRange(0.0001, 1);
    ztol_->setDecimals(4);
    ztol_->setLocale(QLocale::c());
    maxIter_ = new QSpinBox;
    maxIter_->setRange(1, 100);
    r = 0;
    auto pair = [](QWidget* a, QWidget* b) {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(a);
        h->addWidget(b);
        h->addStretch();
        return w;
    };
    tg->addWidget(labelWithInfo(tr2("Mulai (tanggal, jam)", "Start (date, time)"), "unsteady.time"), r, 0);
    tg->addWidget(pair(startDate_, startTime_), r++, 1);
    tg->addWidget(labelWithInfo(tr2("Selesai (tanggal, jam)", "End (date, time)"), "unsteady.time"), r, 0);
    tg->addWidget(pair(endDate_, endTime_), r++, 1);
    tg->addWidget(labelWithInfo(tr2("Interval komputasi", "Computation interval"), "unsteady.interval"), r, 0);
    tg->addWidget(comp_, r++, 1, Qt::AlignLeft);
    tg->addWidget(labelWithInfo(tr2("Interval keluaran", "Output interval"), "unsteady.interval"), r, 0);
    tg->addWidget(out_, r++, 1, Qt::AlignLeft);
    tg->addWidget(labelWithInfo(QStringLiteral("Theta"), "unsteady.theta"), r, 0);
    tg->addWidget(theta_, r++, 1, Qt::AlignLeft);
    tg->addWidget(labelWithInfo(tr2("Toleransi muka air", "Water surface tolerance"), "unsteady.theta"), r, 0);
    tg->addWidget(ztol_, r++, 1, Qt::AlignLeft);
    tg->addWidget(labelWithInfo(tr2("Maks. iterasi per langkah", "Max. iterations per step"), "unsteady.theta"), r, 0);
    tg->addWidget(maxIter_, r++, 1, Qt::AlignLeft);
    tg->setColumnStretch(1, 1);
    tc->body()->addLayout(tg);
    tc->body()->addWidget(makeMuted(tr2("Tanggal ditulis seperti HEC-RAS (01JAN2024) dan jam 0000–2400. Kondisi awal dihitung dengan backwater steady dari debit awal.",
                                        "Dates are written as in HEC-RAS (01JAN2024) and times 0000–2400. Initial conditions come from a steady backwater run with the initial flows.")));
    left->addWidget(tc);
    left->addStretch();
    auto* leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setWidget(leftHost);
    root->addWidget(leftScroll, 3);

    auto* dc = new Card(QStringLiteral("Model Doctor"), "sim.doctor");
    dc->setMinimumWidth(340);
    doctorHead_ = new QLabel;
    doctorHead_->setObjectName("Strong");
    doctorHead_->setWordWrap(true);
    dc->body()->addWidget(doctorHead_);
    doctor_ = new QListWidget;
    doctor_->setWordWrap(true);
    doctor_->setObjectName("NavList");
    dc->body()->addWidget(doctor_, 1);
    root->addWidget(dc, 2);

    debounce_.setSingleShot(true);
    debounce_.setInterval(500);
    connect(&debounce_, &QTimer::timeout, this, &UnsteadyAnalysisView::refreshDoctor);
    connect(s_, &ProjectSession::modelEdited, this, [this]() { if (isVisible()) debounce_.start(); });
    connect(plan_, &QComboBox::activated, this, [this]() { s_->setCurrentPlan(plan_->currentData().toString()); });
    connect(newPlan, &QToolButton::clicked, this, &UnsteadyAnalysisView::newPlan);
    for (auto* le : {title_, shortId_, startDate_, startTime_, endDate_, endTime_}) connect(le, &QLineEdit::editingFinished, this, [this]() { commit(); });
    for (auto* cb : {geom_, flow_, comp_, out_}) connect(cb, &QComboBox::activated, this, [this]() { commit(); });
    for (auto* sp : {theta_, ztol_}) connect(sp, &QDoubleSpinBox::valueChanged, this, [this]() { commit(); });
    connect(maxIter_, &QSpinBox::valueChanged, this, [this]() { commit(); });
    connect(doctor_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        int rr = 0, xx = 0;
        if (s_->findLocation(it->data(Qt::UserRole).toString(), rr, xx)) {
            s_->selectCrossSection(rr, xx);
            emit navigate("xs");
        }
    });
    reload();
}

QString UnsteadyAnalysisView::title() const { return tr2("Analisis Unsteady", "Unsteady Flow Analysis"); }

QWidget* UnsteadyAnalysisView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* data = makeButton(tr2("Data Aliran Unsteady", "Unsteady Flow Data"), false, "unsteady");
    auto* doc = makeButton(QStringLiteral("Model Doctor (F7)"), false, "doctor");
    auto* run = makeButton(tr2("Hitung (F5)", "Compute (F5)"), true, "run");
    h->addWidget(data);
    h->addWidget(doc);
    h->addWidget(run);
    connect(data, &QPushButton::clicked, this, [this]() { emit navigate("unsteady"); });
    connect(doc, &QPushButton::clicked, this, [this]() { refreshDoctor(); emit requestDoctor(); });
    connect(run, &QPushButton::clicked, this, &View::requestRun);
    return w;
}

void UnsteadyAnalysisView::activated() { reload(); }

void UnsteadyAnalysisView::reload() {
    loading_ = true;
    plan_->clear();
    geom_->clear();
    flow_->clear();
    auto* p = s_->project();
    if (p) {
        for (auto& [e, pl] : p->plans)
            plan_->addItem(qs(e) + QStringLiteral(" · ") + qs(pl.model.title) + (pl.model.isUnsteady() ? QStringLiteral(" · unsteady") : QStringLiteral(" · steady")), qs(e));
        for (auto& [e, gg] : p->geometries) geom_->addItem(qs(e) + QStringLiteral(" · ") + qs(gg.model.title), qs(e));
        for (auto& [e, f] : p->unsteadyFlows) flow_->addItem(qs(e) + QStringLiteral(" · ") + qs(f.model.title), qs(e));
        plan_->setCurrentIndex(plan_->findData(s_->currentPlanExt()));
    }
    auto* pl = s_->currentPlan();
    const bool uns = pl && pl->model.isUnsteady();
    for (QWidget* w : std::initializer_list<QWidget*>{title_, shortId_, geom_, flow_, startDate_, startTime_, endDate_, endTime_, comp_, out_, theta_, ztol_, maxIter_})
        w->setEnabled(uns);
    if (uns) {
        const Plan& m = pl->model;
        title_->setText(qs(m.title));
        shortId_->setText(qs(m.shortId));
        geom_->setCurrentIndex(geom_->findData(qs(m.geomFile)));
        flow_->setCurrentIndex(flow_->findData(qs(m.flowFile)));
        startDate_->setText(qs(m.simStartDate));
        startTime_->setText(qs(m.simStartTime));
        endDate_->setText(qs(m.simEndDate));
        endTime_->setText(qs(m.simEndTime));
        auto pick = [](QComboBox* c, const std::string& v) {
            int i = c->findData(qs(v));
            if (i < 0 && !v.empty()) { c->addItem(qs(v), qs(v)); i = c->count() - 1; }
            c->setCurrentIndex(i);
        };
        pick(comp_, m.compInterval);
        pick(out_, m.outputInterval);
        theta_->setValue(m.theta);
        ztol_->setValue(m.unetZTol);
        maxIter_->setValue(m.unetMaxIter);
        output_->setText(QStringLiteral("%1.%2.deras-results.json").arg(qs(p->baseName()), s_->currentPlanExt()));
        note_->setText(tr2("Status Pratinjau: skema implisit Preissmann satu jalur aliran per reach; struktur, storage area, struktur lateral, dan pompa belum dihitung. Hasil belum dibandingkan dengan HEC-RAS.",
                           "Preview status: Preissmann implicit scheme with one flow path per reach; structures, storage areas, lateral structures and pumps are not computed yet. Results are not compared with HEC-RAS."));
    } else {
        for (auto* le : {title_, shortId_, startDate_, startTime_, endDate_, endTime_}) le->clear();
        output_->clear();
        note_->setText(p ? tr2("Plan aktif bukan plan unsteady. Pilih plan unsteady di daftar atau buat baru dengan tombol +.", "The current plan is not an unsteady plan. Pick an unsteady plan in the list or create one with the + button.")
                         : QString());
    }
    loading_ = false;
    refreshDoctor();
}

void UnsteadyAnalysisView::commit() {
    if (loading_) return;
    auto* pl = s_->currentPlan();
    if (!pl || !pl->model.isUnsteady()) return;
    Plan& m = pl->model;
    const Plan before = m;
    m.title = ss(title_->text());
    m.shortId = ss(shortId_->text());
    if (geom_->currentIndex() >= 0) m.geomFile = ss(geom_->currentData().toString());
    if (flow_->currentIndex() >= 0) m.flowFile = ss(flow_->currentData().toString());
    m.simStartDate = ss(startDate_->text().trimmed().toUpper());
    m.simStartTime = ss(startTime_->text().trimmed());
    m.simEndDate = ss(endDate_->text().trimmed().toUpper());
    m.simEndTime = ss(endTime_->text().trimmed());
    m.compInterval = ss(comp_->currentData().toString());
    m.outputInterval = ss(out_->currentData().toString());
    m.theta = theta_->value();
    m.unetZTol = ztol_->value();
    m.unetMaxIter = maxIter_->value();
    if (io::samePlan(m, before)) return;
    s_->markEdited();
    if (m.geomFile != before.geomFile || m.flowFile != before.flowFile) {
        s_->setEditGeometry(QString());
        s_->setEditUnsteady(QString());
    }
}

void UnsteadyAnalysisView::refreshDoctor() {
    doctor_->clear();
    auto* pl = s_->currentPlan();
    if (!s_->isOpen() || !pl || !pl->model.isUnsteady()) {
        doctorHead_->setText(s_->isOpen() ? tr2("Pilih plan unsteady.", "Pick an unsteady plan.") : tr2("Belum ada proyek.", "No project open."));
        doctorHead_->setStyleSheet(QString());
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

void UnsteadyAnalysisView::newPlan() {
    auto* p = s_->project();
    if (!p) return;
    if (p->geometries.empty()) {
        sheetInfo(this, tr2("Plan unsteady baru", "New unsteady plan"), tr2("Plan butuh satu file geometri.", "A plan needs a geometry file."));
        return;
    }
    QString t = tr2("Plan unsteady", "Unsteady plan");
    if (!sheetGetText(this, tr2("Plan unsteady baru", "New unsteady plan"), tr2("Judul plan:", "Plan title:"), t, 0, "plan.title",
                      tr2("Plan baru memakai file geometri yang sedang dibuka dan file aliran unsteady pertama (dibuat bila belum ada). Waktu simulasi awal: 24 jam.",
                          "The new plan uses the geometry file currently open and the first unsteady flow file (created if there is none). Initial simulation window: 24 hours.")))
        return;
    t = t.trimmed();
    if (t.isEmpty()) return;
    std::string u = ss(s_->currentUnsteadyExt());
    if (u.empty()) u = p->addUnsteadyFlow(isID() ? "Aliran unsteady" : "Unsteady flow");
    if (u.empty()) return;
    const std::string e = p->addPlan(ss(t), ss(t.left(12)), ss(s_->currentGeometryExt()), u);
    if (e.empty()) return;
    // the plan file was created as steady; set the unsteady defaults and recreate it as a new unsteady plan
    Plan pl = p->plans.at(e).model;
    pl.flowFile = u;
    pl.simStartDate = "01JAN2024";
    pl.simStartTime = "0000";
    pl.simEndDate = "02JAN2024";
    pl.simEndTime = "0000";
    pl.compInterval = "1MIN";
    pl.outputInterval = "1HOUR";
    p->plans.erase(e);
    p->plans.emplace(e, io::PlanFile::createNew(pl, p->units()));
    s_->markEdited();
    s_->setCurrentPlan(qs(e));
}

}  // namespace deras::ui

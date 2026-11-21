// De-RAS - ui/SedimentView.cpp
#include "SedimentView.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "Common.h"
#include "Help.h"
#include "NumTable.h"
#include "PlotWidget.h"
#include "Theme.h"
#include "Version.h"
#include "Widgets.h"
#include "engines/sediment/Sediment.h"
#include "io/ModuleData.h"
#include "io/RasText.h"

namespace deras::ui {

namespace {
QDoubleSpinBox* spin(double lo, double hi, int dec, double step) {
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi);
    b->setDecimals(dec);
    b->setSingleStep(step);
    b->setLocale(QLocale::c());
    b->setMaximumWidth(140);
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
}  // namespace

SedimentView::SedimentView(ProjectSession* s, Page page, QWidget* parent) : View(s, parent), page_(page) {
    if (page_ == Page::Quasi) buildQuasi();
    else if (page_ == Page::Data) buildData();
    else buildRun();
    connect(s_, &ProjectSession::projectChanged, this, [this]() { if (isVisible()) reload(); });
}

QString SedimentView::viewId() const {
    return page_ == Page::Quasi ? QStringLiteral("quasi") : page_ == Page::Data ? QStringLiteral("sediment") : QStringLiteral("run-sediment");
}
QString SedimentView::title() const {
    return page_ == Page::Quasi ? tr2("Data Quasi-unsteady", "Quasi-unsteady Flow Data")
           : page_ == Page::Data ? tr2("Data Sedimen", "Sediment Data")
                                 : tr2("Analisis Sedimen", "Sediment Analysis");
}
QString SedimentView::helpTopic() const {
    return page_ == Page::Quasi ? QStringLiteral("sediment.quasi") : page_ == Page::Data ? QStringLiteral("sediment.data") : QStringLiteral("sediment.run");
}

QString SedimentView::lenU() const { auto* p = s_->project(); return p && p->units() == UnitSystem::USCustomary ? QStringLiteral("ft") : QStringLiteral("m"); }
QString SedimentView::flowU() const { auto* p = s_->project(); return p && p->units() == UnitSystem::USCustomary ? QStringLiteral("cfs") : QStringLiteral("m³/s"); }
QString SedimentView::loadU() const {
    auto* p = s_->project();
    return p && p->units() == UnitSystem::USCustomary ? tr2("ton US/hari", "tons/day") : tr2("t/hari", "t/day");
}

QWidget* SedimentView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    if (page_ != Page::Quasi) {
        auto* b = makeButton(tr2("Data Quasi-unsteady", "Quasi-unsteady Data"), false, "quasi");
        connect(b, &QPushButton::clicked, this, [this]() { emit navigate("quasi"); });
        h->addWidget(b);
    }
    if (page_ != Page::Data) {
        auto* b = makeButton(tr2("Data Sedimen", "Sediment Data"), false, "sediment");
        connect(b, &QPushButton::clicked, this, [this]() { emit navigate("sediment"); });
        h->addWidget(b);
    }
    if (page_ != Page::Run) {
        auto* b = makeButton(tr2("Analisis Sedimen", "Sediment Analysis"), true, "run-sediment");
        connect(b, &QPushButton::clicked, this, [this]() { emit navigate("run-sediment"); });
        h->addWidget(b);
    }
    return w;
}

// ---------------- quasi-unsteady flow ----------------
void SedimentView::buildQuasi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 6, 0);
    lv->setSpacing(12);
    auto* c1 = new Card(tr2("Lokasi", "Location"), "sediment.quasi");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    geom_ = new QComboBox;
    geom_->setFont(monoFont());
    reach_ = new QComboBox;
    dsSlope_ = spin(0.000001, 1, 6, 0.0001);
    g->addWidget(labelWithInfo(tr2("File geometri", "Geometry file"), "sediment.quasi"), 0, 0);
    g->addWidget(geom_, 0, 1);
    g->addWidget(labelWithInfo(tr2("Reach", "Reach"), "sediment.quasi"), 1, 0);
    g->addWidget(reach_, 1, 1);
    g->addWidget(labelWithInfo(tr2("Kemiringan normal depth hilir", "Downstream normal-depth slope"), "unsteady.normal"), 2, 0);
    g->addWidget(dsSlope_, 2, 1, Qt::AlignLeft);
    g->setColumnStretch(1, 1);
    c1->body()->addLayout(g);
    c1->body()->addWidget(makeMuted(tr2("Pratinjau: satu reach. Debit masuk di penampang paling hulu; batas hilir normal depth, kecuali baris diberi muka air hilir.",
                                        "Preview: one reach. The flow enters at the most upstream section; the downstream boundary is normal depth unless a record has a downstream stage.")));
    lv->addWidget(c1);
    auto* c2 = new Card(tr2("Rangkaian debit", "Flow series"), "sediment.quasi");
    records_ = new NumTable({tr2("Durasi (jam)", "Duration (h)"), tr2("Debit", "Flow"), tr2("Interval hitung (jam)", "Compute increment (h)"),
                             tr2("Muka air hilir (kosong = normal depth)", "Downstream stage (empty = normal depth)")},
                            {"sediment.quasi", "sediment.quasi", "sediment.quasi", "sediment.quasi"});
    records_->setMinimumHeight(260);
    c2->body()->addWidget(records_);
    quasiNote_ = makeMuted(QString());
    c2->body()->addWidget(quasiNote_);
    lv->addWidget(c2);
    lv->addStretch();
    root->addWidget(scrollOf(left), 5);
    auto* c3 = new Card(tr2("Hidrograf quasi-unsteady", "Quasi-unsteady hydrograph"), "sediment.quasi");
    flowPlot_ = new PlotWidget;
    flowPlot_->setEmptyText(tr2("Isi rangkaian debit di kiri.", "Fill the flow series on the left."));
    c3->body()->addWidget(flowPlot_, 1);
    root->addWidget(c3, 6);
    connect(records_, &NumTable::edited, this, [this]() { commit(); plotFlow(); });
    connect(dsSlope_, &QDoubleSpinBox::editingFinished, this, &SedimentView::commit);
    connect(geom_, &QComboBox::currentIndexChanged, this, [this]() { if (loading_) return; fillReaches(); commit(); });
    connect(reach_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) commit(); });
}

void SedimentView::fillReaches() {
    auto* p = s_->project();
    const bool was = loading_;
    loading_ = true;
    reach_->clear();
    if (p) {
        auto it = p->geometries.find(ss(geom_->currentData().toString()));
        if (it != p->geometries.end())
            for (const auto& r : it->second.model.reaches) reach_->addItem(qs(r.label()), QStringList{qs(r.river), qs(r.reach)});
    }
    for (int i = 0; i < reach_->count(); ++i) {
        const QStringList k = reach_->itemData(i).toStringList();
        if (k.size() == 2 && ss(k[0]) == st_.flow.river && ss(k[1]) == st_.flow.reach) reach_->setCurrentIndex(i);
    }
    loading_ = was;
}

void SedimentView::plotFlow() {
    if (!flowPlot_) return;
    QVector<QPointF> pts;
    double t = 0;
    for (const auto& r : st_.flow.records) {
        pts << QPointF(t, r.flow);
        t += r.durationHours;
        pts << QPointF(t, r.flow);
    }
    const Theme& th = Theme::current();
    flowPlot_->setTitles(QString(), tr2("Waktu (jam)", "Time (h)"), tr2("Debit (%1)", "Flow (%1)").arg(flowU()));
    if (pts.isEmpty()) flowPlot_->clear();
    else flowPlot_->setSeries({PlotSeries{tr2("Debit", "Flow"), pts, th.primary, 2.0}});
    double total = 0;
    int steps = 0;
    for (const auto& r : st_.flow.records) {
        total += r.durationHours;
        if (r.durationHours > 0) steps += std::max(1, int(std::ceil(r.durationHours / (r.computeHours > 0 ? std::min(r.computeHours, r.durationHours) : r.durationHours) - 1e-9)));
    }
    if (quasiNote_)
        quasiNote_->setText(tr2("%1 baris, total %2 jam (%3 hari), %4 langkah hitung. Debit dalam %5, muka air dalam %6.",
                                "%1 records, %2 hours in total (%3 days), %4 computation steps. Flow in %5, stage in %6.")
                                .arg(st_.flow.records.size()).arg(num(total, 1), num(total / 24, 2)).arg(steps).arg(flowU(), lenU()));
}

// ---------------- sediment data ----------------
void SedimentView::buildData() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 6, 0);
    lv->setSpacing(12);
    auto* c1 = new Card(tr2("Fungsi angkutan dan sifat material", "Transport function and material"), "sediment.data");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    function_ = new QComboBox;
    function_->addItem(QStringLiteral("Meyer-Peter Müller (1948)"), 0);
    function_->addItem(QStringLiteral("Engelund-Hansen (1967)"), 1);
    function_->addItem(QStringLiteral("Yang (1973, pasir / sand)"), 2);
    gs_ = spin(1.05, 5, 3, 0.01);
    porosity_ = spin(0, 0.89, 3, 0.01);
    temp_ = spin(0, 40, 1, 1);
    maxDepth_ = spin(0, 100, 3, 0.1);
    int r = 0;
    g->addWidget(labelWithInfo(tr2("Fungsi angkutan", "Transport function"), "sediment.function"), r, 0);
    g->addWidget(function_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Berat jenis butir", "Specific gravity"), "sediment.data"), r, 0);
    g->addWidget(gs_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Porositas dasar", "Bed porosity"), "sediment.data"), r, 0);
    g->addWidget(porosity_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Suhu air (°C)", "Water temperature (°C)"), "sediment.data"), r, 0);
    g->addWidget(temp_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Kedalaman dasar yang bisa tergerus", "Erodible bed depth"), "sediment.data"), r, 0);
    g->addWidget(maxDepth_, r++, 1, Qt::AlignLeft);
    g->setColumnStretch(1, 1);
    c1->body()->addLayout(g);
    c1->body()->addWidget(makeMuted(tr2("Dasar yang bergerak: titik penampang di antara stasiun tebing. Satu ukuran butir (d50) mewakili dasar; tanpa pemilahan atau armoring.",
                                        "Movable bed: cross-section points between the bank stations. One grain size (d50) represents the bed; no sorting or armouring.")));
    lv->addWidget(c1);
    auto* c2 = new Card(tr2("Gradasi material dasar", "Bed gradation"), "sediment.gradation");
    gradation_ = new NumTable({tr2("Diameter (mm)", "Diameter (mm)"), tr2("% lolos", "% finer")}, {"sediment.gradation", "sediment.gradation"});
    gradation_->setMinimumHeight(190);
    c2->body()->addWidget(gradation_);
    gradNote_ = makeMuted(QString());
    c2->body()->addWidget(gradNote_);
    lv->addWidget(c2);
    lv->addStretch();
    root->addWidget(scrollOf(left), 1);
    auto* right = new QWidget;
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 6, 0);
    rv->setSpacing(12);
    auto* c3 = new Card(tr2("Muatan sedimen masuk di hulu", "Upstream sediment inflow"), "sediment.inflow");
    inflow_ = new QComboBox;
    inflow_->addItem(tr2("Muatan setimbang (sama dengan kapasitas angkut)", "Equilibrium load (equal to the transport capacity)"), 0);
    inflow_->addItem(tr2("Air jernih (tanpa muatan)", "Clear water (no load)"), 1);
    inflow_->addItem(tr2("Rating curve debit–muatan", "Flow–load rating curve"), 2);
    c3->body()->addWidget(inflow_);
    ratingBox_ = new QWidget;
    auto* rb = new QVBoxLayout(ratingBox_);
    rb->setContentsMargins(0, 0, 0, 0);
    rating_ = new NumTable({tr2("Debit", "Flow"), tr2("Muatan", "Load")}, {"sediment.inflow", "sediment.inflow"});
    rating_->setMinimumHeight(190);
    rb->addWidget(rating_);
    c3->body()->addWidget(ratingBox_);
    c3->body()->addWidget(makeMuted(tr2("Muatan dalam ton per hari (t/hari untuk SI, ton US/hari untuk US). Di antara titik dipakai interpolasi linear.",
                                        "Load in tons per day (metric t/day for SI, US tons/day for US). Linear interpolation between points.")));
    rv->addWidget(c3);
    rv->addStretch();
    root->addWidget(scrollOf(right), 1);
    for (auto* b : {gs_, porosity_, temp_, maxDepth_}) connect(b, &QDoubleSpinBox::editingFinished, this, &SedimentView::commit);
    connect(function_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) commit(); });
    connect(inflow_, &QComboBox::currentIndexChanged, this, [this]() {
        ratingBox_->setVisible(inflow_->currentData().toInt() == 2);
        if (!loading_) commit();
    });
    connect(gradation_, &NumTable::edited, this, &SedimentView::commit);
    connect(rating_, &NumTable::edited, this, &SedimentView::commit);
}

// ---------------- analysis and results ----------------
void SedimentView::buildRun() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* top = new Card(tr2("Hitungan", "Computation"), "sediment.run");
    auto* h = new QHBoxLayout;
    h->setSpacing(10);
    outHours_ = spin(0.1, 100000, 1, 1);
    h->addWidget(labelWithInfo(tr2("Interval keluaran (jam)", "Output interval (h)"), "sediment.run"));
    h->addWidget(outHours_);
    runBtn_ = makeButton(tr2("Hitung sedimen", "Compute sediment"), true, "run-sediment");
    h->addWidget(runBtn_);
    auto* csv = makeButton(tr2("Ekspor CSV…", "Export CSV…"), false, "export");
    h->addWidget(csv);
    h->addStretch();
    top->body()->addLayout(h);
    runInfo_ = makeMuted(QString());
    top->body()->addWidget(runInfo_);
    root->addWidget(top);
    auto* mid = new QHBoxLayout;
    mid->setSpacing(12);
    auto* c1 = new Card(tr2("Profil dasar alur", "Channel bed profile"), "sediment.results");
    auto* th = new QHBoxLayout;
    time_ = new QSlider(Qt::Horizontal);
    timeLabel_ = new QLabel;
    timeLabel_->setObjectName("Mono");
    th->addWidget(new QLabel(tr2("Waktu", "Time")));
    th->addWidget(time_, 1);
    th->addWidget(timeLabel_);
    c1->body()->addLayout(th);
    bedPlot_ = new PlotWidget;
    bedPlot_->setReverseX(true);
    bedPlot_->setEmptyText(tr2("Belum ada hasil. Klik Hitung sedimen.", "No results yet. Click Compute sediment."));
    c1->body()->addWidget(bedPlot_, 1);
    mid->addWidget(c1, 3);
    auto* c2 = new Card(tr2("Perubahan dasar terhadap waktu", "Bed change over time"), "sediment.results");
    changePlot_ = new PlotWidget;
    changePlot_->setEmptyText(tr2("Belum ada hasil.", "No results yet."));
    c2->body()->addWidget(changePlot_, 1);
    mid->addWidget(c2, 2);
    root->addLayout(mid, 3);
    auto* c3 = new Card(tr2("Tabel hasil pada waktu terpilih", "Results table at the selected time"), "sediment.results");
    table_ = new QTableWidget(0, 7);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setAlternatingRowColors(true);
    c3->body()->addWidget(table_, 1);
    summary_ = makeMuted(QString());
    c3->body()->addWidget(summary_);
    root->addWidget(c3, 2);
    connect(outHours_, &QDoubleSpinBox::editingFinished, this, &SedimentView::commit);
    connect(runBtn_, &QPushButton::clicked, this, &SedimentView::runSediment);
    connect(csv, &QPushButton::clicked, this, &SedimentView::exportCsv);
    connect(time_, &QSlider::valueChanged, this, &SedimentView::showResult);
}

// ---------------- load / commit ----------------
void SedimentView::reload() {
    hasRes_ = false;
    load();
    if (page_ == Page::Run) loadResult();
}

void SedimentView::activated() { reload(); }

void SedimentView::load() {
    auto* p = s_->project();
    loading_ = true;
    st_ = p ? io::sedimentFor(p->settings) : SedimentSetup{};
    if (page_ == Page::Quasi) {
        geom_->clear();
        if (p) {
            for (const auto& [ext, gf] : p->geometries) geom_->addItem(QStringLiteral("%1 · %2").arg(qs(ext), qs(gf.model.title)), qs(ext));
            int i = geom_->findData(qs(st_.geomFile));
            if (i < 0 && p->plans.count(p->info().currentPlan)) i = geom_->findData(qs(p->plans.at(p->info().currentPlan).model.geomFile));
            geom_->setCurrentIndex(std::max(i, 0));
        }
        fillReaches();
        dsSlope_->setValue(st_.flow.dsSlope);
        std::vector<std::vector<double>> rows;
        for (const auto& r : st_.flow.records) rows.push_back({r.durationHours, r.flow, r.computeHours, r.dsStage});
        records_->setValues(rows);
        plotFlow();
    } else if (page_ == Page::Data) {
        function_->setCurrentIndex(function_->findData(int(st_.data.function)));
        gs_->setValue(st_.data.specificGravity);
        porosity_->setValue(st_.data.porosity);
        temp_->setValue(st_.data.temperature);
        maxDepth_->setValue(st_.data.maxErodibleDepth);
        maxDepth_->setSuffix(QStringLiteral(" ") + lenU());
        std::vector<std::vector<double>> rows;
        for (const auto& g : st_.data.gradation) rows.push_back({g.first, g.second});
        gradation_->setValues(rows);
        inflow_->setCurrentIndex(inflow_->findData(int(st_.data.inflow)));
        ratingBox_->setVisible(st_.data.inflow == SedimentInflow::Rating);
        rows.clear();
        for (const auto& g : st_.data.rating) rows.push_back({g.first, g.second});
        rating_->setValues(rows);
        rating_->setHorizontalHeaderLabels({tr2("Debit (%1)", "Flow (%1)").arg(flowU()), tr2("Muatan (%1)", "Load (%1)").arg(loadU())});
        const double d50 = gradationDiameter(st_.data.gradation, 50), d90 = gradationDiameter(st_.data.gradation, 90);
        gradNote_->setText(std::isfinite(d50) ? tr2("d50 = %1 mm, d90 = %2 mm (interpolasi log).", "d50 = %1 mm, d90 = %2 mm (log interpolation).").arg(num(d50, 3), num(d90, 3))
                                              : tr2("Isi minimal satu baris: diameter dan persen lolos.", "Enter at least one row: diameter and percent finer."));
    } else {
        outHours_->setValue(st_.outputHours);
    }
    loading_ = false;
}

void SedimentView::commit() {
    auto* p = s_->project();
    if (loading_ || !p) return;
    SedimentSetup st = io::sedimentFor(p->settings);
    if (page_ == Page::Quasi) {
        st.geomFile = ss(geom_->currentData().toString());
        const QStringList k = reach_->currentData().toStringList();
        if (k.size() == 2) {
            st.flow.river = ss(k[0]);
            st.flow.reach = ss(k[1]);
        }
        st.flow.dsSlope = dsSlope_->value();
        std::vector<std::vector<double>> rows;
        records_->values(rows);
        st.flow.records.clear();
        for (const auto& r : rows) {
            if (r.size() < 2 || !std::isfinite(r[1])) continue;
            QuasiRecord q;
            q.durationHours = std::isfinite(r[0]) ? r[0] : 24;
            q.flow = r[1];
            q.computeHours = r.size() > 2 && std::isfinite(r[2]) ? r[2] : q.durationHours;
            q.dsStage = r.size() > 3 ? r[3] : std::nan("");
            st.flow.records.push_back(q);
        }
    } else if (page_ == Page::Data) {
        st.data.function = static_cast<TransportFunction>(function_->currentData().toInt());
        st.data.specificGravity = gs_->value();
        st.data.porosity = porosity_->value();
        st.data.temperature = temp_->value();
        st.data.maxErodibleDepth = maxDepth_->value();
        st.data.inflow = static_cast<SedimentInflow>(inflow_->currentData().toInt());
        std::vector<std::vector<double>> rows;
        gradation_->values(rows);
        st.data.gradation.clear();
        for (const auto& r : rows) if (r.size() >= 2 && std::isfinite(r[0]) && std::isfinite(r[1])) st.data.gradation.emplace_back(r[0], r[1]);
        rows.clear();
        rating_->values(rows);
        st.data.rating.clear();
        for (const auto& r : rows) if (r.size() >= 2 && std::isfinite(r[0]) && std::isfinite(r[1])) st.data.rating.emplace_back(r[0], r[1]);
        std::sort(st.data.rating.begin(), st.data.rating.end());
        const double d50 = gradationDiameter(st.data.gradation, 50), d90 = gradationDiameter(st.data.gradation, 90);
        gradNote_->setText(std::isfinite(d50) ? tr2("d50 = %1 mm, d90 = %2 mm (interpolasi log).", "d50 = %1 mm, d90 = %2 mm (log interpolation).").arg(num(d50, 3), num(d90, 3))
                                              : tr2("Isi minimal satu baris: diameter dan persen lolos.", "Enter at least one row: diameter and percent finer."));
    } else {
        st.outputHours = outHours_->value();
    }
    io::setSediment(p->settings, st);
    st_ = st;
    s_->markEdited();
}

// ---------------- run ----------------
void SedimentView::runSediment() {
    auto* p = s_->project();
    if (!p || running_) return;
    const SedimentSetup st = io::sedimentFor(p->settings);
    auto git = p->geometries.find(st.geomFile);
    if (git == p->geometries.end() || st.flow.records.empty() || st.data.gradation.empty()) {
        sheetInfo(this, tr2("Data belum lengkap", "Data incomplete"),
                  tr2("Isi dulu Data Quasi-unsteady (file geometri, reach, rangkaian debit) dan Data Sedimen (gradasi material dasar).",
                      "First fill the Quasi-unsteady Data (geometry file, reach, flow series) and the Sediment Data (bed gradation)."));
        return;
    }
    Geometry geom = git->second.model;
    io::applyGeometrySettings(p->settings, st.geomFile, geom);
    const UnitSystem units = p->units();
    const std::string base = p->baseName();
    const io::fs::path dir = p->dir();
    running_ = true;
    runBtn_->setEnabled(false);
    runInfo_->setText(tr2("Menghitung…", "Computing…"));
    auto* result = new SedimentResult;
    QThread* th = QThread::create([geom, st, units, result]() { *result = sediment::run(geom, st, units); });
    QPointer<SedimentView> self(this);
    connect(th, &QThread::finished, this, [self, th, result, base, dir]() {
        th->deleteLater();
        std::unique_ptr<SedimentResult> r(result);
        if (!self) return;
        self->running_ = false;
        self->runBtn_->setEnabled(true);
        r->solverVersion = DERAS_VERSION;
        r->startedUtc = ss(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        std::string err;
        io::writeSedimentResult(io::sedimentResultPath(dir, base), *r, err);
        self->res_ = std::move(*r);
        self->hasRes_ = true;
        self->time_->setRange(0, std::max(0, int(self->res_.hours.size()) - 1));
        self->time_->setValue(self->time_->maximum());
        self->showResult();
        if (self->res_.status != RunStatus::Succeeded) {
            QStringList t;
            for (const auto& m : self->res_.messages) if (m.severity == Severity::Error) t << msgText(m);
            sheetInfo(self, tr2("Hitungan sedimen gagal", "Sediment computation failed"), t.join(QStringLiteral("\n")));
        } else {
            showToast(self, tr2("Hitungan sedimen selesai.", "Sediment computation finished."));
        }
    });
    th->start();
}

void SedimentView::loadResult() {
    auto* p = s_->project();
    hasRes_ = false;
    if (p) {
        std::string err;
        const auto path = io::sedimentResultPath(p->dir(), p->baseName());
        if (io::fs::exists(path) && io::readSedimentResult(path, res_, err)) hasRes_ = true;
    }
    if (time_) {
        time_->setRange(0, hasRes_ ? std::max(0, int(res_.hours.size()) - 1) : 0);
        time_->setValue(time_->maximum());
    }
    showResult();
}

void SedimentView::showResult() {
    if (page_ != Page::Run) return;
    const Theme& th = Theme::current();
    if (!hasRes_ || res_.hours.empty()) {
        bedPlot_->clear();
        changePlot_->clear();
        table_->setRowCount(0);
        timeLabel_->clear();
        summary_->setText(QString());
        runInfo_->setText(hasRes_ ? tr2("Hasil terakhir gagal. Lihat pesan.", "The last run failed. See the messages.")
                                  : tr2("Belum ada hasil. Isi Data Quasi-unsteady dan Data Sedimen, lalu klik Hitung sedimen.",
                                        "No results yet. Fill the Quasi-unsteady Data and Sediment Data, then click Compute sediment."));
        if (hasRes_) {
            QStringList t;
            for (const auto& m : res_.messages) t << msgText(m);
            runInfo_->setText(t.join(QStringLiteral("\n")));
        }
        return;
    }
    const int k = std::clamp(time_->value(), 0, int(res_.hours.size()) - 1);
    timeLabel_->setText(tr2("jam %1 · Q %2 %3", "hour %1 · Q %2 %3").arg(num(res_.hours[size_t(k)], 1), num(res_.flow[size_t(k)], 1), flowU()));
    const size_t N = res_.rs.size();
    QVector<QPointF> b0, bk, wk;
    for (size_t i = 0; i < N; ++i) {
        b0 << QPointF(res_.distance[i], res_.invert0[i]);
        if (size_t(k) < res_.invert.size() && i < res_.invert[size_t(k)].size()) bk << QPointF(res_.distance[i], res_.invert[size_t(k)][i]);
        if (size_t(k) < res_.ws.size() && i < res_.ws[size_t(k)].size()) wk << QPointF(res_.distance[i], res_.ws[size_t(k)][i]);
    }
    PlotSeries s0{tr2("Dasar awal", "Initial bed"), b0, th.text3, 1.5, Qt::DashLine};
    PlotSeries sk{tr2("Dasar", "Bed"), bk, th.ground, 2.2};
    PlotSeries sw{tr2("Muka air", "Water surface"), wk, th.water, 2.0};
    sw.fillBelowSeries = 1;
    sw.fillColor = withAlpha(th.water, 0.18);
    bedPlot_->setTitles(QString(), tr2("Jarak dari hilir (%1)", "Distance from downstream (%1)").arg(lenU()), tr2("Elevasi (%1)", "Elevation (%1)").arg(lenU()));
    bedPlot_->setSeries({s0, sk, sw});
    // bed change over time at the upstream, middle and downstream sections
    QVector<PlotSeries> cs;
    const QColor cols[] = {th.primary, th.success, th.warning};
    const size_t pick[] = {0, N / 2, N - 1};
    for (int j = 0; j < 3; ++j) {
        if (j > 0 && pick[j] == pick[j - 1]) continue;
        QVector<QPointF> pts;
        for (size_t t = 0; t < res_.hours.size(); ++t)
            if (pick[j] < res_.change[t].size()) pts << QPointF(res_.hours[t], res_.change[t][pick[j]]);
        cs << PlotSeries{QStringLiteral("RS %1").arg(qs(res_.rs[pick[j]])), pts, cols[j], 2.0};
    }
    changePlot_->setTitles(QString(), tr2("Waktu (jam)", "Time (h)"), tr2("Δ dasar (%1)", "Δ bed (%1)").arg(lenU()));
    changePlot_->setSeries(cs);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({QStringLiteral("RS"), tr2("Jarak (%1)", "Distance (%1)").arg(lenU()), tr2("Dasar awal", "Initial bed"), tr2("Dasar", "Bed"),
                                       tr2("Perubahan", "Change"), tr2("Muka air", "Water surface"), tr2("Muatan (%1)", "Load (%1)").arg(loadU())});
    table_->setRowCount(int(N));
    for (size_t i = 0; i < N; ++i) {
        auto cell = [&](int c, const QString& t) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(c == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(int(i), c, it);
        };
        cell(0, qs(res_.rs[i]));
        cell(1, num(res_.distance[i], 1));
        cell(2, num(res_.invert0[i], 3));
        cell(3, num(res_.invert[size_t(k)][i], 3));
        cell(4, num(res_.change[size_t(k)][i], 3));
        cell(5, num(res_.ws[size_t(k)][i], 3));
        cell(6, num(res_.load[size_t(k)][i], 1));
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setStretchLastSection(true);
    const QString mu = res_.units == UnitSystem::USCustomary ? tr2("ton US", "US tons") : QStringLiteral("t");
    summary_->setText(tr2("%1, d50 %2 mm. Massa masuk %3 %6, keluar %4 %6, perubahan dasar %5 %6 (masuk − keluar = perubahan dasar). Status Pratinjau: belum dibandingkan dengan HEC-RAS.",
                          "%1, d50 %2 mm. Mass in %3 %6, out %4 %6, bed change %5 %6 (in − out = bed change). Preview status: not compared with HEC-RAS yet.")
                          .arg(qs(res_.function), num(res_.d50, 3), num(res_.massIn, 1), num(res_.massOut, 1), num(res_.massChange, 1), mu));
    QStringList t;
    for (const auto& m : res_.messages) if (m.severity != Severity::Info || m.code == "SED_SUBSTEPS") t << severityPrefix(m.severity) + QStringLiteral(" ") + msgText(m);
    runInfo_->setText(tr2("Hasil: %1 (De-RAS %2, %3).", "Results: %1 (De-RAS %2, %3).")
                          .arg(qs(io::fromPath(io::sedimentResultPath(s_->project() ? s_->project()->dir() : io::fs::path(), s_->project() ? s_->project()->baseName() : std::string()).filename())),
                               qs(res_.solverVersion), qs(res_.startedUtc)) +
                      (t.isEmpty() ? QString() : QStringLiteral("\n") + t.join(QStringLiteral("\n"))));
}

void SedimentView::exportCsv() {
    auto* p = s_->project();
    if (!p || !hasRes_ || res_.hours.empty()) return;
    const QString def = QDir(qs(io::fromPath(p->dir()))).filePath(qs(p->baseName()) + QStringLiteral("_sedimen.csv"));
    const QString f = QFileDialog::getSaveFileName(this, tr2("Ekspor hasil sedimen", "Export sediment results"), def, QStringLiteral("CSV (*.csv)"));
    if (f.isEmpty()) return;
    std::string out = "hours,flow,rs,distance,invert0,invert,bed_change,ws,load\n";
    for (size_t t = 0; t < res_.hours.size(); ++t)
        for (size_t i = 0; i < res_.rs.size(); ++i) {
            char line[512];
            std::snprintf(line, sizeof line, "%.4f,%.4f,%s,%.3f,%.4f,%.4f,%.4f,%.4f,%.3f\n", res_.hours[t], res_.flow[t], res_.rs[i].c_str(), res_.distance[i], res_.invert0[i],
                          res_.invert[t][i], res_.change[t][i], res_.ws[t][i], res_.load[t][i]);
            out += line;
        }
    std::string err;
    if (!io::writeFileAtomic(io::toPath(ss(f)), out, err)) {
        showToast(this, tr2("Gagal menulis: %1", "Could not write: %1").arg(qs(err)), "warning");
        return;
    }
    showToast(this, tr2("Tersimpan: %1", "Saved: %1").arg(QFileInfo(f).fileName()));
}

}  // namespace deras::ui

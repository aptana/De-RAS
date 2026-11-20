// De-RAS - ui/WaterQualityView.cpp
#include "WaterQualityView.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
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
#include "io/RasText.h"

namespace deras::ui {

namespace {
QDoubleSpinBox* spin(double lo, double hi, int dec, double step) {
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi);
    b->setDecimals(dec);
    b->setSingleStep(step);
    b->setLocale(QLocale::c());
    b->setMaximumWidth(150);
    return b;
}
QString num(double v, int d) { return std::isfinite(v) ? QString::number(v, 'f', d) : QStringLiteral("—"); }
// output time where the concentration differs most along the reach (a front
// or a pulse is visible); start position of the time slider
int peakIndex(const design::WqResult& r) {
    int best = int(r.hours.size()) - 1;
    double m = -1;
    for (size_t t = 0; t < r.conc.size(); ++t) {
        if (r.conc[t].empty()) continue;
        const auto [lo, hi] = std::minmax_element(r.conc[t].begin(), r.conc[t].end());
        if (*hi - *lo > m + 1e-9) { m = *hi - *lo; best = int(t); }
    }
    return std::max(best, 0);
}
QString massText(double g) {
    // concentration (mg/L = g/m3) x volume (m3) = g; shown in kg
    return QString::number(g / 1000.0, 'f', 2);
}
}  // namespace

WaterQualityView::WaterQualityView(ProjectSession* s, Page page, QWidget* parent) : View(s, parent), page_(page) {
    if (page_ == Page::Data) buildData();
    else buildRun();
    connect(s_, &ProjectSession::projectChanged, this, [this]() { if (isVisible()) reload(); });
}

QString WaterQualityView::title() const { return page_ == Page::Data ? tr2("Data Kualitas Air", "Water Quality Data") : tr2("Analisis Kualitas Air", "Water Quality Analysis"); }
QString WaterQualityView::lenU() const { auto* p = s_->project(); return p && p->units() == UnitSystem::USCustomary ? QStringLiteral("ft") : QStringLiteral("m"); }

QWidget* WaterQualityView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    if (page_ == Page::Run) {
        auto* b = makeButton(tr2("Data Kualitas Air", "Water Quality Data"), false, "water");
        connect(b, &QPushButton::clicked, this, [this]() { emit navigate("wq"); });
        h->addWidget(b);
    } else {
        auto* b = makeButton(tr2("Analisis Kualitas Air", "Water Quality Analysis"), true, "run-water");
        connect(b, &QPushButton::clicked, this, [this]() { emit navigate("run-wq"); });
        h->addWidget(b);
    }
    return w;
}

// ---------------- data ----------------
void WaterQualityView::buildData() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 6, 0);
    lv->setSpacing(12);
    auto* c1 = new Card(tr2("Plan dan konstituen", "Plan and constituent"), "wq.data");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(10);
    plan_ = new QComboBox;
    name_ = new QLineEdit;
    name_->setMaxLength(32);
    initial_ = spin(0, 1e6, 3, 1);
    initial_->setSuffix(QStringLiteral(" mg/L"));
    disp_ = spin(0, 1e5, 3, 1);
    decay_ = spin(0, 1000, 4, 0.1);
    decay_->setSuffix(tr2(" /hari", " /day"));
    int r = 0;
    g->addWidget(labelWithInfo(tr2("Plan unsteady (hidraulika)", "Unsteady plan (hydraulics)"), "wq.data"), r, 0);
    g->addWidget(plan_, r++, 1);
    g->addWidget(labelWithInfo(tr2("Nama konstituen", "Constituent name"), "wq.data"), r, 0);
    g->addWidget(name_, r++, 1);
    g->addWidget(labelWithInfo(tr2("Konsentrasi awal di sungai", "Initial concentration in the river"), "wq.data"), r, 0);
    g->addWidget(initial_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Koefisien dispersi", "Dispersion coefficient"), "wq.data"), r, 0);
    g->addWidget(disp_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Laju peluruhan orde-1", "First-order decay rate"), "wq.data"), r, 0);
    g->addWidget(decay_, r++, 1, Qt::AlignLeft);
    g->setColumnStretch(1, 1);
    c1->body()->addLayout(g);
    planNote_ = makeMuted(QString());
    c1->body()->addWidget(planNote_);
    c1->body()->addWidget(makeMuted(tr2("Pratinjau: satu konstituen di reach pertama plan, adveksi-dispersi 1D dengan peluruhan orde-1 (tanpa suhu, DO atau nutrien). Hidraulika diambil dari hasil unsteady plan yang sudah tersimpan.",
                                        "Preview: one constituent in the first reach of the plan, 1D advection-dispersion with first-order decay (no temperature, DO or nutrients). Hydraulics come from the stored unsteady results of the plan.")));
    lv->addWidget(c1);
    auto* c2 = new Card(tr2("Konsentrasi masuk di hulu", "Upstream inflow concentration"), "wq.data");
    upstream_ = new NumTable({tr2("Jam sejak mulai", "Hours from start"), QStringLiteral("mg/L")}, {"wq.data", "wq.data"});
    upstream_->setMinimumHeight(220);
    c2->body()->addWidget(upstream_);
    c2->body()->addWidget(makeMuted(tr2("Di antara titik dipakai interpolasi linear; sesudah titik terakhir nilainya tetap.", "Linear interpolation between points; after the last point the value is held.")));
    lv->addWidget(c2);
    lv->addStretch();
    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(left);
    root->addWidget(sc, 5);
    auto* c3 = new Card(tr2("Konsentrasi hulu", "Upstream concentration"), "wq.data");
    inflowPlot_ = new PlotWidget;
    inflowPlot_->setEmptyText(tr2("Isi tabel konsentrasi hulu di kiri.", "Fill the upstream concentration table on the left."));
    c3->body()->addWidget(inflowPlot_, 1);
    root->addWidget(c3, 6);
    for (auto* b : {initial_, disp_, decay_}) connect(b, &QDoubleSpinBox::editingFinished, this, &WaterQualityView::commit);
    connect(name_, &QLineEdit::editingFinished, this, &WaterQualityView::commit);
    connect(plan_, &QComboBox::currentIndexChanged, this, [this]() { if (!loading_) commit(); });
    connect(upstream_, &NumTable::edited, this, [this]() { commit(); plotInflow(); });
}

void WaterQualityView::plotInflow() {
    if (!inflowPlot_) return;
    QVector<QPointF> pts;
    for (const auto& p : w_.input.upstream) pts << QPointF(p.first, p.second);
    inflowPlot_->setTitles(QString(), tr2("Waktu (jam)", "Time (h)"), QStringLiteral("%1 (mg/L)").arg(qs(w_.constituent)));
    if (pts.isEmpty()) inflowPlot_->clear();
    else inflowPlot_->setSeries({PlotSeries{tr2("Hulu", "Upstream"), pts, Theme::current().primary, 2.0}});
}

// ---------------- run ----------------
void WaterQualityView::buildRun() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);
    auto* top = new Card(tr2("Hitungan", "Computation"), "wq.run");
    auto* h = new QHBoxLayout;
    h->setSpacing(10);
    runBtn_ = makeButton(tr2("Hitung kualitas air", "Compute water quality"), true, "run-water");
    auto* csv = makeButton(tr2("Ekspor CSV…", "Export CSV…"), false, "export");
    h->addWidget(runBtn_);
    h->addWidget(csv);
    h->addStretch();
    top->body()->addLayout(h);
    runInfo_ = makeMuted(QString());
    top->body()->addWidget(runInfo_);
    root->addWidget(top);
    auto* mid = new QHBoxLayout;
    mid->setSpacing(12);
    auto* c1 = new Card(tr2("Konsentrasi sepanjang sungai", "Concentration along the river"), "wq.run");
    auto* th = new QHBoxLayout;
    time_ = new QSlider(Qt::Horizontal);
    timeLabel_ = new QLabel;
    timeLabel_->setObjectName("Mono");
    th->addWidget(new QLabel(tr2("Waktu", "Time")));
    th->addWidget(time_, 1);
    th->addWidget(timeLabel_);
    c1->body()->addLayout(th);
    alongPlot_ = new PlotWidget;
    alongPlot_->setReverseX(true);
    alongPlot_->setEmptyText(tr2("Belum ada hasil. Klik Hitung kualitas air.", "No results yet. Click Compute water quality."));
    c1->body()->addWidget(alongPlot_, 1);
    mid->addWidget(c1, 3);
    auto* c2 = new Card(tr2("Konsentrasi terhadap waktu", "Concentration over time"), "wq.run");
    timePlot_ = new PlotWidget;
    timePlot_->setEmptyText(tr2("Belum ada hasil.", "No results yet."));
    c2->body()->addWidget(timePlot_, 1);
    mid->addWidget(c2, 2);
    root->addLayout(mid, 3);
    auto* c3 = new Card(tr2("Tabel pada waktu terpilih", "Table at the selected time"), "wq.run");
    table_ = new QTableWidget(0, 3);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setAlternatingRowColors(true);
    c3->body()->addWidget(table_, 1);
    summary_ = makeMuted(QString());
    c3->body()->addWidget(summary_);
    root->addWidget(c3, 2);
    connect(runBtn_, &QPushButton::clicked, this, &WaterQualityView::runWq);
    connect(csv, &QPushButton::clicked, this, &WaterQualityView::exportCsv);
    connect(time_, &QSlider::valueChanged, this, &WaterQualityView::showResult);
}

// ---------------- load / commit ----------------
void WaterQualityView::reload() {
    load();
    if (page_ == Page::Run) loadResult();
}

void WaterQualityView::load() {
    auto* p = s_->project();
    loading_ = true;
    w_ = p ? io::waterQualityFor(p->settings) : io::WqSetup{};
    if (page_ == Page::Data) {
        plan_->clear();
        if (p)
            for (const auto& [ext, pf] : p->plans)
                if (pf.model.isUnsteady()) plan_->addItem(QStringLiteral("%1 · %2").arg(qs(ext), qs(pf.model.title)), qs(ext));
        int i = plan_->findData(qs(w_.plan));
        if (i < 0) i = plan_->findData(s_->currentPlanExt());
        plan_->setCurrentIndex(std::max(0, i));
        if (plan_->count() == 0) plan_->addItem(tr2("(belum ada plan unsteady)", "(no unsteady plan yet)"), QString());
        name_->setText(qs(w_.constituent));
        initial_->setValue(w_.input.initial);
        disp_->setValue(w_.input.dispersion);
        disp_->setSuffix(QStringLiteral(" ") + lenU() + QStringLiteral("²/s"));
        decay_->setValue(w_.input.decayPerDay);
        std::vector<std::vector<double>> rows;
        for (const auto& u : w_.input.upstream) rows.push_back({u.first, u.second});
        upstream_->setValues(rows);
        const QString pe = plan_->currentData().toString();
        planNote_->setText(pe.isEmpty() ? tr2("Buat plan unsteady dulu (Data Aliran Unsteady dan Analisis Unsteady).", "Create an unsteady plan first (Unsteady Flow Data and Unsteady Analysis).")
                           : s_->results(pe) ? tr2("Plan %1 punya hasil unsteady: siap dihitung.", "Plan %1 has unsteady results: ready to compute.").arg(pe)
                                             : tr2("Plan %1 belum punya hasil unsteady. Jalankan Analisis Unsteady dulu.", "Plan %1 has no unsteady results yet. Run the Unsteady Analysis first.").arg(pe));
        plotInflow();
    }
    loading_ = false;
}

void WaterQualityView::commit() {
    auto* p = s_->project();
    if (loading_ || !p || page_ != Page::Data) return;
    io::WqSetup w = io::waterQualityFor(p->settings);
    if (!plan_->currentData().toString().isEmpty()) w.plan = ss(plan_->currentData().toString());
    w.constituent = ss(name_->text().trimmed().isEmpty() ? QStringLiteral("Tracer") : name_->text().trimmed());
    w.input.initial = initial_->value();
    w.input.dispersion = disp_->value();
    w.input.decayPerDay = decay_->value();
    std::vector<std::vector<double>> rows;
    upstream_->values(rows);
    w.input.upstream.clear();
    for (const auto& r : rows) if (r.size() >= 2 && std::isfinite(r[0]) && std::isfinite(r[1])) w.input.upstream.emplace_back(r[0], r[1]);
    std::sort(w.input.upstream.begin(), w.input.upstream.end());
    const json::Value before = p->settings.modules.count("water_quality") ? p->settings.modules["water_quality"] : json::Value();
    io::setWaterQuality(p->settings, w);
    w_ = w;
    if (before.dump(0) != p->settings.modules["water_quality"].dump(0)) s_->markEdited();
}

void WaterQualityView::runWq() {
    auto* p = s_->project();
    if (!p || running_) return;
    const io::WqSetup w = io::waterQualityFor(p->settings);
    if (w.plan.empty() || !p->plans.count(w.plan) || w.input.upstream.empty()) {
        sheetInfo(this, tr2("Data belum lengkap", "Data incomplete"),
                  tr2("Isi dulu Data Kualitas Air: pilih plan unsteady dan isi konsentrasi hulu.", "First fill the Water Quality Data: choose an unsteady plan and enter the upstream concentration."));
        return;
    }
    const RunResult* rr = s_->results(qs(w.plan));
    if (!rr || !rr->isUnsteady()) {
        sheetInfo(this, tr2("Belum ada hasil unsteady", "No unsteady results"), msgText(makeMsg(Severity::Error, "WQ_NO_RESULT", {w.plan})));
        return;
    }
    const std::string ge = p->plans.at(w.plan).model.geomFile;
    if (!p->geometries.count(ge)) return;
    Geometry g = p->geometries.at(ge).model;
    io::applyGeometrySettings(p->settings, ge, g);
    const RunResult hyd = *rr;
    const UnitSystem u = p->units();
    const std::string base = p->baseName();
    const io::fs::path dir = p->dir();
    running_ = true;
    runBtn_->setEnabled(false);
    runInfo_->setText(tr2("Menghitung…", "Computing…"));
    auto* out = new design::WqResult;
    QThread* th = QThread::create([g, hyd, w, u, out]() { *out = design::waterQuality(g, hyd, w.input, u); });
    QPointer<WaterQualityView> self(this);
    const std::string meta = "plan " + w.plan + ", " + w.constituent + ", De-RAS " + DERAS_VERSION + ", " + ss(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    connect(th, &QThread::finished, this, [self, th, out, dir, base, w, meta]() {
        th->deleteLater();
        std::unique_ptr<design::WqResult> r(out);
        if (!self) return;
        self->running_ = false;
        self->runBtn_->setEnabled(true);
        std::string err;
        io::writeWqResult(io::wqResultPath(dir, base, w.plan), *r, meta, err);
        self->res_ = std::move(*r);
        self->resMeta_ = meta;
        self->hasRes_ = true;
        self->time_->setRange(0, std::max(0, int(self->res_.hours.size()) - 1));
        self->time_->setValue(peakIndex(self->res_));
        self->showResult();
        if (self->res_.status != RunStatus::Succeeded) sheetInfo(self, tr2("Hitungan kualitas air gagal", "Water quality computation failed"), qs(self->res_.error));
        else showToast(self, tr2("Hitungan kualitas air selesai.", "Water quality computation finished."));
    });
    th->start();
}

void WaterQualityView::loadResult() {
    auto* p = s_->project();
    hasRes_ = false;
    if (p) {
        const io::WqSetup w = io::waterQualityFor(p->settings);
        std::string err;
        const auto path = io::wqResultPath(p->dir(), p->baseName(), w.plan);
        if (!w.plan.empty() && io::fs::exists(path) && io::readWqResult(path, res_, resMeta_, err)) hasRes_ = true;
    }
    time_->setRange(0, hasRes_ ? std::max(0, int(res_.hours.size()) - 1) : 0);
    time_->setValue(hasRes_ ? peakIndex(res_) : 0);
    showResult();
}

void WaterQualityView::showResult() {
    if (page_ != Page::Run) return;
    const Theme& th = Theme::current();
    if (!hasRes_ || res_.status != RunStatus::Succeeded || res_.hours.empty()) {
        alongPlot_->clear();
        timePlot_->clear();
        table_->setRowCount(0);
        timeLabel_->clear();
        summary_->clear();
        runInfo_->setText(hasRes_ ? tr2("Hasil terakhir gagal: %1", "The last run failed: %1").arg(qs(res_.error))
                                  : tr2("Belum ada hasil. Isi Data Kualitas Air (plan unsteady yang sudah dihitung), lalu klik Hitung kualitas air.",
                                        "No results yet. Fill the Water Quality Data (an unsteady plan that has been computed), then click Compute water quality."));
        return;
    }
    const QString L = lenU();
    const int k = std::clamp(time_->value(), 0, int(res_.hours.size()) - 1);
    timeLabel_->setText(tr2("jam %1", "hour %1").arg(num(res_.hours[size_t(k)], 2)));
    const size_t N = res_.rs.size();
    QVector<QPointF> ck, cmax;
    for (size_t i = 0; i < N; ++i) {
        ck << QPointF(res_.distance[i], res_.conc[size_t(k)][i]);
        double m = 0;
        for (const auto& c : res_.conc) if (i < c.size()) m = std::max(m, c[i]);
        cmax << QPointF(res_.distance[i], m);
    }
    alongPlot_->setTitles(QString(), tr2("Jarak dari hilir (%1)", "Distance from downstream (%1)").arg(L), QStringLiteral("mg/L"));
    alongPlot_->setSeries({PlotSeries{tr2("Maksimum", "Maximum"), cmax, th.text3, 1.4, Qt::DashLine}, PlotSeries{tr2("Waktu terpilih", "Selected time"), ck, th.primary, 2.2}});
    QVector<PlotSeries> cs;
    const QColor cols[] = {th.primary, th.success, th.warning};
    const size_t pick[] = {0, N / 2, N - 1};
    for (int j = 0; j < 3; ++j) {
        if (j > 0 && pick[j] == pick[j - 1]) continue;
        QVector<QPointF> pts;
        for (size_t t = 0; t < res_.hours.size(); ++t)
            if (pick[j] < res_.conc[t].size()) pts << QPointF(res_.hours[t], res_.conc[t][pick[j]]);
        cs << PlotSeries{QStringLiteral("RS %1").arg(qs(res_.rs[pick[j]])), pts, cols[j], 2.0};
    }
    timePlot_->setTitles(QString(), tr2("Waktu (jam)", "Time (h)"), QStringLiteral("mg/L"));
    timePlot_->setSeries(cs);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({QStringLiteral("RS"), tr2("Jarak (%1)", "Distance (%1)").arg(L), tr2("Konsentrasi (mg/L)", "Concentration (mg/L)")});
    table_->setRowCount(int(N));
    for (size_t i = 0; i < N; ++i) {
        auto cell = [&](int c, const QString& t) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(c == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(int(i), c, it);
        };
        cell(0, qs(res_.rs[i]));
        cell(1, num(res_.distance[i], 1));
        cell(2, num(res_.conc[size_t(k)][i], 3));
    }
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setStretchLastSection(true);
    const double bal = (res_.massStored - res_.massStored0) - (res_.massIn - res_.massOut - res_.massDecayed + res_.massCorrection);
    const bool us = s_->project() && s_->project()->units() == UnitSystem::USCustomary;
    summary_->setText(tr2("Neraca massa%1: masuk %2, keluar %3, meluruh %4, koreksi kontinuitas hidraulik %5, tersimpan awal %6, akhir %7; selisih %8. Status Pratinjau: belum dibandingkan dengan HEC-RAS.",
                          "Mass balance%1: in %2, out %3, decayed %4, hydraulic continuity correction %5, stored at start %6, at end %7; difference %8. Preview status: not compared with HEC-RAS yet.")
                          .arg(us ? QStringLiteral(" (mg/L × ft³ / 1000)") : QStringLiteral(" (kg)"), massText(res_.massIn), massText(res_.massOut), massText(res_.massDecayed),
                               massText(res_.massCorrection), massText(res_.massStored0), massText(res_.massStored), massText(bal)));
    runInfo_->setText(tr2("Hasil: %1 (%2), reach %3 / %4.", "Results: %1 (%2), reach %3 / %4.")
                          .arg(qs(io::fromPath(io::wqResultPath(io::fs::path(), s_->project() ? s_->project()->baseName() : std::string(), io::waterQualityFor(s_->project() ? s_->project()->settings : io::ProjectSettings{}).plan).filename())),
                               qs(resMeta_), qs(res_.river), qs(res_.reach)));
}

void WaterQualityView::exportCsv() {
    auto* p = s_->project();
    if (!p || !hasRes_ || res_.hours.empty()) return;
    const QString def = QDir(qs(io::fromPath(p->dir()))).filePath(qs(p->baseName()) + QStringLiteral("_kualitas_air.csv"));
    const QString f = QFileDialog::getSaveFileName(this, tr2("Ekspor hasil kualitas air", "Export water quality results"), def, QStringLiteral("CSV (*.csv)"));
    if (f.isEmpty()) return;
    std::string out = "hours,rs,distance,concentration_mg_l\n";
    for (size_t t = 0; t < res_.hours.size(); ++t)
        for (size_t i = 0; i < res_.rs.size(); ++i) {
            char line[256];
            std::snprintf(line, sizeof line, "%.4f,%s,%.3f,%.5f\n", res_.hours[t], res_.rs[i].c_str(), res_.distance[i], res_.conc[t][i]);
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

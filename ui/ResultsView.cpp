// De-RAS - ui/ResultsView.cpp
#include "ResultsView.h"

#include "Perspective3D.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QPdfWriter>
#include <QTextDocument>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <filesystem>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "PlotWidget.h"
#include "Version.h"
#include "Widgets.h"
#include "app/Report.h"
#include "app/RunService.h"
#include "core/Hash.h"
#include "io/ResultsFile.h"

namespace deras::ui {

namespace {
struct Col { const char* id; const char* en; const char* topic; int unit; };  // unit: 0 none,1 L,2 Q,3 A,4 V
const Col kCols[] = {
    {"Reach", "Reach", "results.col.reach", 0},
    {"RS", "RS", "results.col.rs", 0},
    {"Q total", "Q total", "results.col.q", 2},
    {"Elv. dasar min", "Min Ch El", "results.col.minchel", 1},
    {"Muka air (WS)", "W.S. Elev", "results.col.ws", 1},
    {"WS kritis", "Crit W.S.", "results.col.critws", 1},
    {"Garis energi (EG)", "E.G. Elev", "results.col.eg", 1},
    {"Kemiringan EG", "E.G. Slope", "results.col.egslope", 0},
    {"Kec. alur", "Vel Chnl", "results.col.velch", 4},
    {"Luas basah", "Flow Area", "results.col.area", 3},
    {"Lebar muka air", "Top Width", "results.col.topwidth", 1},
    {"Froude alur", "Froude # Chl", "results.col.froude", 0},
    {"Q bantaran kiri", "Q Left", "results.col.qparts", 2},
    {"Q alur", "Q Channel", "results.col.qparts", 2},
    {"Q bantaran kanan", "Q Right", "results.col.qparts", 2},
    {"Kehilangan gesek", "Friction loss", "results.col.losses", 1},
    {"Kehilangan C&E", "C & E loss", "results.col.losses", 1},
    {"Iterasi", "Trials", "results.col.trials", 0},
    {"Catatan", "Notes", "results.col.notes", 0},
};

struct NavItem { const char* page; const char* icon; const char* id; const char* en; const char* locked; };
const NavItem kNav[] = {
    {"xs", "xs", "Penampang", "Cross section", nullptr},
    {"profile", "profile", "Profil memanjang", "Profile", nullptr},
    {"general", "genprofile", "Profil umum", "General profile", nullptr},
    {"rating", "rating", "Kurva rating", "Rating curve", nullptr},
    {"3d", "cube", "Perspektif 3D", "3D perspective", nullptr},
    {"hydrograph", "hydrograph", "Hidrograf", "Hydrograph", nullptr},
    {"detail", "table-detail", "Tabel detail", "Detailed table", nullptr},
    {"table", "table-grid", "Tabel profil", "Profile table", nullptr},
    {"messages", "warning", "Ringkasan pesan", "Message summary", nullptr},
    {"timeseries", "timeseries", "Time series", "Time series", nullptr},
};
const QStringList kPages{"xs", "profile", "general", "detail", "table", "messages", "rating", "3d", "hydrograph", "timeseries"};

QTableWidgetItem* num(double v, int d) {
    auto* it = new QTableWidgetItem(QString::number(v, 'f', d));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setFont(monoFont());
    return it;
}
}  // namespace

bool ResultsView::isPageLocked(const QString& page, QString* featureId) {
    for (const auto& n : kNav)
        if (page == QLatin1String(n.page) && n.locked) {
            if (featureId) *featureId = QString::fromLatin1(n.locked);
            return true;
        }
    return false;
}

ResultsView::ResultsView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- left navigation ----
    auto* navCard = new Card(tr2("Hasil & tabel", "Results & tables"), "results.view");
    navCard->setFixedWidth(220);
    nav_ = new QListWidget;
    nav_->setObjectName("NavList");
    nav_->setIconSize(QSize(18, 18));
    nav_->setAccessibleName(tr2("Halaman hasil", "Result pages"));
    const Theme& th = Theme::current();
    for (const auto& n : kNav) {
        auto* it = new QListWidgetItem(QIcon(iconPixmap(n.locked ? "lock" : n.icon, 18, n.locked ? th.text3 : th.text2)), tr2(n.id, n.en));
        it->setData(Qt::UserRole, QString::fromLatin1(n.page));
        if (n.locked) {
            it->setForeground(th.text3);
            it->setToolTip(lockedTooltip(QString::fromLatin1(n.locked)));
        }
        nav_->addItem(it);
    }
    navCard->body()->addWidget(nav_, 1);
    root->addWidget(navCard);

    // ---- right: selectors, banners, pages ----
    auto* right = new QVBoxLayout;
    right->setSpacing(10);
    auto* sel = new QHBoxLayout;
    sel->setSpacing(8);
    auto box = [&](const QString& label, QComboBox*& c, int minW) {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(makeMuted(label, false));
        c = new QComboBox;
        c->setMinimumWidth(minW);
        c->setAccessibleName(label);
        h->addWidget(c);
        sel->addWidget(w);
        return w;
    };
    box(tr2("Profil", "Profile"), profile_, 120);
    reachBox_ = box(tr2("Reach", "Reach"), reach_, 180);
    xsBox_ = box("RS", xs_, 120);
    xs_->setFont(monoFont());
    varBox_ = box(tr2("Variabel", "Variable"), variable_, 180);
    variable_->addItem(tr2("Kecepatan alur", "Channel velocity"), 0);
    variable_->addItem(tr2("Bilangan Froude alur", "Channel Froude number"), 1);
    variable_->addItem(tr2("Lebar muka air", "Top width"), 2);
    variable_->addItem(tr2("Luas basah", "Flow area"), 3);
    variable_->addItem(tr2("Kedalaman maks. alur", "Max channel depth"), 4);
    variable_->addItem(tr2("Kemiringan garis energi", "Energy grade slope"), 5);
    sel->addStretch();
    origin_ = new QLabel;
    origin_->setObjectName("BadgeAccent");
    sel->addWidget(origin_);
    right->addLayout(sel);
    stale_ = new QLabel;
    stale_->setObjectName("Banner");
    stale_->setWordWrap(true);
    stale_->hide();
    right->addWidget(stale_);

    pages_ = new QStackedWidget;
    auto plotCard = [&](PlotWidget*& p, const QString& topic) {
        auto* c = new Card(QString(), topic);
        p = new PlotWidget;
        c->body()->addWidget(p, 1);
        pages_->addWidget(c);
    };
    plotCard(xsPlot_, "results.xs");
    plotCard(profilePlot_, "results.profile");
    plotCard(generalPlot_, "results.general");
    auto tableCard = [&](QTableWidget*& t, const QString& topic) {
        auto* c = new Card(QString(), topic);
        t = new QTableWidget;
        t->setHorizontalHeader(new InfoHeader(Qt::Horizontal, t));
        t->verticalHeader()->hide();
        t->setAlternatingRowColors(true);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        c->body()->addWidget(t, 1);
        pages_->addWidget(c);
    };
    tableCard(detail_, "results.detail");
    tableCard(table_, "results.table");
    detail_->setAccessibleName(tr2("Tabel detail", "Detailed table"));
    table_->setAccessibleName(tr2("Tabel profil", "Profile table"));
    auto* mc = new Card(QString(), "results.messages");
    msgs_ = new QListWidget;
    msgs_->setWordWrap(true);
    msgs_->setObjectName("NavList");
    mc->body()->addWidget(msgs_, 1);
    pages_->addWidget(mc);
    plotCard(ratingPlot_, "results.rating");
    {
        auto* c = new Card(QString(), "results.3d");
        p3d_ = new Perspective3D;
        c->body()->addWidget(p3d_, 1);
        pages_->addWidget(c);
    }
    {
        auto* c = new Card(QString(), "results.hydrograph");
        hydroStage_ = new PlotWidget;
        hydroFlow_ = new PlotWidget;
        c->body()->addWidget(hydroStage_, 1);
        c->body()->addWidget(hydroFlow_, 1);
        pages_->addWidget(c);
    }
    tableCard(ts_, "results.timeseries");
    ts_->setAccessibleName(QStringLiteral("Time series"));
    right->addWidget(pages_, 1);
    root->addLayout(right, 1);

    connect(nav_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const QString pg = it->data(Qt::UserRole).toString();
        QString f;
        if (isPageLocked(pg, &f)) {
            emit requestLocked(f);
            loading_ = true;
            nav_->setCurrentRow(static_cast<int>(std::distance(std::begin(kNav), std::find_if(std::begin(kNav), std::end(kNav), [this](const NavItem& n) { return page_ == QLatin1String(n.page); }))));
            loading_ = false;
            return;
        }
        showPage(pg);
    });
    for (QComboBox* c : {profile_, reach_, xs_, variable_})
        connect(c, &QComboBox::currentIndexChanged, this, [this, c]() {
            if (loading_) return;
            if (c == reach_) fillSelectors();
            refreshPage();
        });
    connect(s_, &ProjectSession::resultsUpdated, this, [this](const QString& plan) { if (plan == s_->currentPlanExt()) reload(); });
    connect(s_, &ProjectSession::saved, this, [this]() { if (isVisible()) checkStale(); });
    reload();
    showPage("xs");
}

QString ResultsView::title() const {
    for (const auto& n : kNav)
        if (page_ == QLatin1String(n.page)) return tr2("Hasil — %1", "Results — %1").arg(tr2(n.id, n.en));
    return tr2("Hasil", "Results");
}

QString ResultsView::helpTopic() const { return QStringLiteral("results.") + (page_ == "general" ? QStringLiteral("general") : page_); }

QWidget* ResultsView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* csv = makeButton(tr2("Ekspor CSV…", "Export CSV…"), false, "export");
    auto* rep = makeButton(tr2("Laporan…", "Report…"), true, "file");
    rep->setToolTip(tr2("Laporan run: HTML, Word (.docx), atau PDF", "Run report: HTML, Word (.docx) or PDF"));
    h->addWidget(csv);
    h->addWidget(rep);
    h->addWidget(new InfoButton("results.export"));
    connect(csv, &QPushButton::clicked, this, &ResultsView::exportCsv);
    connect(rep, &QPushButton::clicked, this, &ResultsView::exportReport);
    return w;
}

void ResultsView::showPage(const QString& page) {
    if (isPageLocked(page) || !kPages.contains(page)) return;
    page_ = page;
    loading_ = true;
    for (int i = 0; i < nav_->count(); ++i)
        if (nav_->item(i)->data(Qt::UserRole).toString() == page) nav_->setCurrentRow(i);
    loading_ = false;
    pages_->setCurrentIndex(kPages.indexOf(page));
    xsBox_->setVisible(page == "xs" || page == "detail" || page == "rating" || page == "hydrograph" || page == "timeseries");
    reachBox_->setVisible(page == "xs" || page == "detail" || page == "profile" || page == "general" || page == "rating" || page == "3d" ||
                          page == "hydrograph" || page == "timeseries");
    varBox_->setVisible(page == "general");
    if (page == "messages" || page == "table") reachBox_->setVisible(false);
    refreshPage();
    emit headerChanged();
}

void ResultsView::activated() {
    if (planExt_ != s_->currentPlanExt()) reload();
    else checkStale();
}

void ResultsView::reload() {
    planExt_ = s_->currentPlanExt();
    result_ = planExt_.isEmpty() ? nullptr : s_->results(planExt_);
    const Language lang = isID() ? Language::ID : Language::EN;
    if (!result_) {
        origin_->setText(s_->isOpen() ? tr2("Belum ada hasil", "No results yet") : tr2("Tidak ada proyek", "No project"));
        origin_->setObjectName("BadgeLocked");
    } else {
        origin_->setText(qs(originLabel(result_->origin, lang)) + QStringLiteral(" · ") + qs(result_->meta.solverName) + QStringLiteral(" ") + qs(result_->meta.solverVersion));
        origin_->setObjectName(result_->status == RunStatus::Succeeded ? "BadgeAccent" : "BadgeError");
        origin_->setToolTip(tr2("Metode %1 · selesai %2 UTC (%3 dtk) · hasil belum dibandingkan dengan HEC-RAS untuk model ini.",
                                "Method %1 · finished %2 UTC (%3 s) · equivalence with HEC-RAS has not been validated for this model.")
                                .arg(qs(result_->meta.method), qs(result_->meta.finishedUtc), QString::number(result_->meta.elapsedSeconds, 'f', 2)));
    }
    origin_->style()->unpolish(origin_);
    origin_->style()->polish(origin_);
    loading_ = true;
    const QString keepProfile = profile_->currentText();
    profile_->clear();
    if (result_)
        for (const auto& pr : result_->profiles) profile_->addItem(qs(pr.name));
    const int pi = profile_->findText(keepProfile);
    profile_->setCurrentIndex(pi >= 0 ? pi : (profile_->count() ? 0 : -1));
    const QString keepReach = reach_->currentText();
    reach_->clear();
    if (result_ && !result_->profiles.empty()) {
        QStringList seen;
        for (const auto& x : result_->profiles.front().xs) {
            const QString l = qs(x.river + " / " + x.reach);
            if (!seen.contains(l)) { seen << l; reach_->addItem(l); }
        }
    }
    if (result_)
        for (size_t i = 0; i < result_->extra.size(); ++i) {
            const auto& e = result_->extra[i];
            const QString k = e.kind == "storage" ? QStringLiteral("Storage area") : e.kind == "lateral" ? tr2("Struktur lateral", "Lateral structure")
                            : e.kind == "connection" ? tr2("Koneksi", "Connection") : e.kind == "pump" ? tr2("Pompa", "Pump") : qs(e.kind);
            reach_->addItem(k + QStringLiteral(": ") + qs(e.name), QStringLiteral("extra:%1").arg(i));
        }
    const int ri = reach_->findText(keepReach);
    reach_->setCurrentIndex(ri >= 0 ? ri : (reach_->count() ? 0 : -1));
    loading_ = false;
    fillSelectors();
    checkStale();
    refreshPage();
}

void ResultsView::fillSelectors() {
    loading_ = true;
    const QString keep = xs_->currentText();
    xs_->clear();
    if (const ProfileResult* pr = currentProfile())
        for (const auto& x : pr->xs)
            if (qs(x.river + " / " + x.reach) == reach_->currentText()) xs_->addItem(qs(x.rs));
    int i = xs_->findText(keep);
    // follow the cross section selected elsewhere (schematic / editor)
    if (auto* g = s_->currentGeometry())
        for (const auto& r : g->model.reaches)
            if (r.uid == s_->selectedReach() && qs(r.label()) == reach_->currentText())
                for (const auto& n : r.nodes)
                    if (n.uid == s_->selectedXs()) i = std::max(i, xs_->findText(qs(n.rs)));
    xs_->setCurrentIndex(i >= 0 ? i : (xs_->count() ? 0 : -1));
    loading_ = false;
}

const ProfileResult* ResultsView::currentProfile() const {
    if (!result_) return nullptr;
    const int pi = profile_->currentIndex();
    if (pi < 0 || pi >= static_cast<int>(result_->profiles.size())) return nullptr;
    return &result_->profiles[static_cast<size_t>(pi)];
}

const XsResult* ResultsView::currentXs() const {
    const ProfileResult* pr = currentProfile();
    if (!pr) return nullptr;
    for (const auto& x : pr->xs)
        if (qs(x.river + " / " + x.reach) == reach_->currentText() && qs(x.rs) == xs_->currentText()) return &x;
    return nullptr;
}

void ResultsView::refreshPage() {
    if (page_ == "xs") fillXs();
    else if (page_ == "profile") fillProfile();
    else if (page_ == "general") fillGeneral();
    else if (page_ == "detail") fillDetail();
    else if (page_ == "table") fillTable();
    else if (page_ == "messages") fillMessages();
    else if (page_ == "rating") fillRating();
    else if (page_ == "3d") fill3d();
    else if (page_ == "hydrograph") fillHydrograph();
    else if (page_ == "timeseries") fillTimeSeries();
}

int ResultsView::seriesIndex() const {
    if (!result_ || result_->series.empty() || result_->profiles.empty()) return -1;
    const auto& xs = result_->profiles.front().xs;
    for (size_t i = 0; i < xs.size(); ++i)
        if (qs(xs[i].river + " / " + xs[i].reach) == reach_->currentText() && qs(xs[i].rs) == xs_->currentText()) return int(i);
    return -1;
}

const ExtraSeries* ResultsView::extraSeries() const {
    const QString d = reach_->currentData().toString();
    if (!result_ || !d.startsWith(QStringLiteral("extra:"))) return nullptr;
    const int i = d.mid(6).toInt();
    return i >= 0 && i < int(result_->extra.size()) ? &result_->extra[size_t(i)] : nullptr;
}

void ResultsView::fillHydrograph() {
    hydroStage_->clear();
    hydroFlow_->clear();
    auto* p = s_->project();
    if (const ExtraSeries* e = extraSeries(); e && p) {
        const auto& ts = result_->series;
        const bool stage = e->quantity == "stage";
        PlotSeries ps{qs(e->name), {}, stage ? Theme::current().water : Theme::current().accent, 2.0};
        for (size_t t = 0; t < ts.hours.size() && t < e->values.size(); ++t) ps.pts << QPointF(ts.hours[t], e->values[t]);
        const QString start = qs(rasDateText(ts.startSeconds) + " " + rasTimeText(ts.startSeconds));
        PlotWidget* w = stage ? hydroStage_ : hydroFlow_;
        (stage ? hydroFlow_ : hydroStage_)->setEmptyText(QString());
        w->setTitles(reach_->currentText(), tr2("Jam sejak %1", "Hours since %1").arg(start),
                     stage ? tr2("Elevasi (%1)", "Elevation (%1)").arg(qs(lengthUnit(p->units()))) : tr2("Debit (%1)", "Flow (%1)").arg(qs(flowUnit(p->units()))));
        w->setSeries({ps});
        return;
    }
    const QString none = !result_ ? tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute.")
                         : result_->series.empty() ? tr2("Hidrograf tersedia untuk hasil plan unsteady.", "Hydrographs are available for unsteady plan results.")
                                                   : tr2("Pilih penampang.", "Pick a cross section.");
    const int k = seriesIndex();
    if (k < 0 || !p) {
        hydroStage_->setEmptyText(none);
        hydroFlow_->setEmptyText(QString());
        return;
    }
    const Theme& th = Theme::current();
    const auto& ts = result_->series;
    PlotSeries st{tr2("Muka air", "Stage"), {}, th.water, 2.0};
    PlotSeries fl{tr2("Debit", "Flow"), {}, th.accent, 2.0};
    const double bed = result_->profiles.front().xs[size_t(k)].minChEl;
    PlotSeries bd{tr2("Dasar alur", "Channel invert"), {}, th.ground, 1.2, Qt::DashLine};
    for (size_t t = 0; t < ts.hours.size(); ++t) {
        if (size_t(k) >= ts.ws[t].size()) continue;
        st.pts << QPointF(ts.hours[t], ts.ws[t][size_t(k)]);
        fl.pts << QPointF(ts.hours[t], ts.q[t][size_t(k)]);
        bd.pts << QPointF(ts.hours[t], bed);
    }
    const QString start = qs(rasDateText(ts.startSeconds) + " " + rasTimeText(ts.startSeconds));
    const UnitSystem u = p->units();
    hydroStage_->setTitles(tr2("Hidrograf muka air RS %1", "Stage hydrograph RS %1").arg(xs_->currentText()), tr2("Jam sejak %1", "Hours since %1").arg(start),
                           tr2("Elevasi (%1)", "Elevation (%1)").arg(qs(lengthUnit(u))));
    hydroStage_->setSeries({st, bd});
    hydroFlow_->setTitles(tr2("Hidrograf debit RS %1", "Flow hydrograph RS %1").arg(xs_->currentText()), tr2("Jam sejak %1", "Hours since %1").arg(start),
                          tr2("Debit (%1)", "Flow (%1)").arg(qs(flowUnit(u))));
    hydroFlow_->setSeries({fl});
}

void ResultsView::fillTimeSeries() {
    ts_->clear();
    ts_->setRowCount(0);
    auto* p = s_->project();
    if (const ExtraSeries* e = extraSeries(); e && p) {
        const auto& ts = result_->series;
        const bool stage = e->quantity == "stage";
        ts_->setColumnCount(3);
        ts_->setHorizontalHeaderLabels({tr2("Tanggal jam", "Date time"), tr2("Jam ke-", "Hour"),
                                        stage ? tr2("Muka air (%1)", "W.S. Elev (%1)").arg(qs(lengthUnit(p->units()))) : tr2("Debit (%1)", "Flow (%1)").arg(qs(flowUnit(p->units())))});
        ts_->setRowCount(int(std::min(ts.hours.size(), e->values.size())));
        for (size_t t = 0; t < ts.hours.size() && t < e->values.size(); ++t) {
            const double sec = ts.startSeconds + ts.hours[t] * 3600;
            auto* d = new QTableWidgetItem(qs(rasDateText(sec) + " " + rasTimeText(sec)));
            d->setFont(monoFont());
            ts_->setItem(int(t), 0, d);
            ts_->setItem(int(t), 1, num(ts.hours[t], 2));
            ts_->setItem(int(t), 2, num(e->values[t], stage ? 3 : 2));
        }
        ts_->resizeColumnsToContents();
        return;
    }
    const int k = seriesIndex();
    if (k < 0 || !p) {
        ts_->setColumnCount(1);
        ts_->setHorizontalHeaderLabels({!result_ || result_->series.empty() ? tr2("Time series tersedia untuk hasil plan unsteady.", "Time series are available for unsteady plan results.")
                                                                           : tr2("Pilih penampang.", "Pick a cross section.")});
        return;
    }
    const UnitSystem u = p->units();
    const auto& ts = result_->series;
    ts_->setColumnCount(4);
    ts_->setHorizontalHeaderLabels({tr2("Tanggal jam", "Date time"), tr2("Jam ke-", "Hour"), tr2("Muka air (%1)", "W.S. Elev (%1)").arg(qs(lengthUnit(u))),
                                    tr2("Debit (%1)", "Flow (%1)").arg(qs(flowUnit(u)))});
    ts_->setRowCount(int(ts.hours.size()));
    for (size_t t = 0; t < ts.hours.size(); ++t) {
        const double sec = ts.startSeconds + ts.hours[t] * 3600;
        auto* d = new QTableWidgetItem(qs(rasDateText(sec) + " " + rasTimeText(sec)));
        d->setFont(monoFont());
        ts_->setItem(int(t), 0, d);
        ts_->setItem(int(t), 1, num(ts.hours[t], 2));
        if (size_t(k) < ts.ws[t].size()) {
            ts_->setItem(int(t), 2, num(ts.ws[t][size_t(k)], 3));
            ts_->setItem(int(t), 3, num(ts.q[t][size_t(k)], 2));
        }
    }
    ts_->resizeColumnsToContents();
}

void ResultsView::fill3d() {
    auto* g = s_->currentGeometry();
    const ProfileResult* pr = currentProfile();
    if (!result_ || !pr || !g) {
        p3d_->setEmptyText(tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute."));
        return;
    }
    const Reach* rc = nullptr;
    for (const auto& r : g->model.reaches)
        if (qs(r.label()) == reach_->currentText()) rc = &r;
    if (!rc) { p3d_->setEmptyText(tr2("Pilih reach.", "Pick a reach.")); return; }
    std::vector<double> ws;
    for (const auto* c : rc->crossSections()) {
        double w = std::nan("");
        for (const auto& x : pr->xs)
            if (x.river == rc->river && x.reach == rc->reach && x.rs == c->rs) w = x.ws;
        ws.push_back(w);
    }
    p3d_->setData(rc, ws, tr2("%1 — %2", "%1 — %2").arg(qs(rc->label()), profile_->currentText()));
}

void ResultsView::fillRating() {
    ratingPlot_->clear();
    auto* p = s_->project();
    if (!result_ || !p) { ratingPlot_->setEmptyText(tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute.")); return; }
    const Theme& th = Theme::current();
    const UnitSystem u = p->units();
    struct Pt { double q, ws, eg, crit; };
    std::vector<Pt> pts;
    for (const auto& pr : result_->profiles)
        for (const auto& x : pr.xs)
            if (qs(x.river + " / " + x.reach) == reach_->currentText() && qs(x.rs) == xs_->currentText()) pts.push_back({x.q, x.ws, x.eg, x.critWs});
    if (pts.empty()) { ratingPlot_->setEmptyText(tr2("Pilih penampang.", "Pick a cross section.")); return; }
    std::sort(pts.begin(), pts.end(), [](const Pt& a, const Pt& b) { return a.q < b.q; });
    PlotSeries ws{tr2("Muka air", "Water surface"), {}, th.water, 2.2};
    ws.markers = true;
    PlotSeries eg{tr2("Garis energi", "Energy grade"), {}, th.success, 1.2, Qt::DotLine};
    PlotSeries crit{tr2("Muka air kritis", "Critical WS"), {}, th.error, 1.2, Qt::DashLine};
    for (const auto& q : pts) {
        ws.pts << QPointF(q.q, q.ws);
        eg.pts << QPointF(q.q, q.eg);
        crit.pts << QPointF(q.q, q.crit);
    }
    ratingPlot_->setTitles(tr2("Kurva rating RS %1 (%2 profil)", "Rating curve RS %1 (%2 profiles)").arg(xs_->currentText()).arg(pts.size()),
                           tr2("Debit (%1)", "Flow (%1)").arg(qs(flowUnit(u))), tr2("Elevasi (%1)", "Elevation (%1)").arg(qs(lengthUnit(u))));
    ratingPlot_->setSeries({ws, eg, crit});
    ratingPlot_->setReverseX(false);
}

void ResultsView::checkStale() {
    auto* p = s_->project();
    if (!result_ || !p) { stale_->hide(); return; }
    QStringList why;
    auto check = [&](const std::string& file, const std::string& sha, const char* id, const char* en) {
        std::string bytes;
        if (file.empty() || !io::readFile(p->dir() / io::toPath(file), bytes) || sha256Hex(bytes) != sha) why << tr2(id, en);
    };
    check(result_->meta.geomFile, result_->meta.geomSha256, "geometri", "geometry");
    check(result_->meta.flowFile, result_->meta.flowSha256, "aliran", "flow");
    check(result_->meta.planFile, result_->meta.planSha256, "plan", "plan");
    // Only unsaved edits of the files this result was computed from count; switching the current plan does not.
    bool unsaved = false;
    auto ext = [](const std::string& f) { const auto d = f.rfind('.'); return d == std::string::npos ? std::string() : f.substr(d + 1); };
    if (auto it = p->geometries.find(ext(result_->meta.geomFile)); it != p->geometries.end() && it->second.isModified()) unsaved = true;
    if (auto it = p->steadyFlows.find(ext(result_->meta.flowFile)); it != p->steadyFlows.end() && it->second.isModified()) unsaved = true;
    if (auto it = p->unsteadyFlows.find(ext(result_->meta.flowFile)); it != p->unsteadyFlows.end() && it->second.isModified()) unsaved = true;
    if (auto it = p->plans.find(ext(result_->meta.planFile)); it != p->plans.end() && it->second.isModified()) unsaved = true;
    if (unsaved) why << tr2("perubahan belum disimpan", "unsaved changes");
    if (why.isEmpty()) { stale_->hide(); return; }
    stale_->setText(QStringLiteral("⚠ ") + tr2("Hasil ini mungkin sudah tidak berlaku: data berubah sejak run terakhir (%1). Hitung ulang dengan F5.",
                                               "Results may be stale: input changed since the last run (%1). Run again (F5).").arg(why.join(", ")));
    stale_->show();
}

void ResultsView::fillXs() {
    xsPlot_->clear();
    auto* p = s_->project();
    auto* g = s_->currentGeometry();
    const XsResult* r = currentXs();
    if (!r || !p || !g) {
        xsPlot_->setEmptyText(result_ ? tr2("Pilih penampang.", "Pick a cross section.") : tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute."));
        return;
    }
    const CrossSection* cs = nullptr;
    if (const Reach* rc = g->model.findReach(r->river, r->reach))
        for (const auto* c : rc->crossSections()) if (c->rs == r->rs) cs = c;
    if (!cs || cs->staElev.empty()) {
        xsPlot_->setEmptyText(tr2("Penampang ini tidak ada di geometri sekarang. Mungkin hasilnya dari geometri lama.", "Cross section not found in the current geometry (stale results?)."));
        return;
    }
    const Theme& th = Theme::current();
    const QString lu = qs(lengthUnit(p->units()));
    PlotSeries ground{tr2("Tanah", "Ground"), {}, th.ground, 2.2};
    ground.markers = true;
    for (const auto& pt : cs->staElev) ground.pts << QPointF(pt.station, pt.elev);
    const double x0 = cs->staElev.front().station, x1 = cs->staElev.back().station;
    PlotSeries fill{QString(), {}, Qt::transparent, 1.0};
    for (const auto& pt : cs->staElev) fill.pts << QPointF(pt.station, std::max(pt.elev, r->ws));
    fill.fillBelowSeries = 0;
    fill.fillColor = withAlpha(th.water, 0.32);
    fill.legend = false;
    double wl = x0, wr = x1;
    const auto& se = cs->staElev;
    for (size_t i = 0; i < se.size(); ++i)
        if (se[i].elev <= r->ws) {
            wl = se[i].station;
            if (i > 0 && se[i - 1].elev > r->ws) wl = se[i - 1].station + (se[i].station - se[i - 1].station) * (se[i - 1].elev - r->ws) / (se[i - 1].elev - se[i].elev);
            break;
        }
    for (size_t i = se.size(); i-- > 0;)
        if (se[i].elev <= r->ws) {
            wr = se[i].station;
            if (i + 1 < se.size() && se[i + 1].elev > r->ws) wr = se[i].station + (se[i + 1].station - se[i].station) * (r->ws - se[i].elev) / (se[i + 1].elev - se[i].elev);
            break;
        }
    PlotSeries wsl{tr2("Muka air ", "W.S. ") + profile_->currentText(), {QPointF(wl, r->ws), QPointF(wr, r->ws)}, th.water, 2.2};
    PlotSeries eg{tr2("Garis energi", "Energy grade"), {QPointF(x0, r->eg), QPointF(x1, r->eg)}, th.success, 1.2, Qt::DotLine};
    PlotSeries crit{tr2("WS kritis", "Critical WS"), {QPointF(x0, r->critWs), QPointF(x1, r->critWs)}, th.error, 1.2, Qt::DashLine};
    QVector<PlotMarker> marks;
    if (cs->bankLeft) marks.push_back({*cs->bankLeft, tr2("Tebing kiri", "Left bank"), th.accent2});
    if (cs->bankRight) marks.push_back({*cs->bankRight, tr2("Tebing kanan", "Right bank"), th.accent2});
    xsPlot_->setTitles(QStringLiteral("RS %1 — %2").arg(qs(r->rs), profile_->currentText()), tr2("Stasiun (%1)", "Station (%1)").arg(lu), tr2("Elevasi (%1)", "Elevation (%1)").arg(lu));
    xsPlot_->setSeries({ground, fill, wsl, eg, crit});
    xsPlot_->setMarkers(marks);
}

void ResultsView::fillProfile() {
    profilePlot_->clear();
    auto* p = s_->project();
    const ProfileResult* pr = currentProfile();
    if (!pr || !p) { profilePlot_->setEmptyText(tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute.")); return; }
    const QString lu = qs(lengthUnit(p->units()));
    const Theme& th = Theme::current();
    PlotSeries bed{tr2("Dasar alur", "Channel bed"), {}, th.ground, 2.2};
    PlotSeries ws{tr2("Muka air", "Water surface"), {}, th.water, 2.2};
    PlotSeries crit{tr2("Muka air kritis", "Critical WS"), {}, th.error, 1.2, Qt::DashLine};
    PlotSeries eg{tr2("Garis energi", "Energy grade"), {}, th.success, 1.2, Qt::DotLine};
    for (const auto& x : pr->xs) {
        if (qs(x.river + " / " + x.reach) != reach_->currentText()) continue;
        bed.pts << QPointF(x.channelDistance, x.minChEl);
        ws.pts << QPointF(x.channelDistance, x.ws);
        crit.pts << QPointF(x.channelDistance, x.critWs);
        eg.pts << QPointF(x.channelDistance, x.eg);
    }
    ws.fillBelowSeries = 0;
    ws.fillColor = withAlpha(th.water, 0.30);
    ws.markers = true;
    profilePlot_->setTitles(reach_->currentText() + " — " + profile_->currentText(),
                            tr2("Jarak alur dari hilir (%1)", "Channel distance from downstream (%1)").arg(lu), tr2("Elevasi (%1)", "Elevation (%1)").arg(lu));
    profilePlot_->setSeries({bed, ws, crit, eg});
    profilePlot_->setReverseX(true);
}

void ResultsView::fillGeneral() {
    generalPlot_->clear();
    auto* p = s_->project();
    if (!result_ || !p) { generalPlot_->setEmptyText(tr2("Belum ada hasil. Tekan F5 untuk menghitung plan ini.", "No results yet. Run the plan (F5) to compute.")); return; }
    const UnitSystem u = p->units();
    const int v = variable_->currentData().toInt();
    const QString units[] = {qs(velocityUnit(u)), QString(), qs(lengthUnit(u)), qs(areaUnit(u)), qs(lengthUnit(u)), QString()};
    QVector<PlotSeries> series;
    const QColor cols[] = {Theme::current().water, Theme::current().accent2, Theme::current().warning, Theme::current().success, Theme::current().error};
    int k = 0;
    for (const auto& pr : result_->profiles) {
        PlotSeries s{qs(pr.name), {}, cols[k % 5], 2.0};
        s.markers = true;
        for (const auto& x : pr.xs) {
            if (qs(x.river + " / " + x.reach) != reach_->currentText()) continue;
            const double val = v == 0 ? x.velCh : v == 1 ? x.froudeCh : v == 2 ? x.topWidth : v == 3 ? x.area : v == 4 ? x.ws - x.minChEl : x.egSlope;
            s.pts << QPointF(x.channelDistance, val);
        }
        series << s;
        ++k;
    }
    const QString yt = variable_->currentText() + (units[v].isEmpty() ? QString() : QStringLiteral(" (%1)").arg(units[v]));
    generalPlot_->setTitles(reach_->currentText() + " — " + variable_->currentText(), tr2("Jarak alur dari hilir (%1)", "Channel distance from downstream (%1)").arg(qs(lengthUnit(u))), yt);
    generalPlot_->setSeries(series);
    generalPlot_->setReverseX(true);
}

void ResultsView::fillDetail() {
    detail_->clear();
    detail_->setRowCount(0);
    detail_->setColumnCount(2);
    detail_->setHorizontalHeaderLabels({tr2("Besaran", "Quantity"), tr2("Nilai", "Value")});
    static_cast<InfoHeader*>(detail_->horizontalHeader())->setTopics({"results.detail", "results.detail"});
    const XsResult* x = currentXs();
    auto* p = s_->project();
    if (!x || !p) return;
    const UnitSystem u = p->units();
    const QString L = qs(lengthUnit(u)), Q = qs(flowUnit(u)), A = qs(areaUnit(u)), V = qs(velocityUnit(u));
    struct Row { QString k; double v; int d; QString unit; };
    const QList<Row> rows{
        {tr2("Elevasi garis energi", "E.G. Elev"), x->eg, 3, L},           {tr2("Elevasi muka air", "W.S. Elev"), x->ws, 3, L},
        {tr2("Muka air kritis", "Crit W.S."), x->critWs, 3, L},            {tr2("Elevasi dasar min.", "Min Ch El"), x->minChEl, 3, L},
        {tr2("Kemiringan garis energi", "E.G. Slope"), x->egSlope, 6, QString()},
        {tr2("Q total", "Q Total"), x->q, 3, Q},                           {tr2("Q bantaran kiri", "Q Left OB"), x->qLob, 3, Q},
        {tr2("Q alur", "Q Channel"), x->qCh, 3, Q},                        {tr2("Q bantaran kanan", "Q Right OB"), x->qRob, 3, Q},
        {tr2("Luas basah total", "Flow Area"), x->area, 3, A},             {tr2("Luas bantaran kiri", "Area Left OB"), x->areaLob, 3, A},
        {tr2("Luas alur", "Area Channel"), x->areaCh, 3, A},               {tr2("Luas bantaran kanan", "Area Right OB"), x->areaRob, 3, A},
        {tr2("Kecepatan alur", "Vel Chnl"), x->velCh, 3, V},               {tr2("Kecepatan total", "Vel Total"), x->velTotal, 3, V},
        {tr2("Lebar muka air", "Top Width"), x->topWidth, 3, L},           {tr2("Froude alur", "Froude # Chl"), x->froudeCh, 3, QString()},
        {tr2("Alpha (koef. energi)", "Alpha"), x->alpha, 3, QString()},    {tr2("Kehilangan gesek", "Frctn Loss"), x->frictionLoss, 4, L},
        {tr2("Kehilangan kontraksi/ekspansi", "C & E Loss"), x->ceLoss, 4, L},
        {tr2("Panjang tertimbang debit", "Length Wtd."), x->lengthWeighted, 2, L},
        {tr2("Iterasi", "Trials"), static_cast<double>(x->trials), 0, QString()},
    };
    detail_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        detail_->setItem(i, 0, new QTableWidgetItem(rows[i].k + (rows[i].unit.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(rows[i].unit))));
        detail_->setItem(i, 1, num(rows[i].v, rows[i].d));
    }
    if (x->critical) {
        const int r = detail_->rowCount();
        detail_->insertRow(r);
        detail_->setItem(r, 0, new QTableWidgetItem(tr2("Catatan", "Note")));
        detail_->setItem(r, 1, new QTableWidgetItem(tr2("WS = kedalaman kritis", "WS = critical depth")));
    }
    detail_->resizeColumnsToContents();
    detail_->horizontalHeader()->setStretchLastSection(true);
}

void ResultsView::fillTable() {
    table_->clear();
    table_->setRowCount(0);
    auto* p = s_->project();
    const UnitSystem u = p ? p->units() : UnitSystem::SI;
    QStringList heads, topics;
    for (const auto& c : kCols) {
        QString h = tr2(c.id, c.en);
        if (c.unit == 1) h += " (" + qs(lengthUnit(u)) + ")";
        if (c.unit == 2) h += " (" + qs(flowUnit(u)) + ")";
        if (c.unit == 3) h += " (" + qs(areaUnit(u)) + ")";
        if (c.unit == 4) h += " (" + qs(velocityUnit(u)) + ")";
        heads << h;
        topics << c.topic;
    }
    table_->setColumnCount(heads.size());
    table_->setHorizontalHeaderLabels(heads);
    static_cast<InfoHeader*>(table_->horizontalHeader())->setTopics(topics);
    const ProfileResult* pr = currentProfile();
    if (!pr) return;
    const auto& xs = pr->xs;
    table_->setRowCount(static_cast<int>(xs.size()));
    for (int r = 0; r < static_cast<int>(xs.size()); ++r) {
        const auto& x = xs[static_cast<size_t>(r)];
        int c = 0;
        table_->setItem(r, c++, new QTableWidgetItem(qs(x.river + " / " + x.reach)));
        auto* rs = new QTableWidgetItem(qs(x.rs));
        rs->setFont(monoFont());
        table_->setItem(r, c++, rs);
        table_->setItem(r, c++, num(x.q, 2));
        table_->setItem(r, c++, num(x.minChEl, 2));
        table_->setItem(r, c++, num(x.ws, 2));
        table_->setItem(r, c++, num(x.critWs, 2));
        table_->setItem(r, c++, num(x.eg, 2));
        table_->setItem(r, c++, num(x.egSlope, 6));
        table_->setItem(r, c++, num(x.velCh, 2));
        table_->setItem(r, c++, num(x.area, 2));
        table_->setItem(r, c++, num(x.topWidth, 2));
        table_->setItem(r, c++, num(x.froudeCh, 2));
        table_->setItem(r, c++, num(x.qLob, 2));
        table_->setItem(r, c++, num(x.qCh, 2));
        table_->setItem(r, c++, num(x.qRob, 2));
        table_->setItem(r, c++, num(x.frictionLoss, 3));
        table_->setItem(r, c++, num(x.ceLoss, 3));
        table_->setItem(r, c++, num(x.trials, 0));
        QStringList notes;
        if (x.critical) notes << tr2("WS = kritis", "WS = critical");
        for (const auto& n : x.notes) notes << severityPrefix(n.severity) + " " + msgText(n);
        auto* ni = new QTableWidgetItem(notes.join("; "));
        if (!notes.isEmpty()) ni->setToolTip(notes.join("\n"));
        table_->setItem(r, c++, ni);
    }
    table_->resizeColumnsToContents();
}

void ResultsView::fillMessages() {
    msgs_->clear();
    if (!result_) {
        const QString err = s_->resultsError(planExt_);
        msgs_->addItem(err.isEmpty() ? tr2("Belum ada hasil untuk plan ini.", "No results for this plan yet.") : QStringLiteral("⛔ ") + err);
        return;
    }
    for (const auto& m : result_->messages) {
        auto* it = new QListWidgetItem(severityPrefix(m.severity) + QStringLiteral("  ") + msgText(m) + (m.location.empty() ? QString() : QStringLiteral("\n    ") + qs(m.location)));
        it->setForeground(m.severity == Severity::Error ? Theme::current().error : m.severity == Severity::Warning ? Theme::current().warning : Theme::current().text);
        msgs_->addItem(it);
    }
    for (const auto& pr : result_->profiles)
        for (const auto& x : pr.xs)
            for (const auto& n : x.notes)
                msgs_->addItem(severityPrefix(n.severity) + QStringLiteral("  ") + qs(pr.name) + QStringLiteral(" · RS ") + qs(x.rs) + QStringLiteral(": ") + msgText(n));
    if (msgs_->count() == 0) msgs_->addItem(tr2("Tidak ada pesan.", "No messages."));
}

void ResultsView::exportCsv() {
    if (!hasResults()) { sheetInfo(this, tr2("Ekspor CSV", "Export CSV"), tr2("Belum ada hasil untuk diekspor.", "No results to export.")); return; }
    auto* p = s_->project();
    const QString def = qs(io::fromPath(p->dir() / io::toPath(p->baseName() + "." + ss(planExt_) + ".results.csv")));
    const QString fn = QFileDialog::getSaveFileName(this, tr2("Ekspor hasil (CSV)", "Export results (CSV)"), def, "CSV (*.csv)");
    if (fn.isEmpty()) return;
    const std::string csv = app::resultsCsv(*result_, currentLanguage());
    std::string err;
    if (!io::writeFileAtomic(io::toPath(ss(fn)), csv, err)) {
        sheetInfo(this, tr2("Ekspor CSV", "Export CSV"), tr2("File gagal ditulis. Cek apakah folder bisa ditulisi dan file tidak sedang dibuka program lain.", "Could not write the file. Check folder permissions or whether another program has the file open."));
        return;
    }
    showToast(this, tr2("Hasil diekspor ke CSV", "Results exported to CSV"));
}

void ResultsView::exportReport() {
    if (!result_) { sheetInfo(this, tr2("Laporan", "Report"), tr2("Belum ada hasil untuk dilaporkan.", "No results to report.")); return; }
    auto* p = s_->project();
    const int fmtIdx = sheetChoice(this, tr2("Laporan run", "Run report"),
                                   tr2("Pilih format laporan. Isinya sama: ringkasan run, status validasi, pesan, dan tabel hasil.",
                                       "Choose the report format. The content is the same: run summary, validation status, messages and results table."),
                                   {tr2("Batal", "Cancel"), QStringLiteral("HTML"), QStringLiteral("Word (.docx)"), QStringLiteral("PDF")}, 3);
    if (fmtIdx <= 0) return;
    const char* ext = fmtIdx == 1 ? ".report.html" : fmtIdx == 2 ? ".report.docx" : ".report.pdf";
    const QString filter = fmtIdx == 1 ? QStringLiteral("HTML (*.html)") : fmtIdx == 2 ? QStringLiteral("Word (*.docx)") : QStringLiteral("PDF (*.pdf)");
    const QString def = qs(io::fromPath(p->dir() / io::toPath(p->baseName() + "." + ss(planExt_) + ext)));
    const QString fn = QFileDialog::getSaveFileName(this, tr2("Simpan laporan run", "Save run report"), def, filter);
    if (fn.isEmpty()) return;
    auto* pl = s_->currentPlan();
    const std::string planTitle = pl ? pl->model.title : std::string();
    std::string err;
    bool ok = false;
    if (fmtIdx == 1) {
        QFile lf(":/branding/De-RAS-64.png");
        std::string logo;
        if (lf.open(QIODevice::ReadOnly)) logo = "data:image/png;base64," + lf.readAll().toBase64().toStdString();
        ok = io::writeFileAtomic(io::toPath(ss(fn)), app::runReportHtml(*result_, currentLanguage(), p->info().title, planTitle, logo), err);
    } else {
        const auto blocks = app::runReportBlocks(*result_, currentLanguage(), p->info().title, planTitle);
        if (fmtIdx == 2) {
            ok = app::writeDocx(io::toPath(ss(fn)), blocks, err);
        } else {
            QTextDocument doc;
            doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("logo.png")), QImage(QStringLiteral(":/branding/De-RAS-64.png")));
            doc.setHtml(qs(app::reportBlocksHtml(blocks, "logo.png")));
            QPdfWriter pdf(fn);
            pdf.setPageSize(QPageSize(QPageSize::A4));
            pdf.setPageOrientation(QPageLayout::Landscape);
            pdf.setPageMargins(QMarginsF(14, 14, 14, 14), QPageLayout::Millimeter);
            pdf.setTitle(tr2("Laporan Run De-RAS", "De-RAS Run Report"));
            pdf.setCreator(QStringLiteral("De-RAS ") + QStringLiteral(DERAS_VERSION));
            doc.setPageSize(QSizeF(pdf.width(), pdf.height()));
            doc.print(&pdf);
            ok = QFileInfo(fn).size() > 0;
        }
    }
    if (!ok) {
        sheetInfo(this, tr2("Laporan", "Report"), tr2("Gagal menulis laporan.", "Could not write the report."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(fn));
}

}  // namespace deras::ui

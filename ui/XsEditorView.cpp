// De-RAS - ui/XsEditorView.cpp
#include "XsEditorView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <cmath>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "NumTable.h"
#include "PlotWidget.h"
#include "Widgets.h"
#include "XsUtil.h"

namespace deras::ui {

QDoubleSpinBox* XsEditorView::spin(double lo, double hi, int dec) {
    auto* sp = new QDoubleSpinBox;
    sp->setRange(lo, hi);
    sp->setDecimals(dec);
    sp->setKeyboardTracking(false);
    sp->setLocale(QLocale::c());
    sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sp->setAlignment(Qt::AlignRight);
    sp->setFont(monoFont());
    sp->setMinimumWidth(64);
    connect(sp, &QDoubleSpinBox::valueChanged, this, [this]() { commitFields(); });
    return sp;
}

XsEditorView::XsEditorView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- navigation row ----
    auto* nav = new QHBoxLayout;
    nav->setSpacing(8);
    nav->addWidget(makeMuted(tr2("Sungai / Reach", "River / Reach"), false));
    reach_ = new QComboBox;
    reach_->setMinimumWidth(220);
    reach_->setAccessibleName(tr2("Reach", "Reach"));
    nav->addWidget(reach_);
    nav->addSpacing(12);
    nav->addWidget(makeMuted(tr2("River station", "River station"), false));
    prev_ = new QPushButton;
    prev_->setIcon(lineIcon("back", Theme::current().text2));
    prev_->setToolTip(tr2("Penampang ke hulu (PgUp)", "Upstream cross section (PgUp)"));
    prev_->setShortcut(QKeySequence(Qt::Key_PageUp));
    station_ = new QComboBox;
    station_->setMinimumWidth(130);
    station_->setFont(monoFont());
    station_->setAccessibleName(tr2("River station", "River station"));
    next_ = new QPushButton;
    next_->setIcon(lineIcon("forward", Theme::current().text2));
    next_->setToolTip(tr2("Penampang ke hilir (PgDn)", "Downstream cross section (PgDn)"));
    next_->setShortcut(QKeySequence(Qt::Key_PageDown));
    nav->addWidget(prev_);
    nav->addWidget(station_);
    nav->addWidget(next_);
    nav->addWidget(new InfoButton("geometry.rs"));
    nav->addStretch();
    auto* add = makeButton(tr2("Tambah", "Add"), false, "plus");
    add->setToolTip(tr2("Tambah penampang kosong", "Add an empty cross section"));
    auto* copy = makeButton(tr2("Salin", "Copy"), false, "copy");
    copy->setToolTip(tr2("Salin penampang ini dengan RS baru", "Copy this cross section with a new RS"));
    auto* del = makeButton(tr2("Hapus", "Delete"), false, "trash");
    auto* imp = makeButton(tr2("Impor survei CSV…", "Import survey CSV…"), false, "import");
    nav->addWidget(add);
    nav->addWidget(copy);
    nav->addWidget(del);
    nav->addWidget(withInfo(imp, "geometry.import"));
    root->addLayout(nav);

    // ---- body ----
    stack_ = new QStackedWidget;
    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);

    auto* tCard = new Card(tr2("Stasiun–elevasi", "Station–elevation"), "geometry.staelev");
    tCard->setFixedWidth(270);
    table_ = new NumTable({tr2("Stasiun", "Station"), tr2("Elevasi", "Elevation")}, {"geometry.col.station", "geometry.col.elevation"});
    table_->setAccessibleName(tr2("Tabel stasiun–elevasi", "Station–elevation table"));
    tCard->body()->addWidget(table_, 1);
    tableStatus_ = makeMuted(tr2("Kiri → kanan, menghadap hilir. Tempel dari Excel dengan Ctrl+V.", "Left → right, looking downstream. Paste from Excel with Ctrl+V."));
    tCard->body()->addWidget(tableStatus_);
    row->addWidget(tCard);

    auto* pCard = new Card(tr2("Grafik penampang", "Cross-section plot"), "geometry.plot");
    plot_ = new PlotWidget;
    pCard->body()->addWidget(plot_, 1);
    plotNote_ = makeMuted(QString());
    pCard->body()->addWidget(plotNote_);
    row->addWidget(pCard, 1);

    auto* dCard = new Card(tr2("Data penampang", "Cross-section data"), "geometry.xsdata");
    dCard->setFixedWidth(320);
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(6);
    g->setVerticalSpacing(8);
    rs_ = new QLineEdit;
    rs_->setFont(monoFont());
    rs_->setAccessibleName(tr2("River station", "River station"));
    desc_ = new QLineEdit;
    desc_->setAccessibleName(tr2("Deskripsi", "Description"));
    lenL_ = spin(0, 1e7, 2);
    lenC_ = spin(0, 1e7, 2);
    lenR_ = spin(0, 1e7, 2);
    bankL_ = spin(-1e7, 1e7, 2);
    bankR_ = spin(-1e7, 1e7, 2);
    nL_ = spin(0.001, 1, 3);
    nC_ = spin(0.001, 1, 3);
    nR_ = spin(0.001, 1, 3);
    contr_ = spin(0, 1, 2);
    expan_ = spin(0, 1, 2);
    auto label = [](const QString& t, const QString& topic) { return labelWithInfo(t, topic); };
    auto three = [](QWidget* a, QWidget* b, QWidget* c) {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(a);
        h->addWidget(b);
        if (c) h->addWidget(c);
        return w;
    };
    int r = 0;
    g->addWidget(label("RS", "geometry.rs"), r, 0);
    g->addWidget(rs_, r++, 1);
    g->addWidget(label(tr2("Deskripsi", "Description"), "geometry.description"), r, 0);
    g->addWidget(desc_, r++, 1);
    g->addWidget(makeCaption(tr2("Jarak ke hilir  LOB · Alur · ROB", "Downstream length  LOB · Chan · ROB")), r++, 0, 1, 2);
    g->addWidget(three(lenL_, lenC_, lenR_), r, 0, 1, 2);
    g->addWidget(new InfoButton("geometry.lengths"), r++, 2);
    g->addWidget(makeCaption(tr2("Bank station  kiri · kanan", "Bank stations  left · right")), r++, 0, 1, 2);
    g->addWidget(three(bankL_, bankR_, nullptr), r, 0, 1, 2);
    g->addWidget(new InfoButton("geometry.banks"), r++, 2);
    g->addWidget(makeCaption(tr2("Manning n  LOB · Alur · ROB", "Manning's n  LOB · Chan · ROB")), r++, 0, 1, 2);
    nRow_ = three(nL_, nC_, nR_);
    nVaries_ = makeMuted(QString());
    auto* nWrap = new QWidget;
    auto* nv = new QVBoxLayout(nWrap);
    nv->setContentsMargins(0, 0, 0, 0);
    nv->setSpacing(4);
    nv->addWidget(nRow_);
    nv->addWidget(nVaries_);
    auto* nTable = makeButton(tr2("Tabel titik n…", "n breakpoints…"));
    nTable->setObjectName("Link");
    nv->addWidget(nTable, 0, Qt::AlignLeft);
    g->addWidget(nWrap, r, 0, 1, 2);
    g->addWidget(new InfoButton("geometry.manning"), r++, 2, Qt::AlignTop);
    g->addWidget(makeCaption(tr2("Koef.  kontraksi · ekspansi", "Coef.  contraction · expansion")), r++, 0, 1, 2);
    g->addWidget(three(contr_, expan_, nullptr), r, 0, 1, 2);
    g->addWidget(new InfoButton("geometry.coefficients"), r++, 2);
    g->setColumnStretch(1, 1);
    dCard->body()->addLayout(g);
    units_ = makeMuted(QString());
    dCard->body()->addWidget(units_);
    dCard->body()->addSpacing(4);
    dCard->body()->addWidget(makeCaption(tr2("Data tambahan", "Extra data")));
    auto* chips = new QGridLayout;
    chips->setSpacing(6);
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton;
        b->setObjectName("Chip");
        connect(b, &QPushButton::clicked, this, [this, i]() { extraSheet(i); });
        extra_[i] = b;
        chips->addWidget(b, i / 2, i % 2);
    }
    {
        auto* b = new QPushButton(tr2("n vertikal", "Vertical n"));
        b->setObjectName("Chip");
        b->setIcon(lineIcon("lock", Theme::current().text3));
        b->setToolTip(lockedTooltip("nvertical"));
        b->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(Theme::current().text3)));
        connect(b, &QPushButton::clicked, this, [this]() { emit requestLocked("nvertical"); });
        chips->addWidget(b, 1, 1);
    }
    dCard->body()->addLayout(chips);
    dCard->body()->addStretch();
    row->addWidget(dCard);
    stack_->addWidget(page);

    nodeInfo_ = new QLabel;
    nodeInfo_->setWordWrap(true);
    nodeInfo_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    nodeInfo_->setMargin(24);
    auto* infoCard = new Card;
    infoCard->body()->addWidget(nodeInfo_);
    infoCard->body()->addStretch();
    stack_->addWidget(infoCard);
    auto* emptyCard = new Card;
    auto* empty = makeMuted(tr2("Belum ada penampang di reach ini. Tekan “Tambah” atau impor survei CSV.", "No cross section in this reach yet. Press “Add” or import a survey CSV."));
    empty->setAlignment(Qt::AlignCenter);
    emptyCard->body()->addWidget(empty, 1);
    stack_->addWidget(emptyCard);
    root->addWidget(stack_, 1);

    connect(reach_, &QComboBox::activated, this, [this]() {
        curReach_ = reach_->currentData().toInt();
        curXs_ = 0;
        fillStations();
    });
    connect(station_, &QComboBox::activated, this, [this]() {
        curXs_ = station_->currentData().toInt();
        s_->selectCrossSection(curReach_, curXs_);
    });
    connect(prev_, &QPushButton::clicked, this, [this]() { step(-1); });
    connect(next_, &QPushButton::clicked, this, [this]() { step(1); });
    connect(add, &QPushButton::clicked, this, [this]() { addXs(false); });
    connect(copy, &QPushButton::clicked, this, [this]() { addXs(true); });
    connect(del, &QPushButton::clicked, this, &XsEditorView::deleteXs);
    connect(imp, &QPushButton::clicked, this, &XsEditorView::importSurvey);
    connect(nTable, &QPushButton::clicked, this, &XsEditorView::manningSheet);
    connect(table_, &NumTable::edited, this, &XsEditorView::commitTable);
    connect(rs_, &QLineEdit::editingFinished, this, &XsEditorView::commitFields);
    connect(desc_, &QLineEdit::editingFinished, this, &XsEditorView::commitFields);
    connect(s_, &ProjectSession::crossSectionSelected, this, [this](int rr, int xx) {
        if (loading_) return;
        if (rr == curReach_ && xx == curXs_) return;
        curReach_ = rr;
        curXs_ = xx;
        if (isVisible()) fillReaches();
    });
    connect(s_, &ProjectSession::resultsUpdated, this, [this]() { if (isVisible()) updatePlot(); });
    reload();
}

QString XsEditorView::title() const {
    if (CrossSection* x = const_cast<XsEditorView*>(this)->xs()) return tr2("Penampang RS %1", "Cross Section RS %1").arg(qs(x->rs));
    return tr2("Editor Penampang", "Cross-Section Editor");
}

QList<QPair<QString, QString>> XsEditorView::crumbs() const {
    return {{tr2("Beranda", "Home"), "home"}, {tr2("Data Geometri", "Geometric Data"), "geometry"}};
}

QWidget* XsEditorView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* back = makeButton(tr2("Skema geometri", "Geometry schematic"), false, "geometry");
    auto* save = makeButton(tr2("Simpan geometri", "Save geometry"), true, "save");
    h->addWidget(back);
    h->addWidget(save);
    connect(back, &QPushButton::clicked, this, [this]() { emit navigate("geometry"); });
    connect(save, &QPushButton::clicked, this, &View::requestSave);
    return w;
}

void XsEditorView::reload() {
    curReach_ = s_->selectedReach();
    curXs_ = s_->selectedXs();
    fillReaches();
}

void XsEditorView::activated() {
    curReach_ = s_->selectedReach();
    curXs_ = s_->selectedXs();
    fillReaches();
}

Reach* XsEditorView::reach() {
    auto* g = s_->currentGeometry();
    if (!g) return nullptr;
    for (auto& r : g->model.reaches) if (r.uid == curReach_) return &r;
    return nullptr;
}

CrossSection* XsEditorView::xs() {
    Reach* r = reach();
    if (!r) return nullptr;
    for (auto& n : r->nodes) if (n.uid == curXs_) return &n;
    return nullptr;
}

void XsEditorView::fillReaches() {
    loading_ = true;
    reach_->clear();
    if (auto* g = s_->currentGeometry())
        for (const auto& r : g->model.reaches) reach_->addItem(qs(r.label()), r.uid);
    int i = reach_->findData(curReach_);
    if (i < 0 && reach_->count()) i = 0;
    reach_->setCurrentIndex(i);
    curReach_ = i >= 0 ? reach_->itemData(i).toInt() : 0;
    loading_ = false;
    fillStations();
}

void XsEditorView::fillStations() {
    loading_ = true;
    station_->clear();
    if (Reach* r = reach())
        for (const auto& n : r->nodes) {
            QString t = qs(n.rs);
            if (n.type != NodeType::CrossSection) t += QStringLiteral("  [%1]").arg(qs(nodeTypeName(n.type)));
            station_->addItem(t, n.uid);
        }
    int i = station_->findData(curXs_);
    if (i < 0) {  // prefer the first real cross section
        for (int k = 0; k < station_->count(); ++k)
            if (!station_->itemText(k).contains('[')) { i = k; break; }
        if (i < 0 && station_->count()) i = 0;
    }
    station_->setCurrentIndex(i);
    curXs_ = i >= 0 ? station_->itemData(i).toInt() : 0;
    loading_ = false;
    if (curXs_ && (s_->selectedReach() != curReach_ || s_->selectedXs() != curXs_)) {
        loading_ = true;
        s_->selectCrossSection(curReach_, curXs_);
        loading_ = false;
    }
    prev_->setEnabled(i > 0);
    next_->setEnabled(i >= 0 && i < station_->count() - 1);
    showXs();
    emit headerChanged();
}

void XsEditorView::step(int delta) {
    const int i = station_->currentIndex() + delta;
    if (i < 0 || i >= station_->count()) return;
    curXs_ = station_->itemData(i).toInt();
    fillStations();
}

void XsEditorView::showXs() {
    CrossSection* x = xs();
    const UnitSystem u = s_->project() ? s_->project()->units() : UnitSystem::SI;
    if (!x) { stack_->setCurrentIndex(2); return; }
    if (x->type != NodeType::CrossSection) {
        nodeInfo_->setText(tr2("<b>%1 pada RS %2</b><br><br>Node ini dibaca dan disimpan kembali apa adanya (tanpa kehilangan data), "
                               "tetapi belum bisa diedit atau dihitung oleh De-RAS (struktur hidraulika: M4). "
                               "Selama node ini ada, Model Doctor menolak menjalankan plan agar tidak menghasilkan hasil yang salah.",
                               "<b>%1 at RS %2</b><br><br>This node is read and written back unchanged (no data loss), "
                               "but De-RAS cannot edit or compute it yet (hydraulic structures: M4). "
                               "While this node exists, Model Doctor refuses to run the plan so no wrong results are produced.")
                               .arg(qs(nodeTypeName(x->type)), qs(x->rs)));
        stack_->setCurrentIndex(1);
        return;
    }
    stack_->setCurrentIndex(0);
    loading_ = true;
    rs_->setText(qs(x->rs));
    desc_->setText(qs(x->description));
    lenL_->setValue(x->lenLeft);
    lenC_->setValue(x->lenChannel);
    lenR_->setValue(x->lenRight);
    bankL_->setValue(x->bankLeft.value_or(0));
    bankR_->setValue(x->bankRight.value_or(0));
    contr_->setValue(x->contraction);
    expan_->setValue(x->expansion);
    const ThreeN n = threeN(*x);
    const bool simple = n.simple || x->manning.empty();
    nRow_->setEnabled(simple);
    nL_->setValue(std::isfinite(n.lob) ? n.lob : 0.035);
    nC_->setValue(std::isfinite(n.ch) ? n.ch : 0.035);
    nR_->setValue(std::isfinite(n.rob) ? n.rob : 0.035);
    nVaries_->setText(simple ? (x->manning.empty() ? tr2("Belum ada nilai n: isi tiga nilai di atas.", "No n values yet: fill in the three values above.") : QString())
                             : tr2("n bervariasi (%1 titik). Ubah lewat “Tabel titik n…”.", "n varies (%1 breakpoints). Edit via “n breakpoints…”.").arg(x->manning.size()));
    nVaries_->setVisible(!nVaries_->text().isEmpty());
    std::vector<std::vector<double>> se;
    for (const auto& p : x->staElev) se.push_back({p.station, p.elev});
    table_->setValues(se);
    loading_ = false;
    units_->setText(tr2("Satuan panjang dan elevasi: %1", "Length and elevation unit: %1").arg(qs(lengthUnit(u))));
    updateExtraChips();
    if (!x->ignoredFeatures.empty()) {
        QStringList f;
        for (const auto& s : x->ignoredFeatures) f << qs(s);
        tableStatus_->setText(QStringLiteral("⚠ ") + tr2("Tersimpan di file, tetapi belum ikut dihitung: ", "Kept as-is, not used by the solver yet: ") + f.join(", "));
    } else {
        tableStatus_->setText(tr2("Kiri → kanan, menghadap hilir. Tempel dari Excel dengan Ctrl+V.", "Left → right, looking downstream. Paste from Excel with Ctrl+V."));
    }
    updatePlot();
}

void XsEditorView::commitFields() {
    if (loading_) return;
    CrossSection* x = xs();
    if (!x || x->type != NodeType::CrossSection) return;
    const QString newRs = rs_->text().trimmed();
    bool rsChanged = false;
    if (!newRs.isEmpty() && ss(newRs) != x->rs) {
        x->rs = ss(newRs);
        rsChanged = true;
    }
    x->description = ss(desc_->text());
    x->lenLeft = lenL_->value();
    x->lenChannel = lenC_->value();
    x->lenRight = lenR_->value();
    x->bankLeft = bankL_->value();
    x->bankRight = bankR_->value();
    x->contraction = contr_->value();
    x->expansion = expan_->value();
    if (nRow_->isEnabled()) setThreeN(*x, nL_->value(), nC_->value(), nR_->value());
    s_->markEdited();
    if (rsChanged) {
        const int i = station_->currentIndex();
        if (i >= 0) station_->setItemText(i, newRs);
        emit headerChanged();
    }
    updatePlot();
}

void XsEditorView::commitTable() {
    if (loading_) return;
    CrossSection* x = xs();
    if (!x) return;
    std::vector<std::vector<double>> se;
    int bad = 0;
    table_->values(se, &bad);
    if (bad) {
        tableStatus_->setText(QStringLiteral("⛔ ") + tr2("%1 sel bukan angka valid (ditandai merah). Baris itu tidak disimpan sampai diperbaiki.",
                                                          "%1 cell(s) are not valid numbers (marked red). Those rows are not stored until fixed.").arg(bad));
    } else {
        tableStatus_->setText(tr2("%1 titik.", "%1 points.").arg(se.size()));
    }
    x->staElev.clear();
    for (const auto& r : se) x->staElev.push_back({r[0], r[1]});
    // first-time n: give the section the three values shown in the card
    if (x->manning.empty() && !x->staElev.empty()) setThreeN(*x, nL_->value(), nC_->value(), nR_->value());
    s_->markEdited();
    updatePlot();
}

void XsEditorView::updatePlot() {
    CrossSection* x = xs();
    if (!x || !s_->project()) { plot_->clear(); plotNote_->clear(); return; }
    const Theme& th = Theme::current();
    const QString lu = qs(lengthUnit(s_->project()->units()));
    QVector<PlotSeries> series;
    PlotSeries ground;
    ground.name = tr2("Tanah", "Ground");
    ground.color = th.ground;
    ground.width = 2.2;
    ground.markers = true;
    for (const auto& p : x->staElev) ground.pts << QPointF(p.station, p.elev);
    series << ground;
    // computed water surface of the current plan (if the plan has results)
    QString note;
    Reach* r = reach();
    const QString plan = s_->currentPlanExt();
    if (r && !plan.isEmpty() && !x->staElev.empty()) {
        if (const RunResult* res = s_->results(plan); res && res->status == RunStatus::Succeeded) {
            const double s0 = x->staElev.front().station, s1 = x->staElev.back().station;
            int shown = 0;
            for (size_t k = 0; k < res->profiles.size() && shown < 4; ++k)
                for (const auto& xr : res->profiles[k].xs)
                    if (xr.river == r->river && xr.reach == r->reach && xr.rs == x->rs) {
                        PlotSeries ws;
                        ws.name = QStringLiteral("WS %1").arg(qs(res->profiles[k].name));
                        QColor c = th.water;
                        c = c.lighter(100 + 18 * shown);
                        ws.color = c;
                        ws.width = 1.6;
                        ws.pts = {QPointF(s0, xr.ws), QPointF(s1, xr.ws)};
                        ws.fillBelowSeries = 0;
                        ws.fillColor = withAlpha(th.water, shown == 0 ? 0.28 : 0.10);
                        series << ws;
                        ++shown;
                    }
            if (shown) note = tr2("Muka air: hasil plan %1 (%2). Kalau geometri diubah setelah run, hitung ulang dulu.",
                                  "Water surface: plan %1 results (%2). If the geometry changed after the run, these results are stale — run again.")
                                  .arg(plan, qs(originLabel(res->origin, isID() ? Language::ID : Language::EN)));
        }
    }
    // ineffective areas (dashed box), blocked obstructions (filled) and levees
    auto groundAt = [&](double st) {
        const auto& p = x->staElev;
        if (p.empty()) return 0.0;
        if (st <= p.front().station) return p.front().elev;
        for (size_t i = 0; i + 1 < p.size(); ++i)
            if (st <= p[i + 1].station && p[i + 1].station > p[i].station)
                return p[i].elev + (p[i + 1].elev - p[i].elev) * (st - p[i].station) / (p[i + 1].station - p[i].station);
        return p.back().elev;
    };
    bool first = true;
    for (const auto& b : x->ineffective) {
        PlotSeries s;
        s.name = tr2("Ineffective", "Ineffective");
        s.legend = first;
        first = false;
        s.color = th.warning;
        s.width = 1.4;
        s.style = Qt::DashLine;
        s.pts = {QPointF(b.staStart, groundAt(b.staStart)), QPointF(b.staStart, b.elev), QPointF(b.staEnd, b.elev), QPointF(b.staEnd, groundAt(b.staEnd))};
        series << s;
    }
    first = true;
    for (const auto& b : x->obstructions) {
        PlotSeries s;
        s.name = tr2("Obstruction", "Obstruction");
        s.legend = first;
        first = false;
        s.color = th.text2;
        s.width = 1.4;
        s.pts = {QPointF(b.staStart, b.elev), QPointF(b.staEnd, b.elev)};
        s.fillBelowSeries = 0;
        s.fillColor = withAlpha(th.text2, 0.45);
        series << s;
    }
    first = true;
    for (const LeveePoint* lv : {&x->leveeLeft, &x->leveeRight}) {
        if (!lv->present) continue;
        PlotSeries s;
        s.name = tr2("Levee", "Levee");
        s.legend = first;
        first = false;
        s.color = th.success;
        s.width = 3;
        s.markers = true;
        s.pts = {QPointF(lv->station, groundAt(lv->station)), QPointF(lv->station, lv->elev)};
        series << s;
    }
    QVector<PlotMarker> marks;
    if (x->bankLeft) marks.push_back({*x->bankLeft, tr2("Tebing kiri", "Left bank"), th.error});
    if (x->bankRight) marks.push_back({*x->bankRight, tr2("Tebing kanan", "Right bank"), th.error});
    plot_->setTitles(QStringLiteral("RS %1").arg(qs(x->rs)), tr2("Stasiun (%1)", "Station (%1)").arg(lu), tr2("Elevasi (%1)", "Elevation (%1)").arg(lu));
    plot_->setSeries(series);
    plot_->setMarkers(marks);
    plotNote_->setText(note);
    plotNote_->setVisible(!note.isEmpty());
}

void XsEditorView::addXs(bool copy) {
    Reach* r = reach();
    if (!r) { showToast(this, tr2("Tambahkan reach dulu di Data Geometri.", "Add a reach first in Geometric Data."), "warning"); return; }
    QString rs;
    if (!sheetGetText(this, copy ? tr2("Salin penampang", "Copy cross section") : tr2("Penampang baru", "New cross section"),
                      tr2("River station (RS) penampang baru:", "River station (RS) of the new cross section:"), rs, 8, "geometry.rs",
                      tr2("RS yang lebih besar berarti lebih ke hulu. Penampang otomatis diletakkan sesuai urutan RS.", "Larger RS = further upstream. The section is inserted in RS order.")))
        return;
    rs = rs.trimmed();
    if (rs.isEmpty()) return;
    for (const auto& n : r->nodes)
        if (n.rs == ss(rs)) { showToast(this, tr2("RS %1 sudah ada di reach ini.", "RS %1 already exists in this reach.").arg(rs), "warning"); return; }
    CrossSection n;
    if (copy && xs() && xs()->type == NodeType::CrossSection) n = *xs();
    n.uid = nextUid();
    n.type = NodeType::CrossSection;
    n.rs = ss(rs);
    n.ignoredFeatures.clear();
    const double v = n.rsValue();
    size_t pos = r->nodes.size();
    for (size_t i = 0; i < r->nodes.size(); ++i)
        if (std::isfinite(v) && std::isfinite(r->nodes[i].rsValue()) && r->nodes[i].rsValue() < v) { pos = i; break; }
    r->nodes.insert(r->nodes.begin() + static_cast<long>(pos), n);
    curXs_ = n.uid;
    s_->markEdited();
    fillStations();
    s_->selectCrossSection(curReach_, curXs_);
}

void XsEditorView::deleteXs() {
    Reach* r = reach();
    CrossSection* x = xs();
    if (!r || !x) return;
    if (x->type != NodeType::CrossSection) {
        sheetInfo(this, tr2("Hapus struktur", "Delete structure"), tr2("Struktur belum bisa dihapus di De-RAS (menyusul di M4).", "Structures cannot be deleted in De-RAS yet (M4)."));
        return;
    }
    if (!sheetConfirm(this, tr2("Hapus penampang", "Delete cross section"),
                      tr2("Hapus penampang RS %1? File baru berubah setelah Anda menyimpan.", "Delete cross section RS %1? The change is written to the file only when you save.").arg(qs(x->rs)),
                      tr2("Hapus", "Delete"), true))
        return;
    int idx = 0;
    for (size_t i = 0; i < r->nodes.size(); ++i)
        if (r->nodes[i].uid == x->uid) { idx = static_cast<int>(i); r->nodes.erase(r->nodes.begin() + static_cast<long>(i)); break; }
    curXs_ = r->nodes.empty() ? 0 : r->nodes[static_cast<size_t>(std::min<int>(idx, static_cast<int>(r->nodes.size()) - 1))].uid;
    s_->markEdited();
    fillStations();
}

void XsEditorView::importSurvey() {
    CrossSection* x = xs();
    if (!x || x->type != NodeType::CrossSection) {
        showToast(this, tr2("Pilih penampang tujuan dulu.", "Select the target cross section first."), "warning");
        return;
    }
    const QString fn = QFileDialog::getOpenFileName(this, tr2("Impor survei penampang", "Import cross-section survey"), QString(), tr2("CSV/teks (*.csv *.txt)", "CSV/text (*.csv *.txt)"));
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sheetInfo(this, tr2("Impor survei", "Survey import"), tr2("File tidak bisa dibuka: %1", "The file cannot be opened: %1").arg(fn));
        return;
    }
    QTextStream in(&f);
    std::vector<StaElev> pts;
    int skipped = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList c = line.split(QRegularExpression(line.contains(';') ? ";" : (line.contains('\t') ? "\t" : ",")));
        if (c.size() < 2) { ++skipped; continue; }
        auto a = parseUserNumber(c[0]), b = parseUserNumber(c[1]);
        if (!a || !b) { ++skipped; continue; }
        pts.push_back({*a, *b});
    }
    if (pts.size() < 3) {
        sheetInfo(this, tr2("Impor survei", "Survey import"), tr2("File berisi kurang dari 3 titik stasiun–elevasi yang valid.", "The file contains fewer than 3 valid station–elevation points."));
        return;
    }
    if (!x->staElev.empty() &&
        !sheetConfirm(this, tr2("Impor survei", "Survey import"),
                      tr2("Ganti %1 titik penampang RS %2 dengan %3 titik dari file?", "Replace the %1 points of RS %2 with %3 points from the file?").arg(x->staElev.size()).arg(qs(x->rs)).arg(pts.size()),
                      tr2("Ganti", "Replace")))
        return;
    x->staElev = pts;
    s_->markEdited();
    showXs();
    emit s_->logMessage(tr2("Impor survei: %1 titik dari %2 (%3 baris dilewati karena judul atau tidak valid).", "Survey import: %1 points from %2 (%3 lines skipped: header/invalid).")
                            .arg(pts.size()).arg(fn).arg(skipped));
    showToast(this, tr2("%1 titik diimpor", "%1 points imported").arg(pts.size()));
}

void XsEditorView::manningSheet() {
    CrossSection* x = xs();
    if (!x || x->type != NodeType::CrossSection) return;
    Sheet sh(this, tr2("Titik perubahan Manning n — RS %1", "Manning's n breakpoints — RS %1").arg(qs(x->rs)), "geometry.manning", 460);
    auto* t = new NumTable({tr2("Stasiun mulai", "Start station"), tr2("Manning n", "Manning's n")}, {"geometry.col.nstation", "geometry.col.nvalue"});
    std::vector<std::vector<double>> mn;
    for (const auto& m : x->manning) mn.push_back({m.station, m.n});
    t->setValues(mn);
    t->setMinimumHeight(260);
    sh.body()->addWidget(t);
    sh.body()->addWidget(makeMuted(tr2("Nilai n berlaku dari stasiun ini sampai titik berikutnya (format HEC-RAS horizontal variation).",
                                       "Each n applies from its station to the next breakpoint (HEC-RAS horizontal variation format).")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    std::vector<std::vector<double>> rows;
    sh.setAcceptCheck([&](int) {
        int bad = 0;
        t->values(rows, &bad);
        if (bad) { sh.setError(tr2("%1 sel tidak valid (ditandai merah).", "%1 invalid cell(s) (marked red).").arg(bad)); return false; }
        for (const auto& rr : rows)
            if (rr[1] <= 0 || rr[1] > 1) { sh.setError(tr2("Nilai n harus di antara 0 dan 1.", "n values must be between 0 and 1.")); return false; }
        return true;
    });
    if (sh.exec() != 1) return;
    x->manning.clear();
    for (const auto& rr : rows) x->manning.push_back({rr[0], rr[1]});
    s_->markEdited();
    showXs();
}

void XsEditorView::updateExtraChips() {
    CrossSection* x = xs();
    const int n[3] = {x ? static_cast<int>(x->ineffective.size()) : 0, x ? static_cast<int>(x->obstructions.size()) : 0,
                      x ? (x->leveeLeft.present ? 1 : 0) + (x->leveeRight.present ? 1 : 0) : 0};
    const QString name[3] = {tr2("Ineffective", "Ineffective"), tr2("Obstruction", "Obstructions"), tr2("Levee", "Levees")};
    for (int i = 0; i < 3; ++i) {
        if (!extra_[i]) continue;
        extra_[i]->setText(n[i] ? QStringLiteral("%1 · %2").arg(name[i]).arg(n[i]) : name[i]);
        extra_[i]->setToolTip(tr2("Ubah data ini. Dihitung oleh solver (status Pratinjau: belum dibandingkan dengan HEC-RAS).",
                                  "Edit this data. Used by the solver (Preview status: not compared with HEC-RAS yet)."));
        extra_[i]->setEnabled(x != nullptr);
    }
}

void XsEditorView::extraSheet(int kind) {
    CrossSection* x = xs();
    if (!x || x->type != NodeType::CrossSection) return;
    const QString lu = s_->project() ? qs(lengthUnit(s_->project()->units())) : QString();
    const QString titles[3] = {tr2("Ineffective flow area — RS %1", "Ineffective flow areas — RS %1"),
                               tr2("Blocked obstruction — RS %1", "Blocked obstructions — RS %1"), tr2("Levee — RS %1", "Levees — RS %1")};
    const char* topics[3] = {"geometry.ineffective", "geometry.obstruction", "geometry.levee"};
    Sheet sh(this, titles[kind].arg(qs(x->rs)), topics[kind], 560);
    const double s0 = x->staElev.empty() ? 0 : x->staElev.front().station, s1 = x->staElev.empty() ? 0 : x->staElev.back().station;
    NumTable* t = nullptr;
    QCheckBox* has[2] = {nullptr, nullptr};
    QDoubleSpinBox *sta[2] = {nullptr, nullptr}, *el[2] = {nullptr, nullptr};
    if (kind < 2) {
        QStringList cols{tr2("Stasiun awal (%1)", "Start station (%1)").arg(lu), tr2("Stasiun akhir (%1)", "End station (%1)").arg(lu), tr2("Elevasi (%1)", "Elevation (%1)").arg(lu)};
        QStringList tp{"geometry.col.blockstation", "geometry.col.blockstation", "geometry.col.blockelev"};
        if (kind == 0) { cols << tr2("Permanen (1 = ya)", "Permanent (1 = yes)"); tp << "geometry.col.permanent"; }
        t = new NumTable(cols, tp);
        std::vector<std::vector<double>> v;
        for (const auto& b : kind == 0 ? x->ineffective : x->obstructions) {
            std::vector<double> row{b.staStart, b.staEnd, b.elev};
            if (kind == 0) row.push_back(b.permanent ? 1 : 0);
            v.push_back(row);
        }
        t->setValues(v);
        t->setMinimumHeight(220);
        sh.body()->addWidget(t);
        sh.body()->addWidget(makeMuted(kind == 0
            ? tr2("Air di rentang stasiun ini tidak ikut mengalir selama muka air tidak lebih tinggi dari elevasinya. Yang permanen tidak pernah ikut mengalir.",
                  "Water inside the station range does not convey flow while the water surface is at or below the elevation. Permanent areas never convey flow.")
            : tr2("Bagian penampang di rentang stasiun ini, sampai elevasi yang diisi, dianggap padat (luas hilang, keliling basah bertambah).",
                  "The part of the section inside the station range, up to the given elevation, is treated as solid (area removed, wetted perimeter added).")));
    } else {
        auto* g = new QGridLayout;
        const LeveePoint* lv[2] = {&x->leveeLeft, &x->leveeRight};
        for (int i = 0; i < 2; ++i) {
            has[i] = new QCheckBox(i == 0 ? tr2("Levee kiri", "Left levee") : tr2("Levee kanan", "Right levee"));
            has[i]->setChecked(lv[i]->present);
            sta[i] = spin(-1e7, 1e7, 3);
            el[i] = spin(-1e5, 1e5, 3);
            sta[i]->setValue(lv[i]->present ? lv[i]->station : (i == 0 ? x->bankLeft.value_or(s0) : x->bankRight.value_or(s1)));
            el[i]->setValue(lv[i]->present ? lv[i]->elev : (x->staElev.empty() ? 0 : x->minElevation() + 1));
            g->addWidget(has[i], i, 0);
            g->addWidget(new QLabel(tr2("Stasiun (%1)", "Station (%1)").arg(lu)), i, 1);
            g->addWidget(sta[i], i, 2);
            g->addWidget(new QLabel(tr2("Elevasi (%1)", "Elevation (%1)").arg(lu)), i, 3);
            g->addWidget(el[i], i, 4);
        }
        sh.body()->addLayout(g);
        sh.body()->addWidget(makeMuted(tr2("Air di luar levee (kiri dari levee kiri, kanan dari levee kanan) tidak ikut mengalir sampai muka air melewati elevasi levee.",
                                           "Water beyond a levee (left of the left levee, right of the right levee) does not flow until the water surface is above the levee elevation.")));
    }
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    std::vector<std::vector<double>> rows;
    sh.setAcceptCheck([&](int) {
        if (t) {
            int bad = 0;
            t->values(rows, &bad);
            if (bad) { sh.setError(tr2("%1 sel tidak valid (ditandai merah).", "%1 invalid cell(s) (marked red).").arg(bad)); return false; }
            for (const auto& r : rows) {
                if (r[1] <= r[0]) { sh.setError(tr2("Stasiun akhir harus lebih besar dari stasiun awal.", "The end station must be larger than the start station.")); return false; }
                if (r[0] < s0 - 1e-9 || r[1] > s1 + 1e-9) { sh.setError(tr2("Rentang stasiun harus di dalam penampang (%1 – %2).", "Station ranges must lie inside the section (%1 – %2).").arg(s0).arg(s1)); return false; }
                if (kind == 0 && r[3] != 0 && r[3] != 1) { sh.setError(tr2("Kolom Permanen diisi 0 atau 1.", "The Permanent column takes 0 or 1.")); return false; }
            }
        } else {
            for (int i = 0; i < 2; ++i)
                if (has[i]->isChecked() && (sta[i]->value() < s0 || sta[i]->value() > s1)) {
                    sh.setError(tr2("Stasiun levee harus di dalam penampang (%1 – %2).", "Levee stations must lie inside the section (%1 – %2).").arg(s0).arg(s1));
                    return false;
                }
        }
        return true;
    });
    if (sh.exec() != 1) return;
    if (kind < 2) {
        std::vector<StationBlock> v;
        for (const auto& r : rows) v.push_back({r[0], r[1], r[2], kind == 0 && r[3] == 1});
        if (kind == 0) { if (x->ineffective.empty() && !v.empty()) x->ineffectiveMode = -1; x->ineffective = v; }
        else { if (x->obstructions.empty() && !v.empty()) x->obstructionMode = -1; x->obstructions = v; }
    } else {
        LeveePoint* lv[2] = {&x->leveeLeft, &x->leveeRight};
        for (int i = 0; i < 2; ++i) *lv[i] = has[i]->isChecked() ? LeveePoint{true, sta[i]->value(), el[i]->value()} : LeveePoint{};
    }
    s_->markEdited();
    showXs();
}

}  // namespace deras::ui

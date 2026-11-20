// De-RAS - ui/UnsteadyFlowView.cpp
#include "UnsteadyFlowView.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "Common.h"
#include "Help.h"
#include "Icons.h"
#include "NumTable.h"
#include "PlotWidget.h"
#include "Theme.h"
#include "Widgets.h"
#include "io/RasText.h"

namespace deras::ui {

namespace {
const char* kIntervals[] = {"1MIN", "2MIN", "3MIN", "5MIN", "10MIN", "15MIN", "20MIN", "30MIN", "1HOUR", "2HOUR", "3HOUR", "4HOUR", "6HOUR", "8HOUR", "12HOUR", "1DAY"};

QString uTypeName(UBoundaryType t) {
    switch (t) {
        case UBoundaryType::FlowHydrograph: return tr2("Hidrograf debit", "Flow hydrograph");
        case UBoundaryType::StageHydrograph: return tr2("Hidrograf muka air", "Stage hydrograph");
        case UBoundaryType::LateralInflow: return tr2("Aliran lateral", "Lateral inflow hydrograph");
        case UBoundaryType::UniformLateralInflow: return tr2("Aliran lateral merata", "Uniform lateral inflow hydrograph");
        case UBoundaryType::NormalDepth: return tr2("Normal depth", "Normal depth");
        case UBoundaryType::RatingCurve: return tr2("Rating curve", "Rating curve");
        case UBoundaryType::Precipitation: return tr2("Hujan (presipitasi)", "Precipitation");
        case UBoundaryType::Other: return tr2("Lainnya (disimpan, tidak dihitung)", "Other (kept, not computed)");
        default: return tr2("(belum diisi)", "(not set)");
    }
}

bool isLateral(UBoundaryType t) { return t == UBoundaryType::LateralInflow || t == UBoundaryType::UniformLateralInflow; }

QString num(double v) { return qs(io::formatPlain(v)); }
}  // namespace

UnsteadyFlowView::UnsteadyFlowView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- left: file, locations, initial flows ----
    auto* left = new QVBoxLayout;
    left->setSpacing(12);
    auto* lc = new Card(tr2("Lokasi kondisi batas", "Boundary condition locations"), "unsteady.boundaries");
    auto* top = new QHBoxLayout;
    file_ = new QComboBox;
    file_->setFont(monoFont());
    file_->setAccessibleName(tr2("File aliran unsteady", "Unsteady flow file"));
    auto* nf = new IconButton("plus", tr2("File aliran unsteady baru…", "New unsteady flow file…"));
    top->addWidget(file_, 1);
    top->addWidget(nf);
    lc->body()->addLayout(top);
    list_ = new QListWidget;
    list_->setMinimumHeight(220);
    list_->setObjectName("NavList");
    lc->body()->addWidget(list_, 1);
    auto* br = new QHBoxLayout;
    auto* ends = makeButton(tr2("Lengkapi ujung reach", "Add reach ends"), false, "plus");
    ends->setToolTip(tr2("Tambahkan lokasi kondisi batas untuk setiap ujung hulu dan hilir reach yang belum punya.", "Add a boundary location for every upstream and downstream reach end that has none."));
    auto* lat = makeButton(tr2("Aliran lateral…", "Lateral inflow…"), false, "plus");
    auto* area = makeButton(tr2("Area/BC line…", "Area/BC line…"), false, "plus");
    area->setToolTip(tr2("Kondisi batas untuk storage area atau area 2D (debit, muka air, hujan, BC line)", "Boundary condition of a storage area or 2D area (flow, stage, precipitation, BC line)"));
    auto* del = makeButton(tr2("Hapus", "Delete"), false, "trash");
    del->setObjectName("Danger");
    br->addWidget(ends);
    br->addWidget(lat);
    br->addWidget(area);
    connect(area, &QPushButton::clicked, this, &UnsteadyFlowView::addAreaBoundary);
    br->addStretch();
    br->addWidget(del);
    lc->body()->addLayout(br);
    left->addWidget(lc, 3);

    auto* ic = new Card(tr2("Debit awal (Initial Flow)", "Initial flows"), "unsteady.initial");
    initial_ = new QTableWidget(0, 3);
    initial_->setHorizontalHeaderLabels({tr2("Sungai / Reach", "River / Reach"), QStringLiteral("RS"), tr2("Debit awal", "Initial flow")});
    initial_->verticalHeader()->hide();
    initial_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    initial_->setMinimumHeight(110);
    ic->body()->addWidget(initial_);
    ic->body()->addWidget(makeMuted(tr2("Kosong = debit awal diambil dari nilai pertama hidrograf hulu atau dari reach di hulunya. Kondisi awal dihitung dengan backwater steady.",
                                        "Empty = the initial flow comes from the first upstream hydrograph value or from the reaches upstream. Initial conditions are computed with a steady backwater run.")));
    left->addWidget(ic, 2);
    root->addLayout(left, 2);

    // ---- right: data of the selected location ----
    auto* dc = new Card(tr2("Data kondisi batas", "Boundary condition data"), "unsteady.data");
    where_ = new QLabel;
    where_->setObjectName("Strong");
    dc->body()->addWidget(where_);
    auto* form = new QFormLayout;
    type_ = new QComboBox;
    interval_ = new QComboBox;
    for (const char* i : kIntervals) interval_->addItem(QString::fromLatin1(i), QString::fromLatin1(i));
    form->addRow(labelWithInfo(tr2("Jenis", "Type"), "unsteady.types"), type_);
    form->addRow(labelWithInfo(tr2("Interval data", "Data interval"), "unsteady.interval"), interval_);
    dc->body()->addLayout(form);
    stack_ = new QStackedWidget;
    // page 0: hydrograph
    auto* hp = new QWidget;
    auto* hv = new QHBoxLayout(hp);
    hv->setContentsMargins(0, 0, 0, 0);
    values_ = new NumTable({tr2("Nilai", "Value")}, {"unsteady.hydrograph"});
    values_->setMinimumWidth(170);
    values_->setMaximumWidth(220);
    hv->addWidget(values_);
    plot_ = new PlotWidget;
    hv->addWidget(plot_, 1);
    stack_->addWidget(hp);
    // page 1: normal depth
    auto* np = new QWidget;
    auto* nv = new QFormLayout(np);
    slope_ = new QLineEdit;
    slope_->setFont(monoFont());
    nv->addRow(labelWithInfo(tr2("Kemiringan gesek", "Friction slope"), "unsteady.normal"), slope_);
    stack_->addWidget(np);
    // page 2: rating curve
    rating_ = new NumTable({tr2("Elevasi", "Stage"), tr2("Debit", "Flow")}, {"unsteady.rating", "unsteady.rating"});
    stack_->addWidget(rating_);
    // page 3: nothing to edit
    auto* ep = new QLabel;
    stack_->addWidget(ep);
    dc->body()->addWidget(stack_, 1);
    note_ = makeMuted(QString());
    dc->body()->addWidget(note_);
    root->addWidget(dc, 3);

    connect(file_, &QComboBox::activated, this, [this]() { s_->setEditUnsteady(file_->currentData().toString()); });
    connect(nf, &QToolButton::clicked, this, &UnsteadyFlowView::newFile);
    connect(list_, &QListWidget::currentRowChanged, this, [this]() { if (!loading_) showBoundary(); });
    connect(ends, &QPushButton::clicked, this, &UnsteadyFlowView::completeEnds);
    connect(lat, &QPushButton::clicked, this, &UnsteadyFlowView::addLateral);
    connect(del, &QPushButton::clicked, this, &UnsteadyFlowView::removeBoundary);
    connect(type_, &QComboBox::activated, this, [this]() { commitBoundary(); showBoundary(); });
    connect(interval_, &QComboBox::activated, this, [this]() { commitBoundary(); showBoundary(); });
    connect(values_, &NumTable::edited, this, [this]() { commitBoundary(); });
    connect(rating_, &NumTable::edited, this, [this]() { commitBoundary(); });
    connect(slope_, &QLineEdit::editingFinished, this, [this]() { commitBoundary(); });
    connect(initial_, &QTableWidget::itemChanged, this, [this]() { if (!loading_) commitInitial(); });
    reload();
}

QString UnsteadyFlowView::title() const { return tr2("Data Aliran Unsteady", "Unsteady Flow Data"); }

QWidget* UnsteadyFlowView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* an = makeButton(tr2("Analisis Unsteady", "Unsteady Analysis"), true, "run-unsteady");
    h->addWidget(an);
    connect(an, &QPushButton::clicked, this, [this]() { emit navigate("run-unsteady"); });
    return w;
}

void UnsteadyFlowView::activated() { reload(); }

UnsteadyBoundary* UnsteadyFlowView::current() {
    auto* u = s_->currentUnsteady();
    const int r = list_->currentRow();
    if (!u || r < 0 || r >= int(u->model.boundaries.size())) return nullptr;
    return &u->model.boundaries[size_t(r)];
}

void UnsteadyFlowView::reload() {
    loading_ = true;
    file_->clear();
    if (auto* p = s_->project())
        for (auto& [e, f] : p->unsteadyFlows) file_->addItem(qs(e) + QStringLiteral(" · ") + qs(f.model.title), qs(e));
    file_->setCurrentIndex(file_->findData(s_->currentUnsteadyExt()));
    loading_ = false;
    fillList();
    fillInitial();
}

void UnsteadyFlowView::fillList() {
    loading_ = true;
    const int keep = list_->currentRow();
    list_->clear();
    if (auto* u = s_->currentUnsteady())
        for (const auto& b : u->model.boundaries) {
            QString loc = QStringLiteral("%1 / %2   RS %3").arg(qs(b.river), qs(b.reach), qs(b.rs));
            if (b.type == UBoundaryType::UniformLateralInflow && !b.rsEnd.empty()) loc += QStringLiteral(" – ") + qs(b.rsEnd);
            if (!b.area.empty()) loc = tr2("Area %1", "Area %1").arg(qs(b.area)) + (b.bcLine.empty() ? QString() : QStringLiteral(" · BC line ") + qs(b.bcLine));
            else if (b.river.empty() && b.location.size() > 4 && !b.location[4].empty()) loc = QStringLiteral("Storage area %1").arg(qs(b.location[4]));
            list_->addItem(loc + QStringLiteral("\n    ") + uTypeName(b.type));
        }
    loading_ = false;
    if (list_->count()) list_->setCurrentRow(std::clamp(keep, 0, list_->count() - 1));
    showBoundary();
}

void UnsteadyFlowView::showBoundary() {
    loading_ = true;
    UnsteadyBoundary* b = current();
    const bool has = b != nullptr;
    for (QWidget* w : std::initializer_list<QWidget*>{type_, interval_, stack_}) w->setEnabled(has);
    type_->clear();
    if (!b) {
        where_->setText(s_->currentUnsteady() ? tr2("Pilih lokasi di kiri, atau klik Lengkapi ujung reach.", "Pick a location on the left, or click Add reach ends.")
                                              : tr2("Belum ada file aliran unsteady. Buat dengan tombol + di kiri.", "No unsteady flow file yet. Create one with the + button on the left."));
        values_->setValues({});
        plot_->clear();
        note_->clear();
        loading_ = false;
        return;
    }
    where_->setText(b->area.empty() ? QStringLiteral("%1 / %2 · RS %3").arg(qs(b->river), qs(b->reach), qs(b->rs))
                                    : tr2("Area %1", "Area %1").arg(qs(b->area)) + (b->bcLine.empty() ? QString() : QStringLiteral(" · BC line ") + qs(b->bcLine)));
    std::vector<UBoundaryType> types;
    if (!b->area.empty()) {
        if (!b->bcLine.empty()) types = {UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::NormalDepth};
        else types = {UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::LateralInflow, UBoundaryType::Precipitation};
        if (std::find(types.begin(), types.end(), b->type) == types.end()) types.push_back(b->type);
    } else if (isLateral(b->type)) types = {UBoundaryType::LateralInflow, UBoundaryType::UniformLateralInflow};
    else types = {UBoundaryType::None, UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::NormalDepth, UBoundaryType::RatingCurve};
    if (b->type == UBoundaryType::Other) types = {UBoundaryType::Other};
    for (auto t : types) type_->addItem(uTypeName(t), int(t));
    type_->setCurrentIndex(type_->findData(int(b->type)));
    const bool hydro = b->type == UBoundaryType::FlowHydrograph || b->type == UBoundaryType::StageHydrograph || isLateral(b->type) ||
                       b->type == UBoundaryType::Precipitation;
    interval_->setEnabled(hydro);
    int ii = interval_->findData(qs(b->interval));
    if (ii < 0 && hydro && !b->interval.empty()) { interval_->addItem(qs(b->interval), qs(b->interval)); ii = interval_->count() - 1; }
    interval_->setCurrentIndex(ii);
    const auto* p = s_->project();
    const QString lu = p ? qs(lengthUnit(p->units())) : QString(), qu = p ? qs(flowUnit(p->units())) : QString();
    if (hydro) {
        stack_->setCurrentIndex(0);
        const bool stage = b->type == UBoundaryType::StageHydrograph, rain = b->type == UBoundaryType::Precipitation;
        const QString ru = p && p->units() == UnitSystem::SI ? QStringLiteral("mm") : QStringLiteral("in");
        values_->setHorizontalHeaderItem(0, new QTableWidgetItem(rain ? tr2("Hujan (%1 per interval)", "Rain (%1 per interval)").arg(ru)
                                                                      : (stage ? tr2("Muka air (%1)", "Stage (%1)") : tr2("Debit (%1)", "Flow (%1)")).arg(stage ? lu : qu)));
        std::vector<std::vector<double>> rows;
        for (double v : b->values) rows.push_back({v});
        values_->setValues(rows);
        const double h = intervalSeconds(b->interval) / 3600.0;
        PlotSeries ps;
        ps.name = uTypeName(b->type);
        ps.color = stage ? Theme::current().water : Theme::current().accent;
        for (size_t k = 0; k < b->values.size(); ++k) ps.pts << QPointF(double(k) * h, b->values[k]);
        plot_->setTitles(QString(), tr2("Jam sejak mulai", "Hours from start"), stage ? tr2("Muka air (%1)", "Stage (%1)").arg(lu) : tr2("Debit (%1)", "Flow (%1)").arg(qu));
        plot_->setSeries({ps});
        note_->setText(rain ? tr2("Tinggi hujan per interval (bukan intensitas); baris ke-n berlaku untuk interval yang berakhir pada baris itu. Hujan jatuh di seluruh area.",
                                  "Rain depth per interval (not intensity); row n applies to the interval ending at that row. Rain falls on the whole area.")
                            : tr2("Baris pertama = awal simulasi; tiap baris berikutnya bertambah satu interval data. Bisa ditempel dari Excel (Ctrl+V).",
                           "The first row is the simulation start; each next row is one data interval later. Paste from Excel works (Ctrl+V)."));
    } else if (b->type == UBoundaryType::NormalDepth) {
        stack_->setCurrentIndex(1);
        slope_->setText(num(b->slope));
        note_->setText(tr2("Debit hilir dihitung dari persamaan Manning dengan kemiringan ini pada muka air saat itu.", "The downstream flow comes from Manning's equation with this slope at the current water surface."));
    } else if (b->type == UBoundaryType::RatingCurve) {
        stack_->setCurrentIndex(2);
        std::vector<std::vector<double>> rows;
        for (const auto& r : b->rating) rows.push_back({r.first, r.second});
        rating_->setValues(rows);
        note_->setText(tr2("Pasangan elevasi–debit naik. Format baris Rating Curve di .u## belum diverifikasi dengan HEC-RAS.", "Increasing stage–flow pairs. The Rating Curve line format in the .u## file is not verified against HEC-RAS yet."));
    } else {
        stack_->setCurrentIndex(3);
        note_->setText(b->type == UBoundaryType::Other ? tr2("Jenis ini tetap tersimpan di file tetapi belum dihitung De-RAS.", "This type stays in the file but De-RAS does not compute it yet.")
                                                       : tr2("Pilih jenis kondisi batas.", "Pick a boundary condition type."));
    }
    loading_ = false;
}

void UnsteadyFlowView::commitBoundary() {
    if (loading_) return;
    UnsteadyBoundary* b = current();
    if (!b) return;
    UnsteadyBoundary nb = *b;
    nb.type = static_cast<UBoundaryType>(type_->currentData().toInt());
    auto isHydro = [](UBoundaryType t) { return t == UBoundaryType::FlowHydrograph || t == UBoundaryType::StageHydrograph || isLateral(t) || t == UBoundaryType::Precipitation; };
    const bool hydro = isHydro(nb.type);
    if (hydro) {
        if (interval_->currentIndex() >= 0) nb.interval = ss(interval_->currentData().toString());
        if (nb.interval.empty()) nb.interval = "1HOUR";
        if (b->type == nb.type) {
            std::vector<std::vector<double>> rows;
            if (values_->values(rows)) {
                nb.values.clear();
                for (const auto& r : rows) if (!r.empty()) nb.values.push_back(r[0]);
            }
        } else if (!isHydro(b->type)) {
            nb.values.clear();
        }
    } else {
        nb.interval.clear();
    }
    if (nb.type == UBoundaryType::NormalDepth && b->type == nb.type)
        if (auto v = parseUserNumber(slope_->text())) nb.slope = *v;
    if (nb.type == UBoundaryType::RatingCurve && b->type == nb.type) {
        std::vector<std::vector<double>> rows;
        if (rating_->values(rows)) {
            nb.rating.clear();
            for (const auto& r : rows) if (r.size() >= 2) nb.rating.emplace_back(r[0], r[1]);
        }
    }
    if (nb == *b) return;
    *b = nb;
    s_->markEdited();
    loading_ = true;
    if (auto* it = list_->currentItem()) it->setText(it->text().section('\n', 0, 0) + QStringLiteral("\n    ") + uTypeName(b->type));
    loading_ = false;
    if (hydro && stack_->currentIndex() == 0) {
        const double h = intervalSeconds(b->interval) / 3600.0;
        PlotSeries ps;
        ps.name = uTypeName(b->type);
        ps.color = b->type == UBoundaryType::StageHydrograph ? Theme::current().water : Theme::current().accent;
        for (size_t k = 0; k < b->values.size(); ++k) ps.pts << QPointF(double(k) * h, b->values[k]);
        plot_->setSeries({ps});
    }
}

void UnsteadyFlowView::fillInitial() {
    loading_ = true;
    initial_->setRowCount(0);
    auto* u = s_->currentUnsteady();
    auto* g = s_->currentGeometry();
    if (u && g) {
        for (const auto& r : g->model.reaches) {
            const auto xs = r.crossSections();
            if (xs.empty()) continue;
            const int row = initial_->rowCount();
            initial_->insertRow(row);
            auto* a = new QTableWidgetItem(qs(r.label()));
            a->setFlags(a->flags() & ~Qt::ItemIsEditable);
            a->setData(Qt::UserRole, qs(r.river));
            a->setData(Qt::UserRole + 1, qs(r.reach));
            initial_->setItem(row, 0, a);
            QString rs = qs(xs.front()->rs), q;
            for (const auto& f : u->model.initialFlows)
                if (f.river == r.river && f.reach == r.reach) { rs = qs(f.rs); q = num(f.q); }
            auto* b = new QTableWidgetItem(rs);
            b->setFlags(b->flags() & ~Qt::ItemIsEditable);
            initial_->setItem(row, 1, b);
            auto* c = new QTableWidgetItem(q);
            c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            c->setFont(monoFont());
            initial_->setItem(row, 2, c);
        }
        for (const auto& a : g->model.storageAreas) {
            const int row = initial_->rowCount();
            initial_->insertRow(row);
            auto* a0 = new QTableWidgetItem((a.is2D ? tr2("Area 2D ", "2D area ") : QStringLiteral("Storage area ")) + qs(a.name));
            a0->setFlags(a0->flags() & ~Qt::ItemIsEditable);
            a0->setData(Qt::UserRole + 2, qs(a.name));
            initial_->setItem(row, 0, a0);
            auto* b = new QTableWidgetItem(tr2("elevasi awal", "initial elev."));
            b->setFlags(b->flags() & ~Qt::ItemIsEditable);
            initial_->setItem(row, 1, b);
            QString v;
            for (const auto& e : u->model.initialStorage) if (e.first == a.name) v = num(e.second);
            auto* c = new QTableWidgetItem(v);
            c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            c->setFont(monoFont());
            initial_->setItem(row, 2, c);
        }
    }
    loading_ = false;
}

void UnsteadyFlowView::commitInitial() {
    auto* u = s_->currentUnsteady();
    if (!u) return;
    std::vector<InitialFlow> out;
    std::vector<std::pair<std::string, double>> storage;
    std::vector<std::string> shownAreas;
    for (int r = 0; r < initial_->rowCount(); ++r) {
        const QString t = initial_->item(r, 2) ? initial_->item(r, 2)->text().trimmed() : QString();
        const QString areaName = initial_->item(r, 0)->data(Qt::UserRole + 2).toString();
        if (!areaName.isEmpty()) {
            shownAreas.push_back(ss(areaName));
            if (t.isEmpty()) continue;
            auto v = parseUserNumber(t);
            if (!v) { showToast(this, tr2("Elevasi awal harus angka.", "The initial elevation must be a number."), "warning"); continue; }
            storage.emplace_back(ss(areaName), *v);
            continue;
        }
        if (t.isEmpty()) continue;
        auto v = parseUserNumber(t);
        if (!v || *v < 0) {
            showToast(this, tr2("Debit awal harus angka ≥ 0.", "The initial flow must be a number ≥ 0."), "warning");
            continue;
        }
        InitialFlow f;
        f.river = ss(initial_->item(r, 0)->data(Qt::UserRole).toString());
        f.reach = ss(initial_->item(r, 0)->data(Qt::UserRole + 1).toString());
        f.rs = ss(initial_->item(r, 1)->text());
        f.q = *v;
        out.push_back(f);
    }
    // keep initial flows of reaches not in this geometry
    for (const auto& f : u->model.initialFlows) {
        bool shown = false;
        for (int r = 0; r < initial_->rowCount(); ++r)
            if (initial_->item(r, 0)->data(Qt::UserRole + 2).toString().isEmpty() && ss(initial_->item(r, 0)->data(Qt::UserRole).toString()) == f.river &&
                ss(initial_->item(r, 0)->data(Qt::UserRole + 1).toString()) == f.reach)
                shown = true;
        if (!shown) out.push_back(f);
    }
    for (const auto& e : u->model.initialStorage)
        if (std::find(shownAreas.begin(), shownAreas.end(), e.first) == shownAreas.end()) storage.push_back(e);
    if (out == u->model.initialFlows && storage == u->model.initialStorage) return;
    u->model.initialFlows = out;
    u->model.initialStorage = storage;
    s_->markEdited();
}

void UnsteadyFlowView::completeEnds() {
    auto* u = s_->currentUnsteady();
    auto* g = s_->currentGeometry();
    if (!u || !g) { showToast(this, tr2("Butuh file geometri dan file aliran unsteady.", "Needs a geometry file and an unsteady flow file."), "warning"); return; }
    int added = 0;
    for (const auto& r : g->model.reaches) {
        const auto xs = r.crossSections();
        if (xs.size() < 2) continue;
        bool upJ = false, dnJ = false;
        for (const auto& j : g->model.junctions) {
            for (const auto& e : j.up) if (e.first == r.river && e.second == r.reach) dnJ = true;
            for (const auto& e : j.dn) if (e.first == r.river && e.second == r.reach) upJ = true;
        }
        for (int side = 0; side < 2; ++side) {
            if (side == 0 ? upJ : dnJ) continue;
            const std::string& rs = side == 0 ? xs.front()->rs : xs.back()->rs;
            bool exists = false;
            for (const auto& b : u->model.boundaries)
                if (b.river == r.river && b.reach == r.reach && b.rs == rs && !isLateral(b.type)) exists = true;
            if (exists) continue;
            UnsteadyBoundary b;
            b.river = r.river;
            b.reach = r.reach;
            b.rs = rs;
            b.type = side == 0 ? UBoundaryType::FlowHydrograph : UBoundaryType::NormalDepth;
            b.interval = side == 0 ? "1HOUR" : "";
            if (side == 1) b.slope = 0.001;
            u->model.boundaries.push_back(b);
            ++added;
        }
    }
    if (added) s_->markEdited();
    fillList();
    showToast(this, added ? tr2("%1 lokasi ditambahkan. Hulu: hidrograf debit, hilir: normal depth (bisa diganti).", "%1 locations added. Upstream: flow hydrograph, downstream: normal depth (can be changed).").arg(added)
                          : tr2("Semua ujung reach sudah punya kondisi batas.", "Every reach end already has a boundary condition."),
              added ? "check" : "info");
}

void UnsteadyFlowView::addLateral() {
    auto* u = s_->currentUnsteady();
    auto* g = s_->currentGeometry();
    if (!u || !g || g->model.reaches.empty()) { showToast(this, tr2("Butuh file geometri dan file aliran unsteady.", "Needs a geometry file and an unsteady flow file."), "warning"); return; }
    Sheet sh(this, tr2("Aliran lateral", "Lateral inflow"), "unsteady.lateral", 520);
    auto* form = new QFormLayout;
    auto* reach = new QComboBox;
    for (const auto& r : g->model.reaches) reach->addItem(qs(r.label()));
    auto* kind = new QComboBox;
    kind->addItem(uTypeName(UBoundaryType::LateralInflow), int(UBoundaryType::LateralInflow));
    kind->addItem(uTypeName(UBoundaryType::UniformLateralInflow), int(UBoundaryType::UniformLateralInflow));
    auto* rs1 = new QComboBox;
    auto* rs2 = new QComboBox;
    auto fillRs = [&]() {
        rs1->clear();
        rs2->clear();
        const auto& r = g->model.reaches[size_t(std::max(0, reach->currentIndex()))];
        for (const auto* x : r.crossSections()) { rs1->addItem(qs(x->rs)); rs2->addItem(qs(x->rs)); }
        if (rs2->count()) rs2->setCurrentIndex(rs2->count() - 1);
    };
    fillRs();
    QObject::connect(reach, &QComboBox::currentIndexChanged, &sh, [&](int) { fillRs(); });
    QObject::connect(kind, &QComboBox::currentIndexChanged, &sh, [&](int) { rs2->setEnabled(kind->currentData().toInt() == int(UBoundaryType::UniformLateralInflow)); });
    rs2->setEnabled(false);
    form->addRow(QStringLiteral("Reach"), reach);
    form->addRow(tr2("Jenis", "Type"), kind);
    form->addRow(tr2("RS masuk (hulu)", "Inflow RS (upstream)"), rs1);
    form->addRow(tr2("RS akhir (aliran merata)", "End RS (uniform inflow)"), rs2);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeMuted(tr2("Aliran lateral masuk di ruas sesudah RS ini. Aliran merata dibagi sepanjang ruas sampai RS akhir.",
                                       "A lateral inflow enters on the segment after this RS. A uniform inflow is spread along the reach up to the end RS.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Tambah", "Add"), 1, true);
    sh.setAcceptCheck([&](int res) {
        if (res != 1) return true;
        if (kind->currentData().toInt() == int(UBoundaryType::UniformLateralInflow) && rs2->currentIndex() <= rs1->currentIndex()) {
            sh.setError(tr2("RS akhir harus di hilir RS masuk.", "The end RS must be downstream of the inflow RS."));
            return false;
        }
        return true;
    });
    if (sh.exec() != 1) return;
    const auto& r = g->model.reaches[size_t(reach->currentIndex())];
    UnsteadyBoundary b;
    b.river = r.river;
    b.reach = r.reach;
    b.rs = ss(rs1->currentText());
    b.type = static_cast<UBoundaryType>(kind->currentData().toInt());
    if (b.type == UBoundaryType::UniformLateralInflow) b.rsEnd = ss(rs2->currentText());
    b.interval = "1HOUR";
    u->model.boundaries.push_back(b);
    s_->markEdited();
    fillList();
    list_->setCurrentRow(list_->count() - 1);
}

void UnsteadyFlowView::addAreaBoundary() {
    auto* u = s_->currentUnsteady();
    auto* g = s_->currentGeometry();
    if (!u || !g || g->model.storageAreas.empty()) {
        showToast(this, tr2("Belum ada storage area atau area 2D di geometri.", "There is no storage area or 2D area in the geometry yet."), "warning");
        return;
    }
    Sheet sh(this, tr2("Kondisi batas area", "Area boundary condition"), "unsteady.area", 520);
    auto* form = new QFormLayout;
    auto* area = new QComboBox;
    for (const auto& a : g->model.storageAreas) area->addItem((a.is2D ? tr2("Area 2D: ", "2D area: ") : QStringLiteral("Storage area: ")) + qs(a.name), qs(a.name));
    auto* line = new QComboBox;
    auto* kind = new QComboBox;
    auto refill = [&]() {
        line->clear();
        kind->clear();
        const StorageArea* a = g->model.findArea(ss(area->currentData().toString()));
        line->addItem(tr2("(tanpa BC line)", "(no BC line)"), QString());
        if (a && a->is2D)
            for (const auto& b : g->model.bcLines) if (b.area == a->name) line->addItem(qs(b.name), qs(b.name));
        line->setEnabled(a && a->is2D);
        const bool withLine = !line->currentData().toString().isEmpty();
        std::vector<UBoundaryType> t = withLine ? std::vector<UBoundaryType>{UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::NormalDepth}
                                                : std::vector<UBoundaryType>{UBoundaryType::FlowHydrograph, UBoundaryType::Precipitation, UBoundaryType::StageHydrograph};
        if (a && a->is2D && !withLine) t = {UBoundaryType::Precipitation, UBoundaryType::FlowHydrograph};
        for (auto x : t) kind->addItem(uTypeName(x), int(x));
    };
    refill();
    QObject::connect(area, &QComboBox::currentIndexChanged, &sh, [&](int) { refill(); });
    QObject::connect(line, &QComboBox::activated, &sh, [&](int) {
        const QString keep = line->currentData().toString();
        const StorageArea* a = g->model.findArea(ss(area->currentData().toString()));
        kind->clear();
        std::vector<UBoundaryType> t = !keep.isEmpty() ? std::vector<UBoundaryType>{UBoundaryType::FlowHydrograph, UBoundaryType::StageHydrograph, UBoundaryType::NormalDepth}
                                     : (a && a->is2D ? std::vector<UBoundaryType>{UBoundaryType::Precipitation, UBoundaryType::FlowHydrograph}
                                                     : std::vector<UBoundaryType>{UBoundaryType::FlowHydrograph, UBoundaryType::Precipitation, UBoundaryType::StageHydrograph});
        for (auto x : t) kind->addItem(uTypeName(x), int(x));
    });
    form->addRow(tr2("Area", "Area"), area);
    form->addRow(QStringLiteral("BC line"), line);
    form->addRow(tr2("Jenis", "Type"), kind);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeMuted(tr2("Area 2D: hidrograf debit dan muka air atau normal depth lewat BC line di tepinya; hujan untuk seluruh area. Storage area: debit masuk, muka air, atau hujan.",
                                       "2D area: flow or stage hydrograph, or normal depth through a BC line on its edge; precipitation over the whole area. Storage area: inflow, stage or precipitation.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Tambah", "Add"), 1, true);
    if (sh.exec() != 1) return;
    UnsteadyBoundary b;
    b.area = ss(area->currentData().toString());
    b.bcLine = ss(line->currentData().toString());
    b.type = static_cast<UBoundaryType>(kind->currentData().toInt());
    b.interval = b.type == UBoundaryType::NormalDepth ? "" : "1HOUR";
    if (b.type == UBoundaryType::NormalDepth) b.slope = 0.001;
    u->model.boundaries.push_back(b);
    s_->markEdited();
    fillList();
    list_->setCurrentRow(list_->count() - 1);
}

void UnsteadyFlowView::removeBoundary() {
    auto* u = s_->currentUnsteady();
    const int r = list_->currentRow();
    if (!u || r < 0 || r >= int(u->model.boundaries.size())) return;
    const auto& b = u->model.boundaries[size_t(r)];
    const QString where = b.area.empty() ? QStringLiteral("%1 / %2 RS %3").arg(qs(b.river), qs(b.reach), qs(b.rs)) : tr2("area %1", "area %1").arg(qs(b.area));
    if (!sheetConfirm(this, tr2("Hapus lokasi", "Delete location"), tr2("Hapus kondisi batas di %1?", "Delete the boundary condition at %1?").arg(where),
                      tr2("Hapus", "Delete"), true))
        return;
    u->model.boundaries.erase(u->model.boundaries.begin() + r);
    s_->markEdited();
    fillList();
}

void UnsteadyFlowView::newFile() {
    auto* p = s_->project();
    if (!p) return;
    QString t = tr2("Aliran unsteady", "Unsteady flow");
    if (!sheetGetText(this, tr2("File aliran unsteady baru", "New unsteady flow file"), tr2("Judul:", "Title:"), t, 0, "unsteady.boundaries"))
        return;
    t = t.trimmed();
    if (t.isEmpty()) return;
    const std::string e = p->addUnsteadyFlow(ss(t));
    if (e.empty()) return;
    s_->setEditUnsteady(qs(e));
    s_->markEdited();
    reload();
    completeEnds();
}

}  // namespace deras::ui

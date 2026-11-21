// De-RAS - ui/SteadyFlowView.cpp
#include "SteadyFlowView.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "NumTable.h"
#include "Widgets.h"

namespace deras::ui {

namespace {
QTableWidgetItem* roItem(const QString& t) {
    auto* it = new QTableWidgetItem(t);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}
QTableWidgetItem* numItem(const QString& t) {
    auto* it = new QTableWidgetItem(t);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setFont(monoFont());
    return it;
}
}  // namespace

QString boundaryTypeName(BoundaryType t) {
    switch (t) {
        case BoundaryType::KnownWS: return tr2("Muka air diketahui", "Known W.S.");
        case BoundaryType::CriticalDepth: return tr2("Kedalaman kritis", "Critical depth");
        case BoundaryType::NormalDepth: return tr2("Normal depth", "Normal depth");
        case BoundaryType::RatingCurve: return tr2("Rating curve", "Rating curve");
        default: return tr2("(belum diisi)", "(not set)");
    }
}

namespace {
// Junction at the upstream (true) or downstream (false) end of a reach; empty if none.
QString junctionAt(const Geometry& g, const Reach& r, bool upstreamEnd) {
    for (const auto& j : g.junctions)
        for (const auto& e : upstreamEnd ? j.dn : j.up)
            if (e.first == r.river && e.second == r.reach) return qs(j.name);
    return {};
}
QString boundaryText(const Boundary& d, const QString& lu) {
    QString v = boundaryTypeName(d.type);
    if (d.type == BoundaryType::NormalDepth) v += QStringLiteral(" · S = %1").arg(d.slope, 0, 'g', 6);
    if (d.type == BoundaryType::KnownWS) v += QStringLiteral(" · %1 %2").arg(d.knownWS, 0, 'g', 8).arg(lu);
    if (d.type == BoundaryType::RatingCurve) v += QStringLiteral(" · %1 %2").arg(d.rating.size()).arg(tr2("titik", "points"));
    return v;
}
}  // namespace


SteadyFlowView::SteadyFlowView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- main: discharge table ----
    auto* main = new Card(tr2("Debit per lokasi perubahan debit", "Discharge per flow-change location"), "flow.table");
    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    file_ = new QComboBox;
    file_->setFont(monoFont());
    file_->setMinimumWidth(220);
    file_->setAccessibleName(tr2("File aliran steady", "Steady flow file"));
    top->addWidget(file_);
    auto* nf = new IconButton("plus", tr2("File aliran steady baru…", "New steady flow file…"));
    top->addWidget(nf);
    top->addStretch();
    auto* addLoc = makeButton(tr2("Tambah lokasi…", "Add location…"), false, "plus");
    auto* delLoc = makeButton(tr2("Hapus lokasi", "Remove location"), false, "trash");
    top->addWidget(addLoc);
    top->addWidget(delLoc);
    main->body()->addLayout(top);
    flows_ = new QTableWidget;
    auto* fh = new InfoHeader(Qt::Horizontal, flows_);
    flows_->setHorizontalHeader(fh);
    flows_->verticalHeader()->hide();
    flows_->setAlternatingRowColors(true);
    flows_->setAccessibleName(tr2("Tabel debit", "Discharge table"));
    main->body()->addWidget(flows_, 1);
    status_ = makeMuted(QString());
    main->body()->addWidget(status_);
    root->addWidget(main, 1);

    // ---- side: profiles + boundaries ----
    auto* side = new QVBoxLayout;
    side->setSpacing(12);
    auto* pc = new Card(tr2("Profil", "Profiles"), "flow.profiles");
    pc->setFixedWidth(320);
    auto* pr = new QHBoxLayout;
    pr->addWidget(makeMuted(tr2("Jumlah profil", "Number of profiles"), false));
    pr->addStretch();
    nProf_ = new QSpinBox;
    nProf_->setRange(1, 500);
    nProf_->setKeyboardTracking(false);
    nProf_->setFont(monoFont());
    nProf_->setAccessibleName(tr2("Jumlah profil", "Number of profiles"));
    pr->addWidget(nProf_);
    pc->body()->addLayout(pr);
    profiles_ = new QLabel;
    profiles_->setObjectName("Mono");
    profiles_->setWordWrap(true);
    pc->body()->addWidget(profiles_);
    auto* names = makeButton(tr2("Nama profil…", "Profile names…"));
    names->setObjectName("Link");
    pc->body()->addWidget(names, 0, Qt::AlignLeft);
    side->addWidget(pc);

    auto* bc = new Card(tr2("Kondisi batas", "Boundary conditions"), "flow.boundary");
    bc->setFixedWidth(320);
    bcBody_ = new QVBoxLayout;
    bcBody_->setSpacing(6);
    bc->body()->addLayout(bcBody_);
    auto* bcBtn = makeButton(tr2("Atur kondisi batas…", "Set boundary conditions…"), true, "water");
    bc->body()->addWidget(bcBtn);
    bc->body()->addWidget(makeMuted(tr2("Subkritis memakai batas hilir, superkritis batas hulu, campuran keduanya. Ujung reach di junction tidak butuh batas.",
                                        "Subcritical uses the downstream boundary, supercritical the upstream one, mixed both. Reach ends at a junction need no boundary.")));
    side->addWidget(bc);
    side->addStretch();
    root->addLayout(side);

    connect(file_, &QComboBox::activated, this, [this]() { s_->setEditFlow(file_->currentData().toString()); fillTables(); });
    connect(nf, &QToolButton::clicked, this, &SteadyFlowView::newFlowFile);
    connect(nProf_, &QSpinBox::valueChanged, this, [this](int n) { if (!loading_) setProfileCount(n); });
    connect(flows_, &QTableWidget::itemChanged, this, [this]() { if (!loading_) commitFlows(); });
    connect(addLoc, &QPushButton::clicked, this, &SteadyFlowView::addLocation);
    connect(delLoc, &QPushButton::clicked, this, &SteadyFlowView::removeLocation);
    connect(names, &QPushButton::clicked, this, &SteadyFlowView::profileNamesSheet);
    connect(bcBtn, &QPushButton::clicked, this, &SteadyFlowView::boundarySheet);
    reload();
}

QString SteadyFlowView::title() const { return tr2("Data Aliran Steady", "Steady Flow Data"); }

QWidget* SteadyFlowView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* imp = makeButton(tr2("Impor CSV…", "Import CSV…"), false, "import");
    auto* exp = makeButton(tr2("Ekspor CSV…", "Export CSV…"), false, "export");
    auto* save = makeButton(tr2("Simpan data aliran", "Save flow data"), true, "save");
    h->addWidget(imp);
    h->addWidget(exp);
    h->addWidget(new InfoButton("flow.csv"));
    h->addWidget(save);
    connect(imp, &QPushButton::clicked, this, &SteadyFlowView::importCsv);
    connect(exp, &QPushButton::clicked, this, &SteadyFlowView::exportCsv);
    connect(save, &QPushButton::clicked, this, &View::requestSave);
    return w;
}

io::SteadyFlowFile* SteadyFlowView::flow() { return s_->currentFlow(); }

void SteadyFlowView::reload() {
    fillFiles();
    fillTables();
}

void SteadyFlowView::activated() { reload(); }

void SteadyFlowView::fillFiles() {
    loading_ = true;
    file_->clear();
    if (auto* p = s_->project())
        for (auto& [e, f] : p->steadyFlows) file_->addItem(qs(e) + QStringLiteral(" · ") + qs(f.model.title), qs(e));
    file_->setCurrentIndex(file_->findData(s_->currentFlowExt()));
    file_->setEnabled(file_->count() > 0);
    loading_ = false;
}

void SteadyFlowView::fillTables() {
    loading_ = true;
    auto* f = flow();
    const QString qu = s_->project() ? qs(flowUnit(s_->project()->units())) : QString();
    flows_->clear();
    flows_->setRowCount(0);
    flows_->setColumnCount(0);
    if (!f) {
        profiles_->setText(QString());
        status_->setText(s_->project() ? tr2("Belum ada file aliran steady. Buat dengan tombol + di atas tabel.", "No steady flow file yet. Create one with the + button above the table.") : QString());
        loading_ = false;
        fillBoundarySummary();
        return;
    }
    auto& m = f->model;
    nProf_->setValue(std::max(1, m.profileCount()));
    QStringList pn;
    for (const auto& n : m.profileNames) pn << qs(n);
    profiles_->setText(pn.join(" · "));
    QStringList heads{tr2("Sungai", "River"), tr2("Reach", "Reach"), "RS"};
    QStringList topics{"flow.col.location", "flow.col.location", "flow.col.location"};
    for (const auto& n : m.profileNames) { heads << qs(n) + " (" + qu + ")"; topics << "flow.col.discharge"; }
    flows_->setColumnCount(heads.size());
    flows_->setHorizontalHeaderLabels(heads);
    static_cast<InfoHeader*>(flows_->horizontalHeader())->setTopics(topics);
    flows_->setRowCount(static_cast<int>(m.changes.size()));
    for (int r = 0; r < static_cast<int>(m.changes.size()); ++r) {
        const auto& c = m.changes[static_cast<size_t>(r)];
        flows_->setItem(r, 0, roItem(qs(c.river)));
        flows_->setItem(r, 1, roItem(qs(c.reach)));
        auto* rs = roItem(qs(c.rs));
        rs->setFont(monoFont());
        flows_->setItem(r, 2, rs);
        for (int p = 0; p < m.profileCount(); ++p)
            flows_->setItem(r, 3 + p, numItem(p < static_cast<int>(c.flows.size()) ? QString::number(c.flows[static_cast<size_t>(p)], 'g', 10) : QString()));
    }
    flows_->resizeColumnsToContents();
    for (int c = 3; c < flows_->columnCount(); ++c) flows_->setColumnWidth(c, std::max(flows_->columnWidth(c), 110));
    status_->setText(m.changes.empty() ? tr2("Belum ada lokasi debit. Tekan “Tambah lokasi…” dan pilih penampang paling hulu.",
                                             "No flow location yet. Press “Add location…” and pick the most upstream cross section.")
                                       : tr2("Ketik atau tempel debit langsung di tabel. Nilai tidak valid ditandai merah dan tidak disimpan.",
                                             "Type or paste discharges directly in the table. Invalid values are marked red and not stored."));
    loading_ = false;
    fillBoundarySummary();
}

void SteadyFlowView::fillBoundarySummary() {
    while (QLayoutItem* it = bcBody_->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    auto* f = flow();
    auto* g = s_->currentGeometry();
    if (!f || !g || g->model.reaches.empty()) {
        bcBody_->addWidget(makeMuted(tr2("Butuh geometri dengan reach dan file aliran.", "Needs a geometry with a reach and a flow file.")));
        return;
    }
    const QString lu = qs(lengthUnit(s_->project()->units()));
    for (const auto& r : g->model.reaches) {
        const ReachBoundary* b = f->model.boundary(r.river, r.reach, 0);
        const Boundary d = b ? b->downstream : Boundary{};
        const Boundary u = b ? b->upstream : Boundary{};
        const QString ju = junctionAt(g->model, r, true), jd = junctionAt(g->model, r, false);
        const QString dn = jd.isEmpty() ? boundaryText(d, lu) : tr2("Junction %1", "Junction %1").arg(jd);
        const QString up = ju.isEmpty() ? boundaryText(u, lu) : tr2("Junction %1", "Junction %1").arg(ju);
        const bool okDn = !jd.isEmpty() || d.type != BoundaryType::None;
        auto* l = new QLabel(QStringLiteral("<span style='color:%1'>%2</span><br>%3 %4 %5<br><span style='color:%1'>%6 %7</span>")
                                 .arg(cssColor(Theme::current().text2), qs(r.label()).toHtmlEscaped(), okDn ? QStringLiteral("✓") : QStringLiteral("⚠"),
                                      tr2("Hilir:", "Downstream:"), dn.toHtmlEscaped(), tr2("Hulu:", "Upstream:"), up.toHtmlEscaped()));
        l->setWordWrap(true);
        bcBody_->addWidget(l);
    }
    if (f->model.profileCount() > 1)
        bcBody_->addWidget(makeMuted(tr2("Ini ringkasan profil pertama. Semua profil diatur di lembar kondisi batas.", "Summary for the first profile; all profiles are set in the boundary sheet.")));
}

void SteadyFlowView::commitFlows() {
    auto* f = flow();
    if (!f) return;
    int bad = 0;
    auto& m = f->model;
    loading_ = true;
    for (int r = 0; r < flows_->rowCount() && r < static_cast<int>(m.changes.size()); ++r) {
        auto& c = m.changes[static_cast<size_t>(r)];
        c.flows.resize(static_cast<size_t>(m.profileCount()), 0.0);
        for (int p = 0; p < m.profileCount(); ++p) {
            auto* it = flows_->item(r, 3 + p);
            auto v = parseUserNumber(it ? it->text() : QString());
            if (!v) {
                ++bad;
                if (it) it->setBackground(withAlpha(Theme::current().error, 0.25));
                continue;
            }
            if (it) it->setBackground(QBrush());
            c.flows[static_cast<size_t>(p)] = *v;
        }
    }
    loading_ = false;
    status_->setText(bad ? QStringLiteral("⛔ ") + tr2("%1 sel debit tidak valid (merah). Nilai lama tetap dipakai.", "%1 discharge cell(s) are invalid (red); previous values kept.").arg(bad)
                         : tr2("Ketik atau tempel debit langsung di tabel. Nilai tidak valid ditandai merah dan tidak disimpan.",
                               "Type or paste discharges directly in the table. Invalid values are marked red and not stored."));
    s_->markEdited();
}

void SteadyFlowView::setProfileCount(int n) {
    auto* f = flow();
    if (!f) return;
    auto& m = f->model;
    while (m.profileCount() < n) m.profileNames.push_back("PF " + std::to_string(m.profileCount() + 1));
    while (m.profileCount() > n) m.profileNames.pop_back();
    for (auto& c : m.changes) c.flows.resize(static_cast<size_t>(n), c.flows.empty() ? 0.0 : c.flows.back());
    std::vector<ReachBoundary> keep;
    for (auto& b : m.boundaries) if (b.profile < n) keep.push_back(b);
    m.boundaries = keep;
    if (auto* g = s_->currentGeometry())
        for (const auto& r : g->model.reaches)
            for (int p = 0; p < n; ++p)
                if (!m.boundary(r.river, r.reach, p)) {
                    const ReachBoundary* b0 = m.boundary(r.river, r.reach, 0);
                    const Boundary d0 = b0 ? b0->downstream : Boundary{}, u0 = b0 ? b0->upstream : Boundary{};
                    auto& nb = m.boundaryRef(r.river, r.reach, p);
                    nb.downstream = d0;
                    nb.upstream = u0;
                }
    s_->markEdited();
    fillTables();
}

void SteadyFlowView::profileNamesSheet() {
    auto* f = flow();
    if (!f) return;
    Sheet sh(this, tr2("Nama profil", "Profile names"), "flow.profilenames", 420);
    auto* t = new QTableWidget(f->model.profileCount(), 1);
    t->setHorizontalHeaderLabels({tr2("Nama (maks. 12 karakter, tanpa koma)", "Name (max. 12 characters, no comma)")});
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < f->model.profileCount(); ++i) t->setItem(i, 0, new QTableWidgetItem(qs(f->model.profileNames[static_cast<size_t>(i)])));
    t->setMinimumHeight(std::min(380, 50 + 30 * f->model.profileCount()));
    sh.body()->addWidget(t);
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    sh.setAcceptCheck([&](int) {
        QStringList seen;
        for (int i = 0; i < t->rowCount(); ++i) {
            QString n = t->item(i, 0) ? t->item(i, 0)->text().trimmed() : QString();
            if (n.isEmpty()) { sh.setError(tr2("Nama profil %1 kosong.", "Profile name %1 is empty.").arg(i + 1)); return false; }
            if (n.contains(',')) { sh.setError(tr2("Jangan pakai koma, karena koma menjadi pemisah di file .f##.", "Commas are not allowed (separator in the .f## file).")); return false; }
            if (n.size() > 12) { sh.setError(tr2("Maksimum 12 karakter: %1", "Maximum 12 characters: %1").arg(n)); return false; }
            if (seen.contains(n)) { sh.setError(tr2("Nama ganda: %1", "Duplicate name: %1").arg(n)); return false; }
            seen << n;
        }
        return true;
    });
    if (sh.exec() != 1) return;
    for (int i = 0; i < t->rowCount(); ++i) f->model.profileNames[static_cast<size_t>(i)] = ss(t->item(i, 0)->text().trimmed());
    s_->markEdited();
    fillTables();
}

void SteadyFlowView::addLocation() {
    auto* f = flow();
    auto* g = s_->currentGeometry();
    if (!f || !g || g->model.reaches.empty()) {
        sheetInfo(this, tr2("Tambah lokasi debit", "Add flow location"), tr2("Buat geometri dengan reach dan penampang dulu.", "Create a geometry with a reach and cross sections first."));
        return;
    }
    Sheet sh(this, tr2("Tambah lokasi debit", "Add flow location"), "flow.table", 440);
    auto* form = new QFormLayout;
    auto* rc = new QComboBox;
    auto* xc = new QComboBox;
    xc->setFont(monoFont());
    for (const auto& r : g->model.reaches) rc->addItem(qs(r.label()), r.uid);
    auto fillXs = [&]() {
        xc->clear();
        for (const auto& r : g->model.reaches)
            if (r.uid == rc->currentData().toInt())
                for (const auto* x : r.crossSections()) xc->addItem(qs(x->rs));
    };
    fillXs();
    QObject::connect(rc, &QComboBox::currentIndexChanged, &sh, fillXs);
    form->addRow(tr2("Sungai / Reach", "River / Reach"), rc);
    form->addRow("RS", xc);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeMuted(tr2("Lokasi pertama sebaiknya penampang paling hulu reach. Debit berlaku ke hilir sampai lokasi berikutnya.",
                                       "The first location should be the most upstream section of the reach. The discharge applies downstream until the next location.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Tambah", "Add"), 1, true);
    sh.setAcceptCheck([&](int) {
        if (xc->currentIndex() < 0) { sh.setError(tr2("Reach ini belum punya penampang.", "This reach has no cross section yet.")); return false; }
        return true;
    });
    if (sh.exec() != 1) return;
    for (const auto& r : g->model.reaches)
        if (r.uid == rc->currentData().toInt()) {
            for (const auto& c : f->model.changes)
                if (c.river == r.river && c.reach == r.reach && c.rs == ss(xc->currentText())) {
                    showToast(this, tr2("Lokasi itu sudah ada di tabel.", "That location is already in the table."), "warning");
                    return;
                }
            f->model.changes.push_back({r.river, r.reach, ss(xc->currentText()), std::vector<double>(static_cast<size_t>(f->model.profileCount()), 0.0)});
            if (!f->model.boundary(r.river, r.reach, 0))
                for (int p = 0; p < f->model.profileCount(); ++p) f->model.boundaryRef(r.river, r.reach, p);
        }
    s_->markEdited();
    fillTables();
}

void SteadyFlowView::removeLocation() {
    auto* f = flow();
    const int r = flows_->currentRow();
    if (!f || r < 0 || r >= static_cast<int>(f->model.changes.size())) {
        showToast(this, tr2("Pilih baris lokasi dulu.", "Select a location row first."), "warning");
        return;
    }
    const auto& c = f->model.changes[static_cast<size_t>(r)];
    if (!sheetConfirm(this, tr2("Hapus lokasi", "Remove location"),
                      tr2("Hapus lokasi debit %1 / %2 RS %3?", "Remove flow location %1 / %2 RS %3?").arg(qs(c.river), qs(c.reach), qs(c.rs)), tr2("Hapus", "Remove"), true))
        return;
    f->model.changes.erase(f->model.changes.begin() + r);
    s_->markEdited();
    fillTables();
}

void SteadyFlowView::boundarySheet() {
    auto* f = flow();
    auto* g = s_->currentGeometry();
    if (!f || !g || g->model.reaches.empty()) {
        sheetInfo(this, tr2("Kondisi batas", "Boundary conditions"), tr2("Butuh geometri dengan reach dan file aliran.", "Needs a geometry with a reach and a flow file."));
        return;
    }
    const QString lu = qs(lengthUnit(s_->project()->units()));
    Sheet sh(this, tr2("Kondisi batas", "Boundary conditions"), "flow.boundary", 1060);
    auto* t = new QTableWidget;
    auto* hh = new InfoHeader(Qt::Horizontal, t);
    t->setHorizontalHeader(hh);
    t->verticalHeader()->hide();
    const QStringList heads{tr2("Sungai / Reach", "River / Reach"), tr2("Profil", "Profile"),
                            tr2("Tipe batas hulu", "Upstream type"), tr2("Nilai hulu", "Upstream value"),
                            tr2("Tipe batas hilir", "Downstream type"), tr2("Nilai hilir (kemiringan / elevasi %1)", "Downstream value (slope / elevation %1)").arg(lu),
                            tr2("Rating curve hilir: elevasi:debit; …", "Downstream rating curve: stage:flow; …")};
    t->setColumnCount(heads.size());
    t->setHorizontalHeaderLabels(heads);
    hh->setTopics({"flow.col.bcreach", "flow.col.bcreach", "flow.col.bcupstream", "flow.col.bcvalue", "flow.col.bctype", "flow.col.bcvalue", "flow.col.bcrating"});
    struct Key { std::string river, reach; int profile; bool upJ, dnJ; };
    std::vector<Key> keys;
    QList<QComboBox*> upCombos, dnCombos;
    auto valueText = [](const Boundary& d) {
        return d.type == BoundaryType::NormalDepth ? QString::number(d.slope, 'g', 8)
             : d.type == BoundaryType::KnownWS    ? QString::number(d.knownWS, 'g', 10)
                                                  : QString();
    };
    for (const auto& r : g->model.reaches)
        for (int p = 0; p < f->model.profileCount(); ++p) {
            const int row = t->rowCount();
            t->insertRow(row);
            t->setItem(row, 0, roItem(qs(r.label())));
            t->setItem(row, 1, roItem(qs(f->model.profileNames[static_cast<size_t>(p)])));
            const ReachBoundary* b = f->model.boundary(r.river, r.reach, p);
            const Boundary u = b ? b->upstream : Boundary{};
            const Boundary d = b ? b->downstream : Boundary{};
            const QString ju = junctionAt(g->model, r, true), jd = junctionAt(g->model, r, false);
            auto combo = [&](bool upstream, const Boundary& cur) {
                auto* c = new QComboBox;
                c->setAccessibleName(upstream ? tr2("Tipe batas hulu", "Upstream boundary type") : tr2("Tipe batas hilir", "Downstream boundary type"));
                std::vector<BoundaryType> types{BoundaryType::None, BoundaryType::NormalDepth, BoundaryType::KnownWS, BoundaryType::CriticalDepth};
                if (!upstream) types.push_back(BoundaryType::RatingCurve);
                for (BoundaryType bt : types) c->addItem(boundaryTypeName(bt), static_cast<int>(bt));
                c->setCurrentIndex(std::max(0, c->findData(static_cast<int>(cur.type))));
                return c;
            };
            auto* uc = combo(true, u);
            auto* dc = combo(false, d);
            upCombos << uc;
            dnCombos << dc;
            if (!ju.isEmpty()) {
                auto* it = roItem(tr2("Junction %1", "Junction %1").arg(ju));
                it->setToolTip(tr2("Ujung hulu reach ini tersambung ke junction, jadi tidak perlu kondisi batas.", "The upstream end of this reach joins a junction, so no boundary is needed."));
                t->setItem(row, 2, it);
                t->setItem(row, 3, roItem(QString()));
                uc->hide();
            } else {
                t->setCellWidget(row, 2, uc);
                t->setItem(row, 3, numItem(valueText(u)));
            }
            if (!jd.isEmpty()) {
                auto* it = roItem(tr2("Junction %1", "Junction %1").arg(jd));
                it->setToolTip(tr2("Ujung hilir reach ini tersambung ke junction; muka airnya dihitung dari reach di hilir.", "The downstream end of this reach joins a junction; its water surface comes from the reach downstream."));
                t->setItem(row, 4, it);
                t->setItem(row, 5, roItem(QString()));
                t->setItem(row, 6, roItem(QString()));
                dc->hide();
            } else {
                t->setCellWidget(row, 4, dc);
                t->setItem(row, 5, numItem(valueText(d)));
                QStringList rc;
                for (const auto& pr : d.rating) rc << QStringLiteral("%1:%2").arg(pr.first).arg(pr.second);
                t->setItem(row, 6, new QTableWidgetItem(rc.join("; ")));
            }
            keys.push_back({r.river, r.reach, p, !ju.isEmpty(), !jd.isEmpty()});
        }
    t->resizeColumnsToContents();
    for (int c : {2, 4}) t->setColumnWidth(c, std::max(t->columnWidth(c), 180));
    for (int c : {3, 5}) t->setColumnWidth(c, std::max(t->columnWidth(c), 120));
    t->horizontalHeader()->setStretchLastSection(true);
    t->setMinimumHeight(std::min(420, 60 + 34 * t->rowCount()));
    sh.body()->addWidget(t);
    auto* copyAll = makeButton(tr2("Salin baris terpilih ke semua profil reach itu", "Copy selected row to all profiles of that reach"));
    copyAll->setObjectName("Link");
    sh.body()->addWidget(copyAll, 0, Qt::AlignLeft);
    connect(copyAll, &QPushButton::clicked, &sh, [&]() {
        const int r = t->currentRow();
        if (r < 0) return;
        for (int k = 0; k < t->rowCount(); ++k)
            if (k != r && keys[static_cast<size_t>(k)].river == keys[static_cast<size_t>(r)].river && keys[static_cast<size_t>(k)].reach == keys[static_cast<size_t>(r)].reach) {
                upCombos[k]->setCurrentIndex(upCombos[r]->currentIndex());
                dnCombos[k]->setCurrentIndex(dnCombos[r]->currentIndex());
                for (int c : {3, 5, 6}) t->item(k, c)->setText(t->item(r, c)->text());
            }
    });
    sh.body()->addWidget(makeMuted(tr2("Batas hilir dipakai rezim subkritis dan campuran; batas hulu dipakai rezim superkritis dan campuran. "
                                       "Normal depth: isi kemiringan energi (misalnya 0.001). Muka air diketahui: isi elevasi. Rating curve: pasangan elevasi:debit dipisah titik koma.",
                                       "The downstream boundary is used by the subcritical and mixed regimes; the upstream boundary by the supercritical and mixed regimes. "
                                       "Normal depth: enter the energy slope (e.g. 0.001). Known W.S.: enter the elevation. Rating curve: stage:flow pairs separated by semicolons.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    struct Parsed { BoundaryType type = BoundaryType::None; double v = 0; std::vector<std::pair<double, double>> rating; };
    std::vector<Parsed> pu, pd;
    sh.setAcceptCheck([&](int) {
        pu.clear();
        pd.clear();
        int bad = 0;
        auto check = [&](int r, int valueCol, QComboBox* c, Parsed& p) {
            p.type = static_cast<BoundaryType>(c->currentData().toInt());
            bool ok = true;
            auto v = parseUserNumber(t->item(r, valueCol)->text());
            if (p.type == BoundaryType::NormalDepth) ok = v && *v > 0;
            if (p.type == BoundaryType::KnownWS) ok = v.has_value();
            if (v) p.v = *v;
            t->item(r, valueCol)->setBackground(ok ? QBrush() : QBrush(withAlpha(Theme::current().error, 0.25)));
            return ok;
        };
        for (int r = 0; r < t->rowCount(); ++r) {
            const Key& k = keys[static_cast<size_t>(r)];
            Parsed u, d;
            bool ok = true;
            if (!k.upJ) ok = check(r, 3, upCombos[r], u) && ok;
            if (!k.dnJ) {
                ok = check(r, 5, dnCombos[r], d) && ok;
                bool rok = true;
                for (const QString& pair : t->item(r, 6)->text().split(';', Qt::SkipEmptyParts)) {
                    const auto ab = pair.split(':');
                    auto st = ab.size() == 2 ? parseUserNumber(ab[0]) : std::nullopt;
                    auto q = ab.size() == 2 ? parseUserNumber(ab[1]) : std::nullopt;
                    if (st && q) d.rating.emplace_back(*st, *q); else rok = false;
                }
                if (d.type == BoundaryType::RatingCurve && d.rating.size() < 2) rok = false;
                t->item(r, 6)->setBackground(rok ? QBrush() : QBrush(withAlpha(Theme::current().error, 0.25)));
                ok = ok && rok;
            }
            if (!ok) ++bad;
            pu.push_back(u);
            pd.push_back(d);
        }
        if (bad) { sh.setError(tr2("%1 baris tidak valid (ditandai merah). Kemiringan > 0; rating curve ≥ 2 pasangan.", "%1 invalid row(s) (marked red). Slope > 0; rating curve ≥ 2 pairs.").arg(bad)); return false; }
        return true;
    });
    if (sh.exec() != 1) return;
    for (size_t k = 0; k < keys.size(); ++k) {
        auto& b = f->model.boundaryRef(keys[k].river, keys[k].reach, keys[k].profile);
        auto apply = [](Boundary& dst, const Parsed& p) {
            dst.type = p.type;
            if (p.type == BoundaryType::NormalDepth) dst.slope = p.v;
            if (p.type == BoundaryType::KnownWS) dst.knownWS = p.v;
            dst.rating = p.rating;
        };
        if (!keys[k].upJ) apply(b.upstream, pu[k]);
        if (!keys[k].dnJ) apply(b.downstream, pd[k]);
    }
    s_->markEdited();
    fillBoundarySummary();
    showToast(this, tr2("Kondisi batas diterapkan", "Boundary conditions applied"));
}

void SteadyFlowView::exportCsv() {
    auto* f = flow();
    if (!f) return;
    const QString fn = QFileDialog::getSaveFileName(this, tr2("Ekspor debit", "Export flows"), QString(), "CSV (*.csv)");
    if (fn.isEmpty()) return;
    QFile out(fn);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        sheetInfo(this, tr2("Ekspor debit", "Export flows"), tr2("File tidak bisa ditulis: %1", "The file cannot be written: %1").arg(fn));
        return;
    }
    QTextStream t(&out);
    t << "River,Reach,RS";
    for (const auto& n : f->model.profileNames) t << "," << qs(n);
    t << "\n";
    for (const auto& c : f->model.changes) {
        t << qs(c.river) << "," << qs(c.reach) << "," << qs(c.rs);
        for (double q : c.flows) t << "," << QString::number(q, 'g', 12);
        t << "\n";
    }
    showToast(this, tr2("Debit diekspor", "Flows exported"));
}

void SteadyFlowView::importCsv() {
    auto* f = flow();
    if (!f) return;
    const QString fn = QFileDialog::getOpenFileName(this, tr2("Impor debit", "Import flows"), QString(), "CSV (*.csv *.txt)");
    if (fn.isEmpty()) return;
    QFile in(fn);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream t(&in);
    const QStringList head = t.readLine().split(',');
    if (head.size() < 4 || head[0].trimmed().compare("River", Qt::CaseInsensitive) != 0) {
        sheetInfo(this, tr2("Impor debit", "Import flows"), tr2("Baris judul harus: River,Reach,RS,<profil 1>,<profil 2>,…", "Header row must be: River,Reach,RS,<profile 1>,<profile 2>,…"));
        return;
    }
    SteadyFlow nf = f->model;
    nf.profileNames.clear();
    for (int i = 3; i < head.size(); ++i) nf.profileNames.push_back(ss(head[i].trimmed().left(12)));
    nf.changes.clear();
    int line = 1;
    while (!t.atEnd()) {
        ++line;
        const QStringList c = t.readLine().split(',');
        if (c.size() < 3 + static_cast<int>(nf.profileNames.size())) continue;
        FlowChange fc{ss(c[0].trimmed()), ss(c[1].trimmed()), ss(c[2].trimmed()), {}};
        for (int i = 3; i < c.size(); ++i) {
            auto v = parseUserNumber(c[i]);
            if (!v) {
                sheetInfo(this, tr2("Impor debit", "Import flows"), tr2("Nilai tidak valid di baris %1 kolom %2. Impor dibatalkan dan data tidak berubah.", "Invalid value on line %1 column %2. Import cancelled; data unchanged.").arg(line).arg(i + 1));
                return;
            }
            fc.flows.push_back(*v);
        }
        nf.changes.push_back(fc);
    }
    const int oldCount = f->model.profileCount();
    f->model.profileNames = nf.profileNames;
    f->model.changes = nf.changes;
    loading_ = true;
    nProf_->setValue(f->model.profileCount());
    loading_ = false;
    if (oldCount != f->model.profileCount()) setProfileCount(f->model.profileCount());
    s_->markEdited();
    fillTables();
    showToast(this, tr2("%1 lokasi diimpor", "%1 locations imported").arg(nf.changes.size()));
}

void SteadyFlowView::newFlowFile() {
    auto* p = s_->project();
    if (!p) return;
    QString t = tr2("Aliran steady baru", "New steady flow");
    if (!sheetGetText(this, tr2("File aliran steady baru", "New steady flow file"), tr2("Judul data aliran:", "Flow data title:"), t, 0, "flow.newfile",
                      tr2("De-RAS membuat file .f## dengan nomor kosong terkecil. File itu masuk ke daftar .prj saat Anda menyimpan.", "A new .f## file with the lowest free number is created and listed in the .prj when saved.")))
        return;
    const std::string e = p->addSteadyFlow(ss(t));
    if (e.empty()) return;
    s_->setEditFlow(qs(e));
    s_->markEdited();
    reload();
}

}  // namespace deras::ui

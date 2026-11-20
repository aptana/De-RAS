// De-RAS - ui/StructureSheets.cpp
#include "StructureSheets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

#include "Common.h"
#include "NumTable.h"
#include "ProjectSession.h"
#include "Widgets.h"
#include "engines/steady/Structures.h"
#include "io/Project.h"
#include "io/RasText.h"

namespace deras::ui {

namespace {

QString num(double v) { return qs(io::formatPlain(std::round(v * 10000) / 10000)); }

QString numList(const std::vector<double>& v) {
    QStringList l;
    for (double x : v) l << num(x);
    return l.join("  ");
}

// Numbers separated by spaces or semicolons (',' stays a decimal comma).
bool parseList(const QString& text, std::vector<double>& out) {
    out.clear();
    for (const QString& p : text.split(QRegularExpression("[\\s;]+"), Qt::SkipEmptyParts)) {
        const auto v = parseUserNumber(p);
        if (!v) return false;
        out.push_back(*v);
    }
    return true;
}

QString kindName(NodeType t) {
    switch (t) {
        case NodeType::Bridge: return tr2("Jembatan", "Bridge");
        case NodeType::Culvert: return tr2("Gorong-gorong", "Culvert");
        case NodeType::InlineStructure: return tr2("Struktur inline", "Inline structure");
        case NodeType::LateralStructure: return tr2("Struktur lateral", "Lateral structure");
        default: return qs(nodeTypeName(t));
    }
}

QString helpFor(NodeType t) {
    if (t == NodeType::LateralStructure) return QStringLiteral("geometry.lateral");
    return t == NodeType::Culvert ? QStringLiteral("geometry.culvert")
           : t == NodeType::InlineStructure ? QStringLiteral("geometry.inline") : QStringLiteral("geometry.bridge");
}

// Small editable text table (mixed text / number columns) with add and remove buttons.
struct TextTable {
    QTableWidget* t;
    QWidget* box;
    TextTable(const QStringList& headers, const QStringList& tips) {
        box = new QWidget;
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        t = new QTableWidget(0, int(headers.size()));
        t->setHorizontalHeaderLabels(headers);
        for (int c = 0; c < headers.size() && c < tips.size(); ++c) t->horizontalHeaderItem(c)->setToolTip(tips[c]);
        t->verticalHeader()->hide();
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setMinimumHeight(120);
        v->addWidget(t);
        auto* row = new QHBoxLayout;
        auto* add = makeButton(tr2("Tambah baris", "Add row"), false, "plus");
        auto* del = makeButton(tr2("Hapus baris", "Delete row"), false, "trash");
        row->addWidget(add);
        row->addWidget(del);
        row->addStretch();
        v->addLayout(row);
        QObject::connect(add, &QPushButton::clicked, t, [this]() { addRow({}); });
        QObject::connect(del, &QPushButton::clicked, t, [this]() {
            const int r = t->currentRow();
            if (r >= 0) t->removeRow(r);
        });
    }
    std::function<void(int)> onNewRow;  // fills widgets of a new row
    void addRow(const QStringList& cells) {
        const int r = t->rowCount();
        t->insertRow(r);
        for (int c = 0; c < t->columnCount(); ++c) t->setItem(r, c, new QTableWidgetItem(c < cells.size() ? cells[c] : QString()));
        if (onNewRow) onNewRow(r);
    }
    QString text(int r, int c) const { auto* i = t->item(r, c); return i ? i->text().trimmed() : QString(); }
};

const CrossSection* nearestXs(const Reach& reach, double rv, bool upstream) {
    const CrossSection* best = nullptr;
    double bd = 1e300;
    for (const auto& n : reach.nodes) {
        if (n.type != NodeType::CrossSection) continue;
        const double v = rsNumber(n.rs);
        if (std::isnan(v)) continue;
        const double d = upstream ? v - rv : rv - v;
        if (d > 0 && d < bd) { bd = d; best = &n; }
    }
    return best;
}

void defaults(CrossSection& node, const CrossSection& up, UnitSystem u) {
    const bool si = u == UnitSystem::SI;
    double zmin = 1e300, zmax = -1e300;
    for (const auto& p : up.staElev) { zmin = std::min(zmin, p.elev); zmax = std::max(zmax, p.elev); }
    const double s0 = up.staElev.front().station, s1 = up.staElev.back().station;
    const double centre = up.bankLeft && up.bankRight ? (*up.bankLeft + *up.bankRight) / 2 : (s0 + s1) / 2;
    auto& sd = node.structure;
    sd = StructureData{};
    sd.parsed = true;
    sd.weirCoef = steady::st::defaultWeirCoef(u);
    sd.dist = std::round(std::max(1.0, 0.25 * up.lenChannel));
    sd.width = std::min(si ? 10.0 : 30.0, std::max(1.0, 0.25 * up.lenChannel));
    if (node.type == NodeType::LateralStructure) {
        sd.width = si ? 3 : 10;
        sd.weirCoef = si ? 1.1 : 2.0;
        sd.dist = 0;
        const double len = std::max(1.0, std::round(0.8 * up.lenChannel));
        // crest at the higher bank of the upstream section
        double crest = zmax;
        for (const auto& p : up.staElev)
            if ((up.bankLeft && std::fabs(p.station - *up.bankLeft) < 1e-6) || (up.bankRight && std::fabs(p.station - *up.bankRight) < 1e-6))
                crest = std::min(crest, p.elev);
        crest = std::round(crest * 100) / 100;
        sd.weir = {{0, crest}, {len, crest}};
        sd.lateral.side = 1;
        return;
    }
    if (node.type == NodeType::InlineStructure) {
        sd.width = si ? 5 : 15;
        const double crest = std::round((zmin + 0.4 * (zmax - zmin)) * 100) / 100;
        sd.weir = {{s0, crest}, {s1, crest}};
        return;
    }
    const double high = zmax;
    if (node.type == NodeType::Bridge) {
        const double low = std::round(std::max(zmin + 0.6 * (zmax - zmin), high - (si ? 1.0 : 3.0)) * 100) / 100;
        sd.deckUp = {{s0, high, low}, {s1, high, low}};
    } else {
        sd.deckUp = {{s0, high, high}, {s1, high, high}};
        CulvertGroup c;
        c.name = "Culvert #1";
        c.shape = 1;
        c.rise = c.span = std::min(si ? 1.2 : 4.0, std::max(0.1, 0.5 * (zmax - zmin)));
        c.length = sd.width;
        c.upInvert = c.dnInvert = zmin;
        c.upStations = c.dnStations = {centre};
        sd.culverts = {c};
    }
    sd.deckDn = sd.deckUp;
}

bool editStructure(QWidget* host, ProjectSession* s, Reach& reach, CrossSection& node, bool isNew, int selfIndex) {
    const UnitSystem u = s->project()->units();
    const QString lu = qs(lengthUnit(u));
    auto& openings = s->project()->settings.gateOpenings;
    const bool bridge = node.type == NodeType::Bridge, culvert = node.type == NodeType::Culvert,
               inl = node.type == NodeType::InlineStructure, lat = node.type == NodeType::LateralStructure;
    const std::string latKey = io::lateralKey(reach.river, reach.reach, node.rs);
    Breach breach = lat ? io::breachFor(s->project()->settings, latKey) : Breach{};
    StructureData& sd = node.structure;
    Sheet sh(host, (isNew ? tr2("%1 baru", "New %1") : tr2("Ubah %1", "Edit %1")).arg(kindName(node.type).toLower()), helpFor(node.type), 900);
    auto* head = new QHBoxLayout;
    head->addWidget(makeBadge(tr2("Pratinjau", "Preview"), "BadgePreview"));
    head->addWidget(makeMuted(tr2("Reach %1. Satuan panjang dan elevasi: %2. Hitungan hanya untuk rezim subkritis; format blok struktur di file .g## belum diverifikasi dengan HEC-RAS.",
                                  "Reach %1. Length and elevation unit: %2. Computed for the subcritical regime only; the structure block format in the .g## file is not yet verified against HEC-RAS.")
                                  .arg(qs(reach.label()), lu)),
                  1);
    sh.body()->addLayout(head);

    auto* form = new QFormLayout;
    auto* rsEd = new QLineEdit(qs(node.rs));
    auto* descEd = new QLineEdit(qs(node.description));
    auto* distEd = new QLineEdit(num(sd.dist));
    auto* widthEd = new QLineEdit(num(sd.width));
    auto* coefEd = new QLineEdit(num(sd.weirCoef));
    form->addRow(QStringLiteral("River station"), rsEd);
    form->addRow(tr2("Deskripsi", "Description"), descEd);
    if (lat) {
        form->addRow(tr2("Jarak dari penampang hulu ke awal struktur (%1)", "Distance from the upstream section to the start of the structure (%1)").arg(lu), distEd);
        form->addRow(tr2("Lebar mercu (tegak lurus aliran sungai) (%1)", "Crest width (across the river flow) (%1)").arg(lu), widthEd);
        form->addRow(tr2("Koefisien pelimpah", "Weir coefficient"), coefEd);
    } else {
    form->addRow(tr2("Jarak dari penampang hulu ke muka hulu (%1)", "Distance from the upstream section to the upstream face (%1)").arg(lu), distEd);
    form->addRow((inl ? tr2("Lebar mercu searah aliran (%1)", "Crest width along the flow (%1)") : tr2("Lebar dek/jalan searah aliran (%1)", "Deck/road width along the flow (%1)")).arg(lu), widthEd);
    form->addRow(inl ? tr2("Koefisien pelimpah mercu", "Crest weir coefficient") : tr2("Koefisien pelimpah dek/jalan", "Deck/road weir coefficient"), coefEd);
    }
    QComboBox *sideCb = nullptr, *toCb = nullptr;
    QCheckBox* brOn = nullptr;
    QLineEdit *brSta = nullptr, *brBot = nullptr, *brW = nullptr, *brSs = nullptr, *brT0 = nullptr, *brTf = nullptr;
    if (lat) {
        sideCb = new QComboBox;
        sideCb->addItem(tr2("Tebing kiri (menghadap hilir)", "Left bank (looking downstream)"), 0);
        sideCb->addItem(tr2("Tebing kanan (menghadap hilir)", "Right bank (looking downstream)"), 1);
        sideCb->setCurrentIndex(sd.lateral.side == 0 ? 0 : 1);
        form->addRow(tr2("Posisi", "Position"), sideCb);
        toCb = new QComboBox;
        toCb->addItem(tr2("Keluar sistem (aliran hilang)", "Out of the system (flow leaves)"), QString());
        if (auto* g = s->currentGeometry())
            for (const auto& a : g->model.storageAreas)
                toCb->addItem((a.is2D ? tr2("Area 2D: ", "2D area: ") : tr2("Storage area: ", "Storage area: ")) + qs(a.name), qs(a.name));
        int ti = toCb->findData(qs(sd.lateral.toName));
        if (ti < 0 && !sd.lateral.toName.empty()) { toCb->addItem(tr2("(tidak ditemukan) ", "(not found) ") + qs(sd.lateral.toName), qs(sd.lateral.toName)); ti = toCb->count() - 1; }
        toCb->setCurrentIndex(std::max(ti, 0));
        form->addRow(tr2("Aliran keluar ke", "Flow goes to"), toCb);
    }
    sh.body()->addLayout(form);

    NumTable *deckUp = nullptr, *deckDn = nullptr, *piers = nullptr, *weir = nullptr;
    QCheckBox* sameDn = nullptr;
    bool piersEdited = false;
    std::unique_ptr<TextTable> culv, gates;
    const QStringList deckCols{tr2("Stasiun", "Station"), tr2("Elev. atas (high chord)", "High chord elev."), tr2("Elev. bawah (low chord)", "Low chord elev.")};
    const QStringList deckTips{"geometry.col.deck", "geometry.col.deck", "geometry.col.deck"};
    auto deckRows = [](const std::vector<DeckPoint>& d) {
        std::vector<std::vector<double>> r;
        for (const auto& p : d) r.push_back({p.station, p.high, p.low});
        return r;
    };
    if (!inl) {
        sh.body()->addWidget(makeCaption(culvert ? tr2("Jalan di atas gorong-gorong, sisi hulu", "Road over the culvert, upstream side")
                                                 : tr2("Dek jembatan, sisi hulu", "Bridge deck, upstream side")));
        deckUp = new NumTable(deckCols, deckTips);
        deckUp->setMinimumHeight(130);
        deckUp->setValues(deckRows(sd.deckUp));
        sh.body()->addWidget(deckUp);
        sameDn = new QCheckBox(tr2("Sisi hilir sama dengan sisi hulu", "Downstream side same as upstream"));
        sameDn->setChecked(sd.deckDn == sd.deckUp);
        sh.body()->addWidget(sameDn);
        deckDn = new NumTable(deckCols, deckTips);
        deckDn->setMinimumHeight(130);
        deckDn->setValues(deckRows(sd.deckDn));
        deckDn->setVisible(!sameDn->isChecked());
        sh.body()->addWidget(deckDn);
        QObject::connect(sameDn, &QCheckBox::toggled, deckDn, [deckDn](bool on) { deckDn->setVisible(!on); });
    }
    if (bridge) {
        sh.body()->addWidget(makeCaption(tr2("Pilar", "Piers")));
        piers = new NumTable({tr2("Stasiun tengah", "Centre station"), tr2("Lebar", "Width"), tr2("Elev. dasar", "Bottom elev."), tr2("Elev. puncak", "Top elev.")},
                             {"geometry.col.pier", "geometry.col.pier", "geometry.col.pier", "geometry.col.pier"});
        piers->setMinimumHeight(110);
        std::vector<std::vector<double>> rows;
        for (const auto& p : sd.piers) {
            const double w = p.upWidth.empty() ? 0 : *std::max_element(p.upWidth.begin(), p.upWidth.end());
            rows.push_back({p.upSta, w, p.upElev.empty() ? 0 : p.upElev.front(), p.upElev.empty() ? 0 : p.upElev.back()});
        }
        piers->setValues(rows);
        QObject::connect(piers, &NumTable::edited, piers, [&piersEdited]() { piersEdited = true; });
        sh.body()->addWidget(piers);
        sh.body()->addWidget(makeMuted(tr2("Pilar dihitung sebagai penghalang di bukaan (metode energi) dan dengan rumus Yarnell (K = 1,25). Lebar pilar yang berubah menurut elevasi diringkas menjadi lebar terbesar bila tabel ini diubah.",
                                           "Piers are treated as obstructions in the opening (energy method) and with the Yarnell equation (K = 1.25). A pier width that varies with elevation is reduced to the largest width when this table is edited.")));
    }
    if (culvert) {
        sh.body()->addWidget(makeCaption(tr2("Kelompok gorong-gorong (barrel identik)", "Culvert groups (identical barrels)")));
        culv.reset(new TextTable({tr2("Nama", "Name"), tr2("Bentuk", "Shape"), tr2("Bentang", "Span"), tr2("Tinggi / diameter", "Rise / diameter"), tr2("Panjang", "Length"),
                              QStringLiteral("n"), QStringLiteral("Ke"), QStringLiteral("Kx"), tr2("Chart FHWA", "FHWA chart"), tr2("Scale FHWA", "FHWA scale"),
                              tr2("Invert hulu", "Upstream invert"), tr2("Invert hilir", "Downstream invert"), tr2("Stasiun barrel hulu", "Upstream barrel stations"),
                              tr2("Stasiun barrel hilir (kosong = sama)", "Downstream barrel stations (empty = same)")},
                             {}));
        culv->onNewRow = [&culv](int r) {
            auto* cb = new QComboBox;
            cb->addItem(tr2("Lingkaran", "Circular"), 1);
            cb->addItem(tr2("Kotak", "Box"), 2);
            culv->t->setCellWidget(r, 1, cb);
            for (int c : {5, 6, 7, 8, 9})
                if (culv->text(r, c).isEmpty()) culv->t->item(r, c)->setText(c == 5 ? "0.013" : c == 6 ? "0.5" : c == 7 ? "1" : "1");
        };
        for (const auto& c : sd.culverts) {
            culv->addRow({qs(c.name), {}, num(c.span), num(c.rise), num(c.length), num(c.n), num(c.ke), num(c.kx), QString::number(c.chart),
                          QString::number(c.scale), num(c.upInvert), num(c.dnInvert), numList(c.upStations), c.dnStations == c.upStations ? QString() : numList(c.dnStations)});
            auto* cb = static_cast<QComboBox*>(culv->t->cellWidget(culv->t->rowCount() - 1, 1));
            if (c.shape != 1 && c.shape != 2) cb->addItem(tr2("Bentuk %1 (tidak dihitung)", "Shape %1 (not computed)").arg(c.shape), c.shape);
            cb->setCurrentIndex(cb->findData(c.shape));
        }
        sh.body()->addWidget(culv->box);
        sh.body()->addWidget(makeMuted(tr2("Satu baris = satu kelompok barrel identik; jumlah barrel = jumlah stasiun barrel. Kontrol inlet memakai persamaan FHWA HDS-5 (chart 1, 2, 8; scale 1–3), kontrol outlet memakai kehilangan aliran penuh. Bentuk lain disimpan tetapi belum dihitung.",
                                           "One row = one group of identical barrels; the number of barrels = the number of barrel stations. Inlet control uses the FHWA HDS-5 equations (charts 1, 2, 8; scales 1–3), outlet control the full-flow losses. Other shapes are kept but not computed.")));
    }
    std::map<std::string, double> oldOpen;  // gate name -> opening for this node
    if (lat) {
        sh.body()->addWidget(makeCaption(tr2("Mercu pelimpah lateral (stasiun dari awal struktur ke hilir)", "Lateral weir crest (stations from the start of the structure downstream)")));
        weir = new NumTable({tr2("Stasiun", "Station"), tr2("Elevasi mercu", "Crest elevation")}, {"geometry.col.weir", "geometry.col.weir"});
        weir->setMinimumHeight(120);
        std::vector<std::vector<double>> rows;
        for (const auto& p : sd.weir) rows.push_back({p.station, p.elev});
        weir->setValues(rows);
        sh.body()->addWidget(weir);
        sh.body()->addWidget(makeCaption(tr2("Breach (jebol tanggul)", "Breach (levee failure)")));
        brOn = new QCheckBox(tr2("Hitung breach pada plan unsteady", "Compute a breach in unsteady plans"));
        brOn->setChecked(breach.active);
        sh.body()->addWidget(brOn);
        auto* bf = new QFormLayout;
        brSta = new QLineEdit(num(breach.station));
        brBot = new QLineEdit(num(breach.bottomElev));
        brW = new QLineEdit(num(breach.bottomWidth));
        brSs = new QLineEdit(num(breach.sideSlope));
        brT0 = new QLineEdit(num(breach.startHours));
        brTf = new QLineEdit(num(breach.formHours));
        bf->addRow(tr2("Stasiun tengah breach (%1)", "Breach centre station (%1)").arg(lu), brSta);
        bf->addRow(tr2("Elevasi dasar akhir (%1)", "Final bottom elevation (%1)").arg(lu), brBot);
        bf->addRow(tr2("Lebar dasar akhir (%1)", "Final bottom width (%1)").arg(lu), brW);
        bf->addRow(tr2("Kemiringan sisi (H:V)", "Side slope (H:V)"), brSs);
        bf->addRow(tr2("Mulai (jam setelah awal simulasi)", "Start (hours after the simulation start)"), brT0);
        bf->addRow(tr2("Waktu pembentukan (jam)", "Formation time (hours)"), brTf);
        sh.body()->addLayout(bf);
        auto* frBtn = makeButton(tr2("Perkiraan Froehlich (2008)…", "Froehlich (2008) estimate…"));
        sh.body()->addWidget(frBtn);
        QObject::connect(frBtn, &QPushButton::clicked, &sh, [&, host]() {
            QString vol, hw;
            if (!sheetGetText(host, tr2("Perkiraan Froehlich", "Froehlich estimate"), tr2("Volume air di belakang tanggul (%1)", "Water volume behind the levee (%1)").arg(lu + QStringLiteral("³")), vol)) return;
            if (!sheetGetText(host, tr2("Perkiraan Froehlich", "Froehlich estimate"), tr2("Tinggi breach dari puncak ke dasar (%1)", "Breach height from crest to bottom (%1)").arg(lu), hw)) return;
            auto V = parseUserNumber(vol), H = parseUserNumber(hw);
            if (!V || !H || *V <= 0 || *H <= 0) return;
            const double k = u == UnitSystem::SI ? 1.0 : 0.3048;
            const double Vm = *V * k * k * k, Hm = *H * k;
            // Froehlich (2008): B = 0.27 Ko V^0.32 hb^0.04 (Ko = 1.0 piping), tf = 63.2 sqrt(V / (g hb^2)) s
            const double B = 0.27 * 1.0 * std::pow(Vm, 0.32) * std::pow(Hm, 0.04);
            const double tf = 63.2 * std::sqrt(Vm / (9.81 * Hm * Hm)) / 3600.0;
            brW->setText(num(B / k));
            brTf->setText(num(std::max(0.1, tf)));
            brSs->setText("0.7");
            showToast(host, tr2("Lebar dasar rata-rata dan waktu pembentukan diisi dari rumus Froehlich (2008).", "Average bottom width and formation time filled from the Froehlich (2008) equations."), "info");
        });
        sh.body()->addWidget(makeMuted(tr2("Aliran lewat mercu memakai rumus pelimpah dengan koreksi tenggelam Villemonte; muka air sungai diinterpolasi sepanjang struktur. Data breach disimpan di pengaturan proyek De-RAS (.deras.json). Dihitung hanya pada plan unsteady (Pratinjau).",
                                           "Flow over the crest uses the weir equation with the Villemonte submergence correction; the river water surface is interpolated along the structure. Breach data are stored in the De-RAS project settings (.deras.json). Computed in unsteady plans only (Preview).")));
    }
    if (inl) {
        for (const auto& gt : sd.gates) {
            auto it = openings.find(io::gateKey(reach.river, reach.reach, node.rs, gt.name));
            if (it != openings.end()) oldOpen[gt.name] = it->second;
        }
        sh.body()->addWidget(makeCaption(tr2("Mercu pelimpah", "Weir crest")));
        weir = new NumTable({tr2("Stasiun", "Station"), tr2("Elevasi mercu", "Crest elevation")}, {"geometry.col.weir", "geometry.col.weir"});
        weir->setMinimumHeight(120);
        std::vector<std::vector<double>> rows;
        for (const auto& p : sd.weir) rows.push_back({p.station, p.elev});
        weir->setValues(rows);
        sh.body()->addWidget(weir);
        sh.body()->addWidget(makeCaption(tr2("Pintu", "Gates")));
        gates.reset(new TextTable({tr2("Nama", "Name"), tr2("Lebar", "Width"), tr2("Tinggi", "Height"), tr2("Elev. ambang", "Sill elev."), tr2("Koef. pintu", "Gate coef."),
                               tr2("Stasiun tengah bukaan", "Opening centre stations"), tr2("Bukaan untuk run steady (kosong = penuh)", "Opening for steady runs (empty = full)")},
                              {}));
        gates->onNewRow = [&gates](int r) {
            if (gates->text(r, 0).isEmpty()) gates->t->item(r, 0)->setText(QStringLiteral("Gate #%1").arg(r + 1));
            if (gates->text(r, 4).isEmpty()) gates->t->item(r, 4)->setText("0.6");
        };
        for (const auto& gt : sd.gates) {
            auto it = oldOpen.find(gt.name);
            gates->addRow({qs(gt.name), num(gt.width), num(gt.height), num(gt.invert), num(gt.coef), numList(gt.stations), it != oldOpen.end() ? num(it->second) : QString()});
        }
        sh.body()->addWidget(gates->box);
        sh.body()->addWidget(makeMuted(tr2("Aliran pintu: rumus sluice gate, orifice bila rasio muka air hilir di atas 0,67, dan pelimpah di atas ambang bila muka air belum mencapai bibir pintu. Bukaan disimpan di pengaturan proyek De-RAS (.deras.json), bukan di file aliran HEC-RAS.",
                                           "Gate flow: sluice gate equation, orifice when the tailwater ratio exceeds 0.67, and weir flow over the sill when the water does not reach the gate lip. Openings are stored in the De-RAS project settings (.deras.json), not in the HEC-RAS flow file.")));
    }

    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(isNew ? tr2("Tambah", "Add") : tr2("Simpan perubahan", "Save changes"), 1, true);
    StructureData out;
    std::map<std::string, double> newOpen;
    sh.setAcceptCheck([&](int res) {
        if (res != 1) return true;
        const std::string rs = ss(rsEd->text().trimmed());
        const double rv = rsNumber(rs);
        if (rs.empty() || std::isnan(rv)) { sh.setError(tr2("River station harus berupa angka.", "The river station must be a number.")); return false; }
        for (size_t i = 0; i < reach.nodes.size(); ++i)
            if (int(i) != selfIndex && reach.nodes[i].rs == rs) { sh.setError(tr2("RS %1 sudah ada di reach ini.", "RS %1 already exists in this reach.").arg(qs(rs))); return false; }
        if (!lat && (!nearestXs(reach, rv, true) || !nearestXs(reach, rv, false))) {
            sh.setError(tr2("Struktur harus berada di antara dua penampang (RS di antara penampang hulu dan hilir).", "A structure must lie between two cross sections (RS between an upstream and a downstream section)."));
            return false;
        }
        out = sd;
        out.parsed = true;
        auto d = parseUserNumber(distEd->text()), w = parseUserNumber(widthEd->text()), c = parseUserNumber(coefEd->text());
        if (!d || *d < 0) { sh.setError(tr2("Jarak harus angka ≥ 0.", "The distance must be a number ≥ 0.")); return false; }
        if (!w || *w < 0) { sh.setError(tr2("Lebar harus angka ≥ 0.", "The width must be a number ≥ 0.")); return false; }
        if (!c || *c <= 0) { sh.setError(tr2("Koefisien pelimpah harus lebih dari 0.", "The weir coefficient must be greater than 0.")); return false; }
        out.dist = *d;
        out.width = *w;
        out.weirCoef = *c;
        auto readDeck = [&](NumTable* t, std::vector<DeckPoint>& dk) {
            std::vector<std::vector<double>> rows;
            int bad = 0;
            if (!t->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel dek bukan angka.", "%1 deck table cells are not numbers.").arg(bad)); return false; }
            dk.clear();
            for (const auto& r : rows) {
                if (r.size() < 3) { sh.setError(tr2("Setiap baris dek perlu stasiun, elevasi atas, dan elevasi bawah.", "Every deck row needs station, high chord and low chord.")); return false; }
                if (r[2] > r[1]) { sh.setError(tr2("Elevasi bawah dek tidak boleh di atas elevasi atas (stasiun %1).", "The low chord must not be above the high chord (station %1).").arg(num(r[0]))); return false; }
                if (!dk.empty() && r[0] <= dk.back().station) { sh.setError(tr2("Stasiun dek harus naik.", "Deck stations must increase.")); return false; }
                dk.push_back({r[0], r[1], r[2]});
            }
            if (dk.size() < 2) { sh.setError(tr2("Dek/jalan perlu minimal 2 titik.", "The deck/road needs at least 2 points.")); return false; }
            return true;
        };
        if (deckUp) {
            if (!readDeck(deckUp, out.deckUp)) return false;
            if (sameDn->isChecked()) out.deckDn = out.deckUp;
            else if (!readDeck(deckDn, out.deckDn)) return false;
        }
        if (piers && piersEdited) {
            std::vector<std::vector<double>> rows;
            int bad = 0;
            if (!piers->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel pilar bukan angka.", "%1 pier table cells are not numbers.").arg(bad)); return false; }
            out.piers.clear();
            for (const auto& r : rows) {
                if (r.size() < 4 || r[1] <= 0 || r[3] <= r[2]) { sh.setError(tr2("Setiap pilar perlu stasiun, lebar > 0, dan elevasi puncak di atas dasar.", "Every pier needs a station, a width > 0 and a top above the bottom.")); return false; }
                Pier p;
                p.upSta = p.dnSta = r[0];
                p.upWidth = p.dnWidth = {r[1], r[1]};
                p.upElev = p.dnElev = {r[2], r[3]};
                out.piers.push_back(p);
            }
        }
        if (culv) {
            out.culverts.clear();
            for (int r = 0; r < culv->t->rowCount(); ++r) {
                CulvertGroup cg;
                const int i = r < int(sd.culverts.size()) ? r : -1;
                if (i >= 0) cg = sd.culverts[size_t(i)];
                cg.name = ss(culv->text(r, 0));
                if (cg.name.empty()) cg.name = "Culvert #" + std::to_string(r + 1);
                cg.shape = static_cast<QComboBox*>(culv->t->cellWidget(r, 1))->currentData().toInt();
                double* f[] = {&cg.span, &cg.rise, &cg.length, &cg.n, &cg.ke, &cg.kx};
                for (int k = 0; k < 6; ++k) {
                    const QString t = culv->text(r, 2 + k);
                    if (k == 0 && t.isEmpty() && cg.shape == 1) { cg.span = 0; continue; }
                    auto v = parseUserNumber(t);
                    if (!v || *v < 0) { sh.setError(tr2("Baris gorong-gorong %1: kolom %2 harus angka ≥ 0.", "Culvert row %1: column %2 must be a number ≥ 0.").arg(r + 1).arg(culv->t->horizontalHeaderItem(2 + k)->text())); return false; }
                    *f[k] = *v;
                }
                if (cg.rise <= 0 || (cg.shape == 2 && cg.span <= 0) || cg.length <= 0 || cg.n <= 0) {
                    sh.setError(tr2("Baris gorong-gorong %1: tinggi, panjang, n (dan bentang untuk kotak) harus lebih dari 0.", "Culvert row %1: rise, length, n (and span for a box) must be greater than 0.").arg(r + 1));
                    return false;
                }
                bool ok1 = false, ok2 = false;
                cg.chart = culv->text(r, 8).toInt(&ok1);
                cg.scale = culv->text(r, 9).toInt(&ok2);
                if (!ok1 || !ok2) { sh.setError(tr2("Baris gorong-gorong %1: chart dan scale harus bilangan bulat.", "Culvert row %1: chart and scale must be whole numbers.").arg(r + 1)); return false; }
                auto ui = parseUserNumber(culv->text(r, 10)), di = parseUserNumber(culv->text(r, 11));
                if (!ui || !di) { sh.setError(tr2("Baris gorong-gorong %1: isi invert hulu dan hilir.", "Culvert row %1: enter the upstream and downstream inverts.").arg(r + 1)); return false; }
                cg.upInvert = *ui;
                cg.dnInvert = *di;
                if (!parseList(culv->text(r, 12), cg.upStations) || cg.upStations.empty() || !parseList(culv->text(r, 13), cg.dnStations)) {
                    sh.setError(tr2("Baris gorong-gorong %1: stasiun barrel ditulis sebagai angka dipisah spasi, minimal satu.", "Culvert row %1: barrel stations are numbers separated by spaces, at least one.").arg(r + 1));
                    return false;
                }
                if (cg.dnStations.empty()) cg.dnStations = cg.upStations;
                if (cg.dnStations.size() != cg.upStations.size()) { sh.setError(tr2("Baris gorong-gorong %1: jumlah stasiun hulu dan hilir harus sama.", "Culvert row %1: upstream and downstream station counts must match.").arg(r + 1)); return false; }
                out.culverts.push_back(cg);
            }
            if (out.culverts.empty()) { sh.setError(tr2("Tambahkan minimal satu kelompok gorong-gorong.", "Add at least one culvert group.")); return false; }
        }
        if (lat) {
            out.lateral.side = sideCb->currentData().toInt();
            out.lateral.toName = ss(toCb->currentData().toString());
            if (brOn->isChecked()) {
                Breach b;
                b.active = true;
                QLineEdit* eds[] = {brSta, brBot, brW, brSs, brT0, brTf};
                double* vals[] = {&b.station, &b.bottomElev, &b.bottomWidth, &b.sideSlope, &b.startHours, &b.formHours};
                for (int k = 0; k < 6; ++k) {
                    auto v = parseUserNumber(eds[k]->text());
                    if (!v || (k >= 2 && *v < 0)) { sh.setError(tr2("Data breach harus angka (lebar, kemiringan, dan waktu ≥ 0).", "Breach data must be numbers (width, slope and times ≥ 0).")); return false; }
                    *vals[k] = *v;
                }
                if (b.formHours <= 0) b.formHours = 0.01;
                breach = b;
            } else {
                breach.active = false;
            }
        }
        if (weir) {
            std::vector<std::vector<double>> rows;
            int bad = 0;
            if (!weir->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel mercu bukan angka.", "%1 crest table cells are not numbers.").arg(bad)); return false; }
            out.weir.clear();
            for (const auto& r : rows) {
                if (r.size() < 2) continue;
                if (!out.weir.empty() && r[0] <= out.weir.back().station) { sh.setError(tr2("Stasiun mercu harus naik.", "Crest stations must increase.")); return false; }
                StaElev p;
                p.station = r[0];
                p.elev = r[1];
                out.weir.push_back(p);
            }
            if (out.weir.size() < 2) { sh.setError(tr2("Mercu pelimpah perlu minimal 2 titik.", "The weir crest needs at least 2 points.")); return false; }
        }
        if (weir && !lat) {
            out.gates.clear();
            newOpen.clear();
            for (int r = 0; r < gates->t->rowCount(); ++r) {
                GateGroup gt;
                if (r < int(sd.gates.size())) gt = sd.gates[size_t(r)];
                gt.name = ss(gates->text(r, 0));
                if (gt.name.empty()) gt.name = "Gate #" + std::to_string(r + 1);
                double* f[] = {&gt.width, &gt.height, &gt.invert, &gt.coef};
                for (int k = 0; k < 4; ++k) {
                    auto v = parseUserNumber(gates->text(r, 1 + k));
                    if (!v || (k != 2 && *v <= 0)) { sh.setError(tr2("Pintu %1: kolom %2 harus angka (lebar, tinggi, koefisien > 0).", "Gate %1: column %2 must be a number (width, height, coefficient > 0).").arg(r + 1).arg(gates->t->horizontalHeaderItem(1 + k)->text())); return false; }
                    *f[k] = *v;
                }
                if (!parseList(gates->text(r, 5), gt.stations) || gt.stations.empty()) {
                    sh.setError(tr2("Pintu %1: isi stasiun tengah bukaan (angka dipisah spasi).", "Gate %1: enter the opening centre stations (numbers separated by spaces).").arg(r + 1));
                    return false;
                }
                const QString op = gates->text(r, 6);
                if (!op.isEmpty()) {
                    auto v = parseUserNumber(op);
                    if (!v || *v < 0 || *v > gt.height) { sh.setError(tr2("Pintu %1: bukaan harus antara 0 dan tinggi pintu.", "Gate %1: the opening must lie between 0 and the gate height.").arg(r + 1)); return false; }
                    newOpen[gt.name] = *v;
                    gt.opening = *v;
                } else {
                    gt.opening = -1;
                }
                out.gates.push_back(gt);
            }
        }
        return true;
    });
    if (sh.exec() != 1) return false;
    // gate openings: drop the old keys of this node, then store the new ones
    for (const auto& gt : sd.gates) openings.erase(io::gateKey(reach.river, reach.reach, node.rs, gt.name));
    node.rs = ss(rsEd->text().trimmed());
    node.description = ss(descEd->text().trimmed());
    sd = out;
    for (const auto& [name, v] : newOpen) openings[io::gateKey(reach.river, reach.reach, node.rs, name)] = v;
    if (lat) {
        io::setBreach(s->project()->settings, latKey, Breach{});  // drop the key of the old RS
        io::setBreach(s->project()->settings, io::lateralKey(reach.river, reach.reach, node.rs), breach);
        sd.lateral.breach = breach;
    }
    return true;
}

// Inserts a node so that river stations keep decreasing downstream.
void placeNode(Reach& reach, CrossSection node) {
    const double rv = rsNumber(node.rs);
    size_t at = reach.nodes.size();
    for (size_t i = 0; i < reach.nodes.size(); ++i) {
        const double v = rsNumber(reach.nodes[i].rs);
        if (!std::isnan(v) && v < rv) { at = i; break; }
    }
    reach.nodes.insert(reach.nodes.begin() + long(at), std::move(node));
}

}  // namespace

void structuresSheet(QWidget* host, ProjectSession* s, int reachUid, int kind) {
    const bool inlineKind = kind == 1, lateralKind = kind == 2;
    auto* g = s->currentGeometry();
    if (!g) { showToast(host, tr2("Belum ada file geometri.", "No geometry file yet."), "warning"); return; }
    if (g->model.reaches.empty()) { showToast(host, tr2("Gambar reach dan penampang dulu.", "Draw a reach and cross sections first."), "warning"); return; }
    auto& reaches = g->model.reaches;
    Sheet sh(host, lateralKind ? tr2("Struktur lateral", "Lateral structures") : inlineKind ? tr2("Struktur inline", "Inline structures") : tr2("Jembatan dan gorong-gorong", "Bridges and culverts"),
             lateralKind ? "geometry.lateral" : inlineKind ? "geometry.inline" : "geometry.bridge", 640);
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("Reach")));
    auto* reachCb = new QComboBox;
    int cur = 0;
    for (size_t i = 0; i < reaches.size(); ++i) {
        reachCb->addItem(qs(reaches[i].label()), reaches[i].uid);
        if (reaches[i].uid == reachUid) cur = int(i);
    }
    reachCb->setCurrentIndex(cur);
    top->addWidget(reachCb, 1);
    top->addWidget(makeBadge(tr2("Pratinjau", "Preview"), "BadgePreview"));
    sh.body()->addLayout(top);
    auto* list = new QListWidget;
    list->setMinimumHeight(180);
    sh.body()->addWidget(list);
    auto kindOk = [inlineKind, lateralKind](NodeType t) {
        return lateralKind ? t == NodeType::LateralStructure : inlineKind ? t == NodeType::InlineStructure : (t == NodeType::Bridge || t == NodeType::Culvert);
    };
    auto reach = [&]() -> Reach& { return reaches[size_t(std::max(0, reachCb->currentIndex()))]; };
    std::vector<int> rows;  // node index per list row
    auto fill = [&]() {
        list->clear();
        rows.clear();
        const Reach& r = reach();
        for (size_t i = 0; i < r.nodes.size(); ++i) {
            const auto& n = r.nodes[i];
            if (!kindOk(n.type)) continue;
            QString t = QStringLiteral("RS %1   ·   %2").arg(qs(n.rs), kindName(n.type));
            if (!n.description.empty()) t += QStringLiteral("   ·   ") + qs(n.description);
            if (!n.structure.parsed) t += tr2("   ·   data tidak terbaca (hanya disimpan)", "   ·   data not read (kept only)");
            if (n.type == NodeType::LateralStructure && n.structure.parsed)
                t += QStringLiteral("   ·   → ") + (n.structure.lateral.toName.empty() ? tr2("keluar sistem", "out of the system") : qs(n.structure.lateral.toName));
            list->addItem(t);
            rows.push_back(int(i));
        }
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    QObject::connect(reachCb, &QComboBox::currentIndexChanged, list, [&](int) { fill(); });
    auto* row = new QHBoxLayout;
    std::vector<std::pair<QPushButton*, NodeType>> adds;
    if (lateralKind) adds.push_back({makeButton(tr2("Tambah struktur lateral…", "Add lateral structure…"), false, "plus"), NodeType::LateralStructure});
    else if (inlineKind) adds.push_back({makeButton(tr2("Tambah struktur inline…", "Add inline structure…"), false, "plus"), NodeType::InlineStructure});
    else {
        adds.push_back({makeButton(tr2("Tambah jembatan…", "Add bridge…"), false, "plus"), NodeType::Bridge});
        adds.push_back({makeButton(tr2("Tambah gorong-gorong…", "Add culvert…"), false, "plus"), NodeType::Culvert});
    }
    for (auto& a : adds) row->addWidget(a.first);
    auto* edit = makeButton(tr2("Ubah…", "Edit…"));
    auto* del = makeButton(tr2("Hapus…", "Delete…"), false, "trash");
    del->setObjectName("Danger");
    row->addWidget(edit);
    row->addStretch();
    row->addWidget(del);
    sh.body()->addLayout(row);
    sh.body()->addWidget(makeMuted(tr2("Struktur ditempatkan di antara dua penampang menurut river station-nya. Klik dua kali untuk mengubah.",
                                       "A structure is placed between two cross sections by its river station. Double-click to edit.")));

    for (auto& a : adds) {
        const NodeType type = a.second;
        QObject::connect(a.first, &QPushButton::clicked, &sh, [&, type]() {
            Reach& r = reach();
            // proposed place: below the selected cross section, else the middle of the reach
            std::vector<size_t> xsIdx;
            for (size_t i = 0; i < r.nodes.size(); ++i) if (r.nodes[i].type == NodeType::CrossSection && !std::isnan(rsNumber(r.nodes[i].rs))) xsIdx.push_back(i);
            if (xsIdx.size() < 2) { showToast(host, tr2("Reach ini butuh minimal 2 penampang.", "This reach needs at least 2 cross sections."), "warning"); return; }
            size_t k = (xsIdx.size() - 1) / 2;
            for (size_t j = 0; j + 1 < xsIdx.size(); ++j) if (r.nodes[xsIdx[j]].uid == s->selectedXs()) k = j;
            if (k + 1 >= xsIdx.size()) k = xsIdx.size() - 2;
            const CrossSection& up = r.nodes[xsIdx[k]];
            const CrossSection& dn = r.nodes[xsIdx[k + 1]];
            CrossSection n;
            n.uid = nextUid();
            n.type = type;
            const double ru = rsNumber(up.rs), rd = rsNumber(dn.rs);
            double rv = std::round((ru + rd) / 2);
            if (rv >= ru || rv <= rd) rv = std::round((ru + rd) / 2 * 100) / 100;
            n.rs = io::formatPlain(rv);
            defaults(n, up, s->project()->units());
            if (type == NodeType::LateralStructure)
                for (const auto& a : g->model.storageAreas) { n.structure.lateral.toName = a.name; if (a.is2D) break; }
            if (!editStructure(host, s, r, n, true, -1)) return;
            placeNode(r, n);
            s->markEdited();
            fill();
        });
    }
    auto doEdit = [&]() {
        const int lr = list->currentRow();
        if (lr < 0 || lr >= int(rows.size())) return;
        Reach& r = reach();
        const int idx = rows[size_t(lr)];
        CrossSection n = r.nodes[size_t(idx)];
        if (!n.structure.parsed) {
            sheetInfo(host, kindName(n.type), tr2("Data struktur ini tidak terbaca oleh De-RAS, jadi tidak bisa diubah di sini. Barisnya tetap disimpan apa adanya ke file .g##.",
                                                  "De-RAS could not read this structure's data, so it cannot be edited here. Its lines are kept unchanged in the .g## file."));
            return;
        }
        const std::string oldRs = n.rs;
        if (!editStructure(host, s, r, n, false, idx)) return;
        if (n.rs != oldRs) {
            r.nodes.erase(r.nodes.begin() + idx);
            placeNode(r, n);
        } else {
            r.nodes[size_t(idx)] = n;
        }
        s->markEdited();
        fill();
    };
    QObject::connect(edit, &QPushButton::clicked, &sh, doEdit);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &sh, [&](QListWidgetItem*) { doEdit(); });
    QObject::connect(del, &QPushButton::clicked, &sh, [&]() {
        const int lr = list->currentRow();
        if (lr < 0 || lr >= int(rows.size())) return;
        Reach& r = reach();
        const int idx = rows[size_t(lr)];
        const CrossSection& n = r.nodes[size_t(idx)];
        if (!sheetConfirm(host, tr2("Hapus struktur", "Delete structure"), tr2("Hapus %1 di RS %2? Penampang di sekitarnya tetap ada.", "Delete the %1 at RS %2? The surrounding cross sections stay.").arg(kindName(n.type).toLower(), qs(n.rs)),
                          tr2("Hapus", "Delete"), true))
            return;
        for (const auto& gt : n.structure.gates) s->project()->settings.gateOpenings.erase(io::gateKey(r.river, r.reach, n.rs, gt.name));
        if (n.type == NodeType::LateralStructure) io::setBreach(s->project()->settings, io::lateralKey(r.river, r.reach, n.rs), Breach{});
        r.nodes.erase(r.nodes.begin() + idx);
        s->markEdited();
        fill();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

}  // namespace deras::ui

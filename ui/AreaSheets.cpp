// De-RAS - ui/AreaSheets.cpp
#include "AreaSheets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "Common.h"
#include "NumTable.h"
#include "ProjectSession.h"
#include "Widgets.h"
#include "io/Project.h"
#include "io/RasText.h"

namespace deras::ui {

namespace {

QString num(double v) { return qs(io::formatPlain(std::round(v * 10000) / 10000)); }

io::GeometryFile* geomOrWarn(QWidget* host, ProjectSession* s) {
    auto* g = s->currentGeometry();
    if (!g) showToast(host, tr2("Belum ada file geometri.", "No geometry file yet."), "warning");
    return g;
}

std::vector<std::vector<double>> xyRows(const std::vector<XY>& v) {
    std::vector<std::vector<double>> r;
    for (const auto& p : v) r.push_back({p.first, p.second});
    return r;
}

bool readXY(NumTable* t, std::vector<XY>& out, Sheet& sh, size_t minPts, const QString& what) {
    std::vector<std::vector<double>> rows;
    int bad = 0;
    if (!t->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel koordinat bukan angka.", "%1 coordinate cells are not numbers.").arg(bad)); return false; }
    out.clear();
    for (const auto& r : rows) if (r.size() >= 2) out.emplace_back(r[0], r[1]);
    if (out.size() < minPts) { sh.setError(tr2("%1 perlu minimal %2 titik koordinat.", "%1 needs at least %2 coordinate points.").arg(what).arg(minPts)); return false; }
    return true;
}

bool readWeir(NumTable* t, std::vector<StaElev>& out, Sheet& sh) {
    std::vector<std::vector<double>> rows;
    int bad = 0;
    if (!t->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel mercu bukan angka.", "%1 crest table cells are not numbers.").arg(bad)); return false; }
    out.clear();
    for (const auto& r : rows) {
        if (r.size() < 2) continue;
        if (!out.empty() && r[0] <= out.back().station) { sh.setError(tr2("Stasiun mercu harus naik.", "Crest stations must increase.")); return false; }
        StaElev p;
        p.station = r[0];
        p.elev = r[1];
        out.push_back(p);
    }
    if (out.size() < 2) { sh.setError(tr2("Mercu pelimpah perlu minimal 2 titik.", "The weir crest needs at least 2 points.")); return false; }
    return true;
}

double polyArea(const std::vector<XY>& p) {
    double a = 0;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) a += (p[j].first + p[i].first) * (p[j].second - p[i].second);
    return std::fabs(a) / 2;
}

double lineLength(const std::vector<XY>& l) {
    double s = 0;
    for (size_t i = 1; i < l.size(); ++i) s += std::hypot(l[i].first - l[i - 1].first, l[i].second - l[i - 1].second);
    return s;
}

// Breach fields shared by lateral structures and connections.
struct BreachForm {
    QCheckBox* on;
    QLineEdit *sta, *bot, *w, *ss, *t0, *tf;
    BreachForm(QVBoxLayout* body, const Breach& b, const QString& lu) {
        body->addWidget(makeCaption(tr2("Breach (jebol)", "Breach")));
        on = new QCheckBox(tr2("Hitung breach pada plan unsteady", "Compute a breach in unsteady plans"));
        on->setChecked(b.active);
        body->addWidget(on);
        auto* f = new QFormLayout;
        sta = new QLineEdit(num(b.station));
        bot = new QLineEdit(num(b.bottomElev));
        w = new QLineEdit(num(b.bottomWidth));
        ss = new QLineEdit(num(b.sideSlope));
        t0 = new QLineEdit(num(b.startHours));
        tf = new QLineEdit(num(b.formHours));
        f->addRow(tr2("Stasiun tengah breach (%1)", "Breach centre station (%1)").arg(lu), sta);
        f->addRow(tr2("Elevasi dasar akhir (%1)", "Final bottom elevation (%1)").arg(lu), bot);
        f->addRow(tr2("Lebar dasar akhir (%1)", "Final bottom width (%1)").arg(lu), w);
        f->addRow(tr2("Kemiringan sisi (H:V)", "Side slope (H:V)"), ss);
        f->addRow(tr2("Mulai (jam setelah awal simulasi)", "Start (hours after the simulation start)"), t0);
        f->addRow(tr2("Waktu pembentukan (jam)", "Formation time (hours)"), tf);
        body->addLayout(f);
    }
    bool read(Breach& b, Sheet& sh) const {
        if (!on->isChecked()) { b.active = false; return true; }
        Breach o;
        o.active = true;
        QLineEdit* eds[] = {sta, bot, w, ss, t0, tf};
        double* vals[] = {&o.station, &o.bottomElev, &o.bottomWidth, &o.sideSlope, &o.startHours, &o.formHours};
        for (int k = 0; k < 6; ++k) {
            auto v = parseUserNumber(eds[k]->text());
            if (!v || (k >= 2 && *v < 0)) { sh.setError(tr2("Data breach harus angka (lebar, kemiringan, dan waktu ≥ 0).", "Breach data must be numbers (width, slope and times ≥ 0).")); return false; }
            *vals[k] = *v;
        }
        if (o.formHours <= 0) o.formHours = 0.01;
        b = o;
        return true;
    }
};

QHBoxLayout* listButtons(QPushButton*& add, QPushButton*& edit, QPushButton*& del, const QString& addText) {
    auto* row = new QHBoxLayout;
    add = makeButton(addText, false, "plus");
    edit = makeButton(tr2("Ubah…", "Edit…"));
    del = makeButton(tr2("Hapus…", "Delete…"), false, "trash");
    del->setObjectName("Danger");
    row->addWidget(add);
    row->addWidget(edit);
    row->addStretch();
    row->addWidget(del);
    return row;
}

QString uniqueName(const Geometry& g, const QString& base) {
    for (int i = 1;; ++i) {
        const QString n = QStringLiteral("%1 %2").arg(base).arg(i);
        if (!g.findArea(ss(n))) return n;
    }
}

// ---------------- area editor ----------------
bool editArea(QWidget* host, ProjectSession* s, StorageArea& a, bool isNew) {
    auto* g = s->currentGeometry();
    const UnitSystem u = s->project()->units();
    const QString lu = qs(lengthUnit(u));
    const bool twoD = a.is2D;
    const QString kind = twoD ? tr2("area 2D", "2D area") : tr2("storage area", "storage area");
    Sheet sh(host, (isNew ? tr2("%1 baru", "New %1") : tr2("Ubah %1", "Edit %1")).arg(kind), twoD ? "geometry.area2d" : "geometry.storage", 720);
    auto* head = new QHBoxLayout;
    head->addWidget(makeBadge(tr2("Pratinjau", "Preview"), "BadgePreview"));
    head->addWidget(makeMuted(twoD ? tr2("Grid persegi dengan tabel subgrid dari terrain RAS Mapper; persamaan gelombang difusi implisit. Format blok di file .g## belum diverifikasi dengan HEC-RAS.",
                                         "Square grid with subgrid tables from the RAS Mapper terrain; implicit diffusion-wave equations. The .g## block format is not yet verified against HEC-RAS.")
                                   : tr2("Kolam datar (level pool): satu muka air untuk seluruh area. Format blok di file .g## belum diverifikasi dengan HEC-RAS.",
                                         "Level pool: one water surface for the whole area. The .g## block format is not yet verified against HEC-RAS.")),
                  1);
    sh.body()->addLayout(head);
    auto* form = new QFormLayout;
    auto* nameEd = new QLineEdit(qs(a.name));
    nameEd->setMaxLength(16);
    form->addRow(tr2("Nama (maks. 16 karakter)", "Name (max. 16 characters)"), nameEd);
    QLineEdit *cellEd = nullptr, *nEd = nullptr, *areaEd = nullptr, *minEd = nullptr;
    QComboBox* method = nullptr;
    NumTable* vol = nullptr;
    if (twoD) {
        cellEd = new QLineEdit(a.cellSize > 0 ? num(a.cellSize) : QString());
        cellEd->setPlaceholderText(tr2("kosong = otomatis (sisi terpanjang / 60)", "empty = automatic (longest side / 60)"));
        nEd = new QLineEdit(num(a.manningN));
        form->addRow(tr2("Ukuran sel (%1)", "Cell size (%1)").arg(lu), cellEd);
        form->addRow(tr2("n Manning area", "Area Manning's n"), nEd);
    } else {
        method = new QComboBox;
        method->addItem(tr2("Tabel elevasi–volume", "Elevation–volume table"), 0);
        method->addItem(tr2("Luas × kedalaman di atas elevasi minimum", "Area × depth above a minimum elevation"), 1);
        method->addItem(tr2("Dari terrain di dalam poligon (saat simulasi)", "From the terrain inside the polygon (at run time)"), 2);
        method->setCurrentIndex(a.volElev.size() >= 2 ? 0 : a.area > 0 ? 1 : (a.polygon.size() >= 3 ? 2 : 0));
        form->addRow(tr2("Volume dari", "Volume from"), method);
        areaEd = new QLineEdit(a.area > 0 ? num(a.area) : (a.polygon.size() >= 3 ? num(std::round(polyArea(a.polygon))) : QString()));
        minEd = new QLineEdit(num(a.minElev));
        form->addRow(tr2("Luas (%1)", "Area (%1)").arg(lu + QStringLiteral("²")), areaEd);
        form->addRow(tr2("Elevasi minimum (%1)", "Minimum elevation (%1)").arg(lu), minEd);
    }
    sh.body()->addLayout(form);
    if (!twoD) {
        sh.body()->addWidget(makeCaption(tr2("Tabel elevasi–volume", "Elevation–volume table")));
        vol = new NumTable({tr2("Elevasi", "Elevation"), tr2("Volume (%1)", "Volume (%1)").arg(lu + QStringLiteral("³"))}, {"geometry.col.volume", "geometry.col.volume"});
        vol->setMinimumHeight(130);
        std::vector<std::vector<double>> rows;
        for (const auto& p : a.volElev) rows.push_back({p.first, p.second});
        vol->setValues(rows);
        sh.body()->addWidget(vol);
        auto sync = [method, vol, areaEd, minEd]() {
            const int m = method->currentData().toInt();
            vol->setEnabled(m == 0);
            areaEd->setEnabled(m == 1);
            minEd->setEnabled(m == 1);
        };
        sync();
        QObject::connect(method, &QComboBox::currentIndexChanged, vol, [sync](int) { sync(); });
    }
    sh.body()->addWidget(makeCaption(tr2("Poligon batas (koordinat peta)", "Boundary polygon (map coordinates)")));
    auto* poly = new NumTable({QStringLiteral("X"), QStringLiteral("Y")}, {"geometry.col.xy", "geometry.col.xy"});
    poly->setMinimumHeight(150);
    poly->setValues(xyRows(a.polygon));
    sh.body()->addWidget(poly);
    sh.body()->addWidget(makeMuted(twoD ? tr2("Area 2D butuh terrain di RAS Mapper yang menutupi poligon. Sel dibuat di dalam poligon; aliran masuk lewat struktur lateral, koneksi, BC line, hujan, atau pompa.",
                                              "A 2D area needs a RAS Mapper terrain covering the polygon. Cells are made inside the polygon; water enters through lateral structures, connections, BC lines, precipitation or pumps.")
                                        : tr2("Poligon bisa digambar di kanvas (Gambar storage area) atau diketik/ditempel di tabel ini.",
                                              "The polygon can be drawn on the canvas (Draw storage area) or typed/pasted into this table.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(isNew ? tr2("Tambah", "Add") : tr2("Simpan perubahan", "Save changes"), 1, true);
    StorageArea out;
    sh.setAcceptCheck([&](int r) {
        if (r != 1) return true;
        out = a;
        out.name = ss(nameEd->text().trimmed());
        if (out.name.empty()) { sh.setError(tr2("Isi nama.", "Enter a name.")); return false; }
        for (const auto& o : g->model.storageAreas)
            if (&o != &a && o.name == out.name) { sh.setError(tr2("Nama '%1' sudah dipakai.", "The name '%1' is already used.").arg(qs(out.name))); return false; }
        if (!readXY(poly, out.polygon, sh, twoD ? 3 : 0, tr2("Poligon", "The polygon"))) return false;
        if (!twoD && !out.polygon.empty() && out.polygon.size() < 3) { sh.setError(tr2("Poligon perlu minimal 3 titik (atau kosong).", "The polygon needs at least 3 points (or none).")); return false; }
        if (twoD) {
            if (cellEd->text().trimmed().isEmpty()) out.cellSize = 0;
            else {
                auto v = parseUserNumber(cellEd->text());
                if (!v || *v <= 0) { sh.setError(tr2("Ukuran sel harus lebih dari 0.", "The cell size must be greater than 0.")); return false; }
                out.cellSize = *v;
            }
            auto n = parseUserNumber(nEd->text());
            if (!n || *n <= 0) { sh.setError(tr2("n Manning harus lebih dari 0.", "Manning's n must be greater than 0.")); return false; }
            out.manningN = *n;
        } else {
            const int m = method->currentData().toInt();
            out.volElev.clear();
            out.area = 0;
            if (m == 0) {
                std::vector<std::vector<double>> rows;
                int bad = 0;
                if (!vol->values(rows, &bad)) { sh.setError(tr2("%1 sel tabel volume bukan angka.", "%1 volume cells are not numbers.").arg(bad)); return false; }
                for (const auto& rr : rows) {
                    if (rr.size() < 2) continue;
                    if (!out.volElev.empty() && (rr[0] <= out.volElev.back().first || rr[1] < out.volElev.back().second)) {
                        sh.setError(tr2("Elevasi harus naik dan volume tidak boleh turun.", "Elevations must increase and volumes must not decrease."));
                        return false;
                    }
                    out.volElev.emplace_back(rr[0], rr[1]);
                }
                if (out.volElev.size() < 2) { sh.setError(tr2("Tabel elevasi–volume perlu minimal 2 baris.", "The elevation–volume table needs at least 2 rows.")); return false; }
                out.minElev = out.volElev.front().first;
            } else if (m == 1) {
                auto ar = parseUserNumber(areaEd->text()), mn = parseUserNumber(minEd->text());
                if (!ar || *ar <= 0 || !mn) { sh.setError(tr2("Isi luas (> 0) dan elevasi minimum.", "Enter the area (> 0) and the minimum elevation.")); return false; }
                out.area = *ar;
                out.minElev = *mn;
            } else if (out.polygon.size() < 3) {
                sh.setError(tr2("Volume dari terrain butuh poligon.", "A volume from the terrain needs a polygon."));
                return false;
            }
        }
        return true;
    });
    if (sh.exec() != 1) return false;
    const std::string oldName = a.name;
    a = out;
    if (!isNew && oldName != a.name) {
        // keep references to the area
        for (auto& r : g->model.reaches)
            for (auto& n : r.nodes)
                if (n.type == NodeType::LateralStructure && n.structure.lateral.toName == oldName) n.structure.lateral.toName = a.name;
        for (auto& c : g->model.connections) {
            if (c.upName == oldName) c.upName = a.name;
            if (c.dnName == oldName) c.dnName = a.name;
        }
        for (auto& b : g->model.bcLines) if (b.area == oldName) b.area = a.name;
    }
    return true;
}

}  // namespace

void areasSheet(QWidget* host, ProjectSession* s, bool twoD, const QString& select) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    Sheet sh(host, twoD ? tr2("Area aliran 2D", "2D flow areas") : tr2("Storage area", "Storage areas"), twoD ? "geometry.area2d" : "geometry.storage", 620);
    auto* list = new QListWidget;
    list->setMinimumHeight(170);
    std::vector<size_t> rows;
    const QString lu = qs(lengthUnit(s->project()->units()));
    auto fill = [&](const QString& sel) {
        list->clear();
        rows.clear();
        for (size_t i = 0; i < g->model.storageAreas.size(); ++i) {
            const auto& a = g->model.storageAreas[i];
            if (a.is2D != twoD) continue;
            QString t = qs(a.name);
            if (twoD) t += QStringLiteral("   ·   ") + tr2("sel %1 %2, n %3", "cell %1 %2, n %3").arg(a.cellSize > 0 ? num(a.cellSize) : tr2("otomatis", "auto"), lu, num(a.manningN));
            else t += QStringLiteral("   ·   ") + (a.volElev.size() >= 2 ? tr2("tabel volume %1 baris", "volume table %1 rows").arg(a.volElev.size())
                                                    : a.area > 0 ? tr2("luas %1 %2", "area %1 %2").arg(num(a.area), lu + QStringLiteral("²")) : tr2("volume dari terrain", "volume from terrain"));
            list->addItem(t);
            rows.push_back(i);
            if (qs(a.name) == sel) list->setCurrentRow(list->count() - 1);
        }
        if (list->count() && list->currentRow() < 0) list->setCurrentRow(0);
    };
    fill(select);
    sh.body()->addWidget(list);
    QPushButton *add, *edit, *del;
    sh.body()->addLayout(listButtons(add, edit, del, twoD ? tr2("Tambah area 2D…", "Add 2D area…") : tr2("Tambah storage area…", "Add storage area…")));
    sh.body()->addWidget(makeMuted(tr2("Tip: gambar poligon langsung di kanvas dengan tombol Gambar di atas kanvas.", "Tip: draw the polygon directly on the canvas with the Draw buttons above it.")));
    QObject::connect(add, &QPushButton::clicked, &sh, [&]() {
        StorageArea a;
        a.is2D = twoD;
        a.name = ss(uniqueName(g->model, twoD ? QStringLiteral("Area2D") : QStringLiteral("SA")));
        if (!editArea(host, s, a, true)) return;
        g->model.storageAreas.push_back(a);
        s->markEdited();
        fill(qs(a.name));
    });
    auto doEdit = [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(rows.size())) return;
        if (editArea(host, s, g->model.storageAreas[rows[size_t(r)]], false)) { s->markEdited(); fill(qs(g->model.storageAreas[rows[size_t(r)]].name)); }
    };
    QObject::connect(edit, &QPushButton::clicked, &sh, doEdit);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &sh, [&](QListWidgetItem*) { doEdit(); });
    QObject::connect(del, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(rows.size())) return;
        const std::string name = g->model.storageAreas[rows[size_t(r)]].name;
        int refs = 0;
        for (const auto& rc : g->model.reaches) for (const auto& n : rc.nodes) refs += n.type == NodeType::LateralStructure && n.structure.lateral.toName == name;
        for (const auto& c : g->model.connections) refs += c.upName == name || c.dnName == name;
        for (const auto& b : g->model.bcLines) refs += b.area == name;
        QString text = tr2("Hapus %1?", "Delete %1?").arg(qs(name));
        if (refs) text += QStringLiteral(" ") + tr2("%1 struktur lateral, koneksi, atau BC line masih menunjuk area ini; Model Doctor akan menandainya.",
                                                    "%1 lateral structures, connections or BC lines still refer to this area; Model Doctor will flag them.").arg(refs);
        if (!sheetConfirm(host, tr2("Hapus area", "Delete area"), text, tr2("Hapus", "Delete"), true)) return;
        g->model.storageAreas.erase(g->model.storageAreas.begin() + long(rows[size_t(r)]));
        s->markEdited();
        fill({});
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

void addDrawnArea(QWidget* host, ProjectSession* s, bool twoD, const std::vector<XY>& polygon) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    StorageArea a;
    a.is2D = twoD;
    a.name = ss(uniqueName(g->model, twoD ? QStringLiteral("Area2D") : QStringLiteral("SA")));
    a.polygon = polygon;
    if (twoD) a.cellSize = std::round(std::sqrt(polyArea(polygon)) / 40.0);
    if (!editArea(host, s, a, true)) return;
    g->model.storageAreas.push_back(a);
    s->markEdited();
}

void editAreaByName(QWidget* host, ProjectSession* s, const QString& name) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    StorageArea* a = g->model.findArea(ss(name));
    if (!a) return;
    if (editArea(host, s, *a, false)) s->markEdited();
}

// ---------------- connections ----------------
namespace {
bool editConnection(QWidget* host, ProjectSession* s, SaConnection& c, bool isNew) {
    auto* g = s->currentGeometry();
    const QString lu = qs(lengthUnit(s->project()->units()));
    const std::string oldKey = io::connectionKey(c.name);
    Breach breach = io::breachFor(s->project()->settings, oldKey);
    Sheet sh(host, isNew ? tr2("Koneksi baru", "New connection") : tr2("Ubah koneksi", "Edit connection"), "geometry.connection", 720);
    auto* head = new QHBoxLayout;
    head->addWidget(makeBadge(tr2("Pratinjau", "Preview"), "BadgePreview"));
    head->addWidget(makeMuted(tr2("Pelimpah antara dua storage area / area 2D (rumus pelimpah, koreksi tenggelam Villemonte, aliran dua arah).",
                                  "Weir between two storage / 2D areas (weir equation, Villemonte submergence, flow both ways).")),
                    1);
    sh.body()->addLayout(head);
    auto* form = new QFormLayout;
    auto* nameEd = new QLineEdit(qs(c.name));
    nameEd->setMaxLength(16);
    auto* up = new QComboBox;
    auto* dn = new QComboBox;
    for (const auto& a : g->model.storageAreas) { up->addItem(qs(a.name), qs(a.name)); dn->addItem(qs(a.name), qs(a.name)); }
    up->setCurrentIndex(std::max(0, up->findData(qs(c.upName))));
    dn->setCurrentIndex(std::max(0, dn->findData(qs(c.dnName))));
    if (isNew && dn->count() > 1 && dn->currentIndex() == up->currentIndex()) dn->setCurrentIndex(1);
    auto* coefEd = new QLineEdit(num(c.coef));
    auto* widthEd = new QLineEdit(num(c.width));
    form->addRow(tr2("Nama", "Name"), nameEd);
    form->addRow(tr2("Area hulu (aliran positif dari sini)", "Upstream area (positive flow from here)"), up);
    form->addRow(tr2("Area hilir", "Downstream area"), dn);
    form->addRow(tr2("Koefisien pelimpah", "Weir coefficient"), coefEd);
    form->addRow(tr2("Lebar mercu (%1)", "Crest width (%1)").arg(lu), widthEd);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeCaption(tr2("Mercu pelimpah (stasiun sepanjang garis koneksi)", "Weir crest (stations along the connection line)")));
    auto* weir = new NumTable({tr2("Stasiun", "Station"), tr2("Elevasi mercu", "Crest elevation")}, {"geometry.col.weir", "geometry.col.weir"});
    weir->setMinimumHeight(120);
    std::vector<std::vector<double>> rows;
    for (const auto& p : c.weir) rows.push_back({p.station, p.elev});
    weir->setValues(rows);
    sh.body()->addWidget(weir);
    sh.body()->addWidget(makeCaption(tr2("Garis koneksi (koordinat peta)", "Connection line (map coordinates)")));
    auto* line = new NumTable({QStringLiteral("X"), QStringLiteral("Y")}, {"geometry.col.xy", "geometry.col.xy"});
    line->setMinimumHeight(110);
    line->setValues(xyRows(c.line));
    sh.body()->addWidget(line);
    BreachForm bf(sh.body(), breach, lu);
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(isNew ? tr2("Tambah", "Add") : tr2("Simpan perubahan", "Save changes"), 1, true);
    SaConnection out;
    sh.setAcceptCheck([&](int r) {
        if (r != 1) return true;
        out = c;
        out.name = ss(nameEd->text().trimmed());
        if (out.name.empty()) { sh.setError(tr2("Isi nama.", "Enter a name.")); return false; }
        for (const auto& o : g->model.connections)
            if (&o != &c && o.name == out.name) { sh.setError(tr2("Nama '%1' sudah dipakai.", "The name '%1' is already used.").arg(qs(out.name))); return false; }
        out.upName = ss(up->currentData().toString());
        out.dnName = ss(dn->currentData().toString());
        if (out.upName.empty() || out.upName == out.dnName) { sh.setError(tr2("Pilih dua area yang berbeda.", "Choose two different areas.")); return false; }
        auto cf = parseUserNumber(coefEd->text()), w = parseUserNumber(widthEd->text());
        if (!cf || *cf <= 0 || !w || *w < 0) { sh.setError(tr2("Koefisien harus > 0 dan lebar ≥ 0.", "The coefficient must be > 0 and the width ≥ 0.")); return false; }
        out.coef = *cf;
        out.width = *w;
        if (!readWeir(weir, out.weir, sh)) return false;
        if (!readXY(line, out.line, sh, 0, tr2("Garis koneksi", "The connection line"))) return false;
        if (!bf.read(breach, sh)) return false;
        return true;
    });
    if (sh.exec() != 1) return false;
    c = out;
    io::setBreach(s->project()->settings, oldKey, Breach{});
    io::setBreach(s->project()->settings, io::connectionKey(c.name), breach);
    c.breach = breach;
    return true;
}
}  // namespace

void connectionsSheet(QWidget* host, ProjectSession* s) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    Sheet sh(host, tr2("Koneksi SA/2D", "SA/2D connections"), "geometry.connection", 620);
    auto* list = new QListWidget;
    list->setMinimumHeight(170);
    auto fill = [&]() {
        list->clear();
        for (const auto& c : g->model.connections) list->addItem(QStringLiteral("%1   ·   %2 → %3").arg(qs(c.name), qs(c.upName), qs(c.dnName)));
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    sh.body()->addWidget(list);
    QPushButton *add, *edit, *del;
    sh.body()->addLayout(listButtons(add, edit, del, tr2("Tambah koneksi…", "Add connection…")));
    QObject::connect(add, &QPushButton::clicked, &sh, [&]() {
        if (g->model.storageAreas.size() < 2) { showToast(host, tr2("Koneksi butuh minimal dua storage area atau area 2D.", "A connection needs at least two storage or 2D areas."), "warning"); return; }
        SaConnection c;
        c.name = "Conn " + std::to_string(g->model.connections.size() + 1);
        c.upName = g->model.storageAreas[0].name;
        c.dnName = g->model.storageAreas[1].name;
        c.weir = {{0, 0}, {10, 0}};
        if (!editConnection(host, s, c, true)) return;
        g->model.connections.push_back(c);
        s->markEdited();
        fill();
    });
    auto doEdit = [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(g->model.connections.size())) return;
        if (editConnection(host, s, g->model.connections[size_t(r)], false)) { s->markEdited(); fill(); }
    };
    QObject::connect(edit, &QPushButton::clicked, &sh, doEdit);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &sh, [&](QListWidgetItem*) { doEdit(); });
    QObject::connect(del, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(g->model.connections.size())) return;
        if (!sheetConfirm(host, tr2("Hapus koneksi", "Delete connection"), tr2("Hapus %1?", "Delete %1?").arg(qs(g->model.connections[size_t(r)].name)), tr2("Hapus", "Delete"), true)) return;
        io::setBreach(s->project()->settings, io::connectionKey(g->model.connections[size_t(r)].name), Breach{});
        g->model.connections.erase(g->model.connections.begin() + r);
        s->markEdited();
        fill();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

void addDrawnConnection(QWidget* host, ProjectSession* s, const std::vector<XY>& line) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    if (g->model.storageAreas.size() < 2) { showToast(host, tr2("Koneksi butuh minimal dua storage area atau area 2D.", "A connection needs at least two storage or 2D areas."), "warning"); return; }
    SaConnection c;
    c.name = "Conn " + std::to_string(g->model.connections.size() + 1);
    c.line = line;
    c.upName = g->model.storageAreas[0].name;
    c.dnName = g->model.storageAreas[1].name;
    const double L = std::max(1.0, std::round(lineLength(line)));
    c.weir = {{0, 0}, {L, 0}};
    if (!editConnection(host, s, c, true)) return;
    g->model.connections.push_back(c);
    s->markEdited();
}

// ---------------- BC lines ----------------
namespace {
bool editBcLine(QWidget* host, ProjectSession* s, BcLine& b, bool isNew) {
    auto* g = s->currentGeometry();
    Sheet sh(host, isNew ? tr2("BC line baru", "New BC line") : tr2("Ubah BC line", "Edit BC line"), "geometry.bcline", 560);
    auto* form = new QFormLayout;
    auto* nameEd = new QLineEdit(qs(b.name));
    nameEd->setMaxLength(32);
    auto* area = new QComboBox;
    for (const auto& a : g->model.storageAreas) if (a.is2D) area->addItem(qs(a.name), qs(a.name));
    area->setCurrentIndex(std::max(0, area->findData(qs(b.area))));
    form->addRow(tr2("Nama", "Name"), nameEd);
    form->addRow(tr2("Area 2D", "2D area"), area);
    sh.body()->addLayout(form);
    auto* line = new NumTable({QStringLiteral("X"), QStringLiteral("Y")}, {"geometry.col.xy", "geometry.col.xy"});
    line->setMinimumHeight(140);
    line->setValues(xyRows(b.line));
    sh.body()->addWidget(line);
    sh.body()->addWidget(makeMuted(tr2("Garis digambar di tepi area 2D. Jenis kondisi batasnya (hidrograf debit, muka air, normal depth) diisi di Data Aliran Unsteady.",
                                       "Draw the line along the edge of the 2D area. Its boundary type (flow or stage hydrograph, normal depth) is set in Unsteady Flow Data.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(isNew ? tr2("Tambah", "Add") : tr2("Simpan perubahan", "Save changes"), 1, true);
    BcLine out;
    sh.setAcceptCheck([&](int r) {
        if (r != 1) return true;
        out = b;
        out.name = ss(nameEd->text().trimmed());
        out.area = ss(area->currentData().toString());
        if (out.name.empty() || out.area.empty()) { sh.setError(tr2("Isi nama dan pilih area 2D.", "Enter a name and choose a 2D area.")); return false; }
        for (const auto& o : g->model.bcLines)
            if (&o != &b && o.name == out.name && o.area == out.area) { sh.setError(tr2("Nama '%1' sudah dipakai di area ini.", "The name '%1' is already used in this area.").arg(qs(out.name))); return false; }
        return readXY(line, out.line, sh, 2, tr2("BC line", "The BC line"));
    });
    if (sh.exec() != 1) return false;
    b = out;
    return true;
}
}  // namespace

void bcLinesSheet(QWidget* host, ProjectSession* s) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    Sheet sh(host, tr2("BC line area 2D", "2D area BC lines"), "geometry.bcline", 560);
    auto* list = new QListWidget;
    list->setMinimumHeight(160);
    auto fill = [&]() {
        list->clear();
        for (const auto& b : g->model.bcLines) list->addItem(QStringLiteral("%1   ·   %2").arg(qs(b.name), qs(b.area)));
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    sh.body()->addWidget(list);
    QPushButton *add, *edit, *del;
    sh.body()->addLayout(listButtons(add, edit, del, tr2("Tambah BC line…", "Add BC line…")));
    QObject::connect(add, &QPushButton::clicked, &sh, [&]() {
        BcLine b;
        b.name = "BC " + std::to_string(g->model.bcLines.size() + 1);
        if (!editBcLine(host, s, b, true)) return;
        g->model.bcLines.push_back(b);
        s->markEdited();
        fill();
    });
    auto doEdit = [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(g->model.bcLines.size())) return;
        if (editBcLine(host, s, g->model.bcLines[size_t(r)], false)) { s->markEdited(); fill(); }
    };
    QObject::connect(edit, &QPushButton::clicked, &sh, doEdit);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &sh, [&](QListWidgetItem*) { doEdit(); });
    QObject::connect(del, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(g->model.bcLines.size())) return;
        if (!sheetConfirm(host, tr2("Hapus BC line", "Delete BC line"), tr2("Hapus %1?", "Delete %1?").arg(qs(g->model.bcLines[size_t(r)].name)), tr2("Hapus", "Delete"), true)) return;
        g->model.bcLines.erase(g->model.bcLines.begin() + r);
        s->markEdited();
        fill();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

void addDrawnBcLine(QWidget* host, ProjectSession* s, const std::vector<XY>& line) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    BcLine b;
    b.name = "BC " + std::to_string(g->model.bcLines.size() + 1);
    b.line = line;
    // the 2D area nearest to the line's midpoint
    if (!line.empty()) {
        const XY m = line[line.size() / 2];
        double best = 1e300;
        for (const auto& a : g->model.storageAreas) {
            if (!a.is2D) continue;
            for (const auto& p : a.polygon) {
                const double d = std::hypot(p.first - m.first, p.second - m.second);
                if (d < best) { best = d; b.area = a.name; }
            }
        }
    }
    if (!editBcLine(host, s, b, true)) return;
    g->model.bcLines.push_back(b);
    s->markEdited();
}

// ---------------- pumps ----------------
namespace {
struct EndEditor {
    QComboBox* kind;
    QComboBox* target;
    QLineEdit* rs;
    QLineEdit *x, *y;
    EndEditor(QFormLayout* f, const QString& label, const PumpEnd& e, const Geometry& g, bool allowOut) {
        kind = new QComboBox;
        kind->addItem(tr2("Penampang reach", "Reach cross section"), 0);
        kind->addItem(tr2("Storage area / area 2D", "Storage / 2D area"), 1);
        if (allowOut) kind->addItem(tr2("Keluar sistem", "Out of the system"), 2);
        kind->setCurrentIndex(std::max(0, kind->findData(e.kind)));
        target = new QComboBox;
        rs = new QLineEdit(qs(e.rs));
        rs->setPlaceholderText(QStringLiteral("RS"));
        x = new QLineEdit(num(e.point.first));
        y = new QLineEdit(num(e.point.second));
        auto* row = new QHBoxLayout;
        row->addWidget(kind);
        row->addWidget(target, 1);
        row->addWidget(rs);
        f->addRow(label, row);
        auto* pr = new QHBoxLayout;
        pr->addWidget(new QLabel(QStringLiteral("X")));
        pr->addWidget(x);
        pr->addWidget(new QLabel(QStringLiteral("Y")));
        pr->addWidget(y);
        f->addRow(tr2("  Titik di area 2D", "  Point in a 2D area"), pr);
        auto refill = [this, &g, e]() {
            target->clear();
            const int k = kind->currentData().toInt();
            if (k == 0) for (const auto& r : g.reaches) target->addItem(qs(r.label()), qs(r.river) + "\n" + qs(r.reach));
            if (k == 1) for (const auto& a : g.storageAreas) target->addItem(qs(a.name), qs(a.name));
            const QString want = k == 0 ? qs(e.river) + "\n" + qs(e.reach) : qs(e.area);
            const int i = target->findData(want);
            if (i >= 0) target->setCurrentIndex(i);
            target->setEnabled(k != 2);
            rs->setEnabled(k == 0);
            x->setEnabled(k == 1);
            y->setEnabled(k == 1);
        };
        refill();
        QObject::connect(kind, &QComboBox::currentIndexChanged, target, [refill](int) { refill(); });
    }
    bool read(PumpEnd& e) const {
        e.kind = kind->currentData().toInt();
        if (e.kind == 0) {
            const QStringList p = target->currentData().toString().split('\n');
            if (p.size() != 2 || rs->text().trimmed().isEmpty()) return false;
            e.river = ss(p[0]);
            e.reach = ss(p[1]);
            e.rs = ss(rs->text().trimmed());
        } else if (e.kind == 1) {
            e.area = ss(target->currentData().toString());
            if (e.area.empty()) return false;
            auto vx = parseUserNumber(x->text()), vy = parseUserNumber(y->text());
            e.point = {vx ? *vx : 0, vy ? *vy : 0};
        }
        return true;
    }
};

bool editPump(QWidget* host, ProjectSession* s, PumpStation& p, bool isNew) {
    auto* g = s->currentGeometry();
    const QString fu = qs(flowUnit(s->project()->units())), lu = qs(lengthUnit(s->project()->units()));
    Sheet sh(host, isNew ? tr2("Stasiun pompa baru", "New pump station") : tr2("Ubah stasiun pompa", "Edit pump station"), "geometry.pump", 760);
    auto* head = new QHBoxLayout;
    head->addWidget(makeBadge(tr2("Pratinjau", "Preview"), "BadgePreview"));
    head->addWidget(makeMuted(tr2("Disimpan di pengaturan proyek De-RAS (.deras.json); blok pompa HEC-RAS di file .g## belum ditulis. Setiap grup menyala bila muka air asal ≥ elevasi nyala dan mati bila ≤ elevasi mati.",
                                  "Stored in the De-RAS project settings (.deras.json); the HEC-RAS pump block of the .g## file is not written yet. Each group starts when the source water surface reaches the on elevation and stops at the off elevation.")),
                    1);
    sh.body()->addLayout(head);
    auto* form = new QFormLayout;
    auto* nameEd = new QLineEdit(qs(p.name));
    form->addRow(tr2("Nama", "Name"), nameEd);
    EndEditor from(form, tr2("Asal (dipompa dari)", "From (pumped from)"), p.from, g->model, false);
    EndEditor to(form, tr2("Tujuan", "To"), p.to, g->model, true);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeCaption(tr2("Grup pompa", "Pump groups")));
    auto* groups = new QTableWidget(0, 5);
    groups->setHorizontalHeaderLabels({tr2("Nama", "Name"), tr2("Jumlah", "Count"), tr2("Kapasitas per pompa (%1)", "Capacity per pump (%1)").arg(fu),
                                       tr2("Elev. nyala (%1)", "On elev. (%1)").arg(lu), tr2("Elev. mati (%1)", "Off elev. (%1)").arg(lu)});
    groups->verticalHeader()->hide();
    groups->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    groups->setMinimumHeight(130);
    auto addRow = [groups](const PumpGroup& gp) {
        const int r = groups->rowCount();
        groups->insertRow(r);
        const QString v[] = {qs(gp.name), QString::number(gp.count), num(gp.capacity), num(gp.onElev), num(gp.offElev)};
        for (int c = 0; c < 5; ++c) groups->setItem(r, c, new QTableWidgetItem(v[c]));
    };
    for (const auto& gp : p.groups) addRow(gp);
    sh.body()->addWidget(groups);
    auto* gr = new QHBoxLayout;
    auto* ga = makeButton(tr2("Tambah grup", "Add group"), false, "plus");
    auto* gd = makeButton(tr2("Hapus grup", "Delete group"), false, "trash");
    gr->addWidget(ga);
    gr->addWidget(gd);
    gr->addStretch();
    sh.body()->addLayout(gr);
    QObject::connect(ga, &QPushButton::clicked, groups, [&]() {
        PumpGroup gp;
        gp.name = "Group " + std::to_string(groups->rowCount() + 1);
        addRow(gp);
    });
    QObject::connect(gd, &QPushButton::clicked, groups, [groups]() { if (groups->currentRow() >= 0) groups->removeRow(groups->currentRow()); });
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(isNew ? tr2("Tambah", "Add") : tr2("Simpan perubahan", "Save changes"), 1, true);
    PumpStation out;
    sh.setAcceptCheck([&](int r) {
        if (r != 1) return true;
        out = p;
        out.name = ss(nameEd->text().trimmed());
        if (out.name.empty()) { sh.setError(tr2("Isi nama.", "Enter a name.")); return false; }
        if (!from.read(out.from) || !to.read(out.to)) { sh.setError(tr2("Lengkapi asal dan tujuan (reach dan RS, atau area).", "Complete the source and target (reach and RS, or an area).")); return false; }
        out.groups.clear();
        for (int i = 0; i < groups->rowCount(); ++i) {
            PumpGroup gp;
            auto t = [&](int c) { return groups->item(i, c) ? groups->item(i, c)->text().trimmed() : QString(); };
            gp.name = ss(t(0));
            bool ok = false;
            gp.count = t(1).toInt(&ok);
            auto cap = parseUserNumber(t(2)), on = parseUserNumber(t(3)), off = parseUserNumber(t(4));
            if (!ok || gp.count < 0 || !cap || *cap < 0 || !on || !off) { sh.setError(tr2("Grup %1: isi jumlah (bulat), kapasitas, elevasi nyala dan mati.", "Group %1: enter the count (whole), capacity, on and off elevations.").arg(i + 1)); return false; }
            if (*off > *on) { sh.setError(tr2("Grup %1: elevasi mati tidak boleh di atas elevasi nyala.", "Group %1: the off elevation must not be above the on elevation.").arg(i + 1)); return false; }
            gp.capacity = *cap;
            gp.onElev = *on;
            gp.offElev = *off;
            out.groups.push_back(gp);
        }
        if (out.groups.empty()) { sh.setError(tr2("Tambahkan minimal satu grup pompa.", "Add at least one pump group.")); return false; }
        return true;
    });
    if (sh.exec() != 1) return false;
    p = out;
    return true;
}
}  // namespace

void pumpsSheet(QWidget* host, ProjectSession* s) {
    auto* g = geomOrWarn(host, s);
    if (!g) return;
    const std::string ext = ss(s->currentGeometryExt());
    auto pumps = io::pumpsFor(s->project()->settings, ext);
    Sheet sh(host, tr2("Stasiun pompa", "Pump stations"), "geometry.pump", 620);
    auto* list = new QListWidget;
    list->setMinimumHeight(160);
    auto endText = [](const PumpEnd& e) {
        return e.kind == 0 ? qs(e.reach) + " RS " + qs(e.rs) : e.kind == 1 ? qs(e.area) : tr2("keluar sistem", "out of the system");
    };
    auto fill = [&]() {
        list->clear();
        for (const auto& p : pumps) {
            double cap = 0;
            for (const auto& gp : p.groups) cap += gp.count * gp.capacity;
            list->addItem(QStringLiteral("%1   ·   %2 → %3   ·   %4 %5").arg(qs(p.name), endText(p.from), endText(p.to), num(cap), qs(flowUnit(s->project()->units()))));
        }
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    sh.body()->addWidget(list);
    QPushButton *add, *edit, *del;
    sh.body()->addLayout(listButtons(add, edit, del, tr2("Tambah stasiun pompa…", "Add pump station…")));
    auto store = [&]() {
        io::setPumps(s->project()->settings, ext, pumps);
        g->model.pumps = pumps;
        s->markEdited();
        fill();
    };
    QObject::connect(add, &QPushButton::clicked, &sh, [&]() {
        PumpStation p;
        p.name = "Pompa " + std::to_string(pumps.size() + 1);
        if (!g->model.storageAreas.empty()) { p.from.kind = 1; p.from.area = g->model.storageAreas[0].name; }
        else p.from.kind = 0;
        p.to.kind = g->model.reaches.empty() ? 2 : 0;
        PumpGroup gp;
        gp.name = "Group 1";
        p.groups.push_back(gp);
        if (!editPump(host, s, p, true)) return;
        pumps.push_back(p);
        store();
    });
    auto doEdit = [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(pumps.size())) return;
        if (editPump(host, s, pumps[size_t(r)], false)) store();
    };
    QObject::connect(edit, &QPushButton::clicked, &sh, doEdit);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &sh, [&](QListWidgetItem*) { doEdit(); });
    QObject::connect(del, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0 || r >= int(pumps.size())) return;
        if (!sheetConfirm(host, tr2("Hapus stasiun pompa", "Delete pump station"), tr2("Hapus %1?", "Delete %1?").arg(qs(pumps[size_t(r)].name)), tr2("Hapus", "Delete"), true)) return;
        pumps.erase(pumps.begin() + r);
        store();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

}  // namespace deras::ui

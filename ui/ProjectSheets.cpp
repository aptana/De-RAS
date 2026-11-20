// De-RAS - ui/ProjectSheets.cpp
#include "ProjectSheets.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "Common.h"
#include "Help.h"
#include "Widgets.h"

namespace deras::ui {

bool newProjectSheet(QWidget* win, NewProjectSpec& out) {
    Sheet sh(win, tr2("Proyek baru", "New project"), "wizard.page1", 720);
    auto section = [&](const QString& cap, const QString& topic) {
        auto* h = new QHBoxLayout;
        h->setContentsMargins(0, 6, 0, 0);
        h->addWidget(makeCaption(cap));
        h->addWidget(new InfoButton(topic));
        h->addStretch();
        sh.body()->addLayout(h);
        auto* g = new QGridLayout;
        g->setHorizontalSpacing(10);
        g->setVerticalSpacing(8);
        g->setColumnStretch(1, 1);
        sh.body()->addLayout(g);
        return g;
    };
    // 1. name and folder
    auto* g1 = section(tr2("1 · Nama dan lokasi", "1 · Name and location"), "wizard.page1");
    auto* title = new QLineEdit(tr2("Proyek baru", "New project"));
    auto* base = new QLineEdit(QStringLiteral("ProyekBaru"));
    base->setFont(monoFont());
    base->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_\\-]{1,40}"), base));
    auto* folder = new QLineEdit(QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/De-RAS Projects"));
    folder->setFont(monoFont());
    auto* browse = new IconButton("folder", tr2("Pilih folder…", "Choose folder…"));
    auto* fr = new QHBoxLayout;
    fr->addWidget(folder, 1);
    fr->addWidget(browse);
    g1->addWidget(labelWithInfo(tr2("Judul proyek", "Project title"), "wizard.title"), 0, 0);
    g1->addWidget(title, 0, 1);
    g1->addWidget(labelWithInfo(tr2("Nama file (tanpa spasi)", "File name (no spaces)"), "wizard.basename"), 1, 0);
    g1->addWidget(base, 1, 1);
    g1->addWidget(labelWithInfo(tr2("Folder", "Folder"), "wizard.folder"), 2, 0);
    g1->addLayout(fr, 2, 1);
    auto* files = makeMuted(QString());
    g1->addWidget(files, 3, 1);
    auto updFiles = [&]() {
        files->setText(tr2("Dibuat: %1.prj, .g01, .f01, .p01", "Creates: %1.prj, .g01, .f01, .p01").arg(base->text().isEmpty() ? QStringLiteral("…") : base->text()));
    };
    updFiles();
    QObject::connect(base, &QLineEdit::textChanged, &sh, updFiles);
    QObject::connect(browse, &QToolButton::clicked, &sh, [&]() {
        const QString d = QFileDialog::getExistingDirectory(win, tr2("Pilih folder proyek", "Choose project folder"), folder->text());
        if (!d.isEmpty()) folder->setText(QDir::toNativeSeparators(d));
    });
    // 2. units, CRS, time zone
    auto* g2 = section(tr2("2 · Satuan, koordinat, zona waktu", "2 · Units, coordinates, time zone"), "wizard.page2");
    auto* units = new Segmented;
    units->addOption(tr2("SI (meter, m³/s)", "SI (metres, m³/s)"), 1);
    units->addOption(tr2("US Customary (feet, cfs)", "US Customary (feet, cfs)"), 0);
    units->setValue(1);
    auto* crs = new QLineEdit;
    crs->setPlaceholderText(tr2("mis. EPSG:32749 (opsional)", "e.g. EPSG:32749 (optional)"));
    crs->setFont(monoFont());
    auto* tz = new QComboBox;
    tz->addItems({"UTC+07:00 (WIB)", "UTC+08:00 (WITA)", "UTC+09:00 (WIT)", "UTC+00:00"});
    tz->setEditable(true);
    g2->addWidget(labelWithInfo(tr2("Sistem satuan", "Unit system"), "wizard.units"), 0, 0);
    g2->addWidget(units, 0, 1, Qt::AlignLeft);
    g2->addWidget(labelWithInfo(tr2("Sistem koordinat", "Coordinate system"), "wizard.crs"), 1, 0);
    g2->addWidget(crs, 1, 1);
    g2->addWidget(labelWithInfo(tr2("Zona waktu", "Time zone"), "wizard.timezone"), 2, 0);
    g2->addWidget(tz, 2, 1);
    g2->addWidget(makeMuted(tr2("Setelah data diisi, satuan tidak bisa diganti tanpa konversi. Koordinat dan zona waktu disimpan di file pendamping De-RAS dan baru dipakai mulai M3 (peta) dan M5 (unsteady).",
                                "Units cannot be changed without conversion after data entry. Coordinates and time zone are stored in the De-RAS sidecar and used from M3/M5.")), 3, 1);
    // 3. river / reach and template
    auto* g3 = section(tr2("3 · Sungai, reach, dan template", "3 · River, reach and template"), "wizard.page3");
    auto* river = new QLineEdit(tr2("Sungai", "River"));
    auto* reach = new QLineEdit(tr2("Hilir", "Lower"));
    river->setMaxLength(16);
    reach->setMaxLength(16);
    g3->addWidget(labelWithInfo(tr2("Nama sungai / reach", "River / reach name"), "wizard.river"), 0, 0);
    auto* rr = new QHBoxLayout;
    rr->addWidget(river);
    rr->addWidget(reach);
    g3->addLayout(rr, 0, 1);
    auto* tmpl = new Segmented;
    tmpl->addOption(tr2("Saluran prismatik", "Prismatic channel"), 1);
    tmpl->addOption(tr2("Kosong", "Empty"), 0);
    tmpl->setValue(1);
    g3->addWidget(labelWithInfo(tr2("Template", "Template"), "wizard.page4"), 1, 0);
    g3->addWidget(tmpl, 1, 1, Qt::AlignLeft);
    auto* pw = new QWidget;
    auto* pg = new QGridLayout(pw);
    pg->setContentsMargins(0, 0, 0, 0);
    pg->setHorizontalSpacing(10);
    pg->setVerticalSpacing(6);
    auto dsp = [](double lo, double hi, double v, int dec) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setDecimals(dec);
        s->setValue(v);
        s->setLocale(QLocale::c());
        s->setFont(monoFont());
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setAlignment(Qt::AlignRight);
        return s;
    };
    auto* count = new QSpinBox;
    count->setRange(2, 500);
    count->setValue(11);
    count->setFont(monoFont());
    auto* spacing = dsp(1, 100000, 100, 2);
    auto* bw = dsp(0, 10000, 10, 2);
    auto* sslope = dsp(0, 100, 2, 2);
    auto* bh = dsp(0.1, 1000, 4, 2);
    auto* slope = dsp(0.000001, 0.5, 0.001, 6);
    auto* inv = dsp(-1000, 10000, 100, 2);
    auto* nch = dsp(0.005, 0.5, 0.03, 3);
    auto* nob = dsp(0.005, 0.5, 0.06, 3);
    const QList<QPair<QString, QWidget*>> fields{
        {tr2("Jumlah penampang", "Cross sections"), count}, {tr2("Jarak antar-penampang", "Spacing"), spacing},
        {tr2("Lebar dasar", "Bottom width"), bw},          {tr2("Talud (H:V)", "Side slope (H:V)"), sslope},
        {tr2("Tinggi tebing", "Bank height"), bh},         {tr2("Kemiringan dasar", "Bed slope"), slope},
        {tr2("Elevasi dasar hilir", "Downstream invert"), inv}, {tr2("n alur", "Channel n"), nch},
        {tr2("n bantaran", "Overbank n"), nob}};
    for (int i = 0; i < fields.size(); ++i) {
        pg->addWidget(makeMuted(fields[i].first, false), i / 3, (i % 3) * 2);
        pg->addWidget(fields[i].second, i / 3, (i % 3) * 2 + 1);
    }
    g3->addWidget(pw, 2, 0, 1, 2);
    g3->addWidget(makeMuted(tr2("Template prismatik juga membuat data aliran (3 profil) dan plan subkritis dengan batas hilir normal depth. Semua nilai bisa diubah nanti.",
                                "The prismatic template also creates flow data (3 profiles) and a subcritical plan with a normal-depth downstream boundary. All values can be changed later.")),
                  3, 0, 1, 2);
    QObject::connect(tmpl, &Segmented::changed, pw, [pw](int v) { pw->setEnabled(v == 1); });

    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Buat proyek", "Create project"), 1, true);
    sh.setAcceptCheck([&](int) {
        if (base->text().isEmpty() || title->text().trimmed().isEmpty()) { sh.setError(tr2("Isi judul dan nama file.", "Enter a title and a file name.")); return false; }
        const QString prj = QDir(folder->text()).filePath(base->text() + ".prj");
        if (QFileInfo::exists(prj)) {
            sh.setError(tr2("%1 sudah ada. Pilih nama lain; De-RAS tidak menimpa proyek yang ada.", "%1 already exists. Choose another name; De-RAS never overwrites an existing project.").arg(QDir::toNativeSeparators(prj)));
            return false;
        }
        if (river->text().trimmed().isEmpty() || reach->text().trimmed().isEmpty()) { sh.setError(tr2("Isi nama sungai dan reach.", "Enter the river and reach names.")); return false; }
        return true;
    });
    if (sh.exec() != 1) return false;
    out.folder = folder->text();
    out.baseName = base->text();
    out.title = title->text().trimmed();
    out.si = units->value() == 1;
    out.crs = crs->text().trimmed();
    out.timeZone = tz->currentText().section(' ', 0, 0);
    out.river = river->text().trimmed();
    out.reach = reach->text().trimmed();
    out.prismatic = tmpl->value() == 1;
    out.count = count->value();
    out.spacing = spacing->value();
    out.bottomWidth = bw->value();
    out.sideSlope = sslope->value();
    out.bankHeight = bh->value();
    out.bedSlope = slope->value();
    out.invert = inv->value();
    out.nChannel = nch->value();
    out.nOverbank = nob->value();
    return true;
}

bool descriptionSheet(QWidget* win, QString& text) {
    Sheet sh(win, tr2("Deskripsi proyek", "Project description"), "home.description", 620);
    auto* e = new QPlainTextEdit(text);
    e->setMinimumHeight(240);
    sh.body()->addWidget(e);
    sh.body()->addWidget(makeMuted(tr2("Disimpan di file .prj (BEGIN DESCRIPTION … END DESCRIPTION), sama seperti HEC-RAS.", "Stored in the .prj file (BEGIN DESCRIPTION … END DESCRIPTION), as in HEC-RAS.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    if (sh.exec() != 1) return false;
    text = e->toPlainText();
    return true;
}

}  // namespace deras::ui

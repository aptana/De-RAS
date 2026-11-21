// De-RAS - ui/OptionsView.cpp
#include "OptionsView.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "Widgets.h"
#include "app/ConvertProject.h"
#include "io/RasText.h"

namespace deras::ui {

OptionsView::OptionsView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- appearance ----
    auto* left = new QVBoxLayout;
    left->setSpacing(12);
    auto* ac = new Card(tr2("Tampilan aplikasi", "Appearance"), "tools.theme");
    auto* g = new QGridLayout;
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(12);
    theme_ = new Segmented;
    theme_->addOption(tr2("Tema Gelap", "Dark Theme"), 0);
    theme_->addOption(tr2("Tema Terang", "Light Theme"), 1);
    lang_ = new Segmented;
    lang_->addOption(QStringLiteral("Bahasa Indonesia"), 0);
    lang_->addOption(QStringLiteral("English"), 1);
    font_ = new QSpinBox;
    font_->setRange(12, 20);
    font_->setSuffix(QStringLiteral(" px"));
    font_->setKeyboardTracking(false);
    font_->setFont(monoFont());
    int r = 0;
    g->addWidget(labelWithInfo(tr2("Tema", "Theme"), "tools.theme"), r, 0);
    g->addWidget(theme_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Ukuran huruf", "Font size"), "tools.fontsize"), r, 0);
    g->addWidget(font_, r++, 1, Qt::AlignLeft);
    g->addWidget(labelWithInfo(tr2("Bahasa", "Language"), "tools.language"), r, 0);
    g->addWidget(lang_, r++, 1, Qt::AlignLeft);
    autosave_ = new QSpinBox;
    autosave_->setRange(0, 120);
    autosave_->setSuffix(tr2(" menit", " min"));
    autosave_->setSpecialValueText(tr2("Mati", "Off"));
    autosave_->setKeyboardTracking(false);
    autosave_->setFont(monoFont());
    g->addWidget(labelWithInfo(QStringLiteral("Autosave"), "tools.autosave"), r, 0);
    g->addWidget(autosave_, r++, 1, Qt::AlignLeft);
    g->setColumnStretch(1, 1);
    ac->body()->addLayout(g);
    ac->body()->addWidget(makeMuted(tr2("Perubahan langsung berlaku. Jendela disusun ulang, proyek tetap terbuka, dan perubahan yang belum disimpan tidak hilang.",
                                        "Changes apply immediately; the window is rebuilt without closing the project or discarding unsaved edits.")));
    left->addWidget(ac);

    auto* uc = new Card(tr2("Satuan", "Units"), "project.units");
    uc->body()->addWidget(makeMuted(tr2("Satuan dipilih saat proyek dibuat dan disimpan di file .prj (SI Units atau English Units), sama seperti di HEC-RAS.",
                                        "The project unit system is chosen when the project is created and stored in the .prj file (SI Units or English Units), as in HEC-RAS.")));
    auto* conv = makeButton(tr2("Konversi satuan proyek SI ↔ US…", "Convert project units SI ↔ US…"), false, "copy");
    conv->setToolTip(tr2("Membuat salinan proyek dalam sistem satuan lain (Pratinjau). Proyek asli tidak diubah.",
                         "Makes a copy of the project in the other unit system (Preview). The original project is not changed."));
    connect(conv, &QPushButton::clicked, this, &OptionsView::convertUnits);
    uc->body()->addWidget(makeMuted(tr2("Konversi membuat salinan proyek: geometri, aliran, plan dan data modul De-RAS diubah satuannya. Terrain dan layer GIS tidak dikonversi; hasil lama tidak ikut disalin.",
                                        "The conversion makes a copy of the project: geometry, flow, plan and De-RAS module data are converted. Terrains and GIS layers are not converted; old results are not copied.")));
    uc->body()->addWidget(conv, 0, Qt::AlignLeft);
    left->addWidget(uc);
    left->addStretch();
    root->addLayout(left, 1);

    // ---- project settings (sidecar) ----
    prjCard_ = new Card(tr2("Pengaturan proyek", "Project settings"), "project.settings");
    auto* pg = new QGridLayout;
    pg->setHorizontalSpacing(12);
    pg->setVerticalSpacing(12);
    crs_ = new QLineEdit;
    crs_->setPlaceholderText(tr2("mis. EPSG:32748 (UTM 48S)", "e.g. EPSG:32748 (UTM 48S)"));
    crs_->setFont(monoFont());
    tz_ = new QLineEdit;
    tz_->setFont(monoFont());
    target_ = new QComboBox;
    target_->addItem(QStringLiteral("HEC-RAS 7.00"), QStringLiteral("7.00"));
    notes_ = new QPlainTextEdit;
    notes_->setMaximumHeight(120);
    r = 0;
    pg->addWidget(labelWithInfo(tr2("Sistem koordinat", "Coordinate system"), "project.crs"), r, 0);
    pg->addWidget(crs_, r++, 1);
    pg->addWidget(makeMuted(tr2("Sama dengan RAS Mapper → Atur proyeksi…: kode EPSG atau WKT. RAS Mapper memakainya untuk memeriksa terrain; De-RAS belum memproyeksi ulang data.", "Same as RAS Mapper → Set projection…: an EPSG code or WKT. RAS Mapper uses it to check terrains; De-RAS does not reproject data yet.")), r++, 1);
    pg->addWidget(labelWithInfo(tr2("Zona waktu", "Time zone"), "project.timezone"), r, 0);
    pg->addWidget(tz_, r++, 1);
    pg->addWidget(labelWithInfo(tr2("Versi HEC-RAS tujuan", "Target HEC-RAS version"), "project.target"), r, 0);
    pg->addWidget(target_, r++, 1, Qt::AlignLeft);
    pg->addWidget(labelWithInfo(tr2("Catatan proyek", "Project notes"), "project.notes"), r, 0, Qt::AlignTop);
    pg->addWidget(notes_, r++, 1);
    pg->setColumnStretch(1, 1);
    prjCard_->body()->addLayout(pg);
    prjCard_->body()->addWidget(makeMuted(tr2("Disimpan di <nama>.deras.json di samping .prj; HEC-RAS mengabaikan file ini.",
                                              "Stored in <name>.deras.json next to the .prj; HEC-RAS ignores this file.")));
    prjCard_->body()->addStretch();
    root->addWidget(prjCard_, 1);

    connect(theme_, &Segmented::changed, this, [this](int v) {
        if (loading_) return;
        AppSettings::setTheme(v == 1 ? ThemeName::Light : ThemeName::Dark);
        emit appearanceChanged();
    });
    connect(autosave_, &QSpinBox::valueChanged, this, [this](int m) { if (!loading_) AppSettings::setAutosaveMinutes(m); });
    connect(font_, &QSpinBox::valueChanged, this, [this](int px) {
        if (loading_) return;
        AppSettings::setBaseFontPx(px);
        emit appearanceChanged();
    });
    connect(lang_, &Segmented::changed, this, [this](int v) {
        if (loading_) return;
        const Language l = v == 1 ? Language::EN : Language::ID;
        AppSettings::setLanguage(l);
        setCurrentLanguage(l);
        emit appearanceChanged();
    });
    for (auto* le : {crs_, tz_}) connect(le, &QLineEdit::editingFinished, this, &OptionsView::commitProject);
    connect(target_, &QComboBox::activated, this, &OptionsView::commitProject);
    connect(notes_, &QPlainTextEdit::textChanged, this, &OptionsView::commitProject);
    reload();
}

QString OptionsView::title() const { return tr2("Opsi", "Options"); }

void OptionsView::reload() {
    loading_ = true;
    theme_->setValue(AppSettings::theme() == ThemeName::Light ? 1 : 0);
    lang_->setValue(currentLanguage() == Language::EN ? 1 : 0);
    font_->setValue(AppSettings::baseFontPx());
    autosave_->setValue(AppSettings::autosaveMinutes());
    auto* p = s_->project();
    prjCard_->setEnabled(p != nullptr);
    if (p) {
        crs_->setText(qs(p->settings.coordinateSystem));
        tz_->setText(qs(p->settings.timeZone));
        int i = target_->findData(qs(p->settings.targetRasVersion));
        if (i < 0) {
            target_->addItem(QStringLiteral("HEC-RAS ") + qs(p->settings.targetRasVersion), qs(p->settings.targetRasVersion));
            i = target_->count() - 1;
        }
        target_->setCurrentIndex(i);
        notes_->setPlainText(qs(p->settings.notes));
    } else {
        crs_->clear();
        tz_->clear();
        notes_->clear();
    }
    loading_ = false;
}

void OptionsView::commitProject() {
    if (loading_) return;
    auto* p = s_->project();
    if (!p) return;
    p->settings.coordinateSystem = ss(crs_->text().trimmed());
    p->settings.timeZone = ss(tz_->text().trimmed());
    p->settings.targetRasVersion = ss(target_->currentData().toString());
    p->settings.notes = ss(notes_->toPlainText());
    s_->markEdited();
}

void OptionsView::convertUnits() {
    auto* p = s_->project();
    if (!p) {
        showToast(this, tr2("Buka atau buat proyek dulu.", "Open or create a project first."), "info");
        return;
    }
    if (p->isModified()) {
        sheetInfo(this, tr2("Simpan dulu", "Save first"), tr2("Proyek punya perubahan yang belum disimpan. Simpan proyek dulu, lalu ulangi konversi.",
                                                             "The project has unsaved changes. Save the project first, then convert again."));
        return;
    }
    const bool toUS = p->units() == UnitSystem::SI;
    const UnitSystem target = toUS ? UnitSystem::USCustomary : UnitSystem::SI;
    QString base = qs(p->baseName()) + (toUS ? QStringLiteral("_US") : QStringLiteral("_SI"));
    const QString dir = qs(io::fromPath(p->dir()));
    if (!sheetGetText(this, tr2("Konversi ke %1", "Convert to %1").arg(toUS ? QStringLiteral("US Customary") : QStringLiteral("SI")),
                      tr2("Nama file salinan (di folder yang sama). Proyek asli tetap dalam %1.", "File name of the copy (same folder). The original project stays in %1.")
                          .arg(toUS ? QStringLiteral("SI") : QStringLiteral("US Customary")),
                      base, 60))
        return;
    base = base.trimmed();
    if (base.isEmpty() || base == qs(p->baseName())) return;
    if (QFileInfo::exists(QDir(dir).filePath(base + QStringLiteral(".prj")))) {
        sheetInfo(this, tr2("Nama sudah dipakai", "Name in use"), tr2("%1.prj sudah ada di folder ini. Pilih nama lain.", "%1.prj already exists in this folder. Choose another name.").arg(base));
        return;
    }
    MessageList msgs;
    std::vector<std::string> notes;
    if (!app::convertProjectCopy(p->prjPath(), p->dir(), ss(base), target, msgs, notes)) {
        QStringList t;
        for (const auto& m : msgs) if (m.severity == Severity::Error) t << msgText(m);
        sheetInfo(this, tr2("Konversi gagal", "Conversion failed"), t.join(QStringLiteral("\n")));
        return;
    }
    QString text = tr2("Salinan %1.prj dibuat dalam %2. Hitung ulang plan di salinan karena hasil lama tidak ikut disalin.",
                       "The copy %1.prj was made in %2. Compute the plans of the copy again: old results were not copied.")
                       .arg(base, toUS ? QStringLiteral("US Customary") : QStringLiteral("SI"));
    if (!notes.empty()) {
        QStringList n;
        for (const auto& x : notes) n << qs(x);
        text += tr2("\n\nLayer ini tidak dikonversi dan masih dalam satuan lama: %1.", "\n\nThese layers were not converted and are still in the old units: %1.").arg(n.join(QStringLiteral(", ")));
    }
    const int c = sheetChoice(this, tr2("Konversi selesai", "Conversion finished"), text, {tr2("Tetap di proyek ini", "Stay in this project"), tr2("Buka salinan", "Open the copy")}, 1);
    if (c == 1) emit openProjectRequested(QDir(dir).filePath(base + QStringLiteral(".prj")));
}

}  // namespace deras::ui

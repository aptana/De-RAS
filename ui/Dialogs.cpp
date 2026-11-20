// De-RAS - ui/Dialogs.cpp
#include "Dialogs.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Common.h"
#include "Help.h"
#include "Version.h"
#include "Widgets.h"

namespace deras::ui {

bool firstRunLanguageSheet(QWidget* win) {
    const Language before = currentLanguage();
    Sheet sh(win, QStringLiteral("Pilih bahasa / Choose language"), {}, 460);
    auto* seg = new Segmented;
    seg->addOption(QStringLiteral("English"), 1);
    seg->addOption(QStringLiteral("Bahasa Indonesia"), 0);
    seg->setValue(before == Language::ID ? 0 : 1);
    sh.body()->addWidget(seg, 0, Qt::AlignLeft);
    sh.body()->addWidget(makeMuted(QStringLiteral("English is the default. You can change it any time in Options.\n"
                                                  "Bawaannya English. Bisa diganti kapan saja di Opsi.")));
    sh.addButton(QStringLiteral("Lanjut / Continue"), 1, true);
    sh.exec();
    const Language lang = seg->value() == 1 ? Language::EN : Language::ID;
    AppSettings::setLanguage(lang);
    setCurrentLanguage(lang);
    AppSettings::setFirstRunDone();
    return lang != before;
}

void showAboutSheet(QWidget* win) {
    Sheet sh(win, tr2("Tentang De-RAS", "About De-RAS"), "help.about", 620);
    auto* h = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/branding/De-RAS-128.png").scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    h->addWidget(logo, 0, Qt::AlignTop);
    auto* v = new QVBoxLayout;
    auto* t = new QLabel(QStringLiteral("De-RAS"));
    t->setObjectName("ViewTitle");
    v->addWidget(t);
    auto* ver = new QLabel(QStringLiteral("%1 %2 · %3").arg(tr2("Versi", "Version"), DERAS_VERSION, DERAS_RELEASE_LABEL));
    ver->setObjectName("Mono");
    v->addWidget(ver);
    v->addWidget(makeMuted(tr2("Format file yang ditulis: HEC-RAS %1 (.prj, .g##, .f##, .p##)", "File compatibility target: HEC-RAS %1 (text .prj / .g## / .f## / .p##)").arg(DERAS_TARGET_RAS_VERSION)));
    v->addStretch();
    h->addLayout(v, 1);
    sh.body()->addLayout(h);
    auto* b = new QLabel(tr2(
        "<p>De-RAS adalah perangkat lunak hidraulika sungai dan banjir yang membaca dan menulis file proyek HEC-RAS dan menjalankan perhitungannya sendiri.</p>"
        "<p><b>De-RAS tidak berafiliasi dengan, tidak didukung oleh, dan bukan produk dari Hydrologic Engineering Center (HEC), U.S. Army Corps of Engineers.</b> "
        "HEC-RAS adalah nama produk milik pemiliknya dan disebut di sini hanya untuk menjelaskan kompatibilitas format file.</p>"
        "<p>Mesin hitung De-RAS ditulis ulang dari persamaan yang dipublikasikan (HEC-RAS Hydraulic Reference Manual). Kesamaan hasil dengan HEC-RAS "
        "<b>belum divalidasi</b> kecuali untuk kasus uji yang tercantum di dokumen validasi.</p>"
        "<p>Komponen pihak ketiga: Qt 6 (LGPL v3), font Inter (SIL OFL 1.1), JetBrains Mono (Apache 2.0) — lihat THIRD_PARTY_NOTICES.txt di folder instalasi.</p>",
        "<p>De-RAS is river and flood hydraulics software that reads and writes HEC-RAS project files and runs its own computations.</p>"
        "<p><b>De-RAS is not affiliated with, endorsed by, or a product of the Hydrologic Engineering Center (HEC), U.S. Army Corps of Engineers.</b> "
        "HEC-RAS is a product name of its owner and is mentioned only to describe file-format compatibility.</p>"
        "<p>The De-RAS engine is re-implemented from published equations (HEC-RAS Hydraulic Reference Manual). Equivalence of results with HEC-RAS "
        "<b>has not been validated</b> except for the test cases listed in the validation document.</p>"
        "<p>Third-party components: Qt 6 (LGPL v3), Inter font (SIL OFL 1.1), JetBrains Mono (Apache 2.0) — see THIRD_PARTY_NOTICES.txt in the installation folder.</p>"));
    b->setWordWrap(true);
    b->setTextFormat(Qt::RichText);
    sh.body()->addWidget(b);
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

const QVector<FeatureRow>& featureRows() {
    static const QVector<FeatureRow> rows = {
        {"Aplikasi", "Application", "UI satu jendela, Tema Gelap dan Tema Terang", "Single-window UI, Dark and Light themes", "done", "M1", "M1"},
        {"Aplikasi", "Application", "Bahasa Indonesia / English", "Indonesian / English", "done", "", ""},
        {"Aplikasi", "Application", "Bantuan ⓘ offline dwibahasa, Jendela Bantuan", "Offline bilingual ⓘ help, Help window", "done", "", ""},
        {"Aplikasi", "Application", "Installer Windows dan ZIP portabel", "Windows installer and portable ZIP", "done", "Dibuat dengan kompilasi silang; belum diuji di Windows asli (W1–W4).", "Cross-compiled; not yet tested on real Windows (W1–W4)."},
        {"Proyek", "Project", "Buat/buka/simpan proyek HEC-RAS (.prj)", "Create/open/save HEC-RAS project (.prj)", "done", "Lossless untuk file yang tidak diubah", "Lossless for unchanged files"},
        {"Proyek", "Project", "Duplikasi proyek (Simpan Sebagai)", "Duplicate project (Save As)", "done", "", ""},
        {"Proyek", "Project", "Proyek contoh (proyek uji standar Sungai Uji 1 km)", "Example project (1 km standard test project)", "done", "Data sintetis, berlabel Contoh", "Synthetic data, labelled Example"},
        {"Geometri", "Geometry", "Skema reach, gambar garis tengah reach", "Reach schematic, draw reach centreline", "done", "Skema, bukan peta GIS", "Schematic, not a GIS map"},
        {"Geometri", "Geometry", "Editor penampang melintang 1D, impor survei CSV", "1D cross-section editor, CSV survey import", "done", "", ""},
        {"Geometri", "Geometry", "Ineffective area, levee, obstruction", "Ineffective areas, levees, obstructions", "preview", "Diedit dan dihitung; belum dibandingkan dengan HEC-RAS", "Edited and computed; not yet compared with HEC-RAS"},
        {"Geometri", "Geometry", "n vertikal", "Vertical n", "partial", "Dibaca & dipertahankan di file; belum dihitung (tidak masuk 0.2.3)", "Read & preserved in the file; not computed yet (not in 0.2.3)"},
        {"Geometri", "Geometry", "Junction (buat, ubah, hapus, hitung)", "Junctions (create, edit, delete, compute)", "preview", "Persamaan energi; belum dibandingkan dengan HEC-RAS", "Energy equation; not yet compared with HEC-RAS"},
        {"Geometri", "Geometry", "Jembatan (energi, Yarnell, aliran tekan, pelimpah dek)", "Bridges (energy, Yarnell, pressure flow, deck weir)", "preview", "Hanya subkritis; format blok .g## belum diverifikasi; belum dibandingkan dengan HEC-RAS", "Subcritical only; .g## block format not verified; not yet compared with HEC-RAS"},
        {"Geometri", "Geometry", "Gorong-gorong lingkaran/kotak (FHWA inlet, outlet)", "Circular/box culverts (FHWA inlet, outlet)", "preview", "Bentuk lain disimpan, belum dihitung; format .g## belum diverifikasi", "Other shapes kept, not computed; .g## format not verified"},
        {"Geometri", "Geometry", "Struktur inline: mercu pelimpah dan pintu", "Inline structures: weir crest and gates", "preview", "Bukaan pintu disimpan di .deras.json; format .g## belum diverifikasi", "Gate openings stored in .deras.json; .g## format not verified"},
        {"Geometri", "Geometry", "Bukaan ganda (multiple opening)", "Multiple openings", "unavailable", "Dipertahankan di file; belum dihitung (tidak masuk 0.2.3)", "Preserved in the file; not computed yet (not in 0.2.3)"},
        {"Geometri", "Geometry", "Struktur lateral (pelimpah, breach), storage area, sambungan SA/2D", "Lateral structures (weir, breach), storage areas, SA/2D connections", "preview", "Dihitung di unsteady; breach dan pompa disimpan di .deras.json; belum dibandingkan dengan HEC-RAS", "Computed in unsteady runs; breach and pumps stored in .deras.json; not yet compared with HEC-RAS"},
        {"Geometri", "Geometry", "Area 2D (grid persegi, subgrid terrain), BC line, stasiun pompa", "2D areas (square grid, terrain subgrid), BC lines, pump stations", "preview", "Gelombang difusi; bukan mesh poligon HEC-RAS; belum dibandingkan", "Diffusion wave; not the HEC-RAS polygon mesh; not yet compared"},
        {"Geometri", "Geometry", "Breakline dan wilayah n area 2D", "2D area breaklines and n regions", "unavailable", "Tidak dijadwalkan dalam M1–M8", "Not scheduled in M1–M8"},
        {"Geometri", "Geometry", "Parameter HTab", "HTab parameters", "done", "Tidak perlu: tabel hidraulik dibuat otomatis; baris HTab di file dipertahankan", "Not needed: hydraulic tables are built automatically; HTab lines in the file are kept"},
        {"Aliran", "Flow", "Data aliran steady (.f##), CSV", "Steady flow data (.f##), CSV", "done", "", ""},
        {"Aliran", "Flow", "Batas hilir: normal depth, muka air diketahui, kritis", "Downstream BC: normal depth, known WS, critical", "done", "", ""},
        {"Aliran", "Flow", "Batas rating curve (hilir)", "Rating-curve boundary (downstream)", "partial", "Format .f## belum diverifikasi terhadap HEC-RAS", ".f## format not yet verified against HEC-RAS"},
        {"Aliran", "Flow", "Batas hulu: normal depth, muka air diketahui, kritis", "Upstream BC: normal depth, known WS, critical", "done", "Untuk superkritis/campuran", "For supercritical/mixed"},
        {"Aliran", "Flow", "Aliran unsteady (.u##): hidrograf, normal depth, rating curve, aliran lateral", "Unsteady flow (.u##): hydrographs, normal depth, rating curve, lateral inflow", "preview", "Baris yang tidak dikenal disimpan utuh; format Rating Curve dan lateral belum diverifikasi", "Unknown lines kept unchanged; Rating Curve and lateral formats not verified"},
        {"Aliran", "Flow", "Batas area: hidrograf, normal depth di BC line, hujan; muka air awal area", "Area boundaries: hydrographs, normal depth on BC lines, precipitation; initial area stages", "preview", "Format baris area di .u## belum diverifikasi", "Area line format in .u## not verified"},
        {"Aliran", "Flow", "Data quasi-unsteady (rangkaian debit, batas hilir)", "Quasi-unsteady flow data (flow series, downstream boundary)", "preview", "Disimpan di .deras.json; file .q## HEC-RAS belum dibaca/ditulis", "Stored in .deras.json; HEC-RAS .q## files not read/written yet"},
        {"Aliran", "Flow", "Data sedimen (fungsi angkutan, gradasi, muatan hulu)", "Sediment data (transport function, gradation, inflow load)", "preview", "Disimpan di .deras.json; file .s## HEC-RAS belum dibaca/ditulis", "Stored in .deras.json; HEC-RAS .s## files not read/written yet"},
        {"Simulasi", "Simulation", "Steady subkritis (standard step)", "Steady subcritical (standard step)", "done", "Benchmark analitis lulus; kesamaan dengan HEC-RAS belum dibandingkan", "Analytical benchmarks pass; equivalence with HEC-RAS not yet compared"},
        {"Simulasi", "Simulation", "Steady superkritis / campuran (loncatan hidraulik)", "Steady supercritical / mixed (hydraulic jump)", "preview", "Benchmark analitis lulus; belum dibandingkan dengan HEC-RAS", "Analytical benchmarks pass; not yet compared with HEC-RAS"},
        {"Simulasi", "Simulation", "Model Doctor (pemeriksaan data, langsung)", "Model Doctor (live data checks)", "done", "", ""},
        {"Simulasi", "Simulation", "Jendela Komputasi: proses terpisah, antrean, batal", "Compute window: separate process, queue, cancel", "done", "", ""},
        {"Simulasi", "Simulation", "Unsteady 1D (Preissmann implisit)", "1D unsteady (Preissmann implicit)", "preview", "Subkritis; jembatan, gorong-gorong, struktur inline belum dihitung; belum dibandingkan dengan HEC-RAS; belum menulis .hdf", "Subcritical; bridges, culverts, inline structures not computed; not yet compared with HEC-RAS; no .hdf output yet"},
        {"Simulasi", "Simulation", "Unsteady 1D–2D: storage area, area 2D, pelimpah lateral, pompa", "1D–2D unsteady: storage areas, 2D areas, lateral weirs, pumps", "preview", "Selisih volume dilaporkan per area; belum dibandingkan dengan HEC-RAS", "Volume error reported per area; not yet compared with HEC-RAS"},
        {"Simulasi", "Simulation", "Sedimen quasi-unsteady (MPM, Engelund-Hansen, Yang; Exner)", "Quasi-unsteady sediment (MPM, Engelund-Hansen, Yang; Exner)", "preview", "Satu reach, satu ukuran butir (d50), tanpa armoring; belum dibandingkan dengan HEC-RAS", "One reach, one grain size (d50), no armouring; not yet compared with HEC-RAS"},
        {"Simulasi", "Simulation", "Kualitas air: satu konstituen, adveksi-dispersi, peluruhan orde-1", "Water quality: one constituent, advection-dispersion, first-order decay", "preview", "Memakai hasil unsteady; tanpa suhu/DO/nutrien; file .w## belum dibaca/ditulis; belum dibandingkan", "Uses unsteady results; no temperature/DO/nutrients; .w## files not read/written yet; not yet compared"},
        {"Hasil", "Results", "Penampang, profil memanjang, profil umum", "Cross section, profile, general profile", "done", "", ""},
        {"Hasil", "Results", "Tabel detail dan tabel profil, ringkasan pesan", "Detailed and profile tables, message summary", "done", "", ""},
        {"Hasil", "Results", "Ekspor CSV dan laporan HTML", "CSV export and HTML report", "done", "", ""},
        {"Hasil", "Results", "Laporan Word (.docx) dan PDF", "Word (.docx) and PDF report", "preview", "Dibuat tanpa Microsoft Word; belum dibuka di Word asli", "Written without Microsoft Word; not yet opened in real Word"},
        {"Hasil", "Results", "Kurva rating", "Rating curve", "done", "", ""},
        {"Hasil", "Results", "Hasil .p##.hdf (susunan HEC-RAS)", ".p##.hdf results (HEC-RAS layout)", "preview", "Terbaca h5dump; belum dibuka di HEC-RAS (K3)", "Readable by h5dump; not yet opened in HEC-RAS (K3)"},
        {"Hasil", "Results", "Hidrograf, time series", "Hydrograph, time series", "preview", "Penampang, storage area, struktur lateral, sambungan, pompa", "Cross sections, storage areas, lateral structures, connections, pumps"},
        {"Peta", "Mapping", "Peta hasil area 2D: kedalaman, kecepatan, waktu tiba", "2D area result maps: depth, velocity, arrival time", "preview", "Kedalaman per piksel terrain; kecepatan per sel", "Depth per terrain pixel; velocity per cell"},
        {"Peta", "Mapping", "RAS Mapper: proyeksi, terrain GeoTIFF, gambar latar", "RAS Mapper: projection, GeoTIFF terrain, background pictures", "preview", "Tanpa GDAL; belum bisa memproyeksi ulang", "No GDAL; no reprojection yet"},
        {"Peta", "Mapping", "Potong penampang dari terrain (garis potong GIS)", "Cut cross sections from terrain (GIS cut lines)", "preview", "Belum dibandingkan dengan RAS Mapper", "Not compared with RAS Mapper yet"},
        {"Peta", "Mapping", "Peta genangan steady, ekspor GeoTIFF/PNG", "Steady inundation map, GeoTIFF/PNG export", "preview", "Tanpa cek keterhubungan aliran", "No flow connectivity check"},
        {"Peta", "Mapping", "Terrain .hdf HEC-RAS, reproyeksi, basemap daring", "HEC-RAS .hdf terrain, reprojection, online basemaps", "unavailable", "Tidak masuk 0.2.3", "Not in 0.2.3"},
        {"Hasil", "Results", "Perspektif 3D", "3D perspective", "done", "", ""},
        {"Lainnya", "Other", "Desain hidraulik: aliran seragam, gerusan pilar/kontraksi/abutmen (HEC-18)", "Hydraulic design: uniform flow, pier/contraction/abutment scour (HEC-18)", "preview", "Belum dibandingkan dengan Hydraulic Design HEC-RAS", "Not yet compared with the HEC-RAS Hydraulic Design"},
        {"Lainnya", "Other", "Kalibrasi n Manning (satu faktor, plan steady)", "Manning's n calibration (one factor, steady plan)", "preview", "Faktor tunggal untuk alur atau semua n", "One factor for the channel or all n"},
        {"Lainnya", "Other", "Konversi satuan proyek SI ↔ US (salinan proyek)", "Project unit conversion SI ↔ US (project copy)", "preview", "Terrain dan layer GIS tidak dikonversi", "Terrains and GIS layers are not converted"},
        {"Lainnya", "Other", "Autosave dan pemulihan setelah crash", "Autosave and crash recovery", "done", "Belum diuji di Windows asli", "Not yet tested on real Windows"},
    };
    return rows;
}

void showFeatureMatrixSheet(QWidget* win) {
    Sheet sh(win, tr2("Matriks fitur De-RAS %1", "De-RAS %1 feature matrix").arg(DERAS_VERSION), "help.features", 940);
    auto* tbl = new QTableWidget(featureRows().size(), 4);
    tbl->setHorizontalHeaderLabels({tr2("Area", "Area"), tr2("Fitur", "Feature"), tr2("Status", "Status"), tr2("Catatan", "Notes")});
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->verticalHeader()->hide();
    tbl->setAlternatingRowColors(true);
    const Theme& th = Theme::current();
    int r = 0;
    for (const auto& f : featureRows()) {
        tbl->setItem(r, 0, new QTableWidgetItem(tr2(f.areaId, f.areaEn)));
        tbl->setItem(r, 1, new QTableWidgetItem(tr2(f.id, f.en)));
        const QString st = QString::fromLatin1(f.status);
        auto* si = new QTableWidgetItem(st == "done"      ? QStringLiteral("✓ ") + tr2("Selesai", "Done")
                                        : st == "preview" ? QStringLiteral("◐ ") + tr2("Pratinjau", "Preview")
                                        : st == "partial" ? QStringLiteral("◑ ") + tr2("Parsial", "Partial")
                                                          : QStringLiteral("🔒 ") + tr2("Belum tersedia", "Not available"));
        si->setForeground(st == "done" ? th.success : st == "preview" || st == "partial" ? th.warning : th.text3);
        tbl->setItem(r, 2, si);
        tbl->setItem(r, 3, new QTableWidgetItem(tr2(f.noteId, f.noteEn)));
        ++r;
    }
    tbl->resizeColumnsToContents();
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setMinimumHeight(460);
    sh.body()->addWidget(tbl);
    sh.body()->addWidget(makeMuted(tr2("Pratinjau artinya sudah bisa dipakai, tetapi hasilnya belum dibandingkan dengan HEC-RAS. Belum tersedia artinya tombolnya masih terkunci dan menyebut milestone rencananya.",
                                       "Preview = usable but not yet validated against HEC-RAS. Not available = its button is locked and names its milestone.")));
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
}

}  // namespace deras::ui

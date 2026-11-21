// De-RAS - tools/ui/ui_snapshot.cpp
// Developer tool (not shipped): renders views of the main window, the
// Compute window, the splash and the Model Doctor pop-up offscreen to PNG,
// named by the screenshot rules in screenshots/README.md, and checks that
// viewing a project does not modify it.
//
//   deras-ui-snapshot <version-dir> <version> <id|en> <gelap|terang> <project.prj|-> <shot>[,<shot>...]
//
// <version-dir> is screenshots/<version>. Indonesian + dark images go there as
// <fitur>_<version>.png (the Home view with the project is <version>.png);
// English or light variants go to <version-dir>/varian/ with _en / _terang.
// <project.prj> "-" copies the bundled example (DerasUji1km) to a temp folder.
// Shots: home, beranda-kosong, bilah-atas, data-geometri, editor-penampang,
// aliran-steady, analisis-steady, hasil-penampang, profil-memanjang,
// profil-umum, tabel-detail, tabel-profil, ringkasan-pesan, opsi,
// jendela-komputasi, splash, model-doctor (clears the downstream boundary
// condition of the current steady flow in memory after the modification
// check, never saved), editor-jembatan (adds a bridge to the geometry in
// memory after the check and opens its editor, never saved).
// M6-M8 (0.2.3): geometri-2d, peta-2d, data-quasi, data-sedimen,
// analisis-sedimen, desain-seragam, desain-gerusan, kalibrasi-n (enters
// illustrative observed water surfaces in memory: computed + 0.25),
// data-kualitas-air, analisis-kualitas-air (runs on the p02 results).
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "app/RunService.h"
#include <cstdio>

#include "Common.h"
#include "ComputeWindow.h"
#include "DesignView.h"
#include "DoctorSheet.h"
#include "MainWindow.h"
#include "MapperView.h"
#include "ProjectSession.h"
#include "ResultsView.h"
#include "SplashScreen.h"
#include "StructureSheets.h"
#include "Widgets.h"

using namespace deras;
using namespace deras::ui;

static void settle(int ms) {
    QEventLoop l;
    QTimer::singleShot(ms, &l, &QEventLoop::quit);
    l.exec();
}

int main(int argc, char** argv) {
    qputenv("DERAS_NO_ANIMATION", "1");
    QApplication app(argc, argv);
    // Keep the developer's own De-RAS settings (recent projects, theme) untouched.
    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    if (argc < 7) {
        std::fprintf(stderr, "usage: deras-ui-snapshot <version-dir> <version> <id|en> <gelap|terang> <project.prj|-> <shot,...>\n");
        return 2;
    }
    const QString root = argv[1], ver = argv[2];
    const bool en = QString(argv[3]) == "en";
    const bool light = QString(argv[4]) == "terang";
    QString prj = argv[5];
    const QStringList shots = QString(argv[6]).split(',', Qt::SkipEmptyParts);
    setCurrentLanguage(en ? Language::EN : Language::ID);
    Theme::apply(light ? ThemeName::Light : ThemeName::Dark, 14);
    const bool variant = en || light;
    const QString dir = variant ? root + "/varian" : root;
    QDir().mkpath(dir);
    const QString sfx = QString(en ? "_en" : "") + (light ? "_terang" : "");
    auto file = [&](const QString& name) { return dir + "/" + (name.isEmpty() ? ver : name + "_" + ver) + sfx + ".png"; };
    auto want = [&](const QString& s) { return shots.contains(s); };
    QTemporaryDir tmp;
    if (prj == "-") prj = ProjectSession::installExample(tmp.path() + "/Contoh");
    if (prj.isEmpty() || !QFileInfo::exists(prj)) { std::fprintf(stderr, "project not found\n"); return 3; }
    if (want("jalankan")) {  // run the current plan first so the results views show real output
        deras::app::RunRequest rq;
        rq.prjPath = deras::io::fs::path(prj.toStdWString());
        std::string used;
        const auto r = deras::app::runSteadyPlan(rq, used);
        std::printf("run %s: %s\n", used.c_str(), r.status == deras::RunStatus::Succeeded ? "success" : "not successful");
    }
    if (want("jalankan-unsteady")) {  // also run the unsteady example plan (p02) for the hydrograph shots
        deras::app::RunRequest rq;
        rq.prjPath = deras::io::fs::path(prj.toStdWString());
        rq.planExt = "p02";
        std::string used;
        const auto r = deras::app::runSteadyPlan(rq, used);
        std::printf("run %s: %s\n", used.c_str(), r.status == deras::RunStatus::Succeeded ? "success" : "not successful");
    }

    if (want("splash")) {
        SplashScreen sp;
        sp.show();
        sp.setStatus(tr2("Menyiapkan jendela utama…", "Preparing main window…"));
        sp.setProgress(85);
        settle(600);
        sp.grab().save(file("splash"));
    }
    ProjectSession s;
    MainWindow w(&s);
    w.resize(1440, 900);
    w.show();
    settle(300);
    if (want("beranda-kosong")) w.grab().save(file("beranda-kosong"));
    w.openProject(prj);
    settle(300);
    const QList<QPair<QString, QString>> views{{"home", ""}, {"data-geometri", "geometry"}, {"editor-penampang", "xs"}, {"aliran-steady", "steady"},
                                                {"analisis-steady", "analysis"}, {"hasil-penampang", "results"}, {"opsi", "options"}};
    for (const auto& [shot, id] : views) {
        if (!want(shot)) continue;
        w.go(id.isEmpty() ? QStringLiteral("home") : id);
        settle(300);
        w.grab().save(file(shot == "home" ? QString() : shot));
    }
    if (want("bilah-atas")) {
        w.go("home");
        settle(300);
        w.grab(QRect(0, 0, w.width(), 170)).save(file("bilah-atas"));
    }
    if (auto* rv = w.findChild<ResultsView*>()) {
        const QList<QPair<QString, QString>> pages{{"profil-memanjang", "profile"}, {"profil-umum", "general"}, {"tabel-detail", "detail"},
                                                    {"tabel-profil", "table"}, {"ringkasan-pesan", "messages"}, {"kurva-rating", "rating"}, {"perspektif-3d", "3d"}};
        for (const auto& [shot, pg] : pages) {
            if (!want(shot)) continue;
            w.go("results");
            rv->showPage(pg);
            settle(250);
            w.grab().save(file(shot));
        }
    }
    if (want("ras-mapper") || want("peta-genangan"))
        if (auto* mv = w.findChild<MapperView*>()) {
            w.go("mapper");
            settle(400);
            if (want("ras-mapper")) w.grab().save(file("ras-mapper"));
            if (want("peta-genangan")) {
                mv->computeMap();
                settle(300);
                w.grab().save(file("peta-genangan"));
            }
        }
    if (want("jendela-komputasi"))
        if (auto* cw = w.findChild<ComputeWindow*>()) {
            cw->show();
            settle(300);
            cw->grab().save(file("jendela-komputasi"));
            cw->hide();
        }
    // Regression check: opening and viewing a project must not modify it.
    int rc = 0;
    if (auto* p = s.project()) {
        for (auto& [e, g] : p->geometries) if (g.isModified()) { std::printf("MODIFIED by viewing: %s\n", e.c_str()); rc = 1; }
        for (auto& [e, f] : p->steadyFlows) if (f.isModified()) { std::printf("MODIFIED by viewing: %s\n", e.c_str()); rc = 1; }
        for (auto& [e, pl] : p->plans) if (pl.isModified()) { std::printf("MODIFIED by viewing: %s\n", e.c_str()); rc = 1; }
        for (auto& [e, u] : p->unsteadyFlows) if (u.isModified()) { std::printf("MODIFIED by viewing: %s\n", e.c_str()); rc = 1; }
        if (p->isModified() && rc == 0) { std::printf("MODIFIED by viewing: prj or sidecar\n"); rc = 1; }
    } else {
        std::printf("project did not open\n");
        rc = 1;
    }
    if (rc == 0) std::printf("OK: viewing did not modify the project\n");
    // Unsteady views (M5): switch to the unsteady example plan in memory only.
    if (want("aliran-unsteady") || want("analisis-unsteady") || want("hidrograf") || want("time-series")) {
        if (s.project() && s.project()->plans.count("p02")) s.setCurrentPlan(QStringLiteral("p02"));
        settle(200);
        if (want("aliran-unsteady")) { w.go("unsteady"); settle(400); w.grab().save(file("aliran-unsteady")); }
        if (want("analisis-unsteady")) { w.go("run-unsteady"); settle(600); w.grab().save(file("analisis-unsteady")); }
        if (auto* rv = w.findChild<ResultsView*>()) {
            const QList<QPair<QString, QString>> pages{{"hidrograf", "hydrograph"}, {"time-series", "timeseries"}};
            for (const auto& [shot, pg] : pages) {
                if (!want(shot)) continue;
                w.go("results");
                rv->showPage(pg);
                settle(300);
                w.grab().save(file(shot));
            }
        }
    }
    // M6 (0.2.3): the 2D example plan p03, its geometry and the 2D depth map
    if ((want("geometri-2d") || want("peta-2d")) && s.project() && s.project()->plans.count("p03")) {
        s.setCurrentPlan(QStringLiteral("p03"));
        settle(200);
        if (want("geometri-2d")) { w.go("geometry"); settle(500); w.grab().save(file("geometri-2d")); }
        if (want("peta-2d"))
            if (auto* mv = w.findChild<MapperView*>()) {
                deras::app::RunRequest rq;
                rq.prjPath = deras::io::fs::path(prj.toStdWString());
                rq.planExt = "p03";
                std::string used;
                const auto r = deras::app::runSteadyPlan(rq, used);
                std::printf("run %s: %s\n", used.c_str(), r.status == deras::RunStatus::Succeeded ? "success" : "not successful");
                emit s.resultsUpdated(QStringLiteral("p03"));
                w.go("mapper");
                settle(400);
                mv->show2DResults();
                settle(400);
                w.grab().save(file("peta-2d"));
            }
    }
    // M7 (0.2.3): sediment views of the example (run in the view)
    if (want("data-quasi")) { w.go("quasi"); settle(400); w.grab().save(file("data-quasi")); }
    if (want("data-sedimen")) { w.go("sediment"); settle(400); w.grab().save(file("data-sedimen")); }
    if (want("analisis-sedimen")) {
        w.go("run-sediment");
        settle(300);
        for (auto* btn : w.findChildren<QPushButton*>())
            if (btn->isVisible() && btn->text() == tr2("Hitung sedimen", "Compute sediment")) { btn->click(); break; }
        settle(4000);
        w.grab().save(file("analisis-sedimen"));
    }
    if (want("editor-jembatan") && s.currentGeometry() && !s.currentGeometry()->model.reaches.empty()) {
        Reach& r = s.currentGeometry()->model.reaches[0];
        std::vector<size_t> xi;
        for (size_t i = 0; i < r.nodes.size(); ++i) if (r.nodes[i].type == NodeType::CrossSection) xi.push_back(i);
        if (xi.size() >= 4) {
            const size_t k = xi.size() / 2;
            const CrossSection& up = r.nodes[xi[k - 1]];
            const CrossSection& dn = r.nodes[xi[k]];
            CrossSection b;
            b.uid = nextUid();
            b.type = NodeType::Bridge;
            b.rs = io::formatPlain(std::round((rsNumber(up.rs) + rsNumber(dn.rs)) / 2));
            b.description = isID() ? "Jembatan contoh (hanya untuk tangkapan layar)" : "Example bridge (screenshot only)";
            auto& sd = b.structure;
            sd.parsed = true;
            sd.dist = 10;
            sd.width = 8;
            sd.weirCoef = 1.44;
            double zmax = -1e300, zmin = 1e300;
            for (const auto& p : up.staElev) { zmax = std::max(zmax, p.elev); zmin = std::min(zmin, p.elev); }
            const double s0 = up.staElev.front().station, s1 = up.staElev.back().station;
            sd.deckUp = {{s0, zmax, zmax - 1.2}, {s1, zmax, zmax - 1.2}};
            sd.deckDn = sd.deckUp;
            const double bl = up.bankLeft.value_or(s0), br = up.bankRight.value_or(s1);
            for (double st : {bl + (br - bl) / 3, bl + 2 * (br - bl) / 3}) {
                Pier p;
                p.upSta = p.dnSta = std::round(st * 10) / 10;
                p.upWidth = p.dnWidth = {1.2, 1.2};
                p.upElev = p.dnElev = {zmin - 1, zmax - 1.2};
                sd.piers.push_back(p);
            }
            r.nodes.insert(r.nodes.begin() + long(xi[k]), b);
            s.markEdited();
            w.go("geometry");
            settle(400);
            QTimer::singleShot(300, &w, [&]() {
                for (auto* btn : w.findChildren<QPushButton*>())
                    if (btn->isVisible() && btn->text() == tr2("Ubah…", "Edit…")) {
                        QTimer::singleShot(500, &w, [&]() {
                            w.grab().save(file("editor-jembatan"));
                            for (Sheet* sh : w.findChildren<Sheet*>()) if (sh->isVisible()) sh->done(0);
                        });
                        btn->click();
                        break;
                    }
            });
            structuresSheet(&w, &s, r.uid, false);
        }
    }
    // M8 (0.2.3): hydraulic design, n calibration and water quality
    if (want("desain-seragam") || want("desain-gerusan") || want("kalibrasi-n"))
        if (auto* dv = w.findChild<DesignView*>()) {
            if (s.project() && s.project()->plans.count("p01")) s.setCurrentPlan(QStringLiteral("p01"));
            settle(200);
            w.go("design");
            if (want("desain-seragam")) { dv->showPage(0); settle(400); w.grab().save(file("desain-seragam")); }
            if (want("desain-gerusan")) {
                dv->showPage(1);
                settle(300);
                for (auto* btn : dv->findChildren<QPushButton*>())
                    if (btn->isVisible() && btn->text() == tr2("Isi dari hasil", "Fill from results")) { btn->click(); break; }
                settle(400);
                w.grab().save(file("desain-gerusan"));
            }
            if (want("kalibrasi-n")) {
                dv->showPage(2);
                settle(300);
                if (auto* t = dv->findChild<QTableWidget*>()) {
                    const int n = t->rowCount();
                    for (int r : {1, n / 2, n - 2})
                        if (r >= 0 && r < n && t->item(r, 2) && t->item(r, 3)) {
                            bool ok = false;
                            const double ws = t->item(r, 2)->text().toDouble(&ok);
                            if (ok) t->item(r, 3)->setText(QString::number(ws + 0.25, 'f', 3));
                        }
                }
                for (auto* btn : dv->findChildren<QPushButton*>())
                    if (btn->isVisible() && btn->text() == tr2("Kalibrasi", "Calibrate")) { btn->click(); break; }
                settle(3000);
                w.grab().save(file("kalibrasi-n"));
            }
        }
    if (want("data-kualitas-air")) { w.go("wq"); settle(400); w.grab().save(file("data-kualitas-air")); }
    if (want("analisis-kualitas-air")) {
        w.go("run-wq");
        settle(300);
        for (auto* btn : w.findChildren<QPushButton*>())
            if (btn->isVisible() && btn->text() == tr2("Hitung kualitas air", "Compute water quality")) { btn->click(); break; }
        settle(2500);
        w.grab().save(file("analisis-kualitas-air"));
    }
    if (want("model-doctor") && s.currentPlan()) {
        // T6: a steady flow whose downstream boundary condition is empty (in memory only).
        if (s.project() && s.project()->plans.count("p01")) s.setCurrentPlan(QStringLiteral("p01"));
        if (auto* f = s.currentFlow())
            for (auto& b : f->model.boundaries) b.downstream.type = BoundaryType::None;
        s.markEdited();
        w.go("analysis");
        settle(800);
        QTimer::singleShot(400, &w, [&]() {
            w.grab().save(file("model-doctor"));
            for (Sheet* sh : w.findChildren<Sheet*>()) if (sh->isVisible()) sh->done(0);
        });
        doctorSheet(&w, s.doctor(), s.currentPlanExt() + QStringLiteral(" · ") + qs(s.currentPlan()->model.title));
    }
    std::fflush(stdout);
    std::_Exit(rc);  // skip the save prompt of closeEvent
}

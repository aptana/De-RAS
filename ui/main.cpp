// De-RAS - ui/main.cpp
// Start-up: the splash screen follows the real start-up steps (no fake or
// timed progress) and closes as soon as the main window is shown.
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include "Common.h"
#include "Dialogs.h"
#include "Help.h"
#include "MainWindow.h"
#include "ProjectSession.h"
#include "ComputeWindow.h"
#include "Widgets.h"
#include "SplashScreen.h"
#include "Version.h"

using namespace deras;
using namespace deras::ui;

int main(int argc, char** argv) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName("De-RAS");
    QApplication::setOrganizationName("De-RAS");
    QApplication::setApplicationVersion(DERAS_VERSION);
    QApplication::setWindowIcon(appIcon());

    // Before a language is chosen (first start) the splash speaks English (default since 0.2.1).
    setCurrentLanguage(AppSettings::firstRunDone() ? AppSettings::language() : Language::EN);
    SplashScreen splash;
    splash.show();
    splash.pump();

    // Step 1: settings
    splash.setStatus(tr2("Memuat pengaturan…", "Loading settings…"));
    splash.setProgress(10);
    splash.pump();
    const bool firstRun = !AppSettings::firstRunDone();

    // Step 2: components that must be installed next to the application
    splash.setStatus(tr2("Memeriksa komponen…", "Checking components…"));
    splash.setProgress(30);
    splash.pump();
    if (!QFileInfo::exists(ComputeWindow::enginePath())) {
        const bool cont = splash.showError(
            tr2("Mesin hitung deras-steady tidak ditemukan", "Compute engine deras-steady not found"),
            tr2("File %1 tidak ada. Cara memperbaiki: jalankan lagi installer De-RAS, atau pastikan antivirus tidak mengkarantina file itu. "
                "Anda tetap bisa membuka dan mengedit proyek, tetapi belum bisa menjalankan simulasi.",
                "%1 is missing. Fix: run the De-RAS installer again (Repair/Install), or make sure antivirus has not quarantined the file. "
                "You can still open and edit projects, but cannot run simulations.").arg(QDir::toNativeSeparators(ComputeWindow::enginePath())),
            tr2("Lanjutkan tanpa mesin hitung", "Continue without the engine"));
        if (!cont) return 1;
    }

    // Step 3: help catalogue (same source as the manual)
    splash.setStatus(tr2("Memuat bantuan dan bahasa…", "Loading help and language…"));
    splash.setProgress(50);
    splash.pump();
    if (HelpCatalog::instance().ids().isEmpty()) {
        const bool cont = splash.showError(tr2("Katalog bantuan tidak bisa dibaca", "Help catalogue could not be read"),
                                           tr2("Bantuan ⓘ tidak akan tampil. Cara memperbaiki: pasang ulang De-RAS dengan installer.", "ⓘ help will not be shown. Fix: reinstall De-RAS with the installer."),
                                           tr2("Lanjutkan", "Continue"));
        if (!cont) return 1;
    }

    // Step 4: theme
    splash.setStatus(tr2("Menerapkan tema…", "Applying theme…"));
    splash.setProgress(65);
    splash.pump();
    Theme::apply(AppSettings::theme(), AppSettings::baseFontPx());

    // Step 5: main window
    splash.setStatus(tr2("Menyiapkan jendela utama…", "Preparing main window…"));
    splash.setProgress(85);
    splash.pump();
    ProjectSession session;
    QPointer<MainWindow> win;
    std::function<void()> build;
    build = [&]() {
        auto* w = new MainWindow(&session);
        w->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(w, &MainWindow::rebuildRequested, &app, [&, w]() {
            // Rebuild every widget with the new theme / language / font size; the open
            // project (and any unsaved edits) stays in the session.
            const QRect geo = w->geometry();
            const bool max = w->isMaximized();
            const QString view = w->currentViewId();
            HelpWindow::closeWindow();
            Theme::apply(AppSettings::theme(), AppSettings::baseFontPx());
            w->setAttribute(Qt::WA_DeleteOnClose, false);
            w->hide();
            w->deleteLater();
            build();
            win->setGeometry(geo);
            if (max) win->showMaximized(); else win->show();
            win->go(view, false);
        });
        win = w;
    };
    build();
    // Optional: a .prj passed on the command line (Windows "Open with")
    const QStringList args = QApplication::arguments();
    if (args.size() > 1 && args[1].endsWith(".prj", Qt::CaseInsensitive)) win->openProject(args[1]);

    splash.setStatus(tr2("Siap", "Ready"));
    splash.setProgress(100);
    splash.pump();
    win->show();
    splash.finish(win);

    if (!firstRun) QTimer::singleShot(500, &app, [&]() { if (win) win->checkRecovery(); });
    if (firstRun) {
        QTimer::singleShot(0, &app, [&]() {
            if (firstRunLanguageSheet(win)) emit win->rebuildRequested();
        });
    }
    return app.exec();
}

// View history of the main window (review R3 of build 0.2.0: Back and Alt+Left
// sometimes did nothing). Runs offscreen on the developer machine.
//  H1 back/forward through Beranda → Data Geometri → Editor Penampang
//  H2 Alt+Left / Alt+Right shortcuts with focus inside a text field
//  H3 mouse back button pressed on a child widget (table, canvas)
//  H4 no dead entries: repeated views, closing the project
//  H5 Back is ignored while a sheet is open
#include <QAbstractScrollArea>
#include <QApplication>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "Common.h"
#include "MainWindow.h"
#include "ProjectSession.h"
#include "Widgets.h"
#include "TestMain.h"

using namespace deras;
using namespace deras::ui;

static MainWindow* W = nullptr;
static ProjectSession* S = nullptr;

static void settle(int ms = 50) { QTest::qWait(ms); }

TEST(H1_back_forward) {
    W->go("home");
    W->go("geometry");
    W->go("xs");
    CHECK(W->currentViewId() == "xs");
    W->back();
    CHECK(W->currentViewId() == "geometry");
    W->back();
    CHECK(W->currentViewId() == "home");
    CHECK(!W->canGoBack());
    W->forward();
    CHECK(W->currentViewId() == "geometry");
    W->forward();
    CHECK(W->currentViewId() == "xs");
    CHECK(!W->canGoForward());
}

TEST(H2_alt_arrows_with_focus_in_field) {
    W->go("steady");
    W->go("analysis");
    settle();
    if (auto* e = W->findChild<QLineEdit*>()) e->setFocus();
    QTest::keyClick(W->focusWidget() ? W->focusWidget() : W, Qt::Key_Left, Qt::AltModifier);
    settle();
    CHECK(W->currentViewId() == "steady");
    QTest::keyClick(W->focusWidget() ? W->focusWidget() : W, Qt::Key_Right, Qt::AltModifier);
    settle();
    CHECK(W->currentViewId() == "analysis");
}

TEST(H3_mouse_back_button_on_child) {
    W->go("steady");
    W->go("geometry");
    settle();
    QAbstractScrollArea* area = nullptr;
    for (auto* a : W->findChildren<QAbstractScrollArea*>())
        if (a->isVisible()) { area = a; break; }
    CHECK(area != nullptr);
    if (!area) return;
    QWidget* target = area->viewport();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), target->mapToGlobal(QPointF(5, 5)), Qt::BackButton, Qt::BackButton, Qt::NoModifier);
    QApplication::sendEvent(target, &press);
    settle();
    CHECK(W->currentViewId() == "steady");
}

TEST(H4_no_dead_entries) {
    W->go("geometry");
    W->go("home");
    W->go("geometry");
    W->go("home");
    // every Back must change the view
    for (int i = 0; i < 6 && W->canGoBack(); ++i) {
        const QString before = W->currentViewId();
        W->back();
        CHECK(W->currentViewId() != before);
    }
    W->go("geometry");
    W->go("xs");
    S->close();
    settle();
    CHECK(W->currentViewId() == "home");
    CHECK(!W->canGoBack());
    W->back();  // must not throw or switch to a view that needs a project
    CHECK(W->currentViewId() == "home");
}

TEST(H5_back_blocked_by_sheet) {
    W->go("geometry");
    W->go("steady");
    QString during;
    QTimer::singleShot(200, W, [&]() {
        W->back();
        during = W->currentViewId();
        for (Sheet* sh : W->findChildren<Sheet*>()) if (sh->isVisible()) sh->done(0);
    });
    Sheet sh(W, QStringLiteral("Test"));
    sh.addButton(QStringLiteral("OK"), 0, true);
    sh.exec();
    CHECK(during == "steady");
    W->back();
    CHECK(W->currentViewId() == "geometry");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("DERAS_NO_ANIMATION", "1");
    QApplication app(argc, argv);
    QTemporaryDir settingsDir, projDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    setCurrentLanguage(Language::EN);
    ProjectSession s;
    MainWindow w(&s);
    W = &w;
    S = &s;
    w.resize(1440, 900);
    w.show();
    w.activateWindow();
    const QString prj = ProjectSession::installExample(projDir.path() + "/Contoh");
    w.openProject(prj);
    settle(200);
    int rc = 0;
    for (auto& c : dt::cases()) {
        const int before = dt::failures();
        // each case starts with the example project open
        if (!s.isOpen()) { w.openProject(prj); settle(100); }
        c.fn();
        std::printf("%s %s\n", dt::failures() == before ? "[ OK ]" : "[FAIL]", c.name);
    }
    std::printf("%d checks, %d failures\n", dt::checks(), dt::failures());
    rc = dt::failures() == 0 ? 0 : 1;
    std::fflush(stdout);
    std::_Exit(rc);  // skip the save prompt of closeEvent
}

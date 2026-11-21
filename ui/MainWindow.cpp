// De-RAS - ui/MainWindow.cpp
#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTime>
#include <QVBoxLayout>

#include "Common.h"
#include "ComputeWindow.h"
#include "Dialogs.h"
#include "DoctorSheet.h"
#include "Features.h"
#include "GeometryView.h"
#include "Help.h"
#include "HomeView.h"
#include "Icons.h"
#include "OptionsView.h"
#include "ProjectSheets.h"
#include "ResultsView.h"
#include "MapperView.h"
#include "SteadyAnalysisView.h"
#include "UnsteadyAnalysisView.h"
#include "SedimentView.h"
#include "DesignView.h"
#include "WaterQualityView.h"
#include "UnsteadyFlowView.h"
#include "SteadyFlowView.h"
#include "Version.h"
#include "Widgets.h"
#include "XsEditorView.h"
#include "app/Templates.h"

namespace deras::ui {

namespace {
// Work area: the radial gradient behind the cards (design tokens workA -> workB).
class WorkArea : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent*) override {
        const Theme& t = Theme::current();
        QPainter p(this);
        QRadialGradient g(QPointF(width() * 0.5, height() * 0.35), std::max(width(), height()) * 0.75);
        g.setColorAt(0, t.workA);
        g.setColorAt(1, t.workB);
        p.fillRect(rect(), g);
    }
};

// Header / status strips use the "bar" colour with a hairline.
class Strip : public QWidget {
public:
    Strip(bool top, QWidget* parent = nullptr) : QWidget(parent), top_(top) {}
protected:
    void paintEvent(QPaintEvent*) override {
        const Theme& t = Theme::current();
        QPainter p(this);
        p.fillRect(rect(), t.bar);
        p.setPen(t.line);
        if (top_) p.drawLine(rect().bottomLeft(), rect().bottomRight());
        else p.drawLine(rect().topLeft(), rect().topRight());
    }
private:
    bool top_;
};

QString resultsPageFor(const QString& toolbarId) { return toolbarId.startsWith("res-") ? toolbarId.mid(4) : QString(); }
}  // namespace

MainWindow::MainWindow(ProjectSession* session, QWidget* parent) : QMainWindow(parent), s_(session) {
    setWindowIcon(appIcon());
    setMinimumSize(1024, 680);
    resize(1440, 900);
    auto* central = new WorkArea;
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    compute_ = new ComputeWindow(s_, this);

    root->addWidget(buildHeader());
    auto* tbWrap = new QWidget;
    auto* tbl = new QHBoxLayout(tbWrap);
    tbl->setContentsMargins(14, 12, 14, 8);
    tbl->addWidget(buildToolbar());
    root->addWidget(tbWrap);
    root->addWidget(buildViewBar());

    stack_ = new QStackedWidget;
    root->addWidget(stack_, 1);
    home_ = new HomeView(s_);
    geom_ = new GeometryView(s_);
    xs_ = new XsEditorView(s_);
    steady_ = new SteadyFlowView(s_);
    analysis_ = new SteadyAnalysisView(s_);
    results_ = new ResultsView(s_);
    options_ = new OptionsView(s_);
    mapper_ = new MapperView(s_);
    unsteady_ = new UnsteadyFlowView(s_);
    uanalysis_ = new UnsteadyAnalysisView(s_);
    quasi_ = new SedimentView(s_, SedimentView::Page::Quasi);
    sedData_ = new SedimentView(s_, SedimentView::Page::Data);
    sedRun_ = new SedimentView(s_, SedimentView::Page::Run);
    design_ = new DesignView(s_);
    wqData_ = new WaterQualityView(s_, WaterQualityView::Page::Data);
    wqRun_ = new WaterQualityView(s_, WaterQualityView::Page::Run);
    for (View* v : std::initializer_list<View*>{home_, geom_, xs_, steady_, analysis_, results_, options_, mapper_, unsteady_, uanalysis_, quasi_, sedData_, sedRun_, design_, wqData_, wqRun_}) addView(v);

    root->addWidget(buildDrawer());
    root->addWidget(buildStatusBar());

    // ---- home actions ----
    connect(home_, &HomeView::newProjectRequested, this, &MainWindow::newProject);
    connect(home_, &HomeView::openRequested, this, &MainWindow::openDialog);
    connect(home_, &HomeView::exampleRequested, this, [this]() { openExample(); });
    connect(home_, &HomeView::openPath, this, &MainWindow::openProject);
    connect(home_, &HomeView::editDescriptionRequested, this, &MainWindow::editDescription);
    connect(options_, &OptionsView::openProjectRequested, this, [this](const QString& f) { openProject(f); });
    connect(options_, &OptionsView::appearanceChanged, this, [this]() {
        if (isRunning()) {
            showToast(this, tr2("Tampilan baru dipakai setelah run selesai.", "The appearance applies after the run finishes."), "warning");
            return;
        }
        QTimer::singleShot(0, this, &MainWindow::rebuildRequested);
    });

    // ---- autosave (M8): a recovery copy of unsaved changes every N minutes ----
    autosave_.setInterval(60 * 1000);
    connect(&autosave_, &QTimer::timeout, this, &MainWindow::autosaveTick);
    autosave_.start();
    connect(s_, &ProjectSession::saved, this, &MainWindow::clearRecovery);

    // ---- compute window ----
    connect(compute_, &ComputeWindow::progressChanged, this, [this](int pct) {
        runBar_->setVisible(pct >= 0);
        runLabel_->setVisible(pct >= 0);
        if (pct >= 0) {
            runBar_->setValue(pct);
            runLabel_->setText(tr2("Run %1 · %2%", "Run %1 · %2%").arg(compute_->runningPlan()).arg(pct));
        }
    });
    connect(compute_, &ComputeWindow::runFinished, this, [this](const QString& plan, bool ok) {
        addLog(ok ? tr2("✓ Run %1 selesai.", "✓ Run %1 finished.").arg(plan) : tr2("⛔ Run %1 tidak berhasil. Lihat Jendela Komputasi.", "⛔ Run %1 did not succeed. See the Compute window.").arg(plan), !ok);
        showToast(this, ok ? tr2("Run %1 selesai", "Run %1 finished").arg(plan) : tr2("Run %1 tidak berhasil", "Run %1 did not succeed").arg(plan), ok ? "check" : "warning");
        if (!ok && !compute_->isVisible()) openDrawer(0);
    });
    connect(compute_, &ComputeWindow::viewResultsRequested, this, [this]() {
        go("results");
        activateWindow();
    });

    // ---- session ----
    connect(s_, &ProjectSession::projectChanged, this, [this]() {
        for (View* v : views_) v->reload();
        updateHeader();
        updateEnabled();
        updateDirty();
        updateDoctor();
        updateViewBar();
        if (!s_->isOpen() && currentViewId() != "home" && currentViewId() != "options") {
            history_.clear();
            future_.clear();
            go("home", false);
        }
    });
    connect(s_, &ProjectSession::modelEdited, this, [this]() {
        doctorTimer_.start();
        updateHeader();
    });
    connect(s_, &ProjectSession::saved, this, [this]() { updateDirty(); updateHeader(); });
    connect(s_, &ProjectSession::logMessage, this, [this](const QString& l) { addLog(l, l.startsWith(QStringLiteral("⛔"))); });
    doctorTimer_.setSingleShot(true);
    doctorTimer_.setInterval(500);
    connect(&doctorTimer_, &QTimer::timeout, this, [this]() { updateDoctor(); updateDirty(); });
    statusClear_.setSingleShot(true);
    statusClear_.setInterval(9000);
    connect(&statusClear_, &QTimer::timeout, this, [this]() { statusText_->setText(tr2("Siap", "Ready")); });

    qApp->installEventFilter(this);
    go("home", false);
    updateHeader();
    updateEnabled();
    updateDirty();
    updateDoctor();
    applyWindowChrome(this);
}

MainWindow::~MainWindow() = default;

bool MainWindow::isRunning() const { return compute_->isRunning(); }

QString MainWindow::currentViewId() const {
    auto* v = qobject_cast<View*>(stack_->currentWidget());
    return v ? v->viewId() : QString();
}

// ============================ header & menus ============================
QWidget* MainWindow::buildHeader() {
    auto* h = new Strip(true);
    h->setFixedHeight(44);
    auto* l = new QHBoxLayout(h);
    l->setContentsMargins(14, 0, 14, 0);
    l->setSpacing(8);
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/branding/De-RAS-64.png").scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    l->addWidget(logo, 0, Qt::AlignVCenter);
    auto* name = new QLabel(QStringLiteral("De-RAS"));
    name->setObjectName("Strong");
    l->addWidget(name, 0, Qt::AlignVCenter);
    auto* ver = makeBadge(QStringLiteral("%1 %2").arg(DERAS_VERSION, DERAS_RELEASE_LABEL), "BadgeAccent");
    l->addWidget(ver, 0, Qt::AlignVCenter);
    l->addSpacing(10);
    auto* mb = new QMenuBar;
    mb->setNativeMenuBar(false);
    buildMenus(mb);
    // QMenuBar paints its items from the top edge; centre it vertically so the
    // menu lines up with the logo and name (review R1 of build 0.2.0).
    mb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    l->addWidget(mb, 0, Qt::AlignVCenter);
    l->addStretch();
    exampleBadge_ = makeBadge(tr2("CONTOH", "EXAMPLE"), "BadgeExample");
    exampleBadge_->setToolTip(tr2("Proyek contoh: data buatan untuk belajar, bukan data lapangan.", "Example project: synthetic data for learning, not field data."));
    l->addWidget(exampleBadge_);
    projectChip_ = new QLabel;
    projectChip_->setObjectName("Mono");
    l->addWidget(projectChip_);
    return h;
}

void MainWindow::buildMenus(QMenuBar* mb) {
    auto act = [this](QMenu* m, const QString& text, std::function<void()> fn, const QKeySequence& key = {}, const QString& icon = {}, bool needsProject = false) {
        QAction* a = m->addAction(text);
        if (!icon.isEmpty()) a->setIcon(lineIcon(icon, Theme::current().text2));
        if (!key.isEmpty()) {
            a->setShortcut(key);
            a->setShortcutContext(Qt::ApplicationShortcut);
            addAction(a);  // shortcut works even when the menu bar is hidden behind a sheet
        }
        connect(a, &QAction::triggered, this, fn);
        if (needsProject) needProject_ << a;
        return a;
    };
    // (locked menu entries: none left in 0.2.3; see Features.cpp for the toolbar/editor locks)
    // Berkas
    QMenu* f = mb->addMenu(tr2("&Berkas", "&File"));
    act(f, tr2("Proyek baru…", "New project…"), [this]() { newProject(); }, QKeySequence::New, "plus");
    act(f, tr2("Buka proyek…", "Open project…"), [this]() { openDialog(); }, QKeySequence::Open, "folder");
    recentMenu_ = f->addMenu(tr2("Proyek terakhir", "Recent projects"));
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::fillRecentMenu);
    act(f, tr2("Proyek contoh", "Example project"), [this]() { openExample(); }, {}, "description");
    act(f, tr2("Pulihkan proyek contoh…", "Restore example project…"), [this]() { openExample(true); }, {}, "back");
    f->addSeparator();
    act(f, tr2("Simpan proyek", "Save project"), [this]() { saveProject(); }, QKeySequence::Save, "save", true);
    act(f, tr2("Duplikasi proyek (Simpan Sebagai)…", "Duplicate project (Save As)…"), [this]() { duplicateProject(); }, {}, "copy", true);
    act(f, tr2("Tutup proyek", "Close project"), [this]() { closeProject(); }, {}, "close", true);
    f->addSeparator();
    act(f, tr2("Keluar", "Exit"), [this]() { close(); }, QKeySequence(Qt::ALT | Qt::Key_F4));
    // Edit
    QMenu* e = mb->addMenu(tr2("&Edit", "&Edit"));
    act(e, tr2("Data Geometri", "Geometric Data"), [this]() { go("geometry"); }, QKeySequence(Qt::CTRL | Qt::Key_2), "geometry", true);
    act(e, tr2("Editor Penampang", "Cross-Section Editor"), [this]() { go("xs"); }, {}, "xs", true);
    act(e, tr2("Data Aliran Steady", "Steady Flow Data"), [this]() { go("steady"); }, QKeySequence(Qt::CTRL | Qt::Key_3), "steady", true);
    act(e, tr2("Data Aliran Unsteady", "Unsteady Flow Data"), [this]() { go("unsteady"); }, QKeySequence(Qt::CTRL | Qt::Key_4), "unsteady", true);
    act(e, tr2("Data Quasi-unsteady", "Quasi-unsteady Flow Data"), [this]() { go("quasi"); }, {}, "quasi", true);
    act(e, tr2("Data Sedimen", "Sediment Data"), [this]() { go("sediment"); }, {}, "sediment", true);
    act(e, tr2("Data Kualitas Air", "Water Quality Data"), [this]() { go("wq"); }, {}, "water", true);
    e->addSeparator();
    act(e, tr2("Deskripsi proyek…", "Project description…"), [this]() { editDescription(); }, {}, "description", true);
    // Jalankan
    QMenu* r = mb->addMenu(tr2("&Jalankan", "&Run"));
    act(r, tr2("Analisis Steady", "Steady Flow Analysis"), [this]() { go("analysis"); }, QKeySequence(Qt::CTRL | Qt::Key_5), "run", true);
    act(r, tr2("Hitung plan aktif", "Compute current plan"), [this]() { runCurrentPlan(); }, QKeySequence(Qt::Key_F5), "play", true);
    act(r, QStringLiteral("Model Doctor"), [this]() { runModelDoctor(true); }, QKeySequence(Qt::Key_F7), "doctor", true);
    act(r, tr2("Jendela Komputasi", "Compute window"), [this]() { compute_->popUp(); }, {}, "timeseries");
    act(r, tr2("Analisis Unsteady", "Unsteady Flow Analysis"), [this]() { go("run-unsteady"); }, {}, "run-unsteady", true);
    r->addSeparator();
    act(r, tr2("Analisis Sedimen", "Sediment Analysis"), [this]() { go("run-sediment"); }, {}, "run-sediment", true);
    act(r, tr2("Analisis Kualitas Air", "Water Quality Analysis"), [this]() { go("run-wq"); }, {}, "run-water", true);
    r->addSeparator();
    act(r, tr2("Desain Hidraulik", "Hydraulic Design"), [this]() { go("design"); }, {}, "design", true);
    act(r, tr2("Kalibrasi n Manning", "Manning's n calibration"), [this]() { design_->showPage(2); go("design"); }, {}, "design", true);
    // Tampilan
    QMenu* v = mb->addMenu(tr2("&Tampilan", "&View"));
    act(v, tr2("Beranda", "Home"), [this]() { go("home"); }, QKeySequence(Qt::CTRL | Qt::Key_1), "home");
    act(v, QStringLiteral("RAS Mapper"), [this]() { go("mapper"); }, QKeySequence(Qt::CTRL | Qt::Key_6), "mapper", true);
    act(v, tr2("Hasil", "Results"), [this]() { go("results"); }, QKeySequence(Qt::CTRL | Qt::Key_7), "profile", true);
    QMenu* rv = v->addMenu(tr2("Halaman hasil", "Result pages"));
    const QList<QPair<QString, QString>> pages{{"xs", tr2("Penampang", "Cross section")}, {"profile", tr2("Profil memanjang", "Profile")}, {"general", tr2("Profil umum", "General profile")},
                                               {"detail", tr2("Tabel detail", "Detailed table")}, {"table", tr2("Tabel profil", "Profile table")}, {"messages", tr2("Ringkasan pesan", "Message summary")}, {"rating", tr2("Kurva rating", "Rating curve")}, {"3d", tr2("Perspektif 3D", "3D perspective")},
                                               {"hydrograph", tr2("Hidrograf", "Hydrograph")}, {"timeseries", QStringLiteral("Time series")}};
    for (const auto& pg : pages) act(rv, pg.second, [this, id = pg.first]() { results_->showPage(id); go("results"); }, {}, {}, true);
    v->addSeparator();
    act(v, tr2("Panel Pesan", "Messages drawer"), [this]() { drawer_->isVisible() ? drawer_->hide() : openDrawer(0); }, QKeySequence(Qt::CTRL | Qt::Key_M), "warning");
    act(v, tr2("Kembali", "Back"), [this]() { back(); }, QKeySequence(Qt::ALT | Qt::Key_Left), "back");
    act(v, tr2("Maju", "Forward"), [this]() { forward(); }, QKeySequence(Qt::ALT | Qt::Key_Right), "forward");
    // Opsi
    QMenu* o = mb->addMenu(tr2("&Opsi", "&Options"));
    act(o, tr2("Opsi…", "Options…"), [this]() { go("options"); }, QKeySequence(Qt::CTRL | Qt::Key_Comma), "options");
    auto* tg = new QActionGroup(this);
    for (int i = 0; i < 2; ++i) {
        QAction* a = o->addAction(i == 0 ? tr2("Tema Gelap", "Dark Theme") : tr2("Tema Terang", "Light Theme"));
        a->setCheckable(true);
        a->setChecked((AppSettings::theme() == ThemeName::Light) == (i == 1));
        tg->addAction(a);
        connect(a, &QAction::triggered, this, [this, i]() {
            const ThemeName t = i == 1 ? ThemeName::Light : ThemeName::Dark;
            if (t == AppSettings::theme()) return;
            if (isRunning()) { showToast(this, tr2("Tema baru dipakai setelah run selesai.", "The theme applies after the run finishes."), "warning"); return; }
            AppSettings::setTheme(t);
            QTimer::singleShot(0, this, &MainWindow::rebuildRequested);
        });
    }
    act(o, tr2("Konversi satuan proyek SI ↔ US…", "Convert project units SI ↔ US…"), [this]() { options_->convertUnits(); }, {}, "options", true);
    // Alat GIS
    QMenu* g = mb->addMenu(tr2("Alat &GIS", "&GIS Tools"));
    act(g, QStringLiteral("RAS Mapper"), [this]() { go("mapper"); }, {}, "mapper", true);
    act(g, tr2("Tambah terrain (GeoTIFF)…", "Add terrain (GeoTIFF)…"), [this]() { go("mapper"); mapper_->addTerrain(); }, {}, "plus", true);
    act(g, tr2("Proyeksi proyek…", "Project projection…"), [this]() { go("mapper"); mapper_->projectionSheet(); }, {}, "options", true);
    act(g, tr2("Peta genangan", "Inundation map"), [this]() { go("mapper"); mapper_->computeMap(); }, {}, "water", true);
    // Bantuan
    QMenu* hm = mb->addMenu(tr2("Ban&tuan", "&Help"));
    act(hm, tr2("Bantuan untuk view ini", "Help for this view"), [this]() {
        auto* cv = qobject_cast<View*>(stack_->currentWidget());
        HelpWindow::showTopic(cv ? cv->helpTopic() : QStringLiteral("start"));
    }, QKeySequence(Qt::Key_F1), "help");
    act(hm, tr2("Memulai", "Getting started"), []() { HelpWindow::showTopic("start"); });
    act(hm, tr2("Matriks fitur", "Feature matrix"), [this]() { showFeatureMatrixSheet(this); }, {}, "table-grid");
    hm->addSeparator();
    act(hm, tr2("Tentang De-RAS", "About De-RAS"), [this]() { showAboutSheet(this); }, {}, "info");
}

void MainWindow::fillRecentMenu() {
    recentMenu_->clear();
    const QStringList rec = AppSettings::recentProjects();
    if (rec.isEmpty()) {
        recentMenu_->addAction(tr2("(belum ada)", "(none yet)"))->setEnabled(false);
        return;
    }
    for (const QString& p : rec) {
        QAction* a = recentMenu_->addAction(QDir::toNativeSeparators(p));
        a->setEnabled(QFileInfo::exists(p));
        connect(a, &QAction::triggered, this, [this, p]() { openProject(p); });
    }
}

// ============================ toolbar ============================
QWidget* MainWindow::buildToolbar() {
    toolbar_ = new ToolbarCapsule;
    auto add = [this](const QString& id, const QString& icon, const char* lid, const char* len, const QString& tip = {}) {
        const bool lock = isLocked(id);
        toolbar_->add(id, icon, tr2(lid, len), lock ? lockedTooltip(id) : (tip.isEmpty() ? tr2(lid, len).replace('\n', ' ') : tip), lock ? Availability::Locked : Availability::Available);
    };
    toolbar_->beginGroup(tr2("PROYEK", "PROJECT"));
    // Home button at the far left (review R2 of build 0.2.0); same as Ctrl+1.
    add("home", "home", "Beranda", "Home", tr2("Beranda (Ctrl+1)", "Home (Ctrl+1)"));
    add("open", "folder", "Buka", "Open", tr2("Buka proyek HEC-RAS (.prj) · Ctrl+O", "Open a HEC-RAS project (.prj) · Ctrl+O"));
    add("save", "save", "Simpan", "Save", tr2("Simpan proyek · Ctrl+S", "Save project · Ctrl+S"));
    toolbar_->beginGroup(QStringLiteral("EDITOR"));
    add("geometry", "geometry", "Geometri", "Geometry", tr2("Data Geometri · Ctrl+2", "Geometric Data · Ctrl+2"));
    add("steady", "steady", "Aliran\nSteady", "Steady\nFlow", tr2("Data Aliran Steady · Ctrl+3", "Steady Flow Data · Ctrl+3"));
    add("unsteady", "unsteady", "Aliran\nUnsteady", "Unsteady\nFlow", tr2("Data Aliran Unsteady · Ctrl+4 (Pratinjau)", "Unsteady Flow Data · Ctrl+4 (Preview)"));
    add("quasi", "quasi", "Quasi-\nunsteady", "Quasi-\nunsteady", tr2("Data Quasi-unsteady (Pratinjau)", "Quasi-unsteady Flow Data (Preview)"));
    add("sediment", "sediment", "Sedimen", "Sediment", tr2("Data Sedimen (Pratinjau)", "Sediment Data (Preview)"));
    add("wq", "water", "Kualitas\nAir", "Water\nQuality", tr2("Data Kualitas Air (Pratinjau)", "Water Quality Data (Preview)"));
    toolbar_->beginGroup(tr2("JALANKAN", "RUN"));
    add("analysis", "run", "Analisis\nSteady", "Steady\nAnalysis", tr2("Analisis Steady · Ctrl+5 · Hitung F5", "Steady Flow Analysis · Ctrl+5 · Compute F5"));
    add("run-unsteady", "run-unsteady", "Analisis\nUnsteady", "Unsteady\nAnalysis", tr2("Analisis Unsteady · Hitung F5 (Pratinjau)", "Unsteady Flow Analysis · Compute F5 (Preview)"));
    add("run-sediment", "run-sediment", "Analisis\nSedimen", "Sediment\nAnalysis", tr2("Analisis Sedimen (Pratinjau)", "Sediment Analysis (Preview)"));
    add("run-wq", "run-water", "Analisis\nKual. Air", "Water Qual.\nAnalysis", tr2("Analisis Kualitas Air (Pratinjau)", "Water Quality Analysis (Preview)"));
    add("design", "design", "Desain\nHidraulik", "Hydraulic\nDesign", tr2("Desain Hidraulik: aliran seragam, gerusan, kalibrasi n (Pratinjau)", "Hydraulic Design: uniform flow, scour, n calibration (Preview)"));
    toolbar_->beginGroup(QStringLiteral("GIS"));
    add("mapper", "mapper", "Mapper", "Mapper", tr2("RAS Mapper · Ctrl+6 (Pratinjau)", "RAS Mapper · Ctrl+6 (Preview)"));
    toolbar_->beginGroup(tr2("HASIL & TABEL", "RESULTS & TABLES"));
    add("res-xs", "xs", "Penampang", "Cross\nSection");
    add("res-profile", "profile", "Profil", "Profile");
    add("res-general", "genprofile", "Profil\nUmum", "General\nProfile");
    add("res-rating", "rating", "Kurva\nRating", "Rating\nCurve");
    add("res-3d", "cube", "Perspektif\n3D", "3D\nView");
    add("res-hydrograph", "hydrograph", "Hidrograf", "Hydro-\ngraph");
    add("res-detail", "table-detail", "Tabel\nDetail", "Detailed\nTable");
    add("res-table", "table-grid", "Tabel\nProfil", "Profile\nTable");
    add("res-messages", "warning", "Ringkasan\nPesan", "Message\nSummary");
    add("res-timeseries", "timeseries", "Time\nSeries", "Time\nSeries");
    connect(toolbar_, &ToolbarCapsule::triggered, this, &MainWindow::toolbarTriggered);
    return toolbar_;
}

void MainWindow::toolbarTriggered(const QString& id) {
    if (isLocked(id)) { showLocked(id); return; }
    if (id == "home") { go("home"); return; }
    if (id == "open") { openDialog(); return; }
    if (id == "save") { if (s_->isOpen()) saveProject(); return; }
    if (!s_->isOpen()) {
        showToast(this, tr2("Buka atau buat proyek dulu.", "Open or create a project first."), "info");
        go("home");
        return;
    }
    const QString page = resultsPageFor(id);
    if (!page.isEmpty()) {
        results_->showPage(page);
        go("results");
        return;
    }
    go(id);
}

void MainWindow::showLocked(const QString& featureId) {
    const LockedFeature* f = lockedFeature(featureId);
    if (!f) return;
    showToast(this, tr2(f->nameId, f->nameEn) + QStringLiteral(" — ") + lockedTooltip(featureId), "lock");
}

// ============================ view bar ============================
QWidget* MainWindow::buildViewBar() {
    viewBar_ = new QWidget;
    auto* l = new QHBoxLayout(viewBar_);
    l->setContentsMargins(16, 2, 16, 10);
    l->setSpacing(8);
    back_ = new IconButton("back", tr2("Kembali (Alt+←)", "Back (Alt+←)"), nullptr, 20);
    l->addWidget(back_);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(0);
    crumbs_ = new QHBoxLayout;
    crumbs_->setSpacing(2);
    tv->addLayout(crumbs_);
    auto* tr = new QHBoxLayout;
    tr->setSpacing(8);
    viewTitle_ = new QLabel;
    viewTitle_->setObjectName("ViewTitle");
    tr->addWidget(viewTitle_);
    viewStatus_ = makeBadge(tr2("Pratinjau — belum divalidasi", "Preview — not validated"), "BadgePreview");
    tr->addWidget(viewStatus_);
    viewInfo_ = new QHBoxLayout;
    tr->addLayout(viewInfo_);
    tr->addStretch();
    tv->addLayout(tr);
    l->addLayout(tv, 1);
    actionsHost_ = new QHBoxLayout;
    l->addLayout(actionsHost_);
    connect(back_, &QToolButton::clicked, this, &MainWindow::back);
    return viewBar_;
}

void MainWindow::updateViewBar() {
    auto* v = qobject_cast<View*>(stack_->currentWidget());
    if (!v) return;
    viewBar_->setVisible(v->viewId() != "home");
    back_->setEnabled(canGoBack());
    while (QLayoutItem* it = crumbs_->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    for (const auto& c : v->crumbs()) {
        auto* b = new QPushButton(c.first);
        b->setObjectName("Link");
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral("color:%1;padding:0 2px;").arg(cssColor(Theme::current().text2)));
        connect(b, &QPushButton::clicked, this, [this, id = c.second]() { go(id); });
        crumbs_->addWidget(b);
        auto* sep = new QLabel(QStringLiteral("›"));
        sep->setObjectName("Crumb");
        crumbs_->addWidget(sep);
    }
    auto* cur = new QLabel(v->title());
    cur->setObjectName("Crumb");
    crumbs_->addWidget(cur);
    crumbs_->addStretch();
    viewTitle_->setText(v->title());
    viewStatus_->setVisible(v->status() == "preview");
    while (QLayoutItem* it = viewInfo_->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    viewInfo_->addWidget(new InfoButton(v->helpTopic()));
    QWidget* a = v->actions();
    if (a != currentActions_) {
        if (currentActions_) {
            actionsHost_->removeWidget(currentActions_);
            currentActions_->hide();
        }
        currentActions_ = a;
        if (a) {
            actionsHost_->addWidget(a);
            a->show();
        }
    }
    if (currentActions_) currentActions_->setEnabled(s_->isOpen() || v->viewId() == "options");
    toolbar_->setActive(v->toolbarId());
}

// ============================ views & navigation ============================
void MainWindow::addView(View* v) {
    views_[v->viewId()] = v;
    stack_->addWidget(v);
    connect(v, &View::navigate, this, [this](const QString& id) { go(id); });
    connect(v, &View::headerChanged, this, [this, v]() { if (stack_->currentWidget() == v) updateViewBar(); });
    connect(v, &View::requestRun, this, &MainWindow::runCurrentPlan);
    connect(v, &View::requestDoctor, this, [this]() { runModelDoctor(true); });
    connect(v, &View::requestSave, this, [this]() { saveProject(); });
    connect(v, &View::requestLocked, this, &MainWindow::showLocked);
}

void MainWindow::go(const QString& viewId, bool record) {
    View* v = views_.value(viewId);
    if (!v) return;
    if (!s_->isOpen() && viewId != "home" && viewId != "options") {
        showToast(this, tr2("Buka atau buat proyek dulu.", "Open or create a project first."), "info");
        return;
    }
    if (stack_->currentWidget() == v) {
        v->activated();
        updateViewBar();
        return;
    }
    if (record && stack_->currentWidget()) {
        const QString cur = currentViewId();
        if (history_.isEmpty() || history_.last() != cur) history_ << cur;
        if (history_.size() > 50) history_.removeFirst();
        future_.clear();
    }
    stack_->setCurrentWidget(v);
    v->activated();
    updateViewBar();
    fadeIn(v, 170);
}

// A history entry is usable when it is not the view already shown and the view
// can be opened now (most views need an open project). Unusable entries are
// skipped; in 0.2.0 they made Back silently do nothing (review R3).
bool MainWindow::canVisit(const QString& id) const {
    return views_.contains(id) && id != currentViewId() && (s_->isOpen() || id == "home" || id == "options");
}

bool MainWindow::sheetOpen() const {
    for (const Sheet* sh : findChildren<Sheet*>())
        if (sh->isVisible()) return true;
    return false;
}

bool MainWindow::canGoBack() const {
    for (const QString& id : history_)
        if (canVisit(id)) return true;
    return false;
}

bool MainWindow::canGoForward() const {
    for (const QString& id : future_)
        if (canVisit(id)) return true;
    return false;
}

void MainWindow::back() {
    if (sheetOpen()) return;  // a sheet is modal: finish or cancel it first
    while (!history_.isEmpty()) {
        const QString id = history_.takeLast();
        if (!canVisit(id)) continue;
        const QString cur = currentViewId();
        if (future_.isEmpty() || future_.last() != cur) future_ << cur;
        go(id, false);
        return;
    }
    updateViewBar();
}

void MainWindow::forward() {
    if (sheetOpen()) return;
    while (!future_.isEmpty()) {
        const QString id = future_.takeLast();
        if (!canVisit(id)) continue;
        const QString cur = currentViewId();
        if (history_.isEmpty() || history_.last() != cur) history_ << cur;
        go(id, false);
        return;
    }
    updateViewBar();
}

QStringList MainWindow::backHistory() const { return history_; }

bool MainWindow::eventFilter(QObject* o, QEvent* e) {
    if (e->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(o);
        auto* me = static_cast<QMouseEvent*>(e);
        if (w && w->window() == this && (me->button() == Qt::BackButton || me->button() == Qt::ForwardButton)) {
            me->button() == Qt::BackButton ? back() : forward();
            return true;
        }
    }
    return QMainWindow::eventFilter(o, e);
}

void MainWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::BackButton) { back(); return; }
    if (e->button() == Qt::ForwardButton) { forward(); return; }
    QMainWindow::mousePressEvent(e);
}

// ============================ drawer & status bar ============================
QWidget* MainWindow::buildDrawer() {
    drawer_ = new QWidget;
    auto* outer = new QVBoxLayout(drawer_);
    outer->setContentsMargins(16, 0, 16, 10);
    auto* card = new Card(tr2("Pesan", "Messages"), "messages.drawer");
    card->setFixedHeight(230);
    drawerTabs_ = new Segmented;
    drawerTabs_->addOption(tr2("Log", "Log"), 0);
    drawerTabs_->addOption(QStringLiteral("Model Doctor"), 1);
    card->headerRight()->addWidget(drawerTabs_);
    auto* close = new IconButton("close", tr2("Tutup panel Pesan (Ctrl+M)", "Close the Messages drawer (Ctrl+M)"));
    card->headerRight()->addWidget(close);
    drawerPages_ = new QStackedWidget;
    log_ = new QListWidget;
    log_->setObjectName("NavList");
    log_->setFont(monoFont(Theme::current().basePx - 2));
    log_->setAccessibleName(tr2("Log", "Log"));
    validation_ = new QListWidget;
    validation_->setObjectName("NavList");
    validation_->setWordWrap(true);
    validation_->setAccessibleName(QStringLiteral("Model Doctor"));
    drawerPages_->addWidget(log_);
    drawerPages_->addWidget(validation_);
    card->body()->addWidget(drawerPages_, 1);
    outer->addWidget(card);
    drawer_->hide();
    connect(drawerTabs_, &Segmented::changed, drawerPages_, &QStackedWidget::setCurrentIndex);
    connect(close, &QToolButton::clicked, drawer_, &QWidget::hide);
    connect(validation_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        int r = 0, x = 0;
        if (s_->findLocation(it->data(Qt::UserRole).toString(), r, x)) {
            s_->selectCrossSection(r, x);
            go("xs");
        }
    });
    drawerTabs_->setValue(0);
    return drawer_;
}

void MainWindow::openDrawer(int page) {
    drawerTabs_->setValue(page);
    drawerPages_->setCurrentIndex(page);
    if (!drawer_->isVisible()) {
        drawer_->show();
        fadeIn(drawer_, 150);
    }
}

QWidget* MainWindow::buildStatusBar() {
    auto* sb = new Strip(false);
    sb->setFixedHeight(30);
    auto* l = new QHBoxLayout(sb);
    l->setContentsMargins(14, 0, 10, 0);
    l->setSpacing(12);
    statusText_ = new QLabel(tr2("Siap", "Ready"));
    statusText_->setObjectName("Muted");
    statusText_->setMinimumWidth(120);
    l->addWidget(statusText_, 1);
    runLabel_ = new QPushButton;
    runLabel_->setObjectName("Link");
    runLabel_->setCursor(Qt::PointingHandCursor);
    runLabel_->setToolTip(tr2("Buka Jendela Komputasi", "Open the Compute window"));
    runLabel_->hide();
    connect(runLabel_, &QPushButton::clicked, this, [this]() { compute_->popUp(); });
    runBar_ = new QProgressBar;
    runBar_->setRange(0, 100);
    runBar_->setFixedWidth(140);
    runBar_->setTextVisible(false);
    runBar_->hide();
    l->addWidget(runLabel_);
    l->addWidget(runBar_);
    doctorBtn_ = new QPushButton;
    doctorBtn_->setObjectName("Link");
    doctorBtn_->setCursor(Qt::PointingHandCursor);
    doctorBtn_->setToolTip(tr2("Hasil Model Doctor untuk plan aktif. Klik untuk melihat rinciannya (F7).", "Live Model Doctor for the current plan — click for details (F7)"));
    l->addWidget(doctorBtn_);
    unitsLabel_ = new QLabel;
    unitsLabel_->setObjectName("Muted");
    l->addWidget(unitsLabel_);
    crsLabel_ = new QLabel;
    crsLabel_->setObjectName("Muted");
    crsLabel_->setFont(monoFont(Theme::current().basePx - 2));
    l->addWidget(crsLabel_);
    auto* lang = new QLabel(isID() ? QStringLiteral("ID") : QStringLiteral("EN"));
    lang->setObjectName("Muted");
    lang->setToolTip(tr2("Bahasa tampilan. Ganti di Opsi.", "Interface language — change it in Options"));
    l->addWidget(lang);
    msgBtn_ = new QPushButton(tr2("Pesan", "Messages"));
    msgBtn_->setObjectName("Chip");
    msgBtn_->setIcon(lineIcon("warning", Theme::current().text2));
    msgBtn_->setToolTip(tr2("Buka/tutup panel Pesan (Ctrl+M)", "Open/close the Messages drawer (Ctrl+M)"));
    l->addWidget(msgBtn_);
    connect(msgBtn_, &QPushButton::clicked, this, [this]() { drawer_->isVisible() ? drawer_->hide() : openDrawer(0); });
    connect(doctorBtn_, &QPushButton::clicked, this, [this]() { runModelDoctor(true); });
    connect(compute_, &ComputeWindow::runningChanged, this, [this](bool on) { if (on) statusText_->setText(tr2("Menghitung…", "Computing…")); });
    return sb;
}

void MainWindow::addLog(const QString& line, bool error) {
    auto* it = new QListWidgetItem(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + line);
    if (error) it->setForeground(Theme::current().error);
    log_->addItem(it);
    while (log_->count() > 2000) delete log_->takeItem(0);
    log_->scrollToBottom();
    statusText_->setText(line);
    statusClear_.start();
}

void MainWindow::updateHeader() {
    auto* p = s_->project();
    QString t = QStringLiteral("De-RAS %1").arg(DERAS_VERSION);
    if (p) t = qs(p->info().title) + (p->isModified() ? " •" : "") + QStringLiteral(" — ") + qs(p->baseName()) + QStringLiteral(".prj — ") + t;
    setWindowTitle(t);
    projectChip_->setText(p ? qs(p->baseName()) + QStringLiteral(".prj") + (s_->currentPlanExt().isEmpty() ? QString() : QStringLiteral(" · ") + s_->currentPlanExt())
                            : tr2("Belum ada proyek", "No project"));
    exampleBadge_->setVisible(p && s_->isExample());
    unitsLabel_->setText(p ? (p->units() == UnitSystem::SI ? QStringLiteral("SI") : QStringLiteral("US")) : QString());
    unitsLabel_->setVisible(p != nullptr);
    crsLabel_->setText(p && !p->settings.coordinateSystem.empty() ? qs(p->settings.coordinateSystem) : QString());
    crsLabel_->setVisible(!crsLabel_->text().isEmpty());
}

void MainWindow::updateEnabled() {
    const bool open = s_->isOpen();
    for (QAction* a : needProject_) a->setEnabled(open);
}

void MainWindow::updateDirty() {
    auto* p = s_->project();
    const bool any = p && p->isModified();
    if (auto* b = toolbar_->button("save")) b->setDirty(any);
    bool g = false, f = false, pl = false;
    if (p) {
        for (auto& [e, x] : p->geometries) g = g || x.isModified();
        for (auto& [e, x] : p->steadyFlows) f = f || x.isModified();
        for (auto& [e, x] : p->plans) pl = pl || x.isModified();
    }
    if (auto* b = toolbar_->button("geometry")) b->setDirty(g);
    if (auto* b = toolbar_->button("steady")) b->setDirty(f);
    if (auto* b = toolbar_->button("analysis")) b->setDirty(pl);
}

void MainWindow::updateDoctor() {
    validation_->clear();
    if (!s_->isOpen()) {
        doctorBtn_->setText(QString());
        doctorBtn_->hide();
        return;
    }
    const MessageList msgs = s_->doctor();
    int e = 0, w = 0;
    const Theme& th = Theme::current();
    for (const auto& m : msgs) {
        if (m.severity == Severity::Error) ++e;
        if (m.severity == Severity::Warning) ++w;
        if (m.code == "MD_OK") continue;
        auto* it = new QListWidgetItem(severityPrefix(m.severity) + "  " + msgText(m) + (m.location.empty() ? QString() : QStringLiteral("  ·  ") + qs(m.location)));
        it->setData(Qt::UserRole, qs(m.location));
        it->setForeground(m.severity == Severity::Error ? th.error : m.severity == Severity::Warning ? th.warning : th.text2);
        validation_->addItem(it);
    }
    if (validation_->count() == 0) validation_->addItem(QStringLiteral("✓ ") + tr2("Model lolos pemeriksaan.", "Model passed the checks."));
    doctorBtn_->show();
    doctorBtn_->setText(QStringLiteral("● %1   ● %2").arg(countErrors(e), countWarnings(w)));
    doctorBtn_->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(e ? th.error : w ? th.warning : th.success)));
}

// ============================ project actions ============================
bool MainWindow::saveProject() {
    if (!s_->isOpen()) return false;
    MessageList msgs;
    const bool ok = s_->save(msgs);
    for (const auto& m : msgs) addLog(severityPrefix(m.severity) + " " + msgText(m), m.severity == Severity::Error);
    if (ok) {
        addLog(tr2("Proyek disimpan: %1", "Project saved: %1").arg(QDir::toNativeSeparators(s_->prjPath())));
        showToast(this, tr2("Tersimpan", "Saved"));
    } else {
        openDrawer(0);
        sheetInfo(this, tr2("Gagal menyimpan", "Save failed"),
                  tr2("Proyek gagal disimpan. File lama tidak berubah. Rinciannya ada di panel Pesan.", "The project could not be saved. The previous files were not changed. Details are in the Messages drawer."));
    }
    updateDirty();
    updateHeader();
    return ok;
}

bool MainWindow::maybeSave() {
    auto* p = s_->project();
    if (!p || !p->isModified()) return true;
    const int c = sheetChoice(this, tr2("Perubahan belum disimpan", "Unsaved changes"),
                              tr2("Simpan perubahan pada “%1”?", "Save changes to “%1”?").arg(qs(p->info().title)),
                              {tr2("Batal", "Cancel"), tr2("Jangan simpan", "Don't save"), tr2("Simpan", "Save")}, 2);
    if (c == 2) return saveProject();
    if (c == 1) clearRecovery();   // the user chose to discard the changes
    return c == 1;
}

// ============================ autosave and recovery (M8) ============================
namespace {
QString recoveryRoot() { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery"); }
QString recoveryDirFor(const QString& prj) {
    QString key = QDir::cleanPath(QFileInfo(prj).absoluteFilePath());
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    const QString h = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex().left(10));
    return recoveryRoot() + QStringLiteral("/") + QFileInfo(prj).completeBaseName() + QStringLiteral("-") + h;
}
}  // namespace

void MainWindow::autosaveTick() {
    auto* p = s_->project();
    const int every = AppSettings::autosaveMinutes();
    if (!p || every <= 0 || !p->isModified()) { autosaveTicks_ = 0; return; }
    if (++autosaveTicks_ < every) return;
    autosaveTicks_ = 0;
    MessageList msgs;
    const QString dir = recoveryDirFor(s_->prjPath());
    if (p->writeRecovery(io::toPath(ss(dir)), msgs))
        statusText_->setText(tr2("Autosave %1: salinan pemulihan disimpan (file proyek belum diubah).", "Autosave %1: recovery copy written (project files not changed).")
                                 .arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))));
    else
        for (const auto& m : msgs) addLog(severityPrefix(m.severity) + " Autosave: " + msgText(m), m.severity == Severity::Error);
}

void MainWindow::clearRecovery() {
    autosaveTicks_ = 0;
    if (!s_->isOpen()) return;
    const QString dir = recoveryDirFor(s_->prjPath());
    if (QFileInfo::exists(dir + QStringLiteral("/recovery.json"))) QDir(dir).removeRecursively();
}

void MainWindow::checkRecovery() {
    const QDir root(recoveryRoot());
    if (!root.exists()) return;
    for (const QFileInfo& fi : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
        const QString dir = fi.absoluteFilePath();
        std::string bytes, err;
        json::Value r;
        if (!io::readFile(io::toPath(ss(dir + QStringLiteral("/recovery.json"))), bytes) || !json::parse(bytes, r, err)) continue;
        const QString prj = qs(r.get("project").str());
        const QString when = QFileInfo(dir + QStringLiteral("/recovery.json")).lastModified().toString(QStringLiteral("dd MMM yyyy HH:mm"));
        if (!QFileInfo::exists(prj)) {
            if (sheetConfirm(this, tr2("Salinan pemulihan", "Recovery copy"),
                             tr2("Ada salinan pemulihan (%1) untuk proyek %2, tetapi proyek itu tidak ada lagi. Hapus salinan pemulihannya?",
                                 "There is a recovery copy (%1) for project %2, but that project no longer exists. Delete the recovery copy?").arg(when, QDir::toNativeSeparators(prj)),
                             tr2("Hapus", "Delete"), true))
                QDir(dir).removeRecursively();
            continue;
        }
        const int c = sheetChoice(this, tr2("Pulihkan perubahan yang belum disimpan?", "Restore unsaved changes?"),
                                  tr2("De-RAS ditutup sebelum perubahan pada %1 disimpan. Autosave terakhir: %2.\n\n"
                                      "Pulihkan: proyek dibuka dengan perubahan dari autosave (belum disimpan; tekan Simpan bila sudah diperiksa). "
                                      "Buang: salinan pemulihan dihapus. File proyek tidak diubah oleh pilihan mana pun.",
                                      "De-RAS closed before the changes to %1 were saved. Last autosave: %2.\n\n"
                                      "Restore: the project opens with the autosaved changes (not saved yet; press Save after checking them). "
                                      "Discard: the recovery copy is deleted. Neither choice changes the project files.")
                                      .arg(QDir::toNativeSeparators(prj), when),
                                  {tr2("Buang", "Discard"), tr2("Nanti", "Later"), tr2("Pulihkan", "Restore")}, 2);
        if (c == 0) { QDir(dir).removeRecursively(); continue; }
        if (c != 2) continue;
        openProject(prj);
        auto* p = s_->project();
        if (!p || recoveryDirFor(s_->prjPath()) != dir) continue;
        MessageList msgs;
        if (p->applyRecovery(io::toPath(ss(dir)), msgs)) {
            for (const auto& m : msgs) addLog(severityPrefix(m.severity) + " " + msgText(m), m.severity == Severity::Error);
            emit s_->projectChanged();
            s_->markEdited();
            updateDirty();
            updateHeader();
            addLog(tr2("Perubahan dari autosave %1 dipulihkan. Periksa, lalu Simpan.", "Changes from the autosave of %1 restored. Check them, then Save.").arg(when));
        }
        break;   // one project at a time
    }
}

void MainWindow::openProject(const QString& prjPath) {
    if (isRunning()) { sheetInfo(this, tr2("Run berjalan", "Run in progress"), tr2("Tunggu run selesai atau batalkan dulu.", "Wait for the run to finish or cancel it first.")); return; }
    if (!maybeSave()) return;
    MessageList msgs;
    auto p = io::Project::open(io::toPath(ss(prjPath)), msgs);
    bool warn = false;
    for (const auto& m : msgs) {
        addLog(severityPrefix(m.severity) + " " + msgText(m), m.severity == Severity::Error);
        warn = warn || m.severity != Severity::Info;
    }
    if (!p) {
        QStringList lines;
        for (const auto& m : msgs) if (m.severity == Severity::Error) lines << msgText(m);
        openDrawer(0);
        sheetInfo(this, tr2("Proyek tidak bisa dibuka", "The project could not be opened"), lines.join("\n"));
        return;
    }
    AppSettings::addRecentProject(prjPath);
    s_->setProject(std::move(p));
    history_.clear();
    future_.clear();
    addLog(tr2("Proyek dibuka: %1", "Project opened: %1").arg(QDir::toNativeSeparators(prjPath)));
    if (warn) openDrawer(0);
    go("home", false);
}

void MainWindow::openDialog() {
    const QString fn = QFileDialog::getOpenFileName(this, tr2("Buka proyek HEC-RAS", "Open HEC-RAS project"), QString(), tr2("Proyek HEC-RAS (*.prj)", "HEC-RAS project (*.prj)"));
    if (!fn.isEmpty()) openProject(fn);
}

void MainWindow::newProject() {
    if (isRunning()) { sheetInfo(this, tr2("Run berjalan", "Run in progress"), tr2("Tunggu run selesai atau batalkan dulu.", "Wait for the run to finish or cancel it first.")); return; }
    if (!maybeSave()) return;
    NewProjectSpec sp;
    if (!newProjectSheet(this, sp)) return;
    QDir().mkpath(sp.folder);
    io::ProjectSettings set;
    set.coordinateSystem = ss(sp.crs);
    set.timeZone = ss(sp.timeZone);
    auto p = io::Project::create(io::toPath(ss(sp.folder)), ss(sp.baseName), ss(sp.title), sp.si ? UnitSystem::SI : UnitSystem::USCustomary, set);
    if (!p) { sheetInfo(this, tr2("Proyek baru", "New project"), tr2("Proyek tidak bisa dibuat.", "The project could not be created.")); return; }
    if (sp.prismatic) {
        app::PrismaticSpec ps;
        ps.river = ss(sp.river);
        ps.reach = ss(sp.reach);
        ps.count = sp.count;
        ps.spacing = sp.spacing;
        ps.bottomWidth = sp.bottomWidth;
        ps.sideSlope = sp.sideSlope;
        ps.bankHeight = sp.bankHeight;
        ps.bedSlope = sp.bedSlope;
        ps.dnSlope = sp.bedSlope;
        ps.downstreamInvert = sp.invert;
        ps.nChannel = sp.nChannel;
        ps.nOverbank = sp.nOverbank;
        app::addPrismaticChannel(*p, ps, false);
    } else {
        const std::string g = p->addGeometry(ss(sp.title));
        Reach r;
        r.uid = nextUid();
        r.river = ss(sp.river);
        r.reach = ss(sp.reach);
        p->geometries.at(g).model.reaches.push_back(r);
        const std::string f = p->addSteadyFlow(ss(tr2("Aliran steady", "Steady flow")));
        const std::string pl = p->addPlan(ss(tr2("Plan steady", "Steady plan")), "Steady01", g, f);
        p->info().currentPlan = pl;
    }
    MessageList msgs;
    if (!p->save(msgs)) {
        for (const auto& m : msgs) addLog(severityPrefix(m.severity) + " " + msgText(m), true);
        openDrawer(0);
        sheetInfo(this, tr2("Proyek baru", "New project"), tr2("Proyek gagal disimpan ke folder itu. Pastikan folder itu bisa ditulisi.", "The project could not be saved to the folder. Check write permission."));
        return;
    }
    const QString path = qs(io::fromPath(p->prjPath()));
    AppSettings::addRecentProject(path);
    s_->setProject(std::move(p));
    history_.clear();
    future_.clear();
    addLog(tr2("Proyek baru dibuat: %1", "New project created: %1").arg(QDir::toNativeSeparators(path)));
    go("home", false);
    if (!sp.prismatic) go("geometry");  // Back returns to Beranda
    showToast(this, tr2("Proyek dibuat", "Project created"));
}

void MainWindow::duplicateProject() {
    if (!s_->isOpen()) return;
    if (s_->project()->isModified() && !maybeSave()) return;
    const QString src = s_->prjPath();
    const QFileInfo si(src);
    const QString dst = QFileDialog::getSaveFileName(this, tr2("Simpan duplikat sebagai", "Save duplicate as"), si.dir().filePath(si.completeBaseName() + "_salinan.prj"), "HEC-RAS (*.prj)");
    if (dst.isEmpty()) return;
    const QFileInfo di(dst);
    if (di.exists()) { sheetInfo(this, tr2("Duplikasi", "Duplicate"), tr2("File dengan nama itu sudah ada. Pilih nama lain.", "The target file already exists; choose another name.")); return; }
    MessageList msgs;
    const bool ok = io::Project::duplicate(io::toPath(ss(src)), io::toPath(ss(di.absolutePath())), ss(di.completeBaseName()), msgs);
    for (const auto& m : msgs) addLog(severityPrefix(m.severity) + " " + msgText(m), m.severity == Severity::Error);
    if (!ok) { openDrawer(0); sheetInfo(this, tr2("Duplikasi", "Duplicate"), tr2("Duplikasi gagal. Lihat panel Pesan.", "Duplication failed. See the Messages drawer.")); return; }
    s_->close();
    openProject(di.absoluteFilePath());
}

void MainWindow::openExample(bool restore) {
    if (isRunning()) return;
    if (!maybeSave()) return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/De-RAS Projects/Contoh";
    const QString nativeDir = QDir::toNativeSeparators(dir);
    const bool installed = QFileInfo::exists(QDir(dir).filePath("DerasUji1km.prj"));
    bool replace = false;
    if (installed && restore) {
        replace = sheetConfirm(this, tr2("Pulihkan proyek contoh", "Restore example project"),
                               tr2("Proyek contoh di %1 diganti dengan salinan baru dari De-RAS %2. Folder lama tidak dihapus: "
                                   "folder itu diganti nama menjadi Contoh-lama-<tanggal>, sehingga perubahan Anda tetap ada.",
                                   "The example project in %1 is replaced by a fresh copy from De-RAS %2. The old folder is not deleted: "
                                   "it is renamed to Contoh-lama-<date>, so your changes are kept.").arg(nativeDir, DERAS_VERSION),
                               tr2("Pulihkan", "Restore"));
        if (!replace) return;
    } else if (installed && ProjectSession::exampleVersion(dir) != QStringLiteral(DERAS_VERSION)) {
        const QString old = ProjectSession::exampleVersion(dir);
        const int c = sheetChoice(this, tr2("Proyek contoh versi lama", "Example project from an older version"),
                                  tr2("Proyek contoh di %1 dibuat oleh %2. De-RAS %3 membawa proyek contoh yang lebih baru "
                                      "(antara lain plan p03 dengan area 2D, data sedimen, dan data kualitas air). Perbarui? Folder lama tidak dihapus, "
                                      "hanya diganti nama menjadi Contoh-lama-<tanggal>.",
                                      "The example project in %1 was written by %2. De-RAS %3 includes a newer example "
                                      "(including plan p03 with a 2D area, sediment data and water quality data). Update it? The old folder is not deleted, "
                                      "only renamed to Contoh-lama-<date>.")
                                      .arg(nativeDir, old.isEmpty() ? tr2("versi sebelum 0.2.3", "a version before 0.2.3") : QStringLiteral("De-RAS ") + old,
                                           DERAS_VERSION),
                                  {tr2("Buka yang lama", "Open the old one"), tr2("Perbarui", "Update")}, 1);
        if (c < 0) return;
        replace = c == 1;
    }
    QString movedTo;
    if (replace && s_->isOpen() && QDir::cleanPath(QFileInfo(s_->prjPath()).absolutePath()) == QDir::cleanPath(QFileInfo(dir).absoluteFilePath())) {
        s_->close();
        history_.clear();
        future_.clear();
    }
    const QString prj = replace ? ProjectSession::restoreExample(dir, &movedTo) : ProjectSession::installExample(dir);
    if (prj.isEmpty()) {
        sheetInfo(this, tr2("Proyek contoh", "Example project"),
                  replace ? tr2("Proyek contoh tidak bisa diperbarui di %1. Tutup berkas dari folder itu yang masih terbuka di program lain, lalu coba lagi.",
                                "The example project in %1 could not be updated. Close any file from that folder that is open in another program, then try again.").arg(nativeDir)
                          : tr2("Proyek contoh tidak bisa dibuat di %1.", "The example project could not be created in %1.").arg(nativeDir));
        return;
    }
    openProject(prj);
    if (!movedTo.isEmpty())
        addLog(tr2("Proyek contoh diperbarui ke De-RAS %1. Salinan lama: %2", "Example project updated to De-RAS %1. Old copy: %2").arg(DERAS_VERSION, QDir::toNativeSeparators(movedTo)));
    addLog(tr2("Proyek contoh berisi data buatan (berlabel CONTOH), bukan data lapangan.", "Example project: synthetic data (labelled Example), not field data."));
}

void MainWindow::closeProject() {
    if (isRunning()) { sheetInfo(this, tr2("Run berjalan", "Run in progress"), tr2("Tunggu run selesai atau batalkan dulu.", "Wait for the run to finish or cancel it first.")); return; }
    if (!maybeSave()) return;
    s_->close();
    history_.clear();
    future_.clear();
    go("home", false);
}

void MainWindow::editDescription() {
    auto* p = s_->project();
    if (!p) return;
    QString t = qs(p->info().description);
    if (!descriptionSheet(this, t)) return;
    if (ss(t) == p->info().description) return;
    p->info().description = ss(t);
    s_->markEdited();
    home_->reload();
}

void MainWindow::runModelDoctor(bool open) {
    updateDoctor();
    analysis_->refreshDoctor();
    uanalysis_->refreshDoctor();
    const MessageList msgs = s_->doctor();
    int e = 0, w = 0;
    for (const auto& m : msgs) {
        if (m.severity == Severity::Error) ++e;
        if (m.severity == Severity::Warning) ++w;
    }
    addLog(QStringLiteral("%1 Model Doctor: ").arg(e ? QStringLiteral("⛔") : QStringLiteral("✓")) +
               tr2("%1, %2 untuk plan %3.", "%1, %2 for plan %3.").arg(countErrors(e), countWarnings(w), s_->currentPlanExt()),
           e > 0);
    if (open) openDrawer(1);
}

void MainWindow::runCurrentPlan() {
    if (!s_->isOpen()) return;
    if (!s_->currentPlan()) { showToast(this, tr2("Belum ada plan. Buat plan di Analisis Steady.", "No plan yet. Create one in Steady Flow Analysis."), "warning"); go("analysis"); return; }
    const MessageList msgs = s_->doctor();
    if (hasErrors(msgs)) {
        runModelDoctor(false);
        auto* pl = s_->currentPlan();
        const QString planLabel = s_->currentPlanExt() + (pl && !pl->model.title.empty() ? QStringLiteral(" · ") + qs(pl->model.title) : QString());
        const DoctorChoice c = doctorSheet(this, msgs, planLabel);
        switch (c.kind) {
            case DoctorChoice::OpenDrawer: openDrawer(1); break;
            case DoctorChoice::FixRegime:
                if (pl) {
                    pl->model.regime = FlowRegime::Subcritical;
                    s_->markEdited();
                    analysis_->reload();
                    addLog(tr2("Rezim plan %1 diganti ke Subkritis.", "Plan %1 regime switched to Subcritical.").arg(s_->currentPlanExt()));
                    showToast(this, tr2("Rezim diganti ke Subkritis", "Regime switched to Subcritical"));
                    runCurrentPlan();  // the user asked to compute: continue if nothing else blocks
                }
                break;
            case DoctorChoice::OpenLocation: {
                int r = 0, x = 0;
                if (s_->findLocation(c.location, r, x)) {
                    s_->selectCrossSection(r, x);
                    go("xs");
                }
                break;
            }
            case DoctorChoice::OpenView: go(c.view); break;
            case DoctorChoice::Close: break;
        }
        return;
    }
    compute_->popUp();
    compute_->enqueue(s_->currentPlanExt());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (isRunning()) {
        if (!sheetConfirm(this, tr2("Run berjalan", "Run in progress"), tr2("Run masih berjalan. Batalkan run dan keluar?", "A run is still in progress. Cancel it and quit?"), tr2("Batalkan dan keluar", "Cancel and quit"), true)) {
            e->ignore();
            return;
        }
        compute_->cancel();
    }
    if (!maybeSave()) { e->ignore(); return; }
    compute_->hide();
    HelpWindow::closeWindow();
    e->accept();
}

}  // namespace deras::ui

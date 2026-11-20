// De-RAS - ui/HomeView.cpp
#include "HomeView.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimerEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

#include "Common.h"
#include "Help.h"
#include "Icons.h"
#include "Widgets.h"

namespace deras::ui {

namespace {
QLineEdit* fileField(const QString& placeholder) {
    auto* e = new QLineEdit;
    e->setObjectName("Mono");
    e->setReadOnly(true);
    e->setPlaceholderText(placeholder);
    e->setFocusPolicy(Qt::ClickFocus);
    return e;
}
QLabel* rowLabel(const QString& t) {
    auto* l = new QLabel(t);
    l->setObjectName("Strong");
    l->setMinimumWidth(130);
    return l;
}

// Clickable recent-project tile.
class RecentTile : public QFrame {
public:
    RecentTile(const QString& path, std::function<void()> open, std::function<void()> remove) : open_(std::move(open)) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setMinimumHeight(62);
        const QFileInfo fi(path);
        const bool exists = fi.exists();
        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(14, 10, 10, 10);
        h->setSpacing(12);
        auto* ic = new QLabel;
        ic->setFixedSize(32, 32);
        ic->setAlignment(Qt::AlignCenter);
        const Theme& t = Theme::current();
        ic->setStyleSheet(QStringLiteral("background:%1; border-radius:8px;").arg(cssColor(t.selected)));
        ic->setPixmap(iconPixmap(exists ? "file" : "warning", 16, exists ? t.accent : t.warning));
        h->addWidget(ic);
        auto* v = new QVBoxLayout;
        v->setSpacing(2);
        auto* top = new QHBoxLayout;
        top->setSpacing(8);
        auto* name = new QLabel(fi.completeBaseName());
        name->setObjectName("Strong");
        top->addWidget(name);
        if (path.contains("Contoh", Qt::CaseInsensitive)) top->addWidget(makeBadge(tr2("CONTOH", "EXAMPLE"), "BadgeExample"));
        top->addStretch();
        v->addLayout(top);
        auto* p = new QLabel;
        p->setObjectName("Mono");
        p->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(t.text3)));
        const QString native = QDir::toNativeSeparators(path);
        p->setText(QFontMetrics(monoFont(t.basePx - 2)).elidedText(native, Qt::ElideMiddle, 300));
        p->setToolTip(exists ? native : tr2("File tidak ditemukan (dipindah atau dihapus?): ", "File not found (moved or deleted?): ") + native);
        v->addWidget(p);
        h->addLayout(v, 1);
        auto* x = new IconButton("close", tr2("Hapus dari daftar", "Remove from list"), nullptr, 12);
        QObject::connect(x, &QToolButton::clicked, this, [remove]() { remove(); });
        h->addWidget(x, 0, Qt::AlignTop);
        setAccessibleName(fi.completeBaseName());
    }
protected:
    void paintEvent(QPaintEvent*) override {
        const Theme& t = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(hover_ || hasFocus() ? t.focus : (t.dark ? withAlpha(t.accent, 0.18) : t.line), hasFocus() ? 2 : 1));
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, t.dark ? QColor(13, 24, 44, hover_ ? 235 : 205) : QColor(255, 255, 255));
        g.setColorAt(1, t.dark ? QColor(8, 15, 29, 225) : QColor(251, 252, 254));
        p.setBrush(g);
        p.drawRoundedRect(r, 12, 12);
    }
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }
    void mouseReleaseEvent(QMouseEvent* e) override { if (rect().contains(e->position().toPoint()) && e->button() == Qt::LeftButton) open_(); }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) open_();
        else QFrame::keyPressEvent(e);
    }
private:
    std::function<void()> open_;
    bool hover_ = false;
};
}  // namespace

HomeView::HomeView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    setAttribute(Qt::WA_TranslucentBackground, false);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->addWidget(scroll);
    auto* page = new QWidget;
    page->setAttribute(Qt::WA_TranslucentBackground);
    scroll->setWidget(page);
    scroll->viewport()->setAutoFillBackground(false);
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(24, 24, 24, 24);
    row->addStretch(1);
    auto* col = new QWidget;
    col->setMaximumWidth(1180);
    col->setMinimumWidth(760);
    col->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    row->addWidget(col, 20);
    row->addStretch(1);
    auto* v = new QVBoxLayout(col);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);
    v->addStretch(2);

    card_ = new Card(tr2("Kontrol Proyek", "Project Control"), "home.control");
    card_->setCaptionDot(true);
    card_->setPadding(24);
    headInfo_ = new QLabel;
    headInfo_->setObjectName("Mono");
    headInfo_->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(Theme::current().text2)));
    card_->headerRight()->addWidget(headInfo_);
    card_->body()->addWidget(makeHLine());
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    card_->body()->addLayout(grid);

    // Proyek
    prjTitle_ = new QLineEdit;
    prjTitle_->setPlaceholderText(tr2("Judul proyek", "Project title"));
    prjTitle_->setAccessibleName(tr2("Judul proyek", "Project title"));
    prjFile_ = fileField(tr2("File proyek (.prj)", "Project file (.prj)"));
    folderBtn_ = new IconButton("folder", tr2("Buka folder proyek", "Open project folder"));
    grid->addWidget(rowLabel(tr2("Proyek", "Project")), 0, 0);
    grid->addWidget(prjTitle_, 0, 1);
    grid->addWidget(prjFile_, 0, 2);
    grid->addWidget(folderBtn_, 0, 3);
    auto combo = [](const QString& acc) {
        auto* c = new QComboBox;
        c->setAccessibleName(acc);
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(18);
        return c;
    };
    plan_ = combo(tr2("Plan", "Plan"));
    planFile_ = fileField(tr2("File plan (.p01)", "Plan file (.p01)"));
    planGo_ = new IconButton("arrow-right", tr2("Buka Analisis Steady untuk plan ini", "Open Steady Analysis for this plan"));
    grid->addWidget(rowLabel(tr2("Plan", "Plan")), 1, 0);
    grid->addWidget(plan_, 1, 1);
    grid->addWidget(planFile_, 1, 2);
    grid->addWidget(planGo_, 1, 3);
    geom_ = combo(tr2("Geometri", "Geometry"));
    geomFile_ = fileField(tr2("File geometri (.g01)", "Geometry file (.g01)"));
    geomGo_ = new IconButton("arrow-right", tr2("Buka Data Geometri", "Open Geometric Data"));
    grid->addWidget(rowLabel(tr2("Geometri", "Geometry")), 2, 0);
    grid->addWidget(geom_, 2, 1);
    grid->addWidget(geomFile_, 2, 2);
    grid->addWidget(geomGo_, 2, 3);
    flow_ = combo(tr2("Aliran steady", "Steady flow"));
    flowFile_ = fileField(tr2("File aliran steady (.f01)", "Steady flow file (.f01)"));
    flowGo_ = new IconButton("arrow-right", tr2("Buka Data Aliran Steady", "Open Steady Flow Data"));
    grid->addWidget(rowLabel(tr2("Aliran Steady", "Steady Flow")), 3, 0);
    grid->addWidget(flow_, 3, 1);
    grid->addWidget(flowFile_, 3, 2);
    grid->addWidget(flowGo_, 3, 3);
    uns_ = combo(tr2("Aliran unsteady", "Unsteady flow"));
    unsFile_ = fileField(tr2("File aliran unsteady (.u01)", "Unsteady flow file (.u01)"));
    unsGo_ = new IconButton("arrow-right", tr2("Buka Data Aliran Unsteady (Pratinjau)", "Open Unsteady Flow Data (Preview)"));
    grid->addWidget(rowLabel(tr2("Aliran Unsteady", "Unsteady Flow")), 4, 0);
    grid->addWidget(uns_, 4, 1);
    grid->addWidget(unsFile_, 4, 2);
    grid->addWidget(unsGo_, 4, 3);
    // Deskripsi + units
    auto* dl = rowLabel(tr2("Deskripsi", "Description"));
    grid->addWidget(dl, 5, 0, Qt::AlignTop);
    auto* drow = new QHBoxLayout;
    drow->setSpacing(10);
    desc_ = new QPlainTextEdit;
    desc_->setPlaceholderText(tr2("Deskripsi proyek, datum, dan catatan hidraulik…", "Project description, datum and hydraulic notes…"));
    desc_->setFixedHeight(62);
    desc_->setAccessibleName(tr2("Deskripsi proyek", "Project description"));
    descBtn_ = new IconButton("description", tr2("Edit deskripsi di lembar yang lebih lebar", "Edit the description in a larger sheet"));
    descBtn_->setFixedSize(40, 62);
    units_ = new QLabel;
    units_->setAlignment(Qt::AlignCenter);
    units_->setFixedSize(118, 62);
    drow->addWidget(desc_, 1);
    drow->addWidget(descBtn_);
    drow->addWidget(units_);
    grid->addLayout(drow, 5, 1, 1, 3);

    emptyActions_ = new QWidget;
    auto* ea = new QHBoxLayout(emptyActions_);
    ea->setContentsMargins(0, 6, 0, 0);
    ea->addStretch();
    auto* nb = makeButton(tr2("+  Proyek baru", "+  New project"), true);
    auto* ob = makeButton(tr2("Buka proyek…", "Open project…"));
    auto* xb = makeButton(tr2("Proyek contoh", "Example project"));
    for (auto* b : {nb, ob, xb}) b->setMinimumHeight(38);
    ea->addWidget(withInfo(nb, "hub.new"));
    ea->addWidget(withInfo(ob, "hub.open"));
    ea->addWidget(withInfo(xb, "hub.example"));
    ea->addStretch();
    card_->body()->addWidget(emptyActions_);
    v->addWidget(card_);

    v->addSpacing(8);
    recentCaption_ = makeCaption(tr2("Proyek terakhir", "Recent projects"));
    recentCaption_->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(Theme::current().text2)));
    auto* rh = new QHBoxLayout;
    rh->addWidget(recentCaption_);
    rh->addWidget(new InfoButton("hub.recent"));
    rh->addStretch();
    v->addLayout(rh);
    recentGrid_ = new QGridLayout;
    recentGrid_->setSpacing(12);
    v->addLayout(recentGrid_);
    v->addStretch(3);

    connect(nb, &QPushButton::clicked, this, &HomeView::newProjectRequested);
    connect(ob, &QPushButton::clicked, this, &HomeView::openRequested);
    connect(xb, &QPushButton::clicked, this, &HomeView::exampleRequested);
    connect(folderBtn_, &QToolButton::clicked, this, [this]() {
        if (s_->project()) QDesktopServices::openUrl(QUrl::fromLocalFile(qs(io::fromPath(s_->project()->dir()))));
        else emit openRequested();
    });
    connect(planGo_, &QToolButton::clicked, this, [this]() {
        auto* pl = s_->currentPlan();
        emit navigate(pl && pl->model.isUnsteady() ? "run-unsteady" : "analysis");
    });
    connect(geomGo_, &QToolButton::clicked, this, [this]() { emit navigate("geometry"); });
    connect(flowGo_, &QToolButton::clicked, this, [this]() { emit navigate("steady"); });
    connect(unsGo_, &QToolButton::clicked, this, [this]() { emit navigate("unsteady"); });
    connect(descBtn_, &QToolButton::clicked, this, &HomeView::editDescriptionRequested);
    connect(prjTitle_, &QLineEdit::editingFinished, this, [this]() {
        if (loading_ || !s_->project()) return;
        const std::string t = ss(prjTitle_->text().trimmed());
        if (t.empty() || t == s_->project()->info().title) return;
        s_->project()->info().title = t;
        s_->markEdited();
    });
    connect(desc_, &QPlainTextEdit::textChanged, this, [this]() {
        if (loading_ || !s_->project()) return;
        const std::string d = ss(desc_->toPlainText());
        if (d == s_->project()->info().description) return;
        s_->project()->info().description = d;
        s_->markEdited();
    });
    connect(plan_, &QComboBox::activated, this, [this]() { s_->setCurrentPlan(plan_->currentData().toString()); });
    connect(geom_, &QComboBox::activated, this, [this]() { s_->setEditGeometry(geom_->currentData().toString()); });
    connect(flow_, &QComboBox::activated, this, [this]() { s_->setEditFlow(flow_->currentData().toString()); });
    connect(uns_, &QComboBox::activated, this, [this]() { s_->setEditUnsteady(uns_->currentData().toString()); });
    reload();
}

QString HomeView::title() const { return tr2("Beranda", "Home"); }

void HomeView::activated() { reload(); }

void HomeView::reload() {
    loading_ = true;
    auto* p = s_->project();
    const bool open = p != nullptr;
    for (QWidget* w : std::initializer_list<QWidget*>{prjTitle_, plan_, geom_, flow_, uns_, desc_, planGo_, geomGo_, flowGo_, descBtn_}) w->setEnabled(open);
    unsGo_->setEnabled(open);
    emptyActions_->setVisible(!open);
    for (auto* c : {plan_, geom_, flow_, uns_}) c->clear();
    const Theme& t = Theme::current();
    const bool si = !p || p->units() == UnitSystem::SI;
    units_->setText(QStringLiteral("<span style='color:%1'>●</span>&nbsp; <b>%2</b>").arg(t.accent.name(), si ? "SI Units" : "US Customary"));
    units_->setToolTip(tr2("Sistem satuan proyek (dibaca dari .prj). Konversi satuan proyek belum tersedia.", "Project unit system (read from the .prj). Project unit conversion is not available yet."));
    units_->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; border-radius: 12px; background: %2; color: %3; }")
                              .arg(cssColor(t.focus), cssColor(withAlpha(t.primary, t.dark ? 0.16 : 0.08)), cssColor(t.heading)));
    if (!open) {
        prjTitle_->clear();
        for (auto* e : {prjFile_, planFile_, geomFile_, flowFile_, unsFile_}) e->clear();
        desc_->clear();
        for (auto* c : {plan_, geom_, flow_, uns_}) c->setPlaceholderText(c->accessibleName());
        plan_->setPlaceholderText(tr2("Judul plan", "Plan title"));
        geom_->setPlaceholderText(tr2("Judul geometri", "Geometry title"));
        flow_->setPlaceholderText(tr2("Judul aliran steady", "Steady flow title"));
        uns_->setPlaceholderText(tr2("Judul aliran unsteady", "Unsteady flow title"));
        headInfo_->setText(tr2("Belum ada proyek. Buat proyek baru atau buka file .prj.", "No project — create or open a .prj file"));
        loading_ = false;
        fillRecent();
        return;
    }
    const QString base = qs(p->baseName());
    prjTitle_->setText(qs(p->info().title));
    prjFile_->setText(QDir::toNativeSeparators(qs(io::fromPath(p->prjPath()))));
    prjFile_->setCursorPosition(0);
    desc_->setPlainText(qs(p->info().description));
    for (auto& [e, pl] : p->plans) plan_->addItem(qs(pl.model.title.empty() ? e : pl.model.title), qs(e));
    for (auto& [e, g] : p->geometries) geom_->addItem(qs(g.model.title.empty() ? e : g.model.title), qs(e));
    for (auto& [e, f] : p->steadyFlows) flow_->addItem(qs(f.model.title.empty() ? e : f.model.title), qs(e));
    for (const auto& u : p->info().unsteadyFiles) {
        const QString t2 = s_->unsteadyTitle(qs(u));
        uns_->addItem(t2.isEmpty() ? qs(u) : t2, qs(u));
    }
    plan_->setCurrentIndex(plan_->findData(s_->currentPlanExt()));
    geom_->setCurrentIndex(geom_->findData(s_->currentGeometryExt()));
    flow_->setCurrentIndex(flow_->findData(s_->currentFlowExt()));
    uns_->setCurrentIndex(uns_->findData(s_->currentUnsteadyExt()));
    plan_->setPlaceholderText(tr2("(belum ada plan)", "(no plan yet)"));
    geom_->setPlaceholderText(tr2("(belum ada geometri)", "(no geometry yet)"));
    flow_->setPlaceholderText(tr2("(belum ada data aliran)", "(no flow data yet)"));
    uns_->setPlaceholderText(tr2("(tidak ada)", "(none)"));
    uns_->setEnabled(uns_->count() > 0);
    unsGo_->setEnabled(true);
    planFile_->setText(s_->currentPlanExt().isEmpty() ? QString() : base + "." + s_->currentPlanExt());
    geomFile_->setText(s_->currentGeometryExt().isEmpty() ? QString() : base + "." + s_->currentGeometryExt());
    flowFile_->setText(s_->currentFlowExt().isEmpty() ? QString() : base + "." + s_->currentFlowExt());
    unsFile_->setText(uns_->currentIndex() >= 0 ? base + "." + uns_->currentData().toString() : QString());
    headInfo_->setText(tr2("Proyek: %1.prj · plan aktif %2", "Project: %1.prj · current plan %2").arg(base, s_->currentPlanExt().isEmpty() ? "—" : s_->currentPlanExt()));
    loading_ = false;
    fillRecent();
}

void HomeView::fillRecent() {
    while (QLayoutItem* it = recentGrid_->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }
    const QStringList rec = AppSettings::recentProjects();
    int i = 0;
    for (const QString& p : rec) {
        if (i >= 6) break;
        if (s_->isOpen() && QFileInfo(p) == QFileInfo(s_->prjPath())) continue;
        auto* tile = new RecentTile(p, [this, p]() { emit openPath(p); }, [this, p]() { AppSettings::removeRecentProject(p); QMetaObject::invokeMethod(this, [this]() { fillRecent(); }, Qt::QueuedConnection); });
        recentGrid_->addWidget(tile, i / 3, i % 3);
        ++i;
    }
    for (int c = 0; c < 3; ++c) recentGrid_->setColumnStretch(c, 1);
    if (i == 0) {
        auto* l = makeMuted(s_->isOpen() ? tr2("Belum ada proyek lain yang pernah dibuka.", "No other project opened yet.")
                                         : tr2("Belum ada proyek terakhir. Buat proyek baru atau buka file .prj.", "No recent projects yet. Create a new project or open a .prj file."));
        recentGrid_->addWidget(l, 0, 0, 1, 3);
    }
}

void HomeView::paintEvent(QPaintEvent*) {
    // Faint, slowly moving contour lines and a river course (static when Windows animations are off).
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double W = width(), H = height();
    for (int k = 0; k < 9; ++k) {
        QPainterPath path;
        const double base = H * (0.12 + k * 0.105);
        const double amp = 10 + 6 * std::sin(k * 1.7);
        for (int i = 0; i <= 48; ++i) {
            const double x = W * i / 48.0;
            const double y = base + amp * std::sin(x / W * 6.2 + k * 0.9 + phase_ * (0.6 + 0.1 * k)) + 7 * std::sin(x / W * 13.0 - k);
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        p.setPen(QPen(withAlpha(t.accent, t.dark ? 0.045 : 0.06), 1.0));
        p.drawPath(path);
    }
    QPainterPath river;
    river.moveTo(-20, H * 0.82);
    river.cubicTo(W * 0.25, H * 0.70, W * 0.45, H * 0.95, W * 0.7, H * 0.80);
    river.cubicTo(W * 0.85, H * 0.72, W * 0.95, H * 0.76, W + 20, H * 0.70);
    p.setPen(QPen(withAlpha(t.accent, t.dark ? 0.07 : 0.08), 10, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(river);
    QPen flow(withAlpha(t.accent2, t.dark ? 0.16 : 0.14), 2, Qt::CustomDashLine, Qt::RoundCap);
    flow.setDashPattern({18, 60});
    flow.setDashOffset(-phase_ * 40);
    p.setPen(flow);
    p.drawPath(river);
}

void HomeView::showEvent(QShowEvent* e) {
    View::showEvent(e);
    if (!timer_ && animationsEnabled()) timer_ = startTimer(60);
}

void HomeView::hideEvent(QHideEvent* e) {
    View::hideEvent(e);
    if (timer_) { killTimer(timer_); timer_ = 0; }
}

void HomeView::timerEvent(QTimerEvent* e) {
    if (e->timerId() != timer_) return;
    phase_ += 0.012;
    update();
}

}  // namespace deras::ui

// De-RAS - ui/MapperView.cpp
#include "MapperView.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Common.h"
#include "Help.h"
#include "Icons.h"
#include "NumTable.h"
#include "Widgets.h"
#include "io/RasText.h"

namespace deras::ui {

// ============================ raster images ============================
namespace {
QColor ramp(double t) {
    struct S { double t; int r, g, b; };
    static const S st[] = {{0, 56, 104, 66}, {0.35, 142, 156, 92}, {0.65, 186, 156, 116}, {1, 242, 238, 232}};
    t = std::clamp(t, 0.0, 1.0);
    for (int i = 1; i < 4; ++i)
        if (t <= st[i].t) {
            const double f = (t - st[i - 1].t) / (st[i].t - st[i - 1].t);
            return QColor(int(st[i - 1].r + f * (st[i].r - st[i - 1].r)), int(st[i - 1].g + f * (st[i].g - st[i - 1].g)),
                          int(st[i - 1].b + f * (st[i].b - st[i - 1].b)));
        }
    return QColor(st[3].r, st[3].g, st[3].b);
}

MapImage imageOf(const io::Raster& r, QImage img) {
    MapImage m;
    m.img = std::move(img);
    m.x0 = r.x0;
    m.y0 = r.y0;
    m.x1 = r.xMax();
    m.y1 = r.yMin();
    return m;
}

void elevationRange(const std::vector<io::Raster>& ts, double& lo, double& hi) {
    std::vector<float> v;
    for (const auto& t : ts) {
        const size_t step = std::max<size_t>(1, t.z.size() / 200000);
        for (size_t i = 0; i < t.z.size(); i += step)
            if (!std::isnan(t.z[i])) v.push_back(t.z[i]);
    }
    if (v.empty()) { lo = 0; hi = 1; return; }
    std::sort(v.begin(), v.end());
    lo = v[size_t(v.size() * 0.02)];
    hi = v[std::min(v.size() - 1, size_t(v.size() * 0.98))];
    if (hi <= lo) hi = lo + 1;
}

QString fmt(double v, int d) { return QString::number(v, 'f', d); }
}  // namespace

QImage terrainImage(const io::Raster& r, double zmin, double zmax, int maxSide) {
    const int step = std::max(1, int(std::ceil(double(std::max(r.nx, r.ny)) / maxSide)));
    const int w = (r.nx + step - 1) / step, h = (r.ny + step - 1) / step;
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    const double lx = -0.5, ly = 0.5, lz = 0.7071;  // light from the north-west
    auto at = [&](int c, int rr) -> double {
        c = std::clamp(c, 0, r.nx - 1);
        rr = std::clamp(rr, 0, r.ny - 1);
        return r.cell(c, rr);
    };
    for (int j = 0; j < h; ++j) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(j));
        for (int i = 0; i < w; ++i) {
            const int c = i * step, rr = j * step;
            const double z = r.cell(c, rr);
            if (std::isnan(z)) continue;
            double ze = at(c + step, rr), zw = at(c - step, rr), zn = at(c, rr - step), zs = at(c, rr + step);
            if (std::isnan(ze)) ze = z;
            if (std::isnan(zw)) zw = z;
            if (std::isnan(zn)) zn = z;
            if (std::isnan(zs)) zs = z;
            const double dzdx = (ze - zw) / (2 * step * r.dx), dzdy = (zn - zs) / (2 * step * r.dy);
            const double nx = -dzdx * 3, ny = -dzdy * 3, nz = 1, nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            const double shade = std::max(0.0, (nx * lx + ny * ly + nz * lz) / nl);
            const QColor col = ramp((z - zmin) / (zmax - zmin));
            const double f = 0.45 + 0.6 * shade;
            line[i] = qRgb(std::min(255, int(col.red() * f)), std::min(255, int(col.green() * f)), std::min(255, int(col.blue() * f)));
        }
    }
    return img;
}

QImage depthImage(const io::Raster& d, double maxDepth) {
    QImage img(d.nx, d.ny, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    const double md = maxDepth > 0 ? maxDepth : 1;
    for (int j = 0; j < d.ny; ++j) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(j));
        for (int i = 0; i < d.nx; ++i) {
            const double v = d.cell(i, j);
            if (std::isnan(v)) continue;
            const double t = std::clamp(v / md, 0.0, 1.0);
            line[i] = qRgba(int(165 - t * 157), int(225 - t * 177), int(255 - t * 115), 220);
        }
    }
    return img;
}

// ============================ canvas ============================
MapCanvas::MapCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
}

QPointF MapCanvas::toScreen(double x, double y) const { return {width() / 2.0 + (x - cx_) * scale_, height() / 2.0 - (y - cy_) * scale_}; }

mapping::Pt MapCanvas::toMap(const QPointF& p) const { return {cx_ + (p.x() - width() / 2.0) / scale_, cy_ - (p.y() - height() / 2.0) / scale_}; }

void MapCanvas::extent(double& x0, double& y0, double& x1, double& y1) const {
    x0 = y0 = 1e300;
    x1 = y1 = -1e300;
    auto addPt = [&](double x, double y) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); };
    for (const auto& m : terrain_) { addPt(m.x0, m.y0); addPt(m.x1, m.y1); }
    for (const auto& m : pictures_) { addPt(m.x0, m.y0); addPt(m.x1, m.y1); }
    if (geo_)
        for (const auto& r : geo_->reaches) {
            for (const auto& p : r.xy) addPt(p.first, p.second);
            for (const auto& c : mapping::cutLines(r))
                for (const auto& p : c.line) addPt(p.x, p.y);
        }
    if (geo_)
        for (const auto& a : geo_->storageAreas)
            for (const auto& p : a.polygon) addPt(p.first, p.second);
}

bool MapCanvas::hasContent() const {
    double x0, y0, x1, y1;
    extent(x0, y0, x1, y1);
    return x1 >= x0;
}

void MapCanvas::fit() {
    double x0, y0, x1, y1;
    extent(x0, y0, x1, y1);
    if (x1 < x0) { cx_ = cy_ = 0; scale_ = 1; update(); return; }
    const double w = std::max(1e-6, x1 - x0), h = std::max(1e-6, y1 - y0);
    scale_ = std::min((width() - 60) / w, (height() - 60) / h);
    if (!(scale_ > 0)) scale_ = 1;
    cx_ = (x0 + x1) / 2;
    cy_ = (y0 + y1) / 2;
    fitted_ = true;
    update();
}

void MapCanvas::resizeEvent(QResizeEvent*) {
    if (!fitted_ && hasContent()) fit();
}

void MapCanvas::setCutMode(bool on) {
    if (cutMode_ == on) return;
    cutMode_ = on;
    cut_.clear();
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    emit cutModeChanged(on);
    update();
}

void MapCanvas::drawImage(QPainter& p, const MapImage& m, double opacity) {
    if (m.img.isNull()) return;
    const QPointF a = toScreen(m.x0, m.y0), b = toScreen(m.x1, m.y1);
    p.setOpacity(opacity);
    p.drawImage(QRectF(a, b), m.img);
    p.setOpacity(1);
}

void MapCanvas::paintEvent(QPaintEvent*) {
    const Theme& th = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), th.input);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const auto& m : pictures_) drawImage(p, m, 1.0);
    for (const auto& m : terrain_) drawImage(p, m, pictures_.isEmpty() ? 1.0 : 0.85);
    if (showMap_) drawImage(p, map_, 1.0);
    if (!hasContent()) {
        p.setPen(th.text3);
        p.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                   tr2("Belum ada yang bisa ditampilkan. Tambahkan terrain (GeoTIFF) atau gambar garis tengah reach di Data Geometri.",
                       "Nothing to show yet. Add a terrain (GeoTIFF) or draw a reach centreline in Geometric Data."));
        return;
    }
    if (showGeo_ && geo_) {
        QFont small = font();
        small.setPixelSize(11);
        p.setFont(small);
        for (const auto& r : geo_->reaches) {
            QPolygonF cl;
            for (const auto& pt : r.xy) cl << toScreen(pt.first, pt.second);
            p.setPen(QPen(th.primary, 2.4));
            p.drawPolyline(cl);
            if (cl.size() >= 2) {  // flow arrow at the downstream end
                const QPointF a = cl[cl.size() - 2], b = cl.back();
                const double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
                QPolygonF head;
                head << b << b - QPointF(12 * std::cos(ang - 0.4), 12 * std::sin(ang - 0.4)) << b - QPointF(12 * std::cos(ang + 0.4), 12 * std::sin(ang + 0.4));
                p.setBrush(th.primary);
                p.drawPolygon(head);
                p.setBrush(Qt::NoBrush);
                const QPointF mid = cl[cl.size() / 2];
                p.setPen(th.heading);
                p.drawText(mid + QPointF(8, -6), qs(r.label()));
            }
            for (const auto& c : mapping::cutLines(r)) {
                const CrossSection& xs = r.nodes[size_t(c.nodeIndex)];

                const bool sel = r.uid == selReach_ && xs.uid == selXs_;
                QPolygonF pl;
                for (const auto& pt : c.line) pl << toScreen(pt.x, pt.y);
                QPen pen(sel ? th.accent : QColor(250, 204, 80), sel ? 3 : 1.6);
                if (!c.fromFile) pen.setStyle(Qt::DashLine);
                p.setPen(pen);
                p.drawPolyline(pl);
                if (!pl.isEmpty() && (scale_ * 200 > 40 || sel)) {
                    p.setPen(sel ? th.accent : th.text2);
                    p.drawText(pl.front() + QPointF(4, -4), qs(xs.rs));
                }
            }
        }
    }
    if (showGeo_ && geo_) {
        for (const auto& a : geo_->storageAreas) {
            if (a.polygon.size() < 3) continue;
            QPolygonF poly;
            for (const auto& q : a.polygon) poly << toScreen(q.first, q.second);
            const QColor c = a.is2D ? th.accent2 : th.water;
            p.setPen(QPen(c, 1.8));
            p.setBrush(withAlpha(c, showMap_ && !map_.img.isNull() ? 0.0 : 0.12));
            p.drawPolygon(poly);
            p.setBrush(Qt::NoBrush);
            p.setPen(c);
            p.drawText(poly.boundingRect().center(), qs(a.name));
        }
        for (const auto& cn : geo_->connections) {
            QPolygonF pl;
            for (const auto& q : cn.line) pl << toScreen(q.first, q.second);
            p.setPen(QPen(th.warning, 2.2, Qt::DashLine));
            p.drawPolyline(pl);
        }
        for (const auto& b : geo_->bcLines) {
            QPolygonF pl;
            for (const auto& q : b.line) pl << toScreen(q.first, q.second);
            p.setPen(QPen(th.error, 3));
            p.drawPolyline(pl);
            if (!pl.isEmpty()) p.drawText(pl.back() + QPointF(5, 4), qs(b.name));
        }
    }
    if (cutMode_ && !cut_.empty()) {
        QPolygonF pl;
        for (const auto& pt : cut_) pl << toScreen(pt.x, pt.y);
        pl << mouse_;
        p.setPen(QPen(th.accent, 2, Qt::DashLine));
        p.drawPolyline(pl);
        p.setBrush(th.accent);
        for (int i = 0; i + 1 < pl.size(); ++i) p.drawEllipse(pl[i], 3.5, 3.5);
    }
    if (cutMode_) {
        p.setPen(th.text2);
        p.drawText(QRectF(12, height() - 28, width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   tr2("Klik titik-titik garis potong dari tebing kiri ke kanan (menghadap hilir). Klik ganda untuk selesai, Esc untuk batal.",
                       "Click the cut line points from the left to the right bank (looking downstream). Double-click to finish, Esc to cancel."));
    }
    // scale bar
    const double target = 120 / scale_;
    const double pow10 = std::pow(10.0, std::floor(std::log10(target)));
    double len = pow10;
    for (double f : {1.0, 2.0, 5.0, 10.0}) if (f * pow10 <= target) len = f * pow10;
    const double px = len * scale_;
    p.setPen(QPen(th.text, 2));
    const QPointF o(width() - px - 18, 22);
    p.drawLine(o, o + QPointF(px, 0));
    p.drawLine(o, o + QPointF(0, -5));
    p.drawLine(o + QPointF(px, 0), o + QPointF(px, -5));
    p.drawText(QRectF(o.x(), o.y() + 2, px, 16), Qt::AlignHCenter, QString::number(len) + QStringLiteral(" m"));
}

bool MapCanvas::pickXs(const QPointF& pos, int& reachUid, int& xsUid) const {
    if (!geo_) return false;
    double best = 7;
    bool found = false;
    for (const auto& r : geo_->reaches)
        for (const auto& c : mapping::cutLines(r)) {
            mapping::Line sl;
            for (const auto& pt : c.line) { QPointF q = toScreen(pt.x, pt.y); sl.push_back({q.x(), q.y()}); }
            const double d = mapping::distanceTo(sl, {pos.x(), pos.y()});
            if (d < best) { best = d; reachUid = r.uid; xsUid = r.nodes[size_t(c.nodeIndex)].uid; found = true; }
        }
    return found;
}

void MapCanvas::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() == Qt::RightButton && cutMode_) { setCutMode(false); return; }
    if (e->button() != Qt::LeftButton) return;
    if (cutMode_) {
        cut_.push_back(toMap(e->position()));
        update();
        return;
    }
    panning_ = true;
    moved_ = false;
    panFrom_ = e->position();
    setCursor(Qt::ClosedHandCursor);
}

void MapCanvas::mouseMoveEvent(QMouseEvent* e) {
    mouse_ = e->position();
    if (panning_) {
        const QPointF d = e->position() - panFrom_;
        if (std::abs(d.x()) + std::abs(d.y()) > 2) moved_ = true;
        cx_ -= d.x() / scale_;
        cy_ += d.y() / scale_;
        panFrom_ = e->position();
    }
    const auto m = toMap(e->position());
    emit hovered(m.x, m.y, true);
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* e) {
    if (!panning_) return;
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    if (!moved_) {
        int r = 0, x = 0;
        if (pickXs(e->position(), r, x)) emit xsClicked(r, x);
    }
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* e) {
    if (!cutMode_) return;
    // the double click already added the point once (press); drop duplicates
    while (cut_.size() >= 2 && std::hypot(cut_.back().x - cut_[cut_.size() - 2].x, cut_.back().y - cut_[cut_.size() - 2].y) * scale_ < 3)
        cut_.pop_back();
    if (cut_.size() >= 2) {
        const mapping::Line line = cut_;
        setCutMode(false);
        emit cutFinished(line);
    }
    (void)e;
}

void MapCanvas::wheelEvent(QWheelEvent* e) {
    const double f = std::pow(1.15, e->angleDelta().y() / 120.0);
    const auto before = toMap(e->position());
    scale_ = std::clamp(scale_ * f, 1e-6, 1e6);
    const auto after = toMap(e->position());
    cx_ += before.x - after.x;
    cy_ += before.y - after.y;
    update();
}

void MapCanvas::keyPressEvent(QKeyEvent* e) {
    if (cutMode_ && e->key() == Qt::Key_Escape) { setCutMode(false); return; }
    if (cutMode_ && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && cut_.size() >= 2) {
        const mapping::Line line = cut_;
        setCutMode(false);
        emit cutFinished(line);
        return;
    }
    if (e->key() == Qt::Key_Home) { fit(); return; }
    QWidget::keyPressEvent(e);
}

void MapCanvas::leaveEvent(QEvent*) { emit hovered(0, 0, false); }

// ============================ view ============================
MapperView::MapperView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- left: layers ----
    auto* left = new Card(tr2("Lapisan", "Layers"), "mapper.layers");
    left->setFixedWidth(318);
    auto* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* lw = new QWidget;
    auto* lv = new QVBoxLayout(lw);
    lv->setContentsMargins(0, 0, 4, 0);
    lv->setSpacing(6);
    auto caption = [&](const QString& t, const QString& topic) {
        auto* h = new QHBoxLayout;
        h->setContentsMargins(0, 6, 0, 0);
        h->addWidget(makeCaption(t));
        h->addWidget(new InfoButton(topic));
        h->addStretch();
        lv->addLayout(h);
    };
    auto buttonRow = [&](std::initializer_list<QPushButton*> bs) {
        auto* h = new QHBoxLayout;
        h->setSpacing(6);
        for (auto* b : bs) h->addWidget(b);
        h->addStretch();
        lv->addLayout(h);
    };

    caption(tr2("Proyeksi (CRS)", "Projection (CRS)"), "mapper.crs");
    crsLabel_ = makeMuted({});
    lv->addWidget(crsLabel_);
    crsWarn_ = makeMuted({});
    crsWarn_->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(Theme::current().warning)));
    lv->addWidget(crsWarn_);
    auto* crsBtn = makeButton(tr2("Atur proyeksi…", "Set projection…"), false, "options");
    buttonRow({crsBtn});
    connect(crsBtn, &QPushButton::clicked, this, &MapperView::projectionSheet);

    caption(tr2("Terrain", "Terrain"), "mapper.terrain");
    terrainList_ = new QListWidget;
    terrainList_->setMaximumHeight(72);
    lv->addWidget(terrainList_);
    auto* addT = makeButton(tr2("Tambah GeoTIFF…", "Add GeoTIFF…"), false, "plus");
    auto* remT = makeButton(tr2("Hapus", "Remove"), false, "trash");
    buttonRow({addT, remT});
    connect(addT, &QPushButton::clicked, this, &MapperView::addTerrain);
    connect(remT, &QPushButton::clicked, this, &MapperView::removeTerrain);

    caption(tr2("Gambar latar", "Background pictures"), "mapper.picture");
    pictureList_ = new QListWidget;
    pictureList_->setMaximumHeight(52);
    lv->addWidget(pictureList_);
    auto* addP = makeButton(tr2("Tambah gambar…", "Add picture…"), false, "plus");
    auto* remP = makeButton(tr2("Hapus", "Remove"), false, "trash");
    buttonRow({addP, remP});
    connect(addP, &QPushButton::clicked, this, &MapperView::addPicture);
    connect(remP, &QPushButton::clicked, this, &MapperView::removePicture);

    caption(tr2("Geometri", "Geometry"), "mapper.geometry");
    geoLabel_ = makeMuted({});
    lv->addWidget(geoLabel_);
    showGeo_ = new QCheckBox(tr2("Garis tengah dan garis potong", "Centrelines and cut lines"));
    showGeo_->setChecked(true);
    lv->addWidget(showGeo_);

    caption(tr2("Peta genangan", "Inundation map"), "mapper.inundation");
    planLabel_ = makeMuted({});
    lv->addWidget(planLabel_);
    profile_ = new QComboBox;
    lv->addWidget(profile_);
    computeBtn_ = makeButton(tr2("Hitung peta", "Compute map"), true, "play");
    buttonRow({computeBtn_});
    showMap_ = new QCheckBox(tr2("Tampilkan kedalaman", "Show depth"));
    showMap_->setChecked(true);
    lv->addWidget(showMap_);
    mapStats_ = makeMuted({});
    lv->addWidget(mapStats_);
    caption(tr2("Hasil area 2D", "2D area results"), "mapper.results2d");
    area2d_ = new QComboBox;
    var2d_ = new QComboBox;
    var2d_->addItem(tr2("Kedalaman maksimum", "Maximum depth"), 0);
    var2d_->addItem(tr2("Kedalaman pada waktu", "Depth at time"), 1);
    var2d_->addItem(tr2("Kecepatan maksimum", "Maximum velocity"), 2);
    var2d_->addItem(tr2("Waktu tiba (jam)", "Arrival time (hours)"), 3);
    lv->addWidget(area2d_);
    lv->addWidget(var2d_);
    time2d_ = new QSlider(Qt::Horizontal);
    time2d_->setAccessibleName(tr2("Waktu keluaran 2D", "2D output time"));
    lv->addWidget(time2d_);
    time2dLabel_ = makeMuted({});
    lv->addWidget(time2dLabel_);
    auto* show2d = makeButton(tr2("Tampilkan hasil 2D", "Show 2D results"), false, "play");
    buttonRow({show2d});
    connect(show2d, &QPushButton::clicked, this, &MapperView::show2D);
    connect(var2d_, &QComboBox::activated, this, [this]() { time2d_->setEnabled(var2d_->currentData().toInt() == 1); show2D(); });
    connect(area2d_, &QComboBox::activated, this, [this]() { fill2D(); show2D(); });
    connect(time2d_, &QSlider::valueChanged, this, [this]() { if (var2d_->currentData().toInt() == 1 && time2d_->isEnabled()) show2D(); });
    caption(tr2("Ekspor", "Export"), "mapper.export");
    exportDepth_ = makeButton(tr2("Kedalaman (GeoTIFF)…", "Depth (GeoTIFF)…"), false, "export");
    exportWse_ = makeButton(tr2("Muka air (GeoTIFF)…", "Water surface (GeoTIFF)…"), false, "export");
    auto* exportPng = makeButton(tr2("Gambar peta (PNG)…", "Map image (PNG)…"), false, "picture");
    lv->addWidget(exportDepth_);
    lv->addWidget(exportWse_);
    lv->addWidget(exportPng);
    lv->addStretch();
    sa->setWidget(lw);
    left->body()->addWidget(sa, 1);
    root->addWidget(left);
    connect(computeBtn_, &QPushButton::clicked, this, &MapperView::computeMap);
    connect(showMap_, &QCheckBox::toggled, this, [this](bool on) { canvas_->setShowMap(on); });
    connect(showGeo_, &QCheckBox::toggled, this, [this](bool on) { canvas_->setShowGeometry(on); });
    connect(exportDepth_, &QPushButton::clicked, this, [this]() { exportRaster(true); });
    connect(exportWse_, &QPushButton::clicked, this, [this]() { exportRaster(false); });
    connect(exportPng, &QPushButton::clicked, this, &MapperView::exportPng);

    // ---- centre: tools + map ----
    auto* centre = new Card;
    centre->setPadding(0);
    auto* tools = new QWidget;
    auto* tl = new QHBoxLayout(tools);
    tl->setContentsMargins(12, 10, 12, 10);
    tl->setSpacing(6);
    cutBtn_ = new QPushButton(tr2("Potong penampang", "Cut cross section"));
    cutBtn_->setObjectName("Chip");
    cutBtn_->setCheckable(true);
    cutBtn_->setIcon(lineIcon("draw", Theme::current().accent2));
    cutBtn_->setToolTip(tr2("Gambar garis potong di atas terrain; penampang baru diambil dari terrain", "Draw a cut line on the terrain; the new cross section is taken from the terrain"));
    refreshBtn_ = new QPushButton(tr2("Ambil ulang dari terrain", "Update from terrain"));
    refreshBtn_->setObjectName("Chip");
    refreshBtn_->setIcon(lineIcon("xs", Theme::current().accent2));
    refreshBtn_->setToolTip(tr2("Ganti titik stasiun-elevasi penampang terpilih dengan potongan terrain di garis potongnya", "Replace the station-elevation points of the selected section with the terrain along its cut line"));
    auto* fitBtn = new QPushButton(tr2("Pas layar", "Fit"));
    fitBtn->setObjectName("Chip");
    fitBtn->setToolTip(tr2("Tampilkan semua lapisan (Home)", "Show all layers (Home)"));
    for (auto* b : {cutBtn_, refreshBtn_, fitBtn}) b->setMinimumWidth(b->fontMetrics().horizontalAdvance(b->text()) + 52);
    tl->addWidget(cutBtn_);
    tl->addWidget(refreshBtn_);
    tl->addWidget(fitBtn);
    tl->addStretch();
    tl->addWidget(new InfoButton("mapper.cut"));
    centre->body()->addWidget(tools);
    canvas_ = new MapCanvas;
    centre->body()->addWidget(canvas_, 1);
    coord_ = makeMuted({}, false);
    coord_->setContentsMargins(12, 4, 12, 8);
    coord_->setMinimumHeight(24);
    centre->body()->addWidget(coord_);
    root->addWidget(centre, 1);

    connect(cutBtn_, &QPushButton::toggled, this, [this](bool on) {
        if (on && terrains_.empty()) {
            cutBtn_->setChecked(false);
            showToast(this, tr2("Tambahkan terrain dulu. Penampang baru diambil dari terrain.", "Add a terrain first. New cross sections are taken from the terrain."), "info");
            return;
        }
        canvas_->setCutMode(on);
    });
    connect(canvas_, &MapCanvas::cutModeChanged, this, [this](bool on) { if (cutBtn_->isChecked() != on) cutBtn_->setChecked(on); });
    connect(canvas_, &MapCanvas::cutFinished, this, &MapperView::onCut);
    connect(refreshBtn_, &QPushButton::clicked, this, &MapperView::refreshFromTerrain);
    connect(fitBtn, &QPushButton::clicked, canvas_, &MapCanvas::fit);
    connect(canvas_, &MapCanvas::xsClicked, this, [this](int r, int x) { s_->selectCrossSection(r, x); });
    connect(canvas_, &MapCanvas::hovered, this, [this](double x, double y, bool inside) {
        if (!inside) { coord_->clear(); return; }
        QString t = QStringLiteral("X %1   Y %2").arg(fmt(x, 2), fmt(y, 2));
        const double z = mapping::terrainAt(terrains_, x, y);
        if (!std::isnan(z)) t += QStringLiteral("   ·   ") + tr2("Elevasi terrain %1 m", "Terrain elevation %1 m").arg(fmt(z, 2));
        if (hasMap_ && map_.depth.contains(x, y)) {
            const int c = int((x - map_.depth.x0) / map_.depth.dx), r = int((map_.depth.y0 - y) / map_.depth.dy);
            if (c >= 0 && r >= 0 && c < map_.depth.nx && r < map_.depth.ny && !std::isnan(map_.depth.cell(c, r)))
                t += QStringLiteral("   ·   ") + tr2("Kedalaman %1 m · muka air %2 m", "Depth %1 m · water surface %2 m").arg(fmt(map_.depth.cell(c, r), 2), fmt(map_.wse.cell(c, r), 2));
        }
        coord_->setText(t);
    });
    connect(s_, &ProjectSession::crossSectionSelected, this, [this](int r, int x) { canvas_->setSelection(r, x); updateSelectionTools(); });
    connect(s_, &ProjectSession::modelEdited, this, [this]() {
        if (!isVisible()) return;
        auto* g = s_->currentGeometry();
        canvas_->setGeometry(g ? &g->model : nullptr);
        updateCrsInfo();
    });
    connect(s_, &ProjectSession::resultsUpdated, this, [this](const QString&) { clearMap(); fillProfiles(); fill2D(); });
}

QString MapperView::title() const { return QStringLiteral("RAS Mapper"); }

QWidget* MapperView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* save = makeButton(tr2("Simpan proyek", "Save project"), true, "save");
    h->addWidget(save);
    connect(save, &QPushButton::clicked, this, &View::requestSave);
    return w;
}

void MapperView::reload() {
    terrains_.clear();
    terrainNames_.clear();
    terrainSources_.clear();
    loadedKey_.clear();
    pictureKey_.clear();
    canvas_->setTerrainImages({});
    canvas_->setPictures({});
    clearMap();
    auto* g = s_->currentGeometry();
    canvas_->setGeometry(g ? &g->model : nullptr);
    if (isVisible()) activated();
}

void MapperView::activated() {
    auto* g = s_->currentGeometry();
    canvas_->setGeometry(g ? &g->model : nullptr);
    canvas_->setSelection(s_->selectedReach(), s_->selectedXs());
    loadLayers(false);
    fillLists();
    fillProfiles();
    fill2D();
    updateCrsInfo();
    updateSelectionTools();
    geoLabel_->setText(g ? QStringLiteral("%1 · %2").arg(s_->currentGeometryExt(), qs(g->model.title)) : tr2("Tidak ada geometri.", "No geometry."));
}

QString MapperView::absolutePath(const QString& stored) const {
    if (QDir::isAbsolutePath(stored) || !s_->project()) return stored;
    return QDir(qs(io::fromPath(s_->project()->dir()))).filePath(stored);
}

QString MapperView::relativeOrAbsolute(const QString& path) const {
    if (!s_->project()) return path;
    const QDir d(qs(io::fromPath(s_->project()->dir())));
    const QString rel = d.relativeFilePath(path);
    return rel.startsWith(QStringLiteral("..")) ? QDir::cleanPath(path) : rel;
}

void MapperView::loadLayers(bool force) {
    auto* p = s_->project();
    if (!p) return;
    // terrains: the project's list plus the source GeoTIFFs of HEC-RAS terrains in the .rasmap
    QStringList want, names;
    for (const auto& t : p->settings.terrains) { want << absolutePath(qs(t)); names << QFileInfo(qs(t)).fileName(); }
    QFile rm(qs(io::fromPath(p->pathFor("rasmap"))));
    if (rm.open(QIODevice::ReadOnly)) {
        const QString xml = QString::fromUtf8(rm.readAll());
        static const QRegularExpression layerRe(QStringLiteral("<Layer\\b[^>]*>"));
        static const QRegularExpression fileRe(QStringLiteral("Filename=\"([^\"]+)\""));
        for (auto it = layerRe.globalMatch(xml); it.hasNext();) {
            const QString tag = it.next().captured(0);
            if (!tag.contains(QStringLiteral("TerrainLayer"))) continue;
            auto fm = fileRe.match(tag);
            if (!fm.hasMatch()) continue;
            QString f = fm.captured(1);
            f.replace('\\', '/');
            const QFileInfo hdf(absolutePath(f));
            for (const QFileInfo& tif : hdf.dir().entryInfoList({hdf.completeBaseName() + QStringLiteral(".*.tif")}, QDir::Files)) {
                if (want.contains(tif.absoluteFilePath())) continue;
                want << tif.absoluteFilePath();
                names << tif.fileName() + QStringLiteral(" (RAS Mapper)");
            }
        }
    }
    if (force || want != loadedKey_) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        terrains_.clear();
        terrainNames_.clear();
        terrainSources_.clear();
        QStringList failed;
        for (int i = 0; i < want.size(); ++i) {
            io::Raster r;
            std::string err;
            if (io::readGeoTiff(io::toPath(ss(want[i])), r, err)) {
                terrains_.push_back(std::move(r));
                terrainNames_ << names[i];
                terrainSources_ << want[i];
            } else {
                failed << names[i] + QStringLiteral(": ") + qs(err);
            }
        }
        loadedKey_ = want;
        double lo, hi;
        elevationRange(terrains_, lo, hi);
        QList<MapImage> imgs;
        for (const auto& t : terrains_) imgs << imageOf(t, terrainImage(t, lo, hi));
        canvas_->setTerrainImages(imgs);
        QApplication::restoreOverrideCursor();
        if (!failed.isEmpty())
            showToast(this, tr2("Terrain tidak terbaca: %1", "Terrain could not be read: %1").arg(failed.join(QStringLiteral("; "))), "warning");
        clearMap();
    }
    // pictures
    QStringList pics;
    for (const auto& t : p->settings.pictures) pics << absolutePath(qs(t));
    if (force || pics != pictureKey_) {
        QList<MapImage> imgs;
        for (const QString& f : pics) {
            QImage img(f);
            if (img.isNull()) continue;
            const QFileInfo fi(f);
            const QString ext = fi.suffix().toLower();
            QStringList cands{fi.path() + "/" + fi.completeBaseName() + "." + ext.left(1) + ext.right(1) + "w",
                              f + "w", fi.path() + "/" + fi.completeBaseName() + ".wld"};
            QList<double> w;
            for (const QString& c : cands) {
                QFile wf(c);
                if (!wf.open(QIODevice::ReadOnly)) continue;
                for (const QString& l : QString::fromUtf8(wf.readAll()).split('\n', Qt::SkipEmptyParts)) w << l.trimmed().toDouble();
                break;
            }
            if (w.size() < 6 || w[0] <= 0 || w[1] != 0 || w[2] != 0) continue;
            MapImage m;
            m.img = img.convertToFormat(QImage::Format_ARGB32);
            m.x0 = w[4] - w[0] / 2;
            m.y0 = w[5] + std::fabs(w[3]) / 2;
            m.x1 = m.x0 + w[0] * img.width();
            m.y1 = m.y0 - std::fabs(w[3]) * img.height();
            imgs << m;
        }
        pictureKey_ = pics;
        canvas_->setPictures(imgs);
    }
    if (!canvas_->property("fittedOnce").toBool() && canvas_->hasContent()) {
        canvas_->fit();
        canvas_->setProperty("fittedOnce", true);
    }
}

void MapperView::fillLists() {
    terrainList_->clear();
    for (size_t i = 0; i < terrains_.size(); ++i) {
        const auto& t = terrains_[i];
        auto* it = new QListWidgetItem(terrainNames_[int(i)]);
        it->setToolTip(tr2("%1 × %2 sel, ukuran sel %3 · %4", "%1 × %2 cells, cell size %3 · %4")
                           .arg(t.nx).arg(t.ny).arg(fmt(t.dx, 2), t.epsg > 0 ? QStringLiteral("EPSG:%1").arg(t.epsg) : tr2("tanpa EPSG", "no EPSG")) +
                       QStringLiteral("\n") + QDir::toNativeSeparators(terrainSources_[int(i)]));
        terrainList_->addItem(it);
    }
    if (terrains_.empty()) terrainList_->addItem(tr2("(belum ada terrain)", "(no terrain yet)"));
    pictureList_->clear();
    if (auto* p = s_->project())
        for (const auto& f : p->settings.pictures) pictureList_->addItem(QFileInfo(qs(f)).fileName());
    if (pictureList_->count() == 0) pictureList_->addItem(tr2("(belum ada gambar)", "(no picture yet)"));
}

void MapperView::fillProfiles() {
    const QString cur = profile_->currentText();
    profile_->clear();
    const QString plan = s_->currentPlanExt();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    if (r && r->status == RunStatus::Succeeded) {
        for (const auto& pr : r->profiles) profile_->addItem(qs(pr.name));
        planLabel_->setText(tr2("Hasil plan %1", "Results of plan %1").arg(plan));
        const int i = profile_->findText(cur);
        profile_->setCurrentIndex(i >= 0 ? i : profile_->count() - 1);
    } else {
        planLabel_->setText(tr2("Plan %1 belum punya hasil. Jalankan dulu (F5).", "Plan %1 has no results yet. Run it first (F5).").arg(plan.isEmpty() ? QStringLiteral("-") : plan));
    }
    profile_->setEnabled(profile_->count() > 0);
    computeBtn_->setEnabled(profile_->count() > 0);
    exportDepth_->setEnabled(hasMap_);
    exportWse_->setEnabled(hasMap_);
}

void MapperView::fill2D() {
    const QString plan = s_->currentPlanExt();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    const QString keep = area2d_->currentText();
    area2d_->clear();
    if (r && r->status == RunStatus::Succeeded)
        for (const auto& g : r->grids) area2d_->addItem(qs(g.area));
    if (area2d_->count() == 0) area2d_->addItem(tr2("(belum ada hasil 2D di plan ini)", "(no 2D results in this plan)"));
    const int i = area2d_->findText(keep);
    if (i >= 0) area2d_->setCurrentIndex(i);
    const Grid2DResult* g = nullptr;
    if (r) for (const auto& x : r->grids) if (qs(x.area) == area2d_->currentText()) g = &x;
    area2d_->setEnabled(g != nullptr);
    var2d_->setEnabled(g != nullptr);
    time2d_->setRange(0, g && !g->hours.empty() ? int(g->hours.size()) - 1 : 0);
    time2d_->setEnabled(g && var2d_->currentData().toInt() == 1);
    if (g && !g->hours.empty()) time2d_->setValue(std::min(time2d_->value(), time2d_->maximum()));
    time2dLabel_->setText(g && !g->hours.empty() ? tr2("Jam %1 dari %2", "Hour %1 of %2").arg(fmt(g->hours[size_t(time2d_->value())], 2), fmt(g->hours.back(), 2)) : QString());
}

void MapperView::show2D() {
    const QString plan = s_->currentPlanExt();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    const Grid2DResult* g = nullptr;
    if (r) for (const auto& x : r->grids) if (qs(x.area) == area2d_->currentText()) g = &x;
    if (!g) { showToast(this, tr2("Jalankan plan unsteady yang punya area 2D dulu (F5).", "Run an unsteady plan with a 2D area first (F5)."), "info"); return; }
    const int var = var2d_->currentData().toInt();
    const int ti = time2d_->value();
    if (!g->hours.empty())
        time2dLabel_->setText(tr2("Jam %1 dari %2", "Hour %1 of %2").arg(fmt(g->hours[size_t(std::clamp(ti, 0, int(g->hours.size()) - 1))], 2), fmt(g->hours.back(), 2)));
    std::string err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto m = mapping::grid2dMap(*g, var, ti, terrains_, err);
    QApplication::restoreOverrideCursor();
    if (!err.empty()) { showToast(this, tr2("Peta 2D tidak bisa dibuat: %1", "The 2D map could not be made: %1").arg(qs(err)), "warning"); return; }
    map_ = std::move(m);
    hasMap_ = true;
    const QString what = var2d_->currentText();
    mapLabel_ = plan + QStringLiteral(" · ") + qs(g->area) + QStringLiteral(" · ") + (var == 1 && !g->hours.empty() ? QStringLiteral("t%1h").arg(fmt(g->hours[size_t(ti)], 2)) : what);
    canvas_->setMapImage(imageOf(map_.depth, depthImage(map_.depth, map_.maxDepth)), showMap_->isChecked());
    const bool usU = s_->project() && s_->project()->units() == UnitSystem::USCustomary;
    const QString unit = var == 2 ? (usU ? QStringLiteral("ft/s") : QStringLiteral("m/s")) : var == 3 ? tr2("jam", "h") : (usU ? QStringLiteral("ft") : QStringLiteral("m"));
    mapStats_->setText(tr2("%1 (%2): luas basah %3 ha, nilai maks. %4 %5. Pratinjau.", "%1 (%2): wet area %3 ha, max value %4 %5. Preview.")
                           .arg(qs(g->area), what, fmt(map_.wetArea / 10000.0, 2), fmt(map_.maxDepth, 2), unit));
    exportDepth_->setEnabled(true);
    exportWse_->setEnabled(var <= 1);
}

void MapperView::updateCrsInfo() {
    auto* p = s_->project();
    if (!p) return;
    QString crs = qs(p->settings.coordinateSystem).trimmed();
    QString shown = crs;
    static const QRegularExpression nameRe(QStringLiteral("^(?:PROJCS|GEOGCS|PROJCRS|GEOGCRS)\\[\"([^\"]+)\""));
    if (auto m = nameRe.match(crs); m.hasMatch()) shown = m.captured(1) + QStringLiteral(" (WKT)");
    if (crs.isEmpty()) {
        QFile rm(qs(io::fromPath(p->pathFor("rasmap"))));
        QString fromRasmap;
        if (rm.open(QIODevice::ReadOnly)) {
            static const QRegularExpression pr(QStringLiteral("<RASProjectionFilename\\s+Filename=\"([^\"]+)\""));
            if (auto m = pr.match(QString::fromUtf8(rm.readAll())); m.hasMatch()) fromRasmap = m.captured(1);
        }
        shown = fromRasmap.isEmpty() ? tr2("Belum diatur. Peta tetap bisa dipakai dengan koordinat apa adanya.", "Not set. The map still works with the coordinates as they are.")
                                     : tr2("Dari RAS Mapper: %1", "From RAS Mapper: %1").arg(fromRasmap);
    }
    crsLabel_->setText(shown);
    QString warn;
    static const QRegularExpression epsgRe(QStringLiteral("EPSG[: ]*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const auto em = epsgRe.match(crs);
    for (size_t i = 0; i < terrains_.size(); ++i)
        if (em.hasMatch() && terrains_[i].epsg > 0 && terrains_[i].epsg != em.captured(1).toInt()) {
            warn = tr2("Terrain %1 memakai EPSG:%2, berbeda dengan proyeksi proyek. De-RAS belum bisa memproyeksi ulang; ubah terrain di program GIS dulu.",
                       "Terrain %1 uses EPSG:%2, which differs from the project projection. De-RAS cannot reproject yet; convert the terrain in a GIS program first.")
                       .arg(terrainNames_[int(i)]).arg(terrains_[i].epsg);
            break;
        }
    crsWarn_->setText(warn);
    crsWarn_->setVisible(!warn.isEmpty());
}

void MapperView::projectionSheet() {
    auto* p = s_->project();
    if (!p) return;
    Sheet sh(this, tr2("Proyeksi proyek", "Project projection"), "mapper.crs", 620);
    sh.body()->addWidget(makeMuted(tr2("Isi kode EPSG (mis. EPSG:32748 untuk UTM 48S) atau teks WKT dari file .prj. Disimpan di file De-RAS proyek; file HEC-RAS tidak diubah.",
                                       "Enter an EPSG code (e.g. EPSG:32748 for UTM 48S) or the WKT text of a .prj file. Stored in the De-RAS project file; HEC-RAS files are not changed.")));
    auto* ed = new QLineEdit(qs(p->settings.coordinateSystem));
    ed->setPlaceholderText(QStringLiteral("EPSG:32748"));
    sh.body()->addWidget(ed);
    auto* row = new QHBoxLayout;
    auto* fromPrj = makeButton(tr2("Dari file .prj…", "From .prj file…"), false, "folder");
    auto* fromTer = makeButton(tr2("Dari terrain", "From terrain"), false, "mapper");
    fromTer->setEnabled(!terrains_.empty() && terrains_.front().epsg > 0);
    row->addWidget(fromPrj);
    row->addWidget(fromTer);
    row->addStretch();
    sh.body()->addLayout(row);
    connect(fromPrj, &QPushButton::clicked, &sh, [&]() {
        const QString f = QFileDialog::getOpenFileName(this, tr2("Pilih file proyeksi", "Choose projection file"), qs(io::fromPath(p->dir())), tr2("Proyeksi ESRI (*.prj)", "ESRI projection (*.prj)"));
        if (f.isEmpty()) return;
        QFile file(f);
        if (file.open(QIODevice::ReadOnly)) {
            const QString wkt = QString::fromUtf8(file.readAll()).trimmed();
            if (wkt.startsWith(QStringLiteral("PROJCS")) || wkt.startsWith(QStringLiteral("GEOGCS")) || wkt.contains(QStringLiteral("CRS[")))
                ed->setText(wkt.simplified());
            else
                sh.setError(tr2("File ini bukan proyeksi WKT. File .prj proyek HEC-RAS bukan file proyeksi.", "This file is not a WKT projection. A HEC-RAS project .prj file is not a projection file."));
        }
    });
    connect(fromTer, &QPushButton::clicked, &sh, [&]() { ed->setText(QStringLiteral("EPSG:%1").arg(terrains_.front().epsg)); });
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    if (sh.exec() != 1) return;
    p->settings.coordinateSystem = ss(ed->text().trimmed());
    s_->markEdited();
    updateCrsInfo();
}

void MapperView::addTerrain() {
    auto* p = s_->project();
    if (!p) return;
    const QString f = QFileDialog::getOpenFileName(this, tr2("Tambah terrain", "Add terrain"), qs(io::fromPath(p->dir())), QStringLiteral("GeoTIFF (*.tif *.tiff)"));
    if (f.isEmpty()) return;
    io::Raster r;
    std::string err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = io::readGeoTiff(io::toPath(ss(f)), r, err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        sheetInfo(this, tr2("Terrain tidak terbaca", "Terrain could not be read"),
                  tr2("%1\n\nDe-RAS membaca GeoTIFF satu band tanpa kompresi, LZW, atau Deflate. Simpan ulang terrain dalam format itu di program GIS.",
                      "%1\n\nDe-RAS reads single-band GeoTIFFs without compression, LZW or Deflate. Save the terrain again in that format in a GIS program.").arg(qs(err)));
        return;
    }
    // Copy into <project>/Terrain unless the file is already inside the project folder.
    QString target = f;
    const QDir pd(qs(io::fromPath(p->dir())));
    if (pd.relativeFilePath(f).startsWith(QStringLiteral(".."))) {
        pd.mkpath(QStringLiteral("Terrain"));
        const QFileInfo fi(f);
        target = pd.filePath(QStringLiteral("Terrain/") + fi.fileName());
        for (int n = 2; QFileInfo::exists(target); ++n) target = pd.filePath(QStringLiteral("Terrain/%1_%2.%3").arg(fi.completeBaseName()).arg(n).arg(fi.suffix()));
        if (!QFile::copy(f, target)) {
            showToast(this, tr2("Terrain tidak bisa disalin ke folder proyek.", "The terrain could not be copied to the project folder."), "warning");
            return;
        }
    }
    p->settings.terrains.push_back(ss(relativeOrAbsolute(target)));
    s_->markEdited();
    loadLayers(true);
    fillLists();
    updateCrsInfo();
    canvas_->fit();
    showToast(this, tr2("Terrain ditambahkan: %1 × %2 sel.", "Terrain added: %1 × %2 cells.").arg(r.nx).arg(r.ny));
}

void MapperView::removeTerrain() {
    auto* p = s_->project();
    const int row = terrainList_->currentRow();
    if (!p || row < 0 || row >= int(terrains_.size())) return;
    const QString rel = relativeOrAbsolute(terrainSources_[row]);
    auto it = std::find(p->settings.terrains.begin(), p->settings.terrains.end(), ss(rel));
    if (it == p->settings.terrains.end()) {
        showToast(this, tr2("Terrain ini dari RAS Mapper HEC-RAS. Hapus di RAS Mapper.", "This terrain comes from HEC-RAS RAS Mapper. Remove it there."), "info");
        return;
    }
    if (!sheetConfirm(this, tr2("Hapus terrain", "Remove terrain"), tr2("Lepas %1 dari proyek? File GeoTIFF-nya tidak dihapus.", "Detach %1 from the project? The GeoTIFF file is not deleted.").arg(terrainNames_[row]),
                      tr2("Lepas", "Detach"))) return;
    p->settings.terrains.erase(it);
    s_->markEdited();
    loadLayers(true);
    fillLists();
}

void MapperView::addPicture() {
    auto* p = s_->project();
    if (!p) return;
    const QString f = QFileDialog::getOpenFileName(this, tr2("Tambah gambar latar", "Add background picture"), qs(io::fromPath(p->dir())),
                                                   tr2("Gambar dengan world file (*.png *.jpg *.jpeg *.bmp)", "Pictures with a world file (*.png *.jpg *.jpeg *.bmp)"));
    if (f.isEmpty()) return;
    p->settings.pictures.push_back(ss(relativeOrAbsolute(f)));
    pictureKey_.clear();
    loadLayers(false);
    {
        // a picture without a usable world file is not drawn: tell the user and do not keep it
        const QFileInfo fi(f);
        const QString ext = fi.suffix().toLower();
        const bool hasWorld = QFileInfo::exists(fi.path() + "/" + fi.completeBaseName() + "." + ext.left(1) + ext.right(1) + "w") ||
                              QFileInfo::exists(f + "w") || QFileInfo::exists(fi.path() + "/" + fi.completeBaseName() + ".wld");
        if (!hasWorld) {
            p->settings.pictures.pop_back();
            pictureKey_.clear();
            loadLayers(false);
            sheetInfo(this, tr2("Gambar tanpa world file", "Picture without a world file"),
                      tr2("Gambar butuh world file di folder yang sama (mis. foto.pgw untuk foto.png, foto.jgw untuk foto.jpg) agar posisinya di peta diketahui. Gambar yang diputar belum didukung.",
                          "A picture needs a world file in the same folder (e.g. photo.pgw for photo.png, photo.jgw for photo.jpg) so its map position is known. Rotated pictures are not supported yet."));
            return;
        }
    }
    s_->markEdited();
    fillLists();
    canvas_->fit();
}

void MapperView::removePicture() {
    auto* p = s_->project();
    const int row = pictureList_->currentRow();
    if (!p || row < 0 || row >= int(p->settings.pictures.size())) return;
    p->settings.pictures.erase(p->settings.pictures.begin() + row);
    s_->markEdited();
    loadLayers(false);
    fillLists();
}

void MapperView::clearMap() {
    hasMap_ = false;
    map_ = {};
    mapLabel_.clear();
    canvas_->setMapImage({}, false);
    mapStats_->clear();
    exportDepth_->setEnabled(false);
    exportWse_->setEnabled(false);
}

void MapperView::computeMap() {
    if (!isVisible()) emit navigate(viewId());
    auto* g = s_->currentGeometry();
    const QString plan = s_->currentPlanExt();
    const RunResult* r = plan.isEmpty() ? nullptr : s_->results(plan);
    if (terrains_.empty()) { showToast(this, tr2("Tambahkan terrain dulu.", "Add a terrain first."), "info"); return; }
    if (!g || !r || r->status != RunStatus::Succeeded || profile_->currentIndex() < 0) {
        showToast(this, tr2("Jalankan plan dulu (F5), lalu pilih profil.", "Run the plan first (F5), then pick a profile."), "info");
        return;
    }
    const ProfileResult& pr = r->profiles[size_t(profile_->currentIndex())];
    std::string err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto m = mapping::inundation(g->model, pr, terrains_, err);
    QApplication::restoreOverrideCursor();
    if (!err.empty()) {
        sheetInfo(this, tr2("Peta genangan", "Inundation map"),
                  tr2("Peta tidak bisa dibuat: %1.", "The map could not be made: %1.").arg(qs(err)));
        return;
    }
    map_ = std::move(m);
    hasMap_ = true;
    mapLabel_ = plan + QStringLiteral(" · ") + qs(pr.name);
    MapImage mi = imageOf(map_.depth, depthImage(map_.depth, map_.maxDepth));
    canvas_->setMapImage(mi, showMap_->isChecked());
    const double ha = map_.wetArea / 10000.0;
    mapStats_->setText(tr2("%1: luas genangan %2 ha, kedalaman maks. %3 m, %4 sel. Pratinjau: tanpa cek keterhubungan aliran.",
                           "%1: flooded area %2 ha, max depth %3 m, %4 cells. Preview: no flow connectivity check.")
                           .arg(mapLabel_, fmt(ha, 2), fmt(map_.maxDepth, 2)).arg(map_.wetCells));
    exportDepth_->setEnabled(true);
    exportWse_->setEnabled(true);
    if (map_.wetCells == 0)
        showToast(this, tr2("Tidak ada sel tergenang. Periksa apakah garis potong penampang berada di atas terrain.",
                            "No flooded cells. Check that the cross-section cut lines lie on the terrain."), "warning");
}

void MapperView::exportRaster(bool depth) {
    auto* p = s_->project();
    if (!p || !hasMap_) return;
    QString name = mapLabel_;
    name.replace(QStringLiteral(" · "), QStringLiteral("_"));
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    const QString def = QDir(qs(io::fromPath(p->dir()))).filePath(qs(p->baseName()) + QStringLiteral("_") + name + (depth ? QStringLiteral("_kedalaman.tif") : QStringLiteral("_muka-air.tif")));
    const QString f = QFileDialog::getSaveFileName(this, depth ? tr2("Ekspor kedalaman", "Export depth") : tr2("Ekspor muka air", "Export water surface"), def, QStringLiteral("GeoTIFF (*.tif)"));
    if (f.isEmpty()) return;
    std::string err;
    if (!io::writeGeoTiff(io::toPath(ss(f)), depth ? map_.depth : map_.wse, err)) {
        showToast(this, tr2("Gagal menulis: %1", "Could not write: %1").arg(qs(err)), "warning");
        return;
    }
    showToast(this, tr2("Tersimpan: %1 (float32, no data -9999)", "Saved: %1 (float32, no data -9999)").arg(QFileInfo(f).fileName()));
}

void MapperView::exportPng() {
    auto* p = s_->project();
    if (!p) return;
    const QString f = QFileDialog::getSaveFileName(this, tr2("Ekspor gambar peta", "Export map image"),
                                                   QDir(qs(io::fromPath(p->dir()))).filePath(qs(p->baseName()) + QStringLiteral("_peta.png")), QStringLiteral("PNG (*.png)"));
    if (f.isEmpty()) return;
    if (canvas_->grab().save(f)) showToast(this, tr2("Tersimpan: %1", "Saved: %1").arg(QFileInfo(f).fileName()));
}

void MapperView::updateSelectionTools() {
    refreshBtn_->setEnabled(s_->selectedXs() != 0 && !terrains_.empty());
}

namespace {
double crossingStation(const CrossSection& xs, const mapping::CutLine& c, const mapping::Line& cl) {
    double sa = 0, sb = 0;
    if (c.fromFile && mapping::intersect(cl, c.line, sa, sb) && xs.staElev.size() >= 2) {
        const double L = mapping::length(c.line);
        return xs.staElev.front().station + (L > 0 ? sb / L : 0) * (xs.staElev.back().station - xs.staElev.front().station);
    }
    if (xs.bankLeft && xs.bankRight) return (*xs.bankLeft + *xs.bankRight) / 2;
    return xs.staElev.empty() ? 0 : (xs.staElev.front().station + xs.staElev.back().station) / 2;
}

std::vector<std::pair<double, double>> toPairs(const mapping::Line& l) {
    std::vector<std::pair<double, double>> v;
    for (const auto& p : l) v.emplace_back(std::round(p.x * 1000) / 1000, std::round(p.y * 1000) / 1000);
    return v;
}
}  // namespace

void MapperView::onCut(const mapping::Line& drawn) {
    auto* gf = s_->currentGeometry();
    if (!gf) return;
    Geometry& g = gf->model;
    // reach whose centreline the line crosses
    Reach* reach = nullptr;
    double sNew = 0, sAlong = 0;
    mapping::Line line = drawn;
    for (auto& r : g.reaches) {
        const mapping::Line cl = mapping::centreline(r);
        if (cl.size() >= 2 && mapping::intersect(cl, line, sNew, sAlong)) { reach = &r; break; }
    }
    if (!reach) {
        showToast(this, tr2("Garis potong harus memotong garis tengah reach.", "The cut line must cross a reach centreline."), "warning");
        return;
    }
    const mapping::Line cl = mapping::centreline(*reach);
    // orientation: left to right looking downstream
    mapping::Pt d;
    mapping::pointAt(cl, sNew, &d);
    const mapping::Pt v{line.back().x - line.front().x, line.back().y - line.front().y};
    if (v.x * d.y - v.y * d.x < 0) {  // v points to the left of the flow direction
        std::reverse(line.begin(), line.end());
        sAlong = mapping::length(line) - sAlong;
    }
    // neighbours along the reach
    const auto cuts = mapping::cutLines(*reach);
    const mapping::CutLine* up = nullptr;
    const mapping::CutLine* dn = nullptr;
    for (const auto& c : cuts) {
        if (c.s <= sNew) up = &c;
        else if (!dn) dn = &c;
    }
    const mapping::CutLine* nb = up ? up : dn;
    double total = 0;
    for (const auto& n : reach->nodes) total += n.lenChannel;
    bool allGenerated = true;
    for (const auto& c : cuts) allGenerated = allGenerated && !c.fromFile;
    const double clLen = mapping::length(cl);
    const double factor = allGenerated && clLen > 0 && total > 0 ? total / clLen : 1.0;
    // proposed river station
    QString rsText;
    {
        double rs = std::nan("");
        const double ru = up ? rsNumber(reach->nodes[size_t(up->nodeIndex)].rs) : std::nan("");
        const double rd = dn ? rsNumber(reach->nodes[size_t(dn->nodeIndex)].rs) : std::nan("");
        if (up && dn && !std::isnan(ru) && !std::isnan(rd) && dn->s > up->s) rs = ru + (rd - ru) * (sNew - up->s) / (dn->s - up->s);
        else if (up && !std::isnan(ru)) rs = ru - (sNew - up->s) * factor;
        else if (dn && !std::isnan(rd)) rs = rd + (dn->s - sNew) * factor;
        rsText = std::isnan(rs) ? QString() : qs(io::formatPlain(std::round(rs * 100) / 100));
    }
    const double cell = terrains_.empty() ? 1.0 : terrains_.front().dx;

    Sheet sh(this, tr2("Penampang baru dari terrain", "New cross section from terrain"), "mapper.cut", 560);
    sh.body()->addWidget(makeMuted(tr2("Reach %1. Titik stasiun-elevasi diambil dari terrain di sepanjang garis potong; tebing, n Manning, dan koefisien disalin dari penampang tetangga. Periksa hasilnya di Editor Penampang.",
                                       "Reach %1. Station-elevation points are taken from the terrain along the cut line; bank stations, Manning's n and coefficients are copied from the neighbouring section. Check the result in the Cross-Section Editor.")
                                       .arg(qs(reach->label()))));
    auto* form = new QHBoxLayout;
    form->addWidget(new QLabel(QStringLiteral("River station")));
    auto* rsEd = new QLineEdit(rsText);
    form->addWidget(rsEd, 1);
    form->addWidget(new QLabel(tr2("Jarak sampel (m)", "Sample spacing (m)")));
    auto* stepEd = new QLineEdit(fmt(cell, 2));
    form->addWidget(stepEd);
    sh.body()->addLayout(form);
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Buat penampang", "Create section"), 1, true);
    std::vector<StaElev> pts;
    sh.setAcceptCheck([&](int res) {
        if (res != 1) return true;
        const std::string rs = ss(rsEd->text().trimmed());
        if (rs.empty()) { sh.setError(tr2("Isi river station.", "Enter a river station.")); return false; }
        for (const auto& n : reach->nodes)
            if (n.rs == rs) { sh.setError(tr2("RS %1 sudah ada di reach ini.", "RS %1 already exists in this reach.").arg(qs(rs))); return false; }
        const double rv = rsNumber(rs);
        const double ru = up ? rsNumber(reach->nodes[size_t(up->nodeIndex)].rs) : std::nan("");
        const double rd = dn ? rsNumber(reach->nodes[size_t(dn->nodeIndex)].rs) : std::nan("");
        if (!std::isnan(rv) && ((!std::isnan(ru) && rv >= ru) || (!std::isnan(rd) && rv <= rd))) {
            sh.setError(tr2("RS harus di antara penampang tetangga (hulu lebih besar dari hilir).", "The RS must lie between the neighbouring sections (upstream larger than downstream)."));
            return false;
        }
        const auto step = parseUserNumber(stepEd->text());
        if (!step || *step <= 0) { sh.setError(tr2("Jarak sampel harus lebih dari 0.", "The sample spacing must be greater than 0.")); return false; }
        int missing = 0;
        pts = mapping::sampleLine(terrains_, line, *step, 500, &missing);
        if (pts.size() < 3 || missing > 0) {
            sh.setError(tr2("Sebagian garis potong ada di luar terrain atau di sel tanpa data.", "Part of the cut line lies outside the terrain or on no-data cells."));
            return false;
        }
        return true;
    });
    if (sh.exec() != 1) return;

    CrossSection xs;
    xs.uid = nextUid();
    xs.type = NodeType::CrossSection;
    xs.rs = ss(rsEd->text().trimmed());
    xs.staElev = pts;
    xs.cutLine = toPairs(line);
    xs.description = isID() ? "Dipotong dari terrain di RAS Mapper De-RAS" : "Cut from the terrain in De-RAS RAS Mapper";
    const double sFirst = pts.front().station, sLast = pts.back().station;
    double nL = 0.06, nC = 0.035, nR = 0.06;
    if (nb) {
        const CrossSection& n = reach->nodes[size_t(nb->nodeIndex)];
        xs.contraction = n.contraction;
        xs.expansion = n.expansion;
        xs.manningFormat = n.manningFormat;
        if (n.manning.size() >= 3) { nL = n.manning.front().n; nC = n.manning[1].n; nR = n.manning.back().n; }
        else if (!n.manning.empty()) nL = nC = nR = n.manning.front().n;
        const double crossN = crossingStation(n, *nb, cl);
        if (n.bankLeft && n.bankRight) {
            xs.bankLeft = std::clamp(sAlong + (*n.bankLeft - crossN), sFirst, sLast);
            xs.bankRight = std::clamp(sAlong + (*n.bankRight - crossN), *xs.bankLeft, sLast);
        }
    }
    if (!xs.bankLeft) { xs.bankLeft = sFirst + 0.4 * (sLast - sFirst); xs.bankRight = sFirst + 0.6 * (sLast - sFirst); }
    // snap banks to sampled points (HEC-RAS requires bank stations on points)
    auto snap = [&](double s) {
        double best = pts.front().station;
        for (const auto& p : pts) if (std::fabs(p.station - s) < std::fabs(best - s)) best = p.station;
        return best;
    };
    xs.bankLeft = snap(*xs.bankLeft);
    xs.bankRight = snap(*xs.bankRight);
    xs.manning = {{sFirst, nL}, {*xs.bankLeft, nC}, {*xs.bankRight, nR}};
    // reach lengths: split the upstream neighbour's lengths, or extend the reach
    size_t insertAt = dn ? size_t(dn->nodeIndex) : reach->nodes.size();
    if (up && dn) {
        CrossSection& u = reach->nodes[size_t(up->nodeIndex)];
        const double t = std::clamp((sNew - up->s) / (dn->s - up->s), 0.0, 1.0);
        auto r2 = [](double v) { return std::round(v * 100) / 100; };
        xs.lenLeft = r2(u.lenLeft * (1 - t));
        xs.lenChannel = r2(u.lenChannel * (1 - t));
        xs.lenRight = r2(u.lenRight * (1 - t));
        u.lenLeft = r2(u.lenLeft - xs.lenLeft);
        u.lenChannel = r2(u.lenChannel - xs.lenChannel);
        u.lenRight = r2(u.lenRight - xs.lenRight);
    } else if (dn) {
        const double L = std::round((dn->s - sNew) * factor * 100) / 100;
        xs.lenLeft = xs.lenChannel = xs.lenRight = L;
    } else if (up) {
        CrossSection& u = reach->nodes[size_t(up->nodeIndex)];
        const double L = std::round((sNew - up->s) * factor * 100) / 100;
        u.lenLeft = u.lenChannel = u.lenRight = L;
    }
    reach->nodes.insert(reach->nodes.begin() + long(insertAt), xs);
    const int ruid = reach->uid, xuid = xs.uid;
    s_->markEdited();
    s_->selectCrossSection(ruid, xuid);
    canvas_->update();
    showToast(this, tr2("Penampang RS %1 dibuat (%2 titik). Simpan geometri untuk menulisnya ke file.", "Cross section RS %1 created (%2 points). Save the geometry to write it to the file.")
                        .arg(qs(xs.rs)).arg(pts.size()));
}

void MapperView::refreshFromTerrain() {
    auto* gf = s_->currentGeometry();
    if (!gf || terrains_.empty()) return;
    for (auto& r : gf->model.reaches) {
        if (r.uid != s_->selectedReach()) continue;
        for (const auto& c : mapping::cutLines(r)) {
            CrossSection& xs = r.nodes[size_t(c.nodeIndex)];
            if (xs.uid != s_->selectedXs()) continue;
            int missing = 0;
            auto pts = mapping::sampleLine(terrains_, c.line, terrains_.front().dx, 500, &missing);
            if (pts.size() < 3 || missing > 0) {
                showToast(this, tr2("Garis potong RS %1 keluar dari terrain.", "The cut line of RS %1 leaves the terrain.").arg(qs(xs.rs)), "warning");
                return;
            }
            if (!sheetConfirm(this, tr2("Ambil ulang dari terrain", "Update from terrain"),
                              tr2("Ganti %1 titik stasiun-elevasi RS %2 dengan %3 titik dari terrain? Tebing dan n Manning digeser ke titik terdekat. Perubahan baru tertulis ke file setelah disimpan.",
                                  "Replace the %1 station-elevation points of RS %2 with %3 points from the terrain? Bank stations and Manning's n are moved to the nearest points. The change is written to the file only when saved.")
                                  .arg(xs.staElev.size()).arg(qs(xs.rs)).arg(pts.size()),
                              tr2("Ganti", "Replace"))) return;
            const double off = xs.staElev.front().station;
            for (auto& p : pts) p.station = std::round((p.station + off) * 100) / 100;
            auto snap = [&](double s) {
                double best = pts.front().station;
                for (const auto& p : pts) if (std::fabs(p.station - s) < std::fabs(best - s)) best = p.station;
                return best;
            };
            if (xs.bankLeft) xs.bankLeft = snap(*xs.bankLeft);
            if (xs.bankRight) xs.bankRight = snap(*xs.bankRight);
            if (!xs.manning.empty()) xs.manning.front().station = pts.front().station;
            for (auto& m : xs.manning) m.station = std::clamp(m.station, pts.front().station, pts.back().station);
            xs.staElev = pts;
            xs.cutLine = toPairs(c.line);
            s_->markEdited();
            canvas_->update();
            showToast(this, tr2("RS %1 diperbarui dari terrain.", "RS %1 updated from the terrain.").arg(qs(xs.rs)));
            return;
        }
    }
}

}  // namespace deras::ui

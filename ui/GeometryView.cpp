// De-RAS - ui/GeometryView.cpp
#include "GeometryView.h"
#include "StructureSheets.h"
#include "AreaSheets.h"
#include <QSignalBlocker>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "NumTable.h"
#include "Widgets.h"
#include "XsUtil.h"

namespace deras::ui {

// ============================ GeometryCanvas ============================
GeometryCanvas::GeometryCanvas(ProjectSession* s, QWidget* parent) : QWidget(parent), s_(s) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
    setAccessibleName(tr2("Kanvas skema geometri", "Geometry schematic canvas"));
    connect(s_, &ProjectSession::projectChanged, this, [this]() { rebuild(); update(); });
    connect(s_, &ProjectSession::modelEdited, this, [this]() { rebuild(); update(); });
    connect(s_, &ProjectSession::crossSectionSelected, this, [this]() { update(); });
    rebuild();
}

void GeometryCanvas::resetView() {
    zoom_ = 1;
    pan_ = {};
    update();
}

void GeometryCanvas::setDrawShape(int kind) {
    if (kind == 0) { if (drawing_ && drawKind_ != 0) setDrawMode(false); return; }
    if (drawing_) setDrawMode(false);
    drawKind_ = kind;
    drawing_ = true;
    draft_.clear();
    setCursor(Qt::CrossCursor);
    setFocus();
    emit drawModeChanged(true);
    update();
}

void GeometryCanvas::finishDraft() {
    const int kind = drawKind_;
    const size_t need = (kind == 1 || kind == 2) ? 3 : 2;
    if (size_t(draft_.size()) < need) return;
    const auto pts = draft_;
    setDrawMode(false);
    if (kind == 0) emit reachDrawn(pts);
    else emit shapeDrawn(kind, pts);
}

void GeometryCanvas::setDrawMode(bool on) {
    if (drawing_ == on && (!on || drawKind_ == 0)) return;
    if (on) drawKind_ = 0;
    drawing_ = on;
    draft_.clear();
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    if (on) setFocus();
    emit drawModeChanged(on);
    update();
}

void GeometryCanvas::rebuild() {
    lines_.clear();
    lineUids_.clear();
    lineNames_.clear();
    ticks_.clear();
    synthetic_ = false;
    bounds_ = {};
    auto* g = s_->currentGeometry();
    if (!g) return;
    double yOffset = 0;
    for (const Reach& r : g->model.reaches) {
        QVector<QPointF> pl;
        for (const auto& [x, y] : r.xy) pl << QPointF(x, y);
        // all nodes in file order; cross sections carry the reach lengths
        double total = 0;
        for (size_t i = 0; i + 1 < r.nodes.size(); ++i)
            if (r.nodes[i].type == NodeType::CrossSection) total += r.nodes[i].lenChannel;
        if (pl.size() < 2) {
            synthetic_ = true;
            const double L = total > 0 ? total : 1000.0;
            pl = {QPointF(0, yOffset), QPointF(L, yOffset)};
            yOffset -= std::max(L * 0.25, 100.0);
        }
        lines_ << pl;
        lineUids_ << r.uid;
        lineNames_ << qs(r.river) + QStringLiteral(" · ") + qs(r.reach);
        QVector<double> cum{0};
        for (int i = 1; i < pl.size(); ++i) cum << cum.back() + QLineF(pl[i - 1], pl[i]).length();
        const double plLen = cum.back();
        double dist = 0;
        const int n = static_cast<int>(r.nodes.size());
        int xsIndex = 0, xsCount = 0;
        for (const auto& nd : r.nodes) if (nd.type == NodeType::CrossSection) ++xsCount;
        for (int i = 0; i < n; ++i) {
            const CrossSection& nd = r.nodes[static_cast<size_t>(i)];
            const bool isXs = nd.type == NodeType::CrossSection;
            double u = total > 0 ? dist / total : (xsCount > 1 ? double(xsIndex) / double(xsCount - 1) : 0.0);
            if (!isXs && total > 0) {  // structures sit between their neighbouring sections
                double nextDist = dist;
                for (int k = i - 1; k >= 0; --k)
                    if (r.nodes[static_cast<size_t>(k)].type == NodeType::CrossSection) { nextDist = dist - r.nodes[static_cast<size_t>(k)].lenChannel * 0.5; break; }
                u = nextDist / total;
            }
            const double target = std::clamp(u, 0.0, 1.0) * plLen;
            int k = 1;
            while (k < pl.size() - 1 && cum[k] < target) ++k;
            const double segLen = cum[k] - cum[k - 1];
            const double t = segLen > 0 ? (target - cum[k - 1]) / segLen : 0;
            const QPointF a = pl[k - 1], b = pl[k];
            const QPointF p = a + t * (b - a);
            QPointF nn(-(b.y() - a.y()), b.x() - a.x());
            const double nl = std::hypot(nn.x(), nn.y());
            if (nl > 0) nn /= nl;
            QString kind;
            if (!isXs) {
                switch (nd.type) {
                    case NodeType::Bridge: kind = tr2("Jembatan", "Bridge"); break;
                    case NodeType::Culvert: kind = tr2("Gorong-gorong", "Culvert"); break;
                    case NodeType::InlineStructure: kind = tr2("Struktur inline", "Inline structure"); break;
                    case NodeType::LateralStructure: kind = tr2("Struktur lateral", "Lateral structure"); break;
                    case NodeType::MultipleOpening: kind = tr2("Bukaan ganda", "Multiple opening"); break;
                    default: kind = tr2("Node", "Node"); break;
                }
            }
            ticks_.push_back({r.uid, nd.uid, p, nn, isXs ? QStringLiteral("RS ") + qs(nd.rs) : kind + " " + qs(nd.rs), !isXs, kind});
            if (isXs) {
                dist += nd.lenChannel;
                ++xsIndex;
            }
        }
        for (const auto& q : pl) bounds_ = bounds_.isNull() ? QRectF(q, QSizeF(0.001, 0.001)) : bounds_.united(QRectF(q, QSizeF(0.001, 0.001)));
    }
    auto addPts = [this](const std::vector<XY>& v) {
        for (const auto& q : v) {
            const QRectF r(QPointF(q.first, q.second), QSizeF(0.001, 0.001));
            bounds_ = bounds_.isNull() ? r : bounds_.united(r);
        }
    };
    for (const auto& a : g->model.storageAreas) addPts(a.polygon);
    for (const auto& c : g->model.connections) addPts(c.line);
    for (const auto& b : g->model.bcLines) addPts(b.line);
}

QTransform GeometryCanvas::toScreen() const {
    const QRectF area = QRectF(rect()).adjusted(70, 60, -70, -60);
    QRectF b = bounds_;
    if (b.isNull()) b = QRectF(0, 0, 1000, 600);
    if (b.width() < 1e-6) b.adjust(-std::max(1.0, b.height() * 0.3), 0, std::max(1.0, b.height() * 0.3), 0);
    if (b.height() < 1e-6) b.adjust(0, -b.width() * 0.15, 0, b.width() * 0.15);
    const double sc = std::min(area.width() / b.width(), area.height() / b.height()) * zoom_;
    QTransform t;
    t.translate(area.center().x() + pan_.x(), area.center().y() + pan_.y());
    t.scale(sc, -sc);  // y up (map convention)
    t.translate(-b.center().x(), -b.center().y());
    return t;
}

void GeometryCanvas::paintEvent(QPaintEvent*) {
    const Theme& th = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // background and grid
    p.fillRect(rect(), th.dark ? QColor(7, 13, 24, 200) : QColor("#F3F8FE"));
    p.setPen(QPen(withAlpha(th.accent, th.dark ? 0.05 : 0.06), 1));
    for (int x = 0; x < width(); x += 40) p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 40) p.drawLine(0, y, width(), y);
    const QTransform t = toScreen();
    p.setFont(uiFont(th.basePx - 1));
    if (lines_.isEmpty() && !drawing_) {
        p.setPen(th.text2);
        const QString msg = s_->currentGeometry()
                                ? tr2("Belum ada reach. Pilih “Gambar reach”, lalu klik titik dari hulu ke hilir. Klik ganda untuk selesai.",
                                      "No reach yet. Choose “Draw reach”, then click from upstream to downstream; double-click to finish.")
                                : tr2("Belum ada file geometri. Buat file geometri baru dengan tombol + di kanan atas.",
                                      "No geometry file yet. Create a new geometry file with the + button at the top right.");
        p.drawText(rect().adjusted(40, 0, -40, 0), Qt::AlignCenter | Qt::TextWordWrap, msg);
    }
    // storage areas and 2D areas, connections and BC lines (M6)
    if (auto* g = s_->currentGeometry()) {
        for (const auto& a : g->model.storageAreas) {
            if (a.polygon.size() < 3) continue;
            QPolygonF poly;
            for (const auto& q : a.polygon) poly << t.map(QPointF(q.first, q.second));
            const QColor c = a.is2D ? th.accent2 : th.water;
            p.setPen(QPen(c, 1.8));
            p.setBrush(withAlpha(c, a.is2D ? 0.10 : 0.16));
            p.drawPolygon(poly);
            if (a.is2D && a.cellSize > 0) {
                // a hint of the computation grid
                const double px = a.cellSize * std::fabs(t.m11());
                if (px >= 6) {
                    p.save();
                    QPainterPath clip;
                    clip.addPolygon(poly);
                    p.setClipPath(clip);
                    p.setPen(QPen(withAlpha(c, 0.22), 1));
                    const QRectF br = poly.boundingRect();
                    for (double x = br.left(); x < br.right(); x += px) p.drawLine(QPointF(x, br.top()), QPointF(x, br.bottom()));
                    for (double y = br.top(); y < br.bottom(); y += px) p.drawLine(QPointF(br.left(), y), QPointF(br.right(), y));
                    p.restore();
                }
            }
            p.setPen(c);
            p.drawText(poly.boundingRect().center() - QPointF(20, 0), qs(a.name) + (a.is2D ? QStringLiteral(" (2D)") : QString()));
        }
        for (const auto& cn : g->model.connections) {
            if (cn.line.size() < 2) continue;
            QPainterPath path;
            path.moveTo(t.map(QPointF(cn.line[0].first, cn.line[0].second)));
            for (size_t k = 1; k < cn.line.size(); ++k) path.lineTo(t.map(QPointF(cn.line[k].first, cn.line[k].second)));
            p.setPen(QPen(th.warning, 2.4, Qt::DashLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setPen(th.warning);
            p.drawText(t.map(QPointF(cn.line[0].first, cn.line[0].second)) + QPointF(6, -6), qs(cn.name));
        }
        for (const auto& b : g->model.bcLines) {
            if (b.line.size() < 2) continue;
            QPainterPath path;
            path.moveTo(t.map(QPointF(b.line[0].first, b.line[0].second)));
            for (size_t k = 1; k < b.line.size(); ++k) path.lineTo(t.map(QPointF(b.line[k].first, b.line[k].second)));
            p.setPen(QPen(th.error, 3.2, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setPen(th.error);
            p.drawText(t.map(QPointF(b.line.back().first, b.line.back().second)) + QPointF(6, 4), qs(b.name));
        }
    }
    const int selR = s_->selectedReach(), selX = s_->selectedXs();
    for (int i = 0; i < lines_.size(); ++i) {
        QPainterPath path;
        path.moveTo(t.map(lines_[i][0]));
        for (int k = 1; k < lines_[i].size(); ++k) path.lineTo(t.map(lines_[i][k]));
        const bool sel = lineUids_[i] == selReach_;
        if (sel) {
            p.setPen(QPen(withAlpha(th.accent, 0.25), 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
        }
        p.setPen(QPen(th.water, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
        const QPointF a = t.map(lines_[i][lines_[i].size() - 2]), b = t.map(lines_[i].back());
        const double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
        QPolygonF arrow;
        arrow << b << b - QPointF(std::cos(ang - 0.45), std::sin(ang - 0.45)) * 12 << b - QPointF(std::cos(ang + 0.45), std::sin(ang + 0.45)) * 12;
        p.setPen(Qt::NoPen);
        p.setBrush(th.water);
        p.drawPolygon(arrow);
        p.setPen(th.text2);
        p.drawText(t.map(lines_[i][0]) + QPointF(-12, -16), lineNames_[i]);
    }
    // junctions: a ring where the downstream reach starts
    if (auto* g = s_->currentGeometry()) {
        for (const auto& j : g->model.junctions) {
            const Reach* dn = j.dn.empty() ? nullptr : g->model.findReach(j.dn[0].first, j.dn[0].second);
            if (!dn) continue;
            int li = -1;
            for (int i = 0; i < lineUids_.size(); ++i) if (lineUids_[i] == dn->uid) li = i;
            if (li < 0 || lines_[li].isEmpty()) continue;
            const QPointF c = t.map(lines_[li][0]);
            p.setPen(QPen(th.accent2, 2.2));
            p.setBrush(th.dark ? QColor(7, 13, 24) : QColor("#F3F8FE"));
            p.drawEllipse(c, 7, 7);
            p.setPen(th.accent2);
            p.drawText(c + QPointF(10, 14), qs(j.name));
        }
    }
    for (const Tick& k : ticks_) {
        const QPointF c = t.map(k.at);
        const QPointF dir = QPointF(k.normal.x(), -k.normal.y()) * 15;
        const bool sel = k.reachUid == selR && k.xsUid == selX;
        const bool near = hover_.x() >= 0 && QLineF(c, hover_).length() < 16;
        if (k.structure) {
            const QPointF off = QPointF(-dir.y(), dir.x()) / 15.0 * 4;
            p.setPen(QPen(th.warning, 2.4, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c - dir * 1.15 - off, c + dir * 1.15 - off);
            p.drawLine(c - dir * 1.15 + off, c + dir * 1.15 + off);
            p.setPen(th.warning);
            p.drawText(c + dir * 1.2 + QPointF(6, 4), k.label);
            continue;
        }
        p.setPen(QPen(sel ? (th.dark ? QColor("#FFFFFF") : th.heading) : withAlpha(th.accent2, 0.75), sel ? 3.0 : 1.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c - dir * (sel ? 1.3 : 1.0), c + dir * (sel ? 1.3 : 1.0));
        if (sel) {
            p.setPen(Qt::NoPen);
            p.setBrush(th.dark ? QColor("#FFFFFF") : th.heading);
            p.drawEllipse(c, 4, 4);
        }
        if (sel || near || ticks_.size() <= 24) {
            QFont f = uiFont(th.basePx - (sel ? 0 : 2), sel ? QFont::DemiBold : QFont::Normal);
            p.setFont(f);
            p.setPen(sel ? th.text : th.text2);
            // label beyond the end of the tick that points left (or up), away from the reach line
            const QPointF d = (dir.x() < -0.1 || (std::fabs(dir.x()) <= 0.1 && dir.y() < 0)) ? dir : -dir;
            const QPointF tip = c + d * (sel ? 1.3 : 1.0) * 1.1;
            const QFontMetricsF fm(f);
            const double tw = fm.horizontalAdvance(k.label);
            QPointF at = tip + QPointF(d.x() < 0 ? -tw - 4 : 4, fm.ascent() / 2.5);
            if (std::fabs(d.x()) < 0.3) at = tip + QPointF(-tw / 2, d.y() < 0 ? -4 : fm.ascent() + 2);
            p.drawText(at, k.label);
            p.setFont(uiFont(th.basePx - 1));
        }
    }
    // reach being drawn
    if (drawing_) {
        QPainterPath d;
        for (int i = 0; i < draft_.size(); ++i) {
            const QPointF q = t.map(draft_[i]);
            if (i == 0) d.moveTo(q); else d.lineTo(q);
        }
        if (!draft_.isEmpty() && hover_.x() >= 0) d.lineTo(hover_);
        QPen pen(th.accent2, 2.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(d);
        p.setBrush(th.accent2);
        p.setPen(Qt::NoPen);
        for (const auto& q : draft_) p.drawEllipse(t.map(q), 3.5, 3.5);
        QRectF tip(12, 12, width() - 24, 26);
        p.setPen(th.accent2);
        if (drawKind_ == 1 || drawKind_ == 2 || drawKind_ == 3 || drawKind_ == 4) {
            if ((drawKind_ == 1 || drawKind_ == 2) && draft_.size() >= 2) {
                p.setPen(QPen(th.accent2, 1.2, Qt::DotLine));
                p.drawLine(hover_.x() >= 0 ? hover_ : t.map(draft_.back()), t.map(draft_.front()));
            }
            const QString what = drawKind_ == 1 ? tr2("storage area (poligon)", "storage area (polygon)") : drawKind_ == 2 ? tr2("area 2D (poligon)", "2D area (polygon)")
                                 : drawKind_ == 3 ? tr2("garis koneksi", "connection line") : tr2("BC line di tepi area 2D", "BC line along the 2D area edge");
            p.setPen(th.accent2);
            p.drawText(tip, Qt::AlignLeft | Qt::AlignVCenter,
                       tr2("Gambar %1: klik titik-titiknya · klik ganda atau Enter untuk selesai · Esc untuk batal", "Draw %1: click its points · double-click or Enter to finish · Esc to cancel").arg(what));
        } else
        p.drawText(tip, Qt::AlignLeft | Qt::AlignVCenter,
                   tr2("Gambar reach: klik titik dari HULU ke HILIR · klik ganda atau Enter untuk selesai · Esc untuk batal",
                       "Draw reach: click points from UPSTREAM to DOWNSTREAM · double-click or Enter to finish · Esc to cancel"));
    }
    // footnote: what this canvas is
    p.setPen(th.text3);
    p.setFont(uiFont(th.basePx - 3));
    QString foot = tr2("Skema dari file geometri. Peta GIS ada di RAS Mapper (Ctrl+6). Roda mouse: zoom · seret: geser · klik ganda area kosong: tampilan awal",
                       "Schematic from the geometry file. The GIS map is in RAS Mapper (Ctrl+6). Wheel: zoom · drag: pan · double-click empty: reset");
    if (synthetic_) foot = tr2("Sebagian reach tidak punya koordinat, jadi digambar lurus sesuai jarak penampang. ", "Some reaches have no coordinates: drawn straight by section spacing. ") + foot;
    p.drawText(QRectF(12, height() - 24, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, foot);
}

const GeometryCanvas::Tick* GeometryCanvas::hitTick(const QPointF& pos) const {
    const QTransform t = toScreen();
    double best = 14;
    const Tick* hit = nullptr;
    for (const Tick& k : ticks_) {
        const double d = QLineF(t.map(k.at), pos).length();
        if (d < best) { best = d; hit = &k; }
    }
    return hit;
}

int GeometryCanvas::hitReach(const QPointF& pos) const {
    const QTransform t = toScreen();
    for (int i = 0; i < lines_.size(); ++i)
        for (int k = 1; k < lines_[i].size(); ++k) {
            const QPointF a = t.map(lines_[i][k - 1]), b = t.map(lines_[i][k]);
            const QPointF ab = b - a;
            const double L2 = QPointF::dotProduct(ab, ab);
            const double u = L2 > 0 ? std::clamp(QPointF::dotProduct(pos - a, ab) / L2, 0.0, 1.0) : 0.0;
            if (QLineF(a + u * ab, pos).length() < 7) return lineUids_[i];
        }
    return 0;
}

void GeometryCanvas::mousePressEvent(QMouseEvent* e) {
    setFocus();
    lastMouse_ = pressPos_ = e->position();
    if (drawing_ && e->button() == Qt::LeftButton) {
        bool ok = false;
        const QPointF w = toScreen().inverted(&ok).map(e->position());
        if (ok) draft_ << w;
        update();
        return;
    }
    panning_ = e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton;
}

void GeometryCanvas::mouseReleaseEvent(QMouseEvent* e) {
    const bool click = QLineF(pressPos_, e->position()).length() < 4;
    panning_ = false;
    if (drawing_ || !click || e->button() != Qt::LeftButton) return;
    if (const Tick* k = hitTick(e->position())) {
        if (!k->structure) {
            selReach_ = k->reachUid;
            s_->selectCrossSection(k->reachUid, k->xsUid);
            emit reachSelected(k->reachUid);
        }
    } else if (int r = hitReach(e->position())) {
        selReach_ = r;
        s_->selectCrossSection(r, 0);
        emit reachSelected(r);
    }
    update();
}

void GeometryCanvas::mouseMoveEvent(QMouseEvent* e) {
    if (panning_ && !drawing_) {
        pan_ += e->position() - lastMouse_;
        lastMouse_ = e->position();
    }
    hover_ = e->position();
    update();
}

void GeometryCanvas::mouseDoubleClickEvent(QMouseEvent* e) {
    if (drawing_) {
        // the double-click's second press already added a point; finish
        if (draft_.size() >= 2 && QLineF(toScreen().map(draft_.back()), toScreen().map(draft_[draft_.size() - 2])).length() < 4) draft_.removeLast();
        finishDraft();
        return;
    }
    if (const Tick* k = hitTick(e->position())) {
        if (!k->structure) emit crossSectionActivated(k->reachUid, k->xsUid);
        else emit structureActivated(k->reachUid, k->kind == tr2("Struktur inline", "Inline structure") ? 1 : k->kind == tr2("Struktur lateral", "Lateral structure") ? 2 : 0);
        return;
    }
    if (auto* g = s_->currentGeometry()) {
        bool ok = false;
        const QTransform inv = toScreen().inverted(&ok);
        const QTransform t = toScreen();
        auto nearLine = [&](const std::vector<XY>& l) {
            for (size_t k = 1; k < l.size(); ++k) {
                const QPointF a = t.map(QPointF(l[k - 1].first, l[k - 1].second)), b = t.map(QPointF(l[k].first, l[k].second));
                const QPointF ab = b - a;
                const double L2 = QPointF::dotProduct(ab, ab);
                const double u = L2 > 0 ? std::clamp(QPointF::dotProduct(e->position() - a, ab) / L2, 0.0, 1.0) : 0.0;
                if (QLineF(a + u * ab, e->position()).length() < 7) return true;
            }
            return false;
        };
        for (const auto& c : g->model.connections) if (nearLine(c.line)) { emit lineActivated(3, qs(c.name)); return; }
        for (const auto& b : g->model.bcLines) if (nearLine(b.line)) { emit lineActivated(4, qs(b.name)); return; }
        if (ok) {
            const QPointF w = inv.map(e->position());
            for (const auto& a : g->model.storageAreas) {
                if (a.polygon.size() < 3) continue;
                QPolygonF poly;
                for (const auto& q : a.polygon) poly << QPointF(q.first, q.second);
                if (poly.containsPoint(w, Qt::OddEvenFill)) { emit areaActivated(qs(a.name)); return; }
            }
        }
    }
    resetView();
}

void GeometryCanvas::wheelEvent(QWheelEvent* e) {
    const double f = e->angleDelta().y() > 0 ? 1.2 : 1 / 1.2;
    const double nz = std::clamp(zoom_ * f, 0.2, 80.0);
    // zoom around the cursor
    const QRectF area = QRectF(rect()).adjusted(70, 60, -70, -60);
    const QPointF c = area.center() + pan_;
    const QPointF m = e->position();
    pan_ = (pan_ + area.center() - m) * (nz / zoom_) + m - area.center();
    (void)c;
    zoom_ = nz;
    update();
}

void GeometryCanvas::keyPressEvent(QKeyEvent* e) {
    if (drawing_ && e->key() == Qt::Key_Escape) { setDrawMode(false); return; }
    if (drawing_ && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
        finishDraft();
        return;
    }
    if (drawing_ && e->key() == Qt::Key_Backspace && !draft_.isEmpty()) { draft_.removeLast(); update(); return; }
    QWidget::keyPressEvent(e);
}

// ============================ GeometryView ============================
GeometryView::GeometryView(ProjectSession* s, QWidget* parent) : View(s, parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 0, 16, 14);
    root->setSpacing(12);

    // ---- palette ----
    auto* pal = new Card;
    pal->setPadding(8);
    pal->setFixedWidth(118);
    auto* ph = new QHBoxLayout;
    ph->setContentsMargins(6, 4, 0, 2);
    ph->addWidget(makeCaption(tr2("Editor", "Editors")));
    ph->addWidget(new InfoButton("geometry.palette"));
    ph->addStretch();
    pal->body()->addLayout(ph);
    auto* ps = new QScrollArea;
    ps->setWidgetResizable(true);
    ps->setFrameShape(QFrame::NoFrame);
    ps->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* pw = new QWidget;
    auto* pv = new QVBoxLayout(pw);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(4);
    struct P { const char* id; const char* icon; const char* lid; const char* len; };
    const P items[] = {
        {"reach", "geometry", "Sungai /\nReach", "River /\nReach"},
        {"junction", "junction", "Junction", "Junction"},
        {"xs", "xs", "Penampang", "Cross\nSection"},
        {"bridge", "bridge", "Jembatan /\nGorong-gorong", "Bridge /\nCulvert"},
        {"inline", "inline", "Struktur\nInline", "Inline\nStructure"},
        {"lateral", "lateral", "Struktur\nLateral", "Lateral\nStructure"},
        {"storage", "storage", "Storage\nArea", "Storage\nArea"},
        {"area2d", "grid2d", "Area 2D", "2D Flow\nArea"},
        {"sa2d", "connect", "Koneksi\nSA/2D", "SA/2D\nConn."},
        {"pump", "pump", "Stasiun\nPompa", "Pump\nStation"},
        {"htab", "htab", "Parameter\nHTab", "HTab\nParam."},
        {"picture", "picture", "Gambar\nLatar", "Background\nPicture"},
    };
    for (const P& it : items) {
        const QString id = QString::fromLatin1(it.id);
        const bool locked = isLocked(id);
        const bool preview = id == "bridge" || id == "inline" || id == "junction" || id == "lateral" || id == "storage" || id == "area2d" || id == "sa2d" || id == "pump";
        QString tip = locked ? lockedTooltip(id) : tr2(it.lid, it.len).replace('\n', ' ');
        if (preview) tip += tr2(" (Pratinjau)", " (Preview)");
        auto* b = new ToolbarButton(id, it.icon, tr2(it.lid, it.len), tip,
                                    locked ? Availability::Locked : preview ? Availability::Preview : Availability::Available);
        b->setFixedWidth(98);
        b->setMinimumHeight(56);
        pv->addWidget(b);
        connect(b, &ToolbarButton::clicked, this, &GeometryView::palette);
    }
    pv->addStretch();
    ps->setWidget(pw);
    pal->body()->addWidget(ps, 1);
    root->addWidget(pal);

    // ---- centre: tools + canvas ----
    auto* centre = new Card;
    centre->setPadding(0);
    auto* tools = new QWidget;
    auto* tl = new QHBoxLayout(tools);
    tl->setContentsMargins(12, 10, 12, 10);
    tl->setSpacing(6);
    drawBtn_ = new QPushButton(tr2("Gambar reach", "Draw reach"));
    drawBtn_->setObjectName("Chip");
    drawBtn_->setCheckable(true);
    drawBtn_->setIcon(lineIcon("draw", Theme::current().accent2));
    drawBtn_->setToolTip(tr2("Gambar garis tengah reach baru dari hulu ke hilir", "Draw a new reach centreline from upstream to downstream"));
    drawBtn_->setMinimumWidth(drawBtn_->fontMetrics().horizontalAdvance(drawBtn_->text()) + 52);
    tl->addWidget(drawBtn_);
    struct T { const char* id; const char* lid; const char* len; };
    const T tls[] = {{"draw-storage", "Storage area", "Storage area"}, {"draw-2d", "Area 2D", "2D area"}, {"draw-connect", "Koneksi", "Connection"},
                     {"draw-bc", "Garis BC", "BC line"}};
    // Breaklines and n regions are not scheduled in M1–M8 (feature matrix); no chip, so the bar fits a 1366 px window.
    int shapeKind = 0;
    for (const T& it : tls) {
        auto* b = new QPushButton(tr2(it.lid, it.len));
        b->setObjectName("Chip");
        const QString id = QString::fromLatin1(it.id);
        b->setMinimumWidth(b->fontMetrics().horizontalAdvance(b->text()) + 48);
        tl->addWidget(b);
        if (isLocked(id)) {
            b->setIcon(lineIcon("lock", Theme::current().text3));
            b->setToolTip(lockedTooltip(id));
            connect(b, &QPushButton::clicked, this, [this, id]() { emit requestLocked(id); });
            b->setStyleSheet(QStringLiteral("color:%1;").arg(cssColor(Theme::current().text3)));
            continue;
        }
        const int kind = ++shapeKind;  // order of tls: storage 1, 2D 2, connection 3, BC line 4
        b->setCheckable(true);
        b->setIcon(lineIcon("draw", Theme::current().accent2));
        b->setToolTip(tr2("Klik titik-titik di kanvas; klik ganda untuk selesai", "Click points on the canvas; double-click to finish"));
        b->setProperty("shapeKind", kind);
        shapeBtns_ << b;
        connect(b, &QPushButton::toggled, this, [this, kind](bool on) {
            if (on) canvas_->setDrawShape(kind);
            else if (canvas_->drawShape() == kind) canvas_->setDrawShape(0);
        });
    }
    tl->addStretch();
    file_ = new QComboBox;
    file_->setObjectName("Mono");
    file_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    file_->setMinimumContentsLength(12);
    file_->setAccessibleName(tr2("File geometri", "Geometry file"));
    file_->setFont(monoFont());
    tl->addWidget(file_);
    auto* newFile = new IconButton("plus", tr2("File geometri baru…", "New geometry file…"));
    tl->addWidget(newFile);
    auto* cv = new QVBoxLayout;
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    cv->addWidget(tools);
    cv->addWidget(makeHLine());
    canvas_ = new GeometryCanvas(s_);
    cv->addWidget(canvas_, 1);
    centre->body()->addLayout(cv, 1);
    root->addWidget(centre, 1);

    // ---- right: selected object ----
    props_ = new Card(tr2("Objek terpilih", "Selected object"), "geometry.selected");
    props_->setFixedWidth(290);
    propsBody_ = new QVBoxLayout;
    propsBody_->setSpacing(8);
    props_->body()->addLayout(propsBody_);
    props_->body()->addStretch();
    props_->body()->addWidget(makeHint(tr2("Klik objek di kanvas untuk melihat datanya. Klik ganda penampang untuk membuka Editor Penampang.",
                                           "Click an object on the canvas to see its properties. Double-click a cross section to open its editor in the same window.")));
    root->addWidget(props_);

    connect(drawBtn_, &QPushButton::toggled, canvas_, [this](bool on) {
        if (on) canvas_->setDrawMode(true);
        else if (canvas_->drawMode() && canvas_->drawShape() == 0) canvas_->setDrawMode(false);
    });
    connect(canvas_, &GeometryCanvas::drawModeChanged, drawBtn_, [this](bool) {
        const int k = canvas_->drawShape();
        const bool reach = canvas_->drawMode() && k == 0;
        QSignalBlocker b0(drawBtn_);
        drawBtn_->setChecked(reach);
        for (auto* b : shapeBtns_) { QSignalBlocker bl(b); b->setChecked(b->property("shapeKind").toInt() == k); }
    });
    connect(canvas_, &GeometryCanvas::shapeDrawn, this, &GeometryView::onShapeDrawn);
    connect(canvas_, &GeometryCanvas::areaActivated, this, [this](const QString& n) { editAreaByName(this, s_, n); canvas_->update(); });
    connect(canvas_, &GeometryCanvas::lineActivated, this, [this](int k, const QString&) {
        if (k == 3) connectionsSheet(this, s_); else bcLinesSheet(this, s_);
        canvas_->update();
    });
    connect(canvas_, &GeometryCanvas::reachDrawn, this, &GeometryView::onReachDrawn);
    connect(canvas_, &GeometryCanvas::reachSelected, this, [this](int r) { selReach_ = r; showSelection(); });
    connect(canvas_, &GeometryCanvas::crossSectionActivated, this, [this](int r, int x) {
        s_->selectCrossSection(r, x);
        emit navigate("xs");
    });
    connect(canvas_, &GeometryCanvas::structureActivated, this, [this](int r, int kind) {
        selReach_ = r;
        palette(kind == 1 ? "inline" : kind == 2 ? "lateral" : "bridge");
    });
    connect(s_, &ProjectSession::crossSectionSelected, this, [this](int r) { selReach_ = r; showSelection(); });
    connect(s_, &ProjectSession::modelEdited, this, [this]() { if (isVisible()) showSelection(); });
    connect(file_, &QComboBox::activated, this, [this]() { s_->setEditGeometry(file_->currentData().toString()); });
    connect(newFile, &QToolButton::clicked, this, &GeometryView::newGeometryFile);
    reload();
}

QString GeometryView::title() const { return tr2("Data Geometri", "Geometric Data"); }

QWidget* GeometryView::createActions() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* tab = makeButton(tr2("Tabel…", "Tables…"), false, "table-grid");
    tab->setToolTip(tr2("Tabel panjang ruas dan koefisien semua penampang", "Reach lengths and coefficients of all cross sections"));
    auto* save = makeButton(tr2("Simpan geometri", "Save geometry"), true, "save");
    h->addWidget(tab);
    h->addWidget(save);
    connect(tab, &QPushButton::clicked, this, &GeometryView::lengthsTable);
    connect(save, &QPushButton::clicked, this, &View::requestSave);
    return w;
}

void GeometryView::activated() {
    fillFiles();
    showSelection();
    canvas_->update();
}

void GeometryView::reload() {
    fillFiles();
    selReach_ = s_->selectedReach();
    showSelection();
}

void GeometryView::fillFiles() {
    loading_ = true;
    file_->clear();
    if (auto* p = s_->project())
        for (auto& [e, g] : p->geometries) file_->addItem(qs(e) + QStringLiteral(" · ") + qs(g.model.title), qs(e));
    file_->setCurrentIndex(file_->findData(s_->currentGeometryExt()));
    file_->setEnabled(file_->count() > 0);
    loading_ = false;
}

void GeometryView::showSelection() {
    while (QLayoutItem* it = propsBody_->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        else if (QLayout* l = it->layout()) {
            while (QLayoutItem* c = l->takeAt(0)) { if (c->widget()) c->widget()->deleteLater(); delete c; }
        }
        delete it;
    }
    auto* g = s_->currentGeometry();
    const Reach* reach = nullptr;
    const CrossSection* xs = nullptr;
    if (g)
        for (const auto& r : g->model.reaches)
            if (r.uid == selReach_) {
                reach = &r;
                for (const auto& n : r.nodes) if (n.uid == s_->selectedXs()) xs = &n;
            }
    auto addRow = [this](const QString& k, const QString& v) {
        auto* h = new QHBoxLayout;
        auto* a = makeMuted(k);
        auto* b = new QLabel(v);
        b->setObjectName("Mono");
        b->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        b->setWordWrap(true);
        h->addWidget(a, 1);
        h->addWidget(b, 1);
        propsBody_->addLayout(h);
    };
    const QString lu = s_->project() ? qs(lengthUnit(s_->project()->units())) : QString();
    if (!reach) {
        auto* l = makeMuted(g ? tr2("Belum ada objek terpilih.", "Nothing selected.") : tr2("Tidak ada file geometri.", "No geometry file."));
        propsBody_->addWidget(l);
        if (g) {
            addRow(tr2("Reach", "Reaches"), QString::number(g->model.reaches.size()));
            int nx = 0, ns = 0;
            for (const auto& r : g->model.reaches)
                for (const auto& n : r.nodes) (n.type == NodeType::CrossSection ? nx : ns)++;
            addRow(tr2("Penampang", "Cross sections"), QString::number(nx));
            if (ns) addRow(tr2("Struktur (dipertahankan)", "Structures (kept)"), QString::number(ns));
            if (!g->model.junctions.empty()) addRow(QStringLiteral("Junction"), QString::number(g->model.junctions.size()));
        }
        return;
    }
    if (xs && xs->type == NodeType::CrossSection) {
        auto* t = new QLabel(tr2("Penampang RS %1", "Cross section RS %1").arg(qs(xs->rs)));
        t->setObjectName("CardTitle");
        propsBody_->addWidget(t);
        addRow(tr2("Sungai / Reach", "River / Reach"), qs(reach->river) + " / " + qs(reach->reach));
        addRow(tr2("Titik sta–elev", "Sta–elev points"), QString::number(xs->staElev.size()));
        addRow(tr2("Tebing kiri / kanan", "Left / right bank"),
               (xs->bankLeft ? QString::number(*xs->bankLeft, 'f', 1) : "—") + " / " + (xs->bankRight ? QString::number(*xs->bankRight, 'f', 1) : "—"));
        const ThreeN n = threeN(*xs);
        addRow(tr2("n LOB / Alur / ROB", "n LOB / Chan / ROB"),
               n.simple ? QStringLiteral("%1 / %2 / %3").arg(n.lob, 0, 'f', 3).arg(n.ch, 0, 'f', 3).arg(n.rob, 0, 'f', 3) : tr2("bervariasi (%1 titik)", "varies (%1 breaks)").arg(xs->manning.size()));
        addRow(tr2("Jarak ke hilir (alur)", "Downstream length (chan)"), QString::number(xs->lenChannel, 'f', 1) + " " + lu);
        addRow(tr2("Koef. kontraksi / ekspansi", "Contraction / expansion"), QStringLiteral("%1 / %2").arg(xs->contraction).arg(xs->expansion));
        if (!xs->ignoredFeatures.empty()) {
            QStringList f;
            for (const auto& s : xs->ignoredFeatures) f << qs(s);
            propsBody_->addWidget(makeHint(QStringLiteral("⚠ ") + tr2("Disimpan, belum dihitung: ", "Kept, not computed yet: ") + f.join(", ")));
        }
        auto* open = makeButton(tr2("Buka Editor Penampang", "Open Cross-Section Editor"), true);
        connect(open, &QPushButton::clicked, this, [this]() { emit navigate("xs"); });
        propsBody_->addWidget(open);
    } else if (xs) {
        auto* t = new QLabel(QStringLiteral("%1 RS %2").arg(qs(nodeTypeName(xs->type)), qs(xs->rs)));
        t->setObjectName("CardTitle");
        propsBody_->addWidget(t);
        propsBody_->addWidget(makeMuted(tr2("Jembatan, gorong-gorong, dan struktur inline diubah lewat palet (status Pratinjau). Struktur lain tetap tersimpan utuh di file, tetapi belum dihitung.",
                                            "Bridges, culverts and inline structures are edited from the palette (Preview status). Other structures are kept unchanged in the file but not computed yet.")));
    } else {
        auto* t = new QLabel(qs(reach->river) + " / " + qs(reach->reach));
        t->setObjectName("CardTitle");
        propsBody_->addWidget(t);
        int nx = 0;
        double len = 0;
        for (const auto& n : reach->nodes)
            if (n.type == NodeType::CrossSection) { ++nx; len += n.lenChannel; }
        addRow(tr2("Penampang", "Cross sections"), QString::number(nx));
        addRow(tr2("Panjang alur", "Channel length"), QString::number(len, 'f', 1) + " " + lu);
        addRow(tr2("Koordinat garis tengah", "Centreline points"), reach->xy.size() >= 2 ? QString::number(reach->xy.size()) : tr2("tidak ada", "none"));
        auto* ren = makeButton(tr2("Kelola reach…", "Manage reaches…"));
        connect(ren, &QPushButton::clicked, this, &GeometryView::reachSheet);
        propsBody_->addWidget(ren);
        if (nx) {
            auto* open = makeButton(tr2("Buka Editor Penampang", "Open Cross-Section Editor"), true);
            connect(open, &QPushButton::clicked, this, [this, reach]() {
                for (const auto& n : reach->nodes)
                    if (n.type == NodeType::CrossSection) { s_->selectCrossSection(reach->uid, n.uid); break; }
                emit navigate("xs");
            });
            propsBody_->addWidget(open);
        }
    }
}

void GeometryView::palette(const QString& id) {
    if (isLocked(id)) { emit requestLocked(id); return; }
    if (id == "xs") {
        if (!s_->currentGeometry()) { showToast(this, tr2("Belum ada file geometri.", "No geometry file yet."), "warning"); return; }
        emit navigate("xs");
    } else if (id == "reach") {
        reachSheet();
    } else if (id == "junction") {
        junctionSheet();
    } else if (id == "bridge" || id == "inline" || id == "lateral") {
        structuresSheet(this, s_, selReach_ ? selReach_ : s_->selectedReach(), id == "inline" ? 1 : id == "lateral" ? 2 : 0);
        canvas_->update();
        showSelection();
    } else if (id == "storage" || id == "area2d") {
        areasSheet(this, s_, id == "area2d");
        canvas_->update();
    } else if (id == "sa2d") {
        connectionsSheet(this, s_);
        canvas_->update();
    } else if (id == "pump") {
        pumpsSheet(this, s_);
    } else if (id == "htab") {
        sheetInfo(this, tr2("Parameter HTab", "HTab parameters"),
                  tr2("De-RAS membuat tabel sifat hidraulik setiap penampang sendiri (400 tingkat dari dasar sampai tiga kali tinggi penampang), jadi parameter HTab tidak perlu diisi. Baris HTab yang ada di file .g## (misalnya dari HEC-RAS) tetap disimpan apa adanya.",
                      "De-RAS builds its own hydraulic property table for every cross section (400 levels from the invert to three times the section height), so HTab parameters are not needed. HTab lines already in the .g## file (for example from HEC-RAS) are kept unchanged."));
    } else if (id == "picture") {
        showToast(this, tr2("Gambar latar ditambahkan di RAS Mapper.", "Background pictures are added in RAS Mapper."), "info");
        emit navigate("mapper");
    }
}

void GeometryView::onShapeDrawn(int kind, const QVector<QPointF>& pts) {
    std::vector<XY> v;
    for (const auto& q : pts) v.emplace_back(std::round(q.x() * 100) / 100, std::round(q.y() * 100) / 100);
    if (kind == 1 || kind == 2) addDrawnArea(this, s_, kind == 2, v);
    else if (kind == 3) addDrawnConnection(this, s_, v);
    else if (kind == 4) addDrawnBcLine(this, s_, v);
    canvas_->update();
}

void GeometryView::junctionSheet() {
    auto* g = s_->currentGeometry();
    if (!g) { showToast(this, tr2("Belum ada file geometri.", "No geometry file yet."), "warning"); return; }
    Sheet sh(this, QStringLiteral("Junction"), "geometry.junction", 620);
    auto* list = new QListWidget;
    list->setMinimumHeight(170);
    const QString lu = qs(lengthUnit(s_->project()->units()));
    auto fill = [&]() {
        list->clear();
        for (const auto& j : g->model.junctions) {
            QStringList up, dn;
            for (const auto& u : j.up) up << qs(u.second);
            for (const auto& d : j.dn) dn << qs(d.second);
            list->addItem(QStringLiteral("%1   ·   %2 → %3").arg(qs(j.name), up.join(" + "), dn.join(" + ")));
        }
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    sh.body()->addWidget(list);
    auto* row = new QHBoxLayout;
    auto* add = makeButton(tr2("Tambah…", "Add…"), false, "plus");
    auto* edit = makeButton(tr2("Ubah…", "Edit…"));
    auto* del = makeButton(tr2("Hapus…", "Delete…"), false, "trash");
    del->setObjectName("Danger");
    row->addWidget(add);
    row->addWidget(edit);
    row->addStretch();
    row->addWidget(del);
    sh.body()->addLayout(row);
    sh.body()->addWidget(makeMuted(tr2("Junction menyambung ujung hilir reach di hulu dengan ujung hulu reach di hilir. Muka air dihitung dengan persamaan energi "
                                       "melewati panjang junction (status Pratinjau). Satuan panjang: %1.",
                                       "A junction connects the downstream ends of upstream reaches with the upstream ends of downstream reaches. Water surfaces are "
                                       "carried across with the energy equation over the junction length (Preview status). Length unit: %1.").arg(lu)));
    connect(add, &QPushButton::clicked, &sh, [&]() {
        if (g->model.reaches.size() < 2) { showToast(this, tr2("Butuh minimal 2 reach.", "Needs at least 2 reaches."), "warning"); return; }
        Junction j;
        j.name = ss(QStringLiteral("Junction %1").arg(g->model.junctions.size() + 1));
        if (!editJunction(j, true)) return;
        g->model.junctions.push_back(j);
        g->model.junctionCount = static_cast<int>(g->model.junctions.size());
        s_->markEdited();
        fill();
    });
    connect(edit, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0) return;
        Junction j = g->model.junctions[static_cast<size_t>(r)];
        if (!editJunction(j, false)) return;
        g->model.junctions[static_cast<size_t>(r)] = j;
        s_->markEdited();
        fill();
    });
    connect(del, &QPushButton::clicked, &sh, [&]() {
        const int r = list->currentRow();
        if (r < 0) return;
        if (!sheetConfirm(this, tr2("Hapus junction", "Delete junction"),
                          tr2("Hapus junction %1? Reach-nya tetap ada.", "Delete junction %1? Its reaches stay.").arg(qs(g->model.junctions[static_cast<size_t>(r)].name)),
                          tr2("Hapus", "Delete"), true))
            return;
        g->model.junctions.erase(g->model.junctions.begin() + r);
        g->model.junctionCount = static_cast<int>(g->model.junctions.size());
        s_->markEdited();
        fill();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
    showSelection();
}

bool GeometryView::editJunction(Junction& j, bool isNew) {
    auto* g = s_->currentGeometry();
    const QString lu = qs(lengthUnit(s_->project()->units()));
    Sheet sh(this, isNew ? tr2("Junction baru", "New junction") : tr2("Ubah junction", "Edit junction"), "geometry.junction", 640);
    auto* name = new QLineEdit(qs(j.name));
    name->setMaxLength(16);
    auto* form = new QFormLayout;
    form->addRow(tr2("Nama (maks. 16 karakter)", "Name (max. 16 characters)"), name);
    sh.body()->addLayout(form);
    auto* t = new QTableWidget(static_cast<int>(g->model.reaches.size()), 4);
    t->setHorizontalHeaderLabels({tr2("Sungai / Reach", "River / Reach"), tr2("Masuk (dari hulu)", "In (from upstream)"),
                                  tr2("Keluar (ke hilir)", "Out (downstream)"), tr2("Panjang ke junction (%1)", "Length across junction (%1)").arg(lu)});
    t->verticalHeader()->hide();
    auto in = [](const std::vector<std::pair<std::string, std::string>>& v, const Reach& r) {
        for (size_t i = 0; i < v.size(); ++i) if (v[i].first == r.river && v[i].second == r.reach) return static_cast<int>(i);
        return -1;
    };
    for (int i = 0; i < t->rowCount(); ++i) {
        const Reach& r = g->model.reaches[static_cast<size_t>(i)];
        auto* a = new QTableWidgetItem(qs(r.label()));
        a->setFlags(a->flags() & ~Qt::ItemIsEditable);
        t->setItem(i, 0, a);
        for (int c : {1, 2}) {
            auto* cb = new QTableWidgetItem;
            cb->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            cb->setCheckState(in(c == 1 ? j.up : j.dn, r) >= 0 ? Qt::Checked : Qt::Unchecked);
            t->setItem(i, c, cb);
        }
        const bool multiUp = j.up.size() > 1;
        int k = in(multiUp ? j.up : j.dn, r);
        if (k < 0 && !multiUp) k = in(j.up, r) >= 0 ? 0 : -1;
        double L = k >= 0 && static_cast<size_t>(k) < j.lengths.size() ? j.lengths[static_cast<size_t>(k)] : 0;
        t->setItem(i, 3, new QTableWidgetItem(k >= 0 ? QString::number(L, 'g', 10) : QString()));
    }
    t->resizeColumnsToContents();
    t->horizontalHeader()->setStretchLastSection(true);
    t->setMinimumHeight(std::min(360, 60 + 32 * t->rowCount()));
    sh.body()->addWidget(t);
    sh.body()->addWidget(makeMuted(tr2("Centang reach yang masuk dan keluar. Panjang diisi untuk reach di sisi yang lebih dari satu "
                                       "(reach masuk untuk pertemuan, reach keluar untuk percabangan); untuk junction 1–1 isi di reach masuk.",
                                       "Tick the incoming and outgoing reaches. Enter lengths for the reaches on the side with more than one reach "
                                       "(incoming for a confluence, outgoing for a split); for a 1–1 junction enter it on the incoming reach.")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    Junction res;
    sh.setAcceptCheck([&](int) {
        res = j;
        res.name = ss(name->text().trimmed());
        res.up.clear();
        res.dn.clear();
        res.lengths.clear();
        if (res.name.empty()) { sh.setError(tr2("Isi nama junction.", "Enter a junction name.")); return false; }
        for (const auto& o : g->model.junctions)
            if (o.name == res.name && (isNew || o.name != j.name)) { sh.setError(tr2("Nama junction sudah dipakai.", "The junction name is already used.")); return false; }
        std::vector<int> upRows, dnRows;
        for (int i = 0; i < t->rowCount(); ++i) {
            const bool u = t->item(i, 1)->checkState() == Qt::Checked, d = t->item(i, 2)->checkState() == Qt::Checked;
            if (u && d) { sh.setError(tr2("Satu reach tidak bisa masuk dan keluar sekaligus.", "A reach cannot be both incoming and outgoing.")); return false; }
            const Reach& r = g->model.reaches[static_cast<size_t>(i)];
            if (u) { res.up.push_back({r.river, r.reach}); upRows.push_back(i); }
            if (d) { res.dn.push_back({r.river, r.reach}); dnRows.push_back(i); }
        }
        if (upRows.empty() || dnRows.empty()) { sh.setError(tr2("Pilih minimal satu reach masuk dan satu reach keluar.", "Pick at least one incoming and one outgoing reach.")); return false; }
        if (upRows.size() > 1 && dnRows.size() > 1) { sh.setError(tr2("Junction hanya boleh punya banyak reach di satu sisi.", "A junction may have several reaches on one side only.")); return false; }
        const auto& rows = upRows.size() > 1 ? upRows : (dnRows.size() > 1 ? dnRows : upRows);
        for (int i : rows) {
            auto v = parseUserNumber(t->item(i, 3) ? t->item(i, 3)->text() : QString());
            if (!v || *v < 0) { sh.setError(tr2("Isi panjang 0 atau lebih untuk %1.", "Enter a length of 0 or more for %1.").arg(t->item(i, 0)->text())); return false; }
            res.lengths.push_back(*v);
        }
        return true;
    });
    if (sh.exec() != 1) return false;
    j = res;
    return true;
}

void GeometryView::onReachDrawn(const QVector<QPointF>& pts) {
    auto* g = s_->currentGeometry();
    if (!g) return;
    Sheet sh(this, tr2("Reach baru", "New reach"), "geometry.reach", 460);
    auto* form = new QFormLayout;
    auto* river = new QLineEdit(g->model.reaches.empty() ? tr2("Sungai", "River") : qs(g->model.reaches.front().river));
    auto* reach = new QLineEdit(QStringLiteral("Reach %1").arg(g->model.reaches.size() + 1));
    river->setMaxLength(16);
    reach->setMaxLength(16);
    form->addRow(tr2("Nama sungai", "River name"), river);
    form->addRow(tr2("Nama reach", "Reach name"), reach);
    sh.body()->addLayout(form);
    sh.body()->addWidget(makeMuted(tr2("Paling banyak 16 karakter (batas format HEC-RAS). Garis tengah disimpan sebagai koordinat reach (Reach XY) dengan %1 titik. "
                                       "Setelah itu tambahkan penampang di Editor Penampang.",
                                       "Maximum 16 characters (HEC-RAS format limit). The centreline is stored as reach coordinates (Reach XY) with %1 points. "
                                       "Then add cross sections in the Cross-Section Editor.").arg(pts.size())));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Buat reach", "Create reach"), 1, true);
    sh.setAcceptCheck([&](int) {
        if (river->text().trimmed().isEmpty() || reach->text().trimmed().isEmpty()) { sh.setError(tr2("Isi nama sungai dan reach.", "Enter the river and reach names.")); return false; }
        if (g->model.findReach(ss(river->text().trimmed()), ss(reach->text().trimmed()))) { sh.setError(tr2("Reach dengan nama itu sudah ada.", "A reach with that name already exists.")); return false; }
        return true;
    });
    if (sh.exec() != 1) return;
    Reach r;
    r.uid = nextUid();
    r.river = ss(river->text().trimmed());
    r.reach = ss(reach->text().trimmed());
    for (const auto& q : pts) r.xy.emplace_back(q.x(), q.y());
    g->model.reaches.push_back(r);
    selReach_ = r.uid;
    s_->selectCrossSection(r.uid, 0);
    s_->markEdited();
    showToast(this, tr2("Reach dibuat: %1 / %2", "Reach created: %1 / %2").arg(qs(r.river), qs(r.reach)));
}

void GeometryView::reachSheet() {
    auto* g = s_->currentGeometry();
    if (!g) { showToast(this, tr2("Belum ada file geometri.", "No geometry file yet."), "warning"); return; }
    Sheet sh(this, tr2("Sungai / Reach", "River / Reach"), "geometry.reach", 560);
    auto* list = new QListWidget;
    list->setMinimumHeight(180);
    auto fill = [&]() {
        list->clear();
        for (const auto& r : g->model.reaches) {
            int nx = 0;
            for (const auto& n : r.nodes) if (n.type == NodeType::CrossSection) ++nx;
            auto* it = new QListWidgetItem(QStringLiteral("%1 / %2   ·   %3 %4").arg(qs(r.river), qs(r.reach)).arg(nx).arg(tr2("penampang", "cross sections")));
            it->setData(Qt::UserRole, r.uid);
            list->addItem(it);
        }
        if (list->count()) list->setCurrentRow(0);
    };
    fill();
    sh.body()->addWidget(list);
    auto* row = new QHBoxLayout;
    auto* add = makeButton(tr2("Tambah (tanpa gambar)…", "Add (without drawing)…"), false, "plus");
    auto* ren = makeButton(tr2("Ganti nama…", "Rename…"));
    auto* del = makeButton(tr2("Hapus…", "Delete…"), false, "trash");
    del->setObjectName("Danger");
    row->addWidget(add);
    row->addWidget(ren);
    row->addStretch();
    row->addWidget(del);
    sh.body()->addLayout(row);
    sh.body()->addWidget(makeMuted(tr2("Untuk menggambar garis tengah reach, tutup lembar ini lalu pilih “Gambar reach” di atas kanvas.",
                                       "To draw a reach centreline, close this sheet and choose “Draw reach” above the canvas.")));
    auto current = [&]() -> Reach* {
        auto* it = list->currentItem();
        if (!it) return nullptr;
        for (auto& r : g->model.reaches) if (r.uid == it->data(Qt::UserRole).toInt()) return &r;
        return nullptr;
    };
    connect(add, &QPushButton::clicked, &sh, [&]() {
        QString river = g->model.reaches.empty() ? tr2("Sungai", "River") : qs(g->model.reaches.front().river);
        if (!sheetGetText(this, tr2("Reach baru", "New reach"), tr2("Nama sungai (maks. 16 karakter):", "River name (max. 16 characters):"), river, 16)) return;
        QString rname = QStringLiteral("Reach %1").arg(g->model.reaches.size() + 1);
        if (!sheetGetText(this, tr2("Reach baru", "New reach"), tr2("Nama reach (maks. 16 karakter):", "Reach name (max. 16 characters):"), rname, 16)) return;
        if (g->model.findReach(ss(river), ss(rname))) { showToast(this, tr2("Reach dengan nama itu sudah ada.", "A reach with that name already exists."), "warning"); return; }
        Reach r;
        r.uid = nextUid();
        r.river = ss(river);
        r.reach = ss(rname);
        g->model.reaches.push_back(r);
        s_->markEdited();
        fill();
    });
    connect(ren, &QPushButton::clicked, &sh, [&]() {
        Reach* r = current();
        if (!r) return;
        QString rname = qs(r->reach);
        if (!sheetGetText(this, tr2("Ganti nama reach", "Rename reach"), tr2("Nama reach (maks. 16 karakter):", "Reach name (max. 16 characters):"), rname, 16)) return;
        const std::string old = r->reach;
        r->reach = ss(rname);
        // keep steady-flow references consistent
        if (auto* p = s_->project())
            for (auto& [e, f] : p->steadyFlows) {
                for (auto& c : f.model.changes) if (c.river == r->river && c.reach == old) c.reach = r->reach;
                for (auto& b : f.model.boundaries) if (b.river == r->river && b.reach == old) b.reach = r->reach;
            }
        for (auto& j : g->model.junctions)
            for (auto* side : {&j.up, &j.dn})
                for (auto& e : *side) if (e.first == r->river && e.second == old) e.second = r->reach;
        s_->markEdited();
        fill();
    });
    connect(del, &QPushButton::clicked, &sh, [&]() {
        Reach* r = current();
        if (!r) return;
        if (!sheetConfirm(this, tr2("Hapus reach", "Delete reach"),
                          tr2("Hapus reach %1 / %2 beserta semua penampangnya? File baru berubah setelah Anda menyimpan.",
                              "Delete reach %1 / %2 with all its cross sections? The change is written to the file only when you save.").arg(qs(r->river), qs(r->reach)),
                          tr2("Hapus", "Delete"), true))
            return;
        const int uid = r->uid;
        for (auto& j : g->model.junctions)
            for (auto* side : {&j.up, &j.dn})
                side->erase(std::remove_if(side->begin(), side->end(), [&](const auto& e) { return e.first == r->river && e.second == r->reach; }), side->end());
        g->model.junctions.erase(std::remove_if(g->model.junctions.begin(), g->model.junctions.end(),
                                                [](const Junction& j) { return j.up.empty() || j.dn.empty(); }), g->model.junctions.end());
        g->model.junctionCount = static_cast<int>(g->model.junctions.size());
        g->model.reaches.erase(std::remove_if(g->model.reaches.begin(), g->model.reaches.end(), [uid](const Reach& x) { return x.uid == uid; }), g->model.reaches.end());
        s_->markEdited();
        fill();
    });
    sh.addButton(tr2("Tutup", "Close"), 1, true);
    sh.exec();
    showSelection();
}

void GeometryView::lengthsTable() {
    auto* g = s_->currentGeometry();
    if (!g) return;
    Sheet sh(this, tr2("Tabel panjang ruas dan koefisien", "Reach lengths and coefficients"), "geometry.lengths", 880);
    const QString lu = qs(lengthUnit(s_->project()->units()));
    auto* tw = new QTableWidget;
    QStringList heads{tr2("Sungai / Reach", "River / Reach"), "RS", tr2("Jarak LOB (%1)", "LOB length (%1)").arg(lu), tr2("Jarak alur (%1)", "Channel length (%1)").arg(lu),
                      tr2("Jarak ROB (%1)", "ROB length (%1)").arg(lu), tr2("Kontraksi", "Contraction"), tr2("Ekspansi", "Expansion")};
    tw->setColumnCount(heads.size());
    tw->setHorizontalHeaderLabels(heads);
    tw->verticalHeader()->hide();
    tw->setAlternatingRowColors(true);
    struct Ref { int reach, xs; };
    QVector<Ref> refs;
    for (const auto& r : g->model.reaches)
        for (const auto& n : r.nodes) {
            if (n.type != NodeType::CrossSection) continue;
            const int row = tw->rowCount();
            tw->insertRow(row);
            auto* a = new QTableWidgetItem(qs(r.river) + " / " + qs(r.reach));
            a->setFlags(a->flags() & ~Qt::ItemIsEditable);
            auto* b = new QTableWidgetItem(qs(n.rs));
            b->setFlags(b->flags() & ~Qt::ItemIsEditable);
            tw->setItem(row, 0, a);
            tw->setItem(row, 1, b);
            const double vals[] = {n.lenLeft, n.lenChannel, n.lenRight, n.contraction, n.expansion};
            for (int c = 0; c < 5; ++c) {
                auto* it = new QTableWidgetItem(QString::number(vals[c], 'g', 10));
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                it->setFont(monoFont());
                tw->setItem(row, 2 + c, it);
            }
            refs.push_back({r.uid, n.uid});
        }
    tw->resizeColumnsToContents();
    tw->horizontalHeader()->setStretchLastSection(true);
    tw->setMinimumHeight(std::min(460, 40 + 30 * tw->rowCount()));
    sh.body()->addWidget(tw);
    sh.body()->addWidget(makeMuted(tr2("Ubah nilai langsung di tabel. Desimal boleh pakai koma atau titik. Nilai yang tidak valid ditolak, bukan diganti nol.",
                                       "Edit values directly in the table. Numbers may use a decimal comma or point. Invalid values are rejected (never turned into zero).")));
    sh.addButton(tr2("Batal", "Cancel"), 0);
    sh.addButton(tr2("Terapkan", "Apply"), 1, true);
    sh.setAcceptCheck([&](int) {
        int bad = 0;
        for (int r = 0; r < tw->rowCount(); ++r)
            for (int c = 2; c < 7; ++c) {
                auto v = parseUserNumber(tw->item(r, c)->text());
                const bool ok = v && *v >= 0 && (c < 5 || *v <= 1.0);
                tw->item(r, c)->setBackground(ok ? QBrush() : QBrush(withAlpha(Theme::current().error, 0.25)));
                if (!ok) ++bad;
            }
        if (bad) { sh.setError(tr2("%1 sel tidak valid (ditandai merah). Panjang ≥ 0; koefisien 0–1.", "%1 invalid cell(s) (marked red). Lengths ≥ 0; coefficients 0–1.").arg(bad)); return false; }
        return true;
    });
    if (sh.exec() != 1) return;
    for (int r = 0; r < tw->rowCount(); ++r)
        for (auto& rc : g->model.reaches)
            if (rc.uid == refs[r].reach)
                for (auto& n : rc.nodes)
                    if (n.uid == refs[r].xs) {
                        n.lenLeft = *parseUserNumber(tw->item(r, 2)->text());
                        n.lenChannel = *parseUserNumber(tw->item(r, 3)->text());
                        n.lenRight = *parseUserNumber(tw->item(r, 4)->text());
                        n.contraction = *parseUserNumber(tw->item(r, 5)->text());
                        n.expansion = *parseUserNumber(tw->item(r, 6)->text());
                    }
    s_->markEdited();
    showToast(this, tr2("Tabel diterapkan", "Table applied"));
}

void GeometryView::newGeometryFile() {
    auto* p = s_->project();
    if (!p) return;
    QString t = tr2("Geometri baru", "New geometry");
    if (!sheetGetText(this, tr2("File geometri baru", "New geometry file"), tr2("Judul geometri:", "Geometry title:"), t, 0, "geometry.newfile",
                      tr2("De-RAS membuat file .g## dengan nomor kosong terkecil. File itu masuk ke daftar .prj saat Anda menyimpan.", "A new .g## file with the lowest free number is created and listed in the .prj when saved.")))
        return;
    const std::string e = p->addGeometry(ss(t));
    if (e.empty()) return;
    s_->setEditGeometry(qs(e));
    s_->markEdited();
    fillFiles();
}

}  // namespace deras::ui

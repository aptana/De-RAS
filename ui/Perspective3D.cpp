// De-RAS - ui/Perspective3D.cpp
#include "Perspective3D.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Common.h"
#include "Theme.h"

namespace deras::ui {

Perspective3D::Perspective3D(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr2("Seret untuk memutar, roda mouse untuk melebih-lebihkan skala vertikal.", "Drag to rotate, mouse wheel for vertical exaggeration."));
}

void Perspective3D::setData(const Reach* reach, const std::vector<double>& ws, const QString& title) {
    sections_.clear();
    title_ = title;
    if (!reach) { update(); return; }
    const auto xs = reach->crossSections();
    // distance of each section from the downstream end (sum of main-channel lengths)
    std::vector<double> y(xs.size(), 0);
    for (size_t i = xs.size(); i-- > 1;) y[i - 1] = y[i] + xs[i - 1]->lenChannel;
    double zlo = 1e300, zhi = -1e300, xr = 0;
    for (size_t i = 0; i < xs.size(); ++i) {
        const CrossSection& c = *xs[i];
        if (c.staElev.size() < 2) continue;
        Sec s;
        s.rs = qs(c.rs);
        s.y = y[i];
        const double centre = c.bankLeft && c.bankRight ? (*c.bankLeft + *c.bankRight) / 2 : (c.staElev.front().station + c.staElev.back().station) / 2;
        for (const auto& p : c.staElev) {
            s.pts.emplace_back(p.station - centre, p.elev);
            zlo = std::min(zlo, p.elev);
            zhi = std::max(zhi, p.elev);
            xr = std::max(xr, std::fabs(p.station - centre));
        }
        if (i < ws.size() && std::isfinite(ws[i])) {
            s.ws = ws[i];
            s.hasWs = true;
            zhi = std::max(zhi, s.ws);
            // outermost water edges of the wetted part around the channel
            const auto& pts = s.pts;
            s.wl = pts.front().x();
            s.wr = pts.back().x();
            for (size_t k = 0; k < pts.size(); ++k)
                if (pts[k].y() <= s.ws) {
                    s.wl = k > 0 ? pts[k - 1].x() + (pts[k].x() - pts[k - 1].x()) * (pts[k - 1].y() - s.ws) / (pts[k - 1].y() - pts[k].y()) : pts[k].x();
                    break;
                }
            for (size_t k = pts.size(); k-- > 0;)
                if (pts[k].y() <= s.ws) {
                    s.wr = k + 1 < pts.size() ? pts[k].x() + (pts[k + 1].x() - pts[k].x()) * (s.ws - pts[k].y()) / (pts[k + 1].y() - pts[k].y()) : pts[k].x();
                    break;
                }
        }
        sections_.push_back(std::move(s));
    }
    zmin_ = zlo;
    if (!sections_.empty() && zhi > zlo) {
        const double horiz = std::max(2 * xr, y.empty() ? 0.0 : y.front());
        vex_ = std::clamp(0.08 * horiz / (zhi - zlo), 1.0, 20.0);
    }
    update();
}

void Perspective3D::mousePressEvent(QMouseEvent* e) { last_ = e->position(); }

void Perspective3D::mouseMoveEvent(QMouseEvent* e) {
    if (!(e->buttons() & Qt::LeftButton)) return;
    const QPointF d = e->position() - last_;
    last_ = e->position();
    az_ = std::fmod(az_ + d.x() * 0.4, 360.0);
    tilt_ = std::clamp(tilt_ + d.y() * 0.3, 0.0, 89.0);
    update();
}

void Perspective3D::wheelEvent(QWheelEvent* e) {
    vex_ = std::clamp(vex_ * std::pow(1.12, e->angleDelta().y() / 120.0), 0.5, 200.0);
    update();
}

void Perspective3D::paintEvent(QPaintEvent*) {
    const Theme& th = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), th.input);
    if (sections_.empty()) {
        p.setPen(th.text3);
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, empty_);
        return;
    }
    const double a = az_ * M_PI / 180, t = tilt_ * M_PI / 180;
    const double ca = std::cos(a), sa = std::sin(a), ct = std::cos(t), st = std::sin(t);
    // view coordinates: u (right), v (up on screen), depth (into the screen)
    auto view = [&](double x, double y, double z, double& depth) {
        const double xr = x * ca - y * sa, yr = x * sa + y * ca;
        const double zz = (z - zmin_) * vex_;
        depth = yr * ct + zz * st;
        return QPointF(xr, zz * ct - yr * st);
    };
    double u0 = 1e300, u1 = -1e300, v0 = 1e300, v1 = -1e300, dd;
    for (const auto& s : sections_)
        for (const auto& q : s.pts) {
            const QPointF v = view(q.x(), s.y, q.y(), dd);
            u0 = std::min(u0, v.x()); u1 = std::max(u1, v.x()); v0 = std::min(v0, v.y()); v1 = std::max(v1, v.y());
        }
    const QRectF area = rect().adjusted(24, 40, -24, -24);
    const double sc = std::min(area.width() / std::max(1e-9, u1 - u0), area.height() / std::max(1e-9, v1 - v0));
    auto scr = [&](const QPointF& v) { return QPointF(area.center().x() + (v.x() - (u0 + u1) / 2) * sc, area.center().y() - (v.y() - (v0 + v1) / 2) * sc); };

    struct Prim { double depth; int kind; size_t i; };  // kind 0 ground line, 1 water between i and i+1
    std::vector<Prim> prims;
    for (size_t i = 0; i < sections_.size(); ++i) {
        view(0, sections_[i].y, sections_[i].pts.front().y(), dd);
        prims.push_back({dd, 0, i});
        if (i + 1 < sections_.size() && sections_[i].hasWs && sections_[i + 1].hasWs) {
            double d2;
            view(0, (sections_[i].y + sections_[i + 1].y) / 2, sections_[i].ws, d2);
            prims.push_back({d2 + 1e-6, 1, i});
        }
    }
    std::sort(prims.begin(), prims.end(), [](const Prim& x, const Prim& y) { return x.depth > y.depth; });
    QColor water = th.water;
    water.setAlpha(150);
    for (const auto& pr : prims) {
        const Sec& s = sections_[pr.i];
        if (pr.kind == 1) {
            const Sec& n = sections_[pr.i + 1];
            QPolygonF q;
            q << scr(view(s.wl, s.y, s.ws, dd)) << scr(view(s.wr, s.y, s.ws, dd)) << scr(view(n.wr, n.y, n.ws, dd)) << scr(view(n.wl, n.y, n.ws, dd));
            p.setPen(QPen(th.water, 1));
            p.setBrush(water);
            p.drawPolygon(q);
            continue;
        }
        QPolygonF g;
        for (const auto& q : s.pts) g << scr(view(q.x(), s.y, q.y(), dd));
        // ground fill down to the lowest elevation gives the sections a solid look
        QPolygonF f = g;
        f << scr(view(s.pts.back().x(), s.y, zmin_, dd)) << scr(view(s.pts.front().x(), s.y, zmin_, dd));
        QColor gf = th.ground;
        gf.setAlpha(60);
        p.setPen(Qt::NoPen);
        p.setBrush(gf);
        p.drawPolygon(f);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(th.ground, 1.8));
        p.drawPolyline(g);
        if (s.hasWs) {
            p.setPen(QPen(th.water, 2));
            p.drawLine(scr(view(s.wl, s.y, s.ws, dd)), scr(view(s.wr, s.y, s.ws, dd)));
        }
        p.setPen(th.text2);
        p.drawText(g.front() + QPointF(-4 - p.fontMetrics().horizontalAdvance(s.rs), 4), s.rs);
    }
    p.setPen(th.heading);
    p.drawText(QRectF(0, 8, width(), 24), Qt::AlignHCenter, title_);
    p.setPen(th.text3);
    p.drawText(QRectF(12, height() - 22, width() - 24, 18), Qt::AlignRight,
               tr2("Skala vertikal ×%1 · putar %2° · miring %3°", "Vertical exaggeration ×%1 · rotation %2° · tilt %3°")
                   .arg(QString::number(vex_, 'f', 1)).arg(int(az_)).arg(int(tilt_)));
}

}  // namespace deras::ui

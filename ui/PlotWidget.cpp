// De-RAS - ui/PlotWidget.cpp
#include "PlotWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <limits>

#include "Common.h"

namespace deras::ui {

QVector<double> niceTicks(double lo, double hi, int target) {
    QVector<double> t;
    if (!(hi > lo)) return t;
    double raw = (hi - lo) / std::max(1, target);
    double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double step = mag;
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (raw <= m * mag) { step = m * mag; break; }
    for (double v = std::ceil(lo / step) * step; v <= hi + step * 1e-9; v += step) t << (std::fabs(v) < step * 1e-9 ? 0.0 : v);
    return t;
}

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(160);
    setAccessibleName(tr2("Grafik", "Plot"));
}

void PlotWidget::setTitles(const QString& t, const QString& x, const QString& y) {
    title_ = t;
    xTitle_ = x;
    yTitle_ = y;
    update();
}

void PlotWidget::setSeries(const QVector<PlotSeries>& s) {
    series_ = s;
    update();
}

void PlotWidget::clear() {
    series_.clear();
    markers_.clear();
    update();
}

void PlotWidget::paintEvent(QPaintEvent*) {
    const Theme& th = Theme::current();
    const QColor plotBg = th.dark ? QColor(9, 16, 29) : QColor("#FBFDFF");
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), plotBg);
    QFont f = font();
    QFontMetrics fm(f);
    double x0 = std::numeric_limits<double>::infinity(), x1 = -x0, y0 = x0, y1 = -x0;
    for (const auto& s : series_)
        for (const auto& q : s.pts) {
            if (!std::isfinite(q.x()) || !std::isfinite(q.y())) continue;
            x0 = std::min(x0, q.x()); x1 = std::max(x1, q.x());
            y0 = std::min(y0, q.y()); y1 = std::max(y1, q.y());
        }
    const int left = fm.horizontalAdvance("00000.0") + 26, right = 16, top = title_.isEmpty() ? 14 : fm.height() + 18,
              bottom = fm.height() * 2 + 22;
    QRect area(left, top, width() - left - right, height() - top - bottom);
    p.setPen(th.heading);
    QFont tf = f;
    tf.setBold(true);
    p.setFont(tf);
    p.drawText(QRect(0, 4, width(), fm.height() + 6), Qt::AlignHCenter, title_);
    p.setFont(f);
    if (!std::isfinite(x0) || area.width() < 40 || area.height() < 40) {
        p.setPen(th.text2);
        p.drawText(rect(), Qt::AlignCenter, empty_.isEmpty() ? tr2("Tidak ada data untuk ditampilkan.", "No data to display.") : empty_);
        return;
    }
    if (x1 - x0 < 1e-9) { x0 -= 1; x1 += 1; }
    if (y1 - y0 < 1e-9) { y0 -= 1; y1 += 1; }
    double py = (y1 - y0) * 0.06;
    y0 -= py; y1 += py;
    auto X = [&](double x) {
        double t = (x - x0) / (x1 - x0);
        if (reverseX_) t = 1 - t;
        return area.left() + t * area.width();
    };
    auto Y = [&](double y) { return area.bottom() - (y - y0) / (y1 - y0) * area.height(); };

    // grid + ticks
    p.setPen(QPen(th.line, 1));
    p.drawRect(area);
    for (double v : niceTicks(x0, x1, std::max(3, area.width() / 90))) {
        double xx = X(v);
        p.setPen(QPen(th.line, 1, Qt::DotLine));
        p.drawLine(QPointF(xx, area.top()), QPointF(xx, area.bottom()));
        p.setPen(th.text2);
        p.drawText(QRectF(xx - 50, area.bottom() + 4, 100, fm.height()), Qt::AlignHCenter, QString::number(v, 'g', 7));
    }
    for (double v : niceTicks(y0, y1, std::max(3, area.height() / 50))) {
        double yy = Y(v);
        p.setPen(QPen(th.line, 1, Qt::DotLine));
        p.drawLine(QPointF(area.left(), yy), QPointF(area.right(), yy));
        p.setPen(th.text2);
        p.drawText(QRectF(0, yy - fm.height() / 2.0, left - 6, fm.height()), Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'g', 7));
    }
    p.setPen(th.text);
    p.drawText(QRect(area.left(), area.bottom() + fm.height() + 6, area.width(), fm.height() + 4), Qt::AlignHCenter, xTitle_);
    p.save();
    p.translate(12, area.center().y());
    p.rotate(-90);
    p.drawText(QRect(-area.height() / 2, -fm.height() + 4, area.height(), fm.height() + 4), Qt::AlignHCenter, yTitle_);
    p.restore();

    p.setClipRect(area.adjusted(1, 1, -1, -1));
    // fills
    for (const auto& s : series_) {
        if (s.fillBelowSeries < 0 || s.fillBelowSeries >= series_.size() || s.pts.size() < 2) continue;
        // interpolation below needs ascending x (profiles run upstream -> downstream, i.e. descending)
        auto byX = [](QVector<QPointF> v) {
            std::stable_sort(v.begin(), v.end(), [](const QPointF& l, const QPointF& r) { return l.x() < r.x(); });
            return v;
        };
        const QVector<QPointF> base = byX(series_[s.fillBelowSeries].pts);
        const QVector<QPointF> top = byX(s.pts);
        if (base.size() < 2) continue;
        // sample the base polyline at fine x positions between the series' x range and clip where the base is above
        double a = top.front().x(), b = top.back().x();
        auto interp = [](const QVector<QPointF>& v, double x) {
            if (x <= v.front().x()) return v.front().y();
            if (x >= v.back().x()) return v.back().y();
            for (int i = 1; i < v.size(); ++i)
                if (v[i].x() >= x) {
                    double t = (v[i].x() == v[i - 1].x()) ? 1.0 : (x - v[i - 1].x()) / (v[i].x() - v[i - 1].x());
                    return v[i - 1].y() + t * (v[i].y() - v[i - 1].y());
                }
            return v.back().y();
        };
        const int N = std::max(2, area.width());
        QPainterPath path;
        bool open = false;
        QVector<QPointF> upper, lower;
        auto flush = [&]() {
            if (upper.size() >= 2) {
                QPolygonF poly;
                for (const auto& q : upper) poly << q;
                for (int i = lower.size() - 1; i >= 0; --i) poly << lower[i];
                path.addPolygon(poly);
            }
            upper.clear();
            lower.clear();
        };
        for (int i = 0; i <= N; ++i) {
            double x = a + (b - a) * i / N;
            double topY = interp(top, x), bot = interp(base, x);
            if (topY > bot) {
                upper << QPointF(X(x), Y(topY));
                lower << QPointF(X(x), Y(bot));
                open = true;
            } else if (open) {
                flush();
                open = false;
            }
        }
        flush();
        p.fillPath(path, s.fillColor);
    }
    // lines
    for (const auto& s : series_) {
        if (s.pts.isEmpty()) continue;
        QPen pen(s.color, s.width, s.style);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        QPolygonF poly;
        for (const auto& q : s.pts) poly << QPointF(X(q.x()), Y(q.y()));
        p.drawPolyline(poly);
        if (s.markers) {
            p.setBrush(plotBg);
            for (const auto& q : poly) p.drawEllipse(q, 3, 3);
            p.setBrush(Qt::NoBrush);
        }
    }
    for (const auto& m : markers_) {
        double xx = X(m.x);
        p.setPen(QPen(m.color, 1.5, Qt::DashLine));
        p.drawLine(QPointF(xx, area.top()), QPointF(xx, area.bottom()));
        p.setPen(m.color);
        p.drawText(QPointF(xx + 4, area.top() + fm.height()), m.label);
    }
    p.setClipping(false);
    // legend
    int lx = area.left() + 8, ly = area.top() + 8;
    int lw = 0, n = 0;
    for (const auto& s : series_) if (s.legend && !s.name.isEmpty()) { lw = std::max(lw, fm.horizontalAdvance(s.name)); ++n; }
    if (n > 0) {
        QRect box(lx, ly, lw + 40, n * (fm.height() + 2) + 8);
        QColor bgc = plotBg;
        bgc.setAlpha(225);
        p.setPen(th.line);
        p.setBrush(bgc);
        p.drawRoundedRect(box, 6, 6);
        int yy = ly + 4;
        for (const auto& s : series_) {
            if (!s.legend || s.name.isEmpty()) continue;
            p.setPen(QPen(s.color, s.width + 1, s.style));
            p.drawLine(lx + 8, yy + fm.height() / 2, lx + 28, yy + fm.height() / 2);
            p.setPen(th.text);
            p.drawText(lx + 34, yy + fm.ascent(), s.name);
            yy += fm.height() + 2;
        }
    }
    if (hasHover_ && area.contains(hover_.toPoint())) {
        double t = (hover_.x() - area.left()) / area.width();
        if (reverseX_) t = 1 - t;
        double xv = x0 + t * (x1 - x0);
        double yv = y0 + (area.bottom() - hover_.y()) / area.height() * (y1 - y0);
        QString s = QStringLiteral("x = %1   y = %2").arg(xv, 0, 'f', 2).arg(yv, 0, 'f', 3);
        p.setPen(th.text2);
        p.drawText(QRect(area.right() - fm.horizontalAdvance(s) - 8, area.bottom() - fm.height() - 4, fm.horizontalAdvance(s) + 4, fm.height()), s);
    }
}

void PlotWidget::mouseMoveEvent(QMouseEvent* e) {
    hover_ = e->position();
    hasHover_ = true;
    update();
}

void PlotWidget::leaveEvent(QEvent*) {
    hasHover_ = false;
    update();
}

}  // namespace deras::ui

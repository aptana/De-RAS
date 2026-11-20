// De-RAS - ui/Icons.cpp
#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace deras::ui {

namespace {
QPainterPath poly(std::initializer_list<QPointF> pts) {
    QPainterPath p;
    bool first = true;
    for (const auto& q : pts) {
        if (first) p.moveTo(q); else p.lineTo(q);
        first = false;
    }
    return p;
}
QPainterPath rrect(double x, double y, double w, double h, double r) {
    QPainterPath p;
    p.addRoundedRect(QRectF(x, y, w, h), r, r);
    return p;
}
QPainterPath ellipse(double cx, double cy, double rx, double ry) {
    QPainterPath p;
    p.addEllipse(QPointF(cx, cy), rx, ry);
    return p;
}
QPainterPath wave(double x0, double x1, double y, double amp, int halfWaves) {
    QPainterPath p;
    p.moveTo(x0, y);
    const double w = (x1 - x0) / halfWaves;
    for (int i = 0; i < halfWaves; ++i) {
        const double a = x0 + i * w;
        const double s = (i % 2 == 0) ? -amp : amp;
        p.cubicTo(a + w * 0.35, y + s, a + w * 0.65, y + s, a + w, y);
    }
    return p;
}
}  // namespace

void paintIcon(QPainter* p, const QRectF& r, const QString& n, const QColor& c, double stroke) {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->translate(r.topLeft());
    p->scale(r.width() / 24.0, r.height() / 24.0);
    QPen pen(c, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    auto S = [&](const QPainterPath& path) { p->drawPath(path); };
    auto L = [&](double x1, double y1, double x2, double y2) { p->drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    auto dotAt = [&](double x, double y, double rr) {
        p->save();
        p->setPen(Qt::NoPen);
        p->setBrush(c);
        p->drawEllipse(QPointF(x, y), rr, rr);
        p->restore();
    };

    if (n == "folder") {
        S(poly({{3, 7}, {3, 18.5}, {21, 18.5}, {21, 8.5}, {12, 8.5}, {10, 6}, {3, 6}, {3, 7}}));
    } else if (n == "save") {
        S(rrect(4, 4, 16, 16, 2));
        S(poly({{8, 4}, {8, 9}, {15, 9}, {15, 4}}));
        S(rrect(8, 13, 8, 7, 1));
    } else if (n == "geometry" || n == "xs") {
        S(poly({{3, 6}, {7, 8}, {9.5, 17}, {14.5, 17}, {17, 8}, {21, 6}}));
        if (n == "geometry") L(8.5, 12.5, 15.5, 12.5);
    } else if (n == "steady") {
        L(4, 9, 19, 9);
        S(poly({{16, 6}, {19, 9}, {16, 12}}));
        L(4, 15.5, 19, 15.5);
        S(poly({{16, 12.5}, {19, 15.5}, {16, 18.5}}));
    } else if (n == "unsteady") {
        S(poly({{3, 17}, {7, 10}, {10.5, 14}, {15, 6}, {21, 16}}));
    } else if (n == "quasi") {
        S(poly({{3, 18}, {7, 18}, {7, 14}, {11, 14}, {11, 10}, {15, 10}, {15, 6.5}, {21, 6.5}}));
    } else if (n == "sediment") {
        S(wave(3, 21, 8, 1.6, 4));
        dotAt(7, 14, 1.4); dotAt(12, 13.5, 1.4); dotAt(17, 14.2, 1.4);
        L(3, 18.5, 21, 18.5);
    } else if (n == "water") {
        QPainterPath d;
        d.moveTo(12, 3.5);
        d.cubicTo(12, 3.5, 6, 10.5, 6, 14.5);
        d.cubicTo(6, 18, 8.7, 20.5, 12, 20.5);
        d.cubicTo(15.3, 20.5, 18, 18, 18, 14.5);
        d.cubicTo(18, 10.5, 12, 3.5, 12, 3.5);
        S(d);
    } else if (n == "run" || n == "play") {
        S(poly({{7, 5}, {19, 12}, {7, 19}, {7, 5}}));
    } else if (n == "run-unsteady") {
        S(poly({{4, 5}, {13, 12}, {4, 19}, {4, 5}}));
        S(poly({{14, 15}, {16, 11}, {18, 14}, {21, 8}}));
    } else if (n == "run-sediment") {
        S(poly({{4, 5}, {13, 12}, {4, 19}, {4, 5}}));
        dotAt(17, 15, 1.3); dotAt(20, 12.5, 1.3); dotAt(16.5, 10, 1.3);
    } else if (n == "run-water") {
        S(poly({{4, 5}, {13, 12}, {4, 19}, {4, 5}}));
        QPainterPath d;
        d.moveTo(18, 8); d.cubicTo(18, 8, 15, 12, 15, 14); d.cubicTo(15, 15.8, 16.3, 17, 18, 17);
        d.cubicTo(19.7, 17, 21, 15.8, 21, 14); d.cubicTo(21, 12, 18, 8, 18, 8);
        S(d);
    } else if (n == "design") {
        S(poly({{5, 19}, {5.8, 15.5}, {15.5, 5.8}, {18.2, 8.5}, {8.5, 18.2}, {5, 19}}));
        L(13.5, 7.8, 16.2, 10.5);
    } else if (n == "mapper" || n == "layers") {
        S(poly({{12, 4}, {21, 8.5}, {12, 13}, {3, 8.5}, {12, 4}}));
        S(poly({{3, 12.5}, {12, 17}, {21, 12.5}}));
        S(poly({{3, 16.5}, {12, 21}, {21, 16.5}}));
    } else if (n == "profile") {
        S(poly({{3, 16}, {9, 13.5}, {15, 11}, {21, 7}}));
        S(poly({{3, 19.5}, {9, 17.5}, {15, 15.5}, {21, 12}}));
    } else if (n == "genprofile") {
        S(poly({{4, 4}, {4, 20}, {20, 20}}));
        S(poly({{7, 15}, {10.5, 11}, {13.5, 13.5}, {19, 7}}));
    } else if (n == "rating") {
        S(poly({{4, 4}, {4, 20}, {20, 20}}));
        QPainterPath c2; c2.moveTo(6, 18); c2.cubicTo(9, 16, 12, 11, 19, 6); S(c2);
    } else if (n == "cube") {
        S(poly({{12, 3.5}, {20, 7.8}, {20, 16.2}, {12, 20.5}, {4, 16.2}, {4, 7.8}, {12, 3.5}}));
        S(poly({{4, 7.8}, {12, 12}, {20, 7.8}}));
        L(12, 12, 12, 20.5);
    } else if (n == "hydrograph") {
        L(3, 20, 21, 20);
        QPainterPath h; h.moveTo(3, 19); h.cubicTo(8, 19, 9, 5, 12, 5); h.cubicTo(15, 5, 15.5, 16, 21, 18); S(h);
    } else if (n == "table-detail" || n == "table") {
        S(rrect(4, 4, 16, 16, 2));
        L(4, 9, 20, 9);
        L(10, 9, 10, 20);
    } else if (n == "table-grid") {
        S(rrect(4, 4, 16, 16, 2));
        L(4, 9.3, 20, 9.3); L(4, 14.6, 20, 14.6);
        L(9.3, 4, 9.3, 20); L(14.6, 4, 14.6, 20);
    } else if (n == "warning") {
        S(poly({{12, 4}, {21, 19.5}, {3, 19.5}, {12, 4}}));
        L(12, 10, 12, 14);
        dotAt(12, 16.8, 1.0);
    } else if (n == "timeseries") {
        S(poly({{4, 4}, {4, 20}, {20, 20}}));
        S(poly({{7, 16}, {10, 12}, {13, 14}, {16, 8}, {19, 10}}));
    } else if (n == "lock") {
        S(rrect(5, 11, 14, 9.5, 2));
        QPainterPath a; a.moveTo(8, 11); a.lineTo(8, 8); a.cubicTo(8, 4.5, 16, 4.5, 16, 8); a.lineTo(16, 11); S(a);
    } else if (n == "info") {
        S(ellipse(12, 12, 8.5, 8.5));
        L(12, 11, 12, 16.5);
        dotAt(12, 7.8, 1.1);
    } else if (n == "back") {
        S(poly({{14.5, 6}, {8.5, 12}, {14.5, 18}}));
    } else if (n == "forward" || n == "chevron-right") {
        S(poly({{9.5, 6}, {15.5, 12}, {9.5, 18}}));
    } else if (n == "chevron-down") {
        S(poly({{6, 9.5}, {12, 15.5}, {18, 9.5}}));
    } else if (n == "arrow-right") {
        L(5, 12, 19, 12);
        S(poly({{13.5, 6.5}, {19, 12}, {13.5, 17.5}}));
    } else if (n == "close") {
        L(6.5, 6.5, 17.5, 17.5); L(17.5, 6.5, 6.5, 17.5);
    } else if (n == "check") {
        S(poly({{5, 12.5}, {10, 17.5}, {19.5, 7}}));
    } else if (n == "junction") {
        L(5, 4, 12, 12); L(19, 4, 12, 12); L(12, 12, 12, 20.5);
        dotAt(12, 12, 2.2);
    } else if (n == "bridge") {
        L(3, 7, 21, 7); L(3, 10, 21, 10);
        L(6, 10, 6, 19); L(12, 10, 12, 19); L(18, 10, 18, 19);
        L(3, 19, 21, 19);
    } else if (n == "inline") {
        L(3, 9, 21, 9);
        S(rrect(5, 9, 14, 9, 1));
        L(9.6, 9, 9.6, 18); L(14.3, 9, 14.3, 18);
        L(3, 20, 21, 20);
    } else if (n == "lateral") {
        L(3, 8, 21, 8); L(3, 16, 21, 16);
        L(7, 8, 7, 16); L(17, 8, 17, 16);
    } else if (n == "storage") {
        S(ellipse(12, 12, 8.5, 5.5));
    } else if (n == "grid2d") {
        S(rrect(4, 5, 16, 14, 2));
        L(4, 9.7, 20, 9.7); L(4, 14.3, 20, 14.3);
        L(9.3, 5, 9.3, 19); L(14.7, 5, 14.7, 19);
    } else if (n == "connect") {
        S(rrect(3, 7, 7, 10, 2));
        S(rrect(14, 7, 7, 10, 1));
        L(10, 12, 14, 12);
    } else if (n == "pump") {
        S(ellipse(10, 13, 5.5, 5.5));
        S(poly({{14, 9}, {20, 9}, {20, 6}}));
        L(10, 13, 13, 10.5);
    } else if (n == "htab") {
        S(poly({{4, 20}, {4, 4}}));
        L(4, 20, 20, 20);
        L(8, 16, 18, 16); L(8, 12, 15, 12); L(8, 8, 12, 8);
    } else if (n == "picture") {
        S(rrect(3.5, 5, 17, 14, 2));
        S(poly({{6, 17}, {10.5, 11.5}, {14, 15}, {16, 13}, {19, 17}}));
        dotAt(15.5, 9, 1.4);
    } else if (n == "draw") {
        S(poly({{4, 17}, {9, 9}, {14, 13}, {20, 6}}));
        dotAt(4, 17, 1.4); dotAt(20, 6, 1.4);
    } else if (n == "line" || n == "breakline") {
        L(4, 18, 20, 6);
        if (n == "breakline") { L(9, 8, 11, 10); L(13, 14, 15, 16); }
    } else if (n == "region") {
        S(poly({{5, 7}, {15, 4.5}, {20, 11}, {16, 19}, {6, 18}, {5, 7}}));
    } else if (n == "pan") {
        L(12, 3.5, 12, 20.5); L(3.5, 12, 20.5, 12);
        S(poly({{9.5, 6}, {12, 3.5}, {14.5, 6}}));
        S(poly({{9.5, 18}, {12, 20.5}, {14.5, 18}}));
        S(poly({{6, 9.5}, {3.5, 12}, {6, 14.5}}));
        S(poly({{18, 9.5}, {20.5, 12}, {18, 14.5}}));
    } else if (n == "zoom" || n == "search") {
        S(ellipse(10.5, 10.5, 6, 6));
        L(15, 15, 20, 20);
        if (n == "zoom") { L(8, 10.5, 13, 10.5); L(10.5, 8, 10.5, 13); }
    } else if (n == "measure") {
        S(poly({{3, 15}, {15, 3}, {21, 9}, {9, 21}, {3, 15}}));
        L(7, 11, 9, 13); L(10, 8, 12, 10); L(13, 5, 15, 7);
    } else if (n == "identify") {
        S(ellipse(12, 12, 8.5, 8.5));
        L(12, 11, 12, 16.5);
        dotAt(12, 7.8, 1.1);
    } else if (n == "plus") {
        L(12, 5, 12, 19); L(5, 12, 19, 12);
    } else if (n == "copy") {
        S(rrect(8, 8, 12, 12, 2));
        S(poly({{16, 8}, {16, 5}, {4, 5}, {4, 16}, {8, 16}}));
    } else if (n == "trash") {
        L(4, 7, 20, 7);
        S(poly({{6, 7}, {7, 20}, {17, 20}, {18, 7}}));
        S(poly({{9, 7}, {9, 4.5}, {15, 4.5}, {15, 7}}));
    } else if (n == "import") {
        L(12, 3, 12, 15);
        S(poly({{7.5, 10.5}, {12, 15}, {16.5, 10.5}}));
        S(poly({{4, 15}, {4, 20}, {20, 20}, {20, 15}}));
    } else if (n == "export") {
        L(12, 15, 12, 3);
        S(poly({{7.5, 7.5}, {12, 3}, {16.5, 7.5}}));
        S(poly({{4, 15}, {4, 20}, {20, 20}, {20, 15}}));
    } else if (n == "doctor") {
        S(rrect(4, 4, 16, 16, 3));
        L(12, 8, 12, 16); L(8, 12, 16, 12);
    } else if (n == "stop") {
        S(rrect(6, 6, 12, 12, 2));
    } else if (n == "home") {
        S(poly({{4, 11}, {12, 4}, {20, 11}}));
        S(poly({{6.5, 9.5}, {6.5, 20}, {17.5, 20}, {17.5, 9.5}}));
    } else if (n == "options") {
        S(ellipse(12, 12, 3, 3));
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4;
            L(12 + 6 * std::cos(a), 12 + 6 * std::sin(a), 12 + 8.5 * std::cos(a), 12 + 8.5 * std::sin(a));
        }
        S(ellipse(12, 12, 6, 6));
    } else if (n == "help") {
        S(ellipse(12, 12, 8.5, 8.5));
        QPainterPath q; q.moveTo(9.5, 9.5); q.cubicTo(9.5, 6.5, 14.5, 6.5, 14.5, 9.5); q.cubicTo(14.5, 11.5, 12, 11.5, 12, 14); S(q);
        dotAt(12, 16.8, 1.0);
    } else if (n == "dot") {
        dotAt(12, 12, 4);
    } else if (n == "circle") {
        S(ellipse(12, 12, 4, 4));
    } else if (n == "file" || n == "description") {
        S(poly({{6, 3.5}, {14, 3.5}, {19, 8.5}, {19, 20.5}, {6, 20.5}, {6, 3.5}}));
        S(poly({{14, 3.5}, {14, 8.5}, {19, 8.5}}));
        if (n == "description") { L(9, 13, 16, 13); L(9, 16.5, 16, 16.5); }
    } else {
        S(ellipse(12, 12, 7, 7));
    }
    p->restore();
}

QPixmap iconPixmap(const QString& name, int px, const QColor& color, double dpr) {
    QPixmap pm(static_cast<int>(std::ceil(px * dpr)), static_cast<int>(std::ceil(px * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paintIcon(&p, QRectF(0, 0, px, px), name, color);
    return pm;
}

QIcon lineIcon(const QString& name, const QColor& color) {
    QIcon ic;
    for (int s : {16, 20, 24, 32}) ic.addPixmap(iconPixmap(name, s, color, 2.0));
    return ic;
}

}  // namespace deras::ui

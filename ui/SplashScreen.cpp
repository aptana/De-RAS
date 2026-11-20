// De-RAS - ui/SplashScreen.cpp
#include "SplashScreen.h"

#include <QApplication>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QScreen>
#include <cmath>

#include "Common.h"
#include "Version.h"
#include "Widgets.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace deras::ui {

namespace {
constexpr int VW = 1000, VH = 554, SB = 46;  // canonical layout (status bar 46 px)

double hash2(int ix, int iy, int seed) {
    uint32_t n = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iy) * 668265263u + static_cast<uint32_t>(seed) * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= n >> 16;
    return n / 4294967296.0;
}
double noise(double x, double y, int seed) {
    int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    double fx = x - ix, fy = y - iy;
    double ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    double a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed), c = hash2(ix, iy + 1, seed), d = hash2(ix + 1, iy + 1, seed);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
double fbm(double x, double y, int oct, int seed) {
    double v = 0, amp = 0.5, fr = 1, norm = 0;
    for (int i = 0; i < oct; ++i) {
        v += amp * noise(x * fr, y * fr, seed + i * 97);
        norm += amp;
        fr *= 2.03;
        amp *= 0.5;
    }
    return v / norm;
}
double cr(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}
struct P3 { double x, y, w; };
}  // namespace

bool SplashScreen::animationsEnabled() { return deras::ui::animationsEnabled(); }

SplashScreen::SplashScreen() : QWidget(nullptr, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(appIcon());
    setAccessibleName(QStringLiteral("De-RAS"));
    QRect avail = QApplication::primaryScreen() ? QApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1280, 800);
    scale_ = std::min(1.0, std::min(avail.width() * 0.66 / VW, avail.height() * 0.70 / (VH + SB)));
    setFixedSize(static_cast<int>(VW * scale_), static_cast<int>((VH + SB) * scale_));
    move(avail.center() - QPoint(width() / 2, height() / 2));
    // Official icon files, used at their native sizes (no runtime downscaling of the large logo).
    icon_ = QPixmap(QStringLiteral(":/branding/De-RAS-64.png"));
    iconSmall_ = QPixmap(QStringLiteral(":/branding/De-RAS-24.png"));
    animate_ = deras::ui::animationsEnabled();  // static background when Windows animations are off
    buildRivers();
    buildStatic();
    status_ = tr2("Memulai De-RAS…", "Starting De-RAS…");
    connect(&timer_, &QTimer::timeout, this, &SplashScreen::tick);
    timer_.start(16);
    clock_.start();
    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(450);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void SplashScreen::buildRivers() {
    const QVector<QVector<P3>> specs = {
        {{-30, 230, 23}, {145, 318, 29}, {320, 395, 36}, {470, 314, 42}, {610, 240, 47}, {735, 326, 54}, {840, 405, 62}, {1040, 490, 72}},
        {{450, -20, 9}, {482, 54, 12}, {515, 120, 15}, {556, 184, 19}, {596, 246, 24}},
        {{120, 565, 11}, {205, 515, 14}, {280, 465, 18}, {342, 420, 21}, {396, 378, 25}},
        {{632, 246, 21}, {732, 210, 18}, {830, 195, 16}, {936, 228, 18}, {1040, 270, 21}},
    };
    const double speeds[] = {0.085, 0.095, 0.09, 0.088};
    for (int b = 0; b < specs.size(); ++b) {
        const auto& pts = specs[b];
        QVector<P3> pad;
        pad << P3{2 * pts[0].x - pts[1].x, 2 * pts[0].y - pts[1].y, pts[0].w};
        pad += pts;
        const auto& l = pts.back();
        const auto& l2 = pts[pts.size() - 2];
        pad << P3{2 * l.x - l2.x, 2 * l.y - l2.y, l.w};
        Branch br;
        br.speed = speeds[b];
        br.main = b == 0;
        const int samples = b == 0 ? 180 : 100;
        const int segs = pts.size() - 1;
        for (int i = 0; i <= samples; ++i) {
            double u = double(i) / samples, sc = u * segs;
            int s = std::min(segs - 1, static_cast<int>(std::floor(sc)));
            double t = sc - s;
            Node n;
            n.x = cr(pad[s].x, pad[s + 1].x, pad[s + 2].x, pad[s + 3].x, t);
            n.y = cr(pad[s].y, pad[s + 1].y, pad[s + 2].y, pad[s + 3].y, t);
            n.w = cr(pad[s].w, pad[s + 1].w, pad[s + 2].w, pad[s + 3].w, t) * (1 + 0.08 * std::sin(u * M_PI * 5 + 0.6));
            n.nx = n.ny = 0;
            br.nodes << n;
        }
        for (int i = 0; i < br.nodes.size(); ++i) {
            const Node& a = br.nodes[std::max(0, i - 1)];
            const Node& c = br.nodes[std::min(int(br.nodes.size()) - 1, i + 1)];
            double dx = c.x - a.x, dy = c.y - a.y, len = std::hypot(dx, dy);
            if (len <= 0) len = 1;
            br.nodes[i].nx = -dy / len;
            br.nodes[i].ny = dx / len;
        }
        rivers_ << br;
    }
}

QPointF SplashScreen::along(const Branch& b, double u, double off) const {
    u = std::clamp(u, 0.0, 1.0);
    double e = u * (b.nodes.size() - 1);
    int i0 = static_cast<int>(std::floor(e)), i1 = std::min(int(b.nodes.size()) - 1, i0 + 1);
    double f = e - i0;
    const Node& a = b.nodes[i0];
    const Node& c = b.nodes[i1];
    double w = a.w + (c.w - a.w) * f;
    return QPointF(a.x + (c.x - a.x) * f + (a.nx + (c.nx - a.nx) * f) * off * w * 0.5,
                   a.y + (c.y - a.y) * f + (a.ny + (c.ny - a.ny) * f) * off * w * 0.5);
}

void SplashScreen::buildStatic() {
    static_ = QImage(VW, VH, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&static_);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, VW, VH);
    g.setColorAt(0, QColor("#d7e9f8"));
    g.setColorAt(0.48, QColor("#9ec5e8"));
    g.setColorAt(1, QColor("#6da4d6"));
    p.fillRect(0, 0, VW, VH, g);
    QRadialGradient glow(QPointF(220, 120), 420, QPointF(170, 95));
    glow.setColorAt(0, QColor(235, 246, 255, 165));
    glow.setColorAt(0.6, QColor(215, 233, 248, 56));
    glow.setColorAt(1, QColor(158, 197, 232, 0));
    p.fillRect(0, 0, VW, VH, glow);

    // watercolour / grunge wash (low-resolution fBm texture, smoothly upscaled)
    QImage tex(250, 139, QImage::Format_ARGB32);
    for (int y = 0; y < tex.height(); ++y)
        for (int x = 0; x < tex.width(); ++x) {
            double nx = double(x) / tex.width(), ny = double(y) / tex.height();
            double wx = fbm(nx * 3.5, ny * 3.5, 3, 411) * 0.65, wy = fbm(nx * 3.5 + 4.2, ny * 3.5 + 1.9, 3, 733) * 0.65;
            double v = fbm(nx * 3.2 + wx, ny * 3.2 + wy, 4, 109) * 0.68 + fbm(nx * 14 - wy, ny * 14 + wx, 3, 919) * 0.32 +
                       (hash2(x, y, 5) - 0.5) * 0.14;
            if (v < 0.48) tex.setPixelColor(x, y, QColor(16, 68, 134, static_cast<int>(std::min(1.0, (0.48 - v) * 2.4) * 62)));
            else tex.setPixelColor(x, y, QColor(228, 243, 255, static_cast<int>(std::min(1.0, (v - 0.48) * 2.1) * 54)));
        }
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(0.8);
    p.drawImage(QRect(0, 0, VW, VH), tex);
    p.restore();

    // DEM contours (marching squares on an elevation field shaped by the valleys)
    const int cell = 8, nx = VW / cell + 2, ny = VH / cell + 2;
    QVector<double> h(nx * ny);
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            double x = i * cell, y = j * cell;
            double d = 1e9;
            for (const auto& b : rivers_)
                for (int k = 0; k < b.nodes.size(); k += 3) d = std::min(d, std::hypot(x - b.nodes[k].x, y - b.nodes[k].y) - b.nodes[k].w * 0.5);
            double valley = 1 - std::exp(-std::max(0.0, d) / 160.0);
            h[j * nx + i] = 0.62 * valley + 0.38 * fbm(x / 260.0, y / 260.0, 4, 77);
        }
    auto H = [&](int i, int j) { return h[j * nx + i]; };
    for (int lev = 1; lev < 26; ++lev) {
        double L = lev / 26.0;
        bool index = lev % 5 == 0;
        p.setPen(QPen(QColor(10, 56, 112, index ? 70 : 38), index ? 1.2 : 0.8));
        for (int j = 0; j + 1 < ny; ++j)
            for (int i = 0; i + 1 < nx; ++i) {
                double a = H(i, j), b = H(i + 1, j), c = H(i + 1, j + 1), d = H(i, j + 1);
                int code = (a > L) | ((b > L) << 1) | ((c > L) << 2) | ((d > L) << 3);
                if (code == 0 || code == 15) continue;
                double x = i * cell, y = j * cell;
                auto lerp = [&](double v1, double v2) { return (L - v1) / (v2 - v1); };
                QPointF top(x + cell * lerp(a, b), y), right(x + cell, y + cell * lerp(b, c)), bottom(x + cell * lerp(d, c), y + cell),
                    left(x, y + cell * lerp(a, d));
                auto seg = [&](QPointF p1, QPointF p2) { p.drawLine(p1, p2); };
                switch (code) {
                    case 1: case 14: seg(left, top); break;
                    case 2: case 13: seg(top, right); break;
                    case 3: case 12: seg(left, right); break;
                    case 4: case 11: seg(right, bottom); break;
                    case 6: case 9: seg(top, bottom); break;
                    case 7: case 8: seg(left, bottom); break;
                    case 5: seg(left, top); seg(right, bottom); break;
                    case 10: seg(top, right); seg(left, bottom); break;
                }
            }
    }
    // river bodies
    for (const auto& b : rivers_) {
        QPolygonF poly;
        for (const auto& n : b.nodes) poly << QPointF(n.x + n.nx * n.w * 0.5, n.y + n.ny * n.w * 0.5);
        for (int i = b.nodes.size() - 1; i >= 0; --i) {
            const auto& n = b.nodes[i];
            poly << QPointF(n.x - n.nx * n.w * 0.5, n.y - n.ny * n.w * 0.5);
        }
        p.setPen(QPen(QColor(3, 30, 82, 120), 2.2));
        p.setBrush(QColor(2, 82, 170, 205));
        p.drawPolygon(poly);
        QPolygonF thalweg;
        for (const auto& n : b.nodes) thalweg << QPointF(n.x, n.y);
        p.setPen(QPen(QColor(1, 45, 110, 150), std::max(2.0, b.nodes[b.nodes.size() / 2].w * 0.32), Qt::SolidLine, Qt::RoundCap));
        p.drawPolyline(thalweg);
    }
    // bottom horizon strip
    QLinearGradient sg(0, 0, VW, 0);
    sg.setColorAt(0, QColor(15, 42, 78, 20));
    sg.setColorAt(0.65, QColor(18, 52, 94, 41));
    sg.setColorAt(1, QColor(215, 236, 252, 87));
    p.fillRect(0, VH - 44, VW, 44, sg);
    p.setPen(QColor(10, 56, 112, 31));
    p.drawLine(0, VH - 44, VW, VH - 44);
}

void SplashScreen::setStatus(const QString& text) {
    status_ = text;
    update();
}

void SplashScreen::setProgress(double pct) {
    target_ = std::clamp(pct, 0.0, 100.0);
    if (!animate_) { shown_ = target_; update(); }
}

void SplashScreen::pump() { QApplication::processEvents(QEventLoop::AllEvents, 30); }

void SplashScreen::tick() {
    double dt = clock_.restart() / 1000.0;
    if (dt > 0.1) dt = 0.1;
    t_ += dt;
    shown_ += (target_ - shown_) * std::min(1.0, dt * 10.0);
    update();
}

bool SplashScreen::showError(const QString& title, const QString& detail, const QString& continueLabel) {
    error_ = true;
    status_ = QStringLiteral("\u26D4 ") + title;
    errorDetail_ = detail;
    timer_.stop();
    setWindowOpacity(1.0);
    auto* bar = new QWidget(this);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(0, 0, 0, 0);
    auto* cont = new QPushButton(continueLabel, bar);
    auto* quit = new QPushButton(tr2("Keluar", "Quit"), bar);
    h->addWidget(cont);
    h->addWidget(quit);
    bar->adjustSize();
    bar->move(width() - bar->width() - static_cast<int>(28 * scale_), static_cast<int>((VH - 50) * scale_));
    bar->show();
    update();
    QEventLoop loop;
    bool result = false;
    connect(cont, &QPushButton::clicked, &loop, [&]() { result = true; loop.quit(); });
    connect(quit, &QPushButton::clicked, &loop, [&]() { result = false; loop.quit(); });
    loop.exec();
    bar->deleteLater();
    error_ = false;
    return result;
}

void SplashScreen::finish(QWidget* w) {
    setProgress(100);
    timer_.stop();
    if (w) {
        w->show();
        w->raise();
        w->activateWindow();
    }
    close();
}

void SplashScreen::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(scale_, scale_);
    p.drawImage(QPoint(0, 0), static_);

    // animated flow: streaks travelling downstream + particles (static frame when animations are off)
    for (const auto& b : animate_ ? rivers_ : QVector<Branch>{}) {
        const int lanes = b.main ? 3 : 2;
        for (int l = 0; l < lanes; ++l) {
            double off = lanes == 1 ? 0 : -0.5 + double(l) / (lanes - 1);
            QPainterPath path;
            for (int i = 0; i <= 120; ++i) {
                QPointF q = along(b, i / 120.0, off * 0.7);
                if (i == 0) path.moveTo(q); else path.lineTo(q);
            }
            QPen pen(QColor(214, 242, 255, b.main ? 120 : 90), b.main ? 1.6 : 1.1);
            pen.setDashPattern({6, 14});
            pen.setDashOffset(-t_ * 60 * b.speed * 10 - l * 7);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
        const int np = b.main ? 26 : 12;
        p.setPen(Qt::NoPen);
        for (int k = 0; k < np; ++k) {
            double u = std::fmod(k / double(np) + t_ * b.speed * 0.35, 1.0);
            double off = std::sin(k * 12.9898) * 0.6;
            QPointF q = along(b, u, off);
            double a = 0.55 + 0.45 * std::sin(t_ * 3 + k);
            p.setBrush(QColor(235, 250, 255, static_cast<int>(150 * a)));
            p.drawEllipse(q, 1.6, 1.6);
        }
    }

    // brand header
    const int hx = 44, hy = 40;
    QPainterPath clip;
    clip.addRoundedRect(QRectF(hx, hy, 64, 64), 14, 14);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(7, 33, 66, 60));
    p.drawRoundedRect(QRectF(hx + 1, hy + 6, 64, 64), 14, 14);
    p.setClipPath(clip);
    p.drawPixmap(hx, hy, icon_);
    p.restore();
    QFont fName(QStringLiteral("Segoe UI"));
    fName.setPixelSize(38);
    fName.setWeight(QFont::ExtraBold);
    p.setFont(fName);
    QFontMetrics fm(fName);
    const int base = hy + 46;
    int x = hx + 64 + 18;
    p.setPen(QColor("#072142"));
    p.drawText(x, base, QStringLiteral("De"));
    x += fm.horizontalAdvance(QStringLiteral("De"));
    p.setPen(QColor("#026aa7"));
    p.drawText(x, base, QStringLiteral("-"));
    x += fm.horizontalAdvance(QStringLiteral("-"));
    p.setPen(QColor("#072142"));
    p.drawText(x, base, QStringLiteral("RAS"));
    x += fm.horizontalAdvance(QStringLiteral("RAS")) + 10;
    QFont fVer = fName;
    fVer.setPixelSize(33);
    fVer.setWeight(QFont::Light);
    p.setFont(fVer);
    p.setPen(QColor("#164475"));
    const QString ver = QStringLiteral("v") + QStringLiteral(DERAS_VERSION);
    p.drawText(x, base, ver);
    x += QFontMetrics(fVer).horizontalAdvance(ver) + 12;
    QFont fBadge(QStringLiteral("Consolas"));
    fBadge.setStyleHint(QFont::Monospace);
    fBadge.setPixelSize(11);
    fBadge.setBold(true);
    fBadge.setLetterSpacing(QFont::PercentageSpacing, 114);
    const QString badge = QStringLiteral(DERAS_RELEASE_LABEL);
    QFontMetrics bm(fBadge);
    QRectF br(x, base - 26, bm.horizontalAdvance(badge) + 18, 20);
    p.setPen(QPen(QColor(7, 33, 66, 66), 1));
    p.setBrush(QColor(215, 238, 255, 184));
    p.drawRoundedRect(br, 10, 10);
    p.setFont(fBadge);
    p.setPen(QColor("#072142"));
    p.drawText(br, Qt::AlignCenter, badge);
    QFont fTag(QStringLiteral("Segoe UI"));
    fTag.setPixelSize(14);
    p.setFont(fTag);
    p.setPen(QColor(7, 33, 66, 200));
    p.drawText(hx + 64 + 18, base + 24, tr2("Hidraulika sungai dan banjir • kompatibel dengan format file HEC-RAS",
                                            "River and flood hydraulics • compatible with the HEC-RAS file format"));

    // publisher mark (bottom right of the main area)
    QFont fPub(QStringLiteral("Segoe UI"));
    fPub.setPixelSize(13);
    fPub.setBold(true);
    fPub.setLetterSpacing(QFont::PercentageSpacing, 108);
    p.setFont(fPub);
    const QString pub = QStringLiteral("De-RAS");
    int pw = QFontMetrics(fPub).horizontalAdvance(pub);
    int px = VW - 28 - pw;
    p.setPen(QColor("#072142"));
    p.drawText(px, VH - 16, pub);
    p.drawPixmap(px - 8 - 24, VH - 34, iconSmall_);

    // status bar
    QRect sb(0, VH, VW, SB);
    QLinearGradient sg(0, VH, 0, VH + SB);
    sg.setColorAt(0, QColor("#162233"));
    sg.setColorAt(1, QColor("#121c29"));
    p.fillRect(sb, sg);
    p.setPen(QColor("#24354d"));
    p.drawLine(0, VH, VW, VH);
    QFont fs(QStringLiteral("Segoe UI"));
    fs.setPixelSize(13);
    p.setFont(fs);
    p.setPen(error_ ? QColor("#ff8f8f") : QColor("#d6e4f5"));
    const int trackW = 370;
    QRect textRect(28, VH, error_ ? VW - 56 : VW - 28 - trackW - 52, SB);
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(fs).elidedText(status_, Qt::ElideRight, textRect.width()));
    if (error_) {
        QRectF er(28, VH - 150, VW - 56, 92);
        p.setPen(QPen(QColor("#D64545"), 1.5));
        p.setBrush(QColor(18, 28, 41, 235));
        p.drawRoundedRect(er, 8, 8);
        p.setPen(QColor("#e9f1fb"));
        p.drawText(er.adjusted(14, 10, -14, -10), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, errorDetail_);
        return;
    }
    QRectF track(VW - 28 - trackW, VH + SB / 2.0 - 5, trackW, 10);
    p.setPen(QPen(QColor(255, 255, 255, 20), 1));
    p.setBrush(QColor("#080d14"));
    p.drawRoundedRect(track, 5, 5);
    double fillW = (trackW - 4) * shown_ / 100.0;
    if (fillW > 2) {
        QRectF fill(track.left() + 2, track.top() + 2, fillW, 6);
        QLinearGradient fg(fill.left(), 0, fill.right(), 0);
        fg.setColorAt(0, QColor("#0284c7"));
        fg.setColorAt(0.55, QColor("#38bdf8"));
        fg.setColorAt(0.88, QColor("#bae6fd"));
        fg.setColorAt(1, QColor("#ffffff"));
        // glow
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(56, 189, 248, 60));
        p.drawRoundedRect(fill.adjusted(-2, -2, 2, 2), 5, 5);
        p.setBrush(fg);
        p.drawRoundedRect(fill, 3, 3);
        // travelling shimmer inside the filled part
        double sx = fill.left() + std::fmod(t_ * 180.0, std::max(30.0, fillW + 40)) - 30;
        QLinearGradient sh(sx, 0, sx + 30, 0);
        sh.setColorAt(0, QColor(255, 255, 255, 0));
        sh.setColorAt(0.5, QColor(255, 255, 255, 170));
        sh.setColorAt(1, QColor(255, 255, 255, 0));
        p.save();
        QPainterPath fp;
        fp.addRoundedRect(fill, 3, 3);
        p.setClipPath(fp);
        p.fillRect(QRectF(sx, fill.top(), 30, fill.height()), sh);
        p.restore();
    }
}

}  // namespace deras::ui

// De-RAS - ui/SplashScreen.h
// Animated start-up screen (frameless, centred). Design follows the
// reference supplied by the user (de_ras_v1_0_splash_screen.html): blue
// terrain background with DEM contours, animated river network, brand
// header with the official De-RAS icon (unaltered PNG from branding/),
// dark status bar with changing status text and a glowing progress capsule.
// Status and progress are driven by the real start-up steps in main.cpp.
#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace deras::ui {

class SplashScreen : public QWidget {
    Q_OBJECT
public:
    SplashScreen();
    void setStatus(const QString& text);
    void setProgress(double percent);          // target value 0..100 (animated)
    // Lets the animation repaint between real start-up steps (no artificial delay).
    void pump();
    // Shows an actionable start-up error in the status bar and waits for the
    // user: returns true for "continue anyway", false for "quit".
    bool showError(const QString& title, const QString& detail, const QString& continueLabel);
    void finish(QWidget* mainWindow);          // close as soon as the main window is ready
    static bool animationsEnabled();          // honours the Windows "show animations" setting

protected:
    void paintEvent(QPaintEvent*) override;

private:
    struct Node { double x, y, w, nx, ny; };
    struct Branch { QVector<Node> nodes; double speed; bool main; };
    void buildRivers();
    void buildStatic();
    void tick();
    QPointF along(const Branch& b, double u, double offset) const;

    QVector<Branch> rivers_;
    QImage static_;
    QPixmap icon_, iconSmall_;
    QString status_;
    bool error_ = false;
    QString errorDetail_;
    bool animate_ = true;
    double target_ = 0, shown_ = 0;
    double t_ = 0;
    double scale_ = 1.0;
    QTimer timer_;
    QElapsedTimer clock_;
};

}  // namespace deras::ui

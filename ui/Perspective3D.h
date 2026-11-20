// De-RAS - ui/Perspective3D.h
// X-Y-Z perspective plot of one reach (Results → 3D perspective): ground
// lines of every cross section placed along the reach by the main-channel
// lengths, and the water surface of one profile as a surface between the
// sections. Drag to rotate, wheel to change the vertical exaggeration.
#pragma once

#include <QWidget>
#include <vector>

#include "core/Model.h"
#include "core/Results.h"

namespace deras::ui {

class Perspective3D : public QWidget {
    Q_OBJECT
public:
    explicit Perspective3D(QWidget* parent = nullptr);
    // reach == nullptr clears the plot; ws holds one water surface per Type-1 section (NaN = none)
    void setData(const Reach* reach, const std::vector<double>& ws, const QString& title);
    void setEmptyText(const QString& t) { empty_ = t; sections_.clear(); update(); }
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    struct Sec {
        QString rs;
        std::vector<QPointF> pts;  // (offset from channel centre, elevation)
        double y = 0;              // distance from the downstream section
        double ws = 0;
        bool hasWs = false;
        double wl = 0, wr = 0;     // water edges (offsets)
    };
    std::vector<Sec> sections_;
    QString title_, empty_;
    double az_ = -50, tilt_ = 32, vex_ = 5;
    double zmin_ = 0;
    QPointF last_;
};

}  // namespace deras::ui

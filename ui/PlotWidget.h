// De-RAS - ui/PlotWidget.h
// Lightweight engineering plot (QPainter): line series, optional fill to a
// baseline series, point markers, nice axis ticks, legend and axis titles
// with units. Used for cross-section and longitudinal profile plots.
#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace deras::ui {

struct PlotSeries {
    QString name;
    QVector<QPointF> pts;
    QColor color;
    double width = 2.0;
    Qt::PenStyle style = Qt::SolidLine;
    bool markers = false;
    int fillBelowSeries = -1;   // index of a series to fill down to (e.g. water above ground)
    QColor fillColor;
    bool legend = true;
};

struct PlotMarker {          // vertical marker (e.g. bank station)
    double x;
    QString label;
    QColor color;
};

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);
    void setTitles(const QString& title, const QString& xTitle, const QString& yTitle);
    void setSeries(const QVector<PlotSeries>& s);
    void setMarkers(const QVector<PlotMarker>& m) { markers_ = m; update(); }
    void setEmptyText(const QString& t) { empty_ = t; update(); }
    void setReverseX(bool r) { reverseX_ = r; update(); }
    void clear();
    QSize sizeHint() const override { return {520, 300}; }
    QSize minimumSizeHint() const override { return {240, 160}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;

private:
    QVector<PlotSeries> series_;
    QVector<PlotMarker> markers_;
    QString title_, xTitle_, yTitle_, empty_;
    bool reverseX_ = false;
    QPointF hover_;
    bool hasHover_ = false;
};

QVector<double> niceTicks(double lo, double hi, int target);

}  // namespace deras::ui

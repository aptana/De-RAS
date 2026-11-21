// De-RAS - ui/GeometryView.h
// Data Geometri: editor palette on the left (as in the HEC-RAS Geometric
// Data window), drawing tools on top, the schematic canvas in the middle
// and the selected-object panel on the right — all inside the main window.
// Choosing "Penampang" opens the cross-section editor as a sub-view.
#pragma once

#include <QPointF>
#include <QVector>

#include "View.h"

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace deras::ui {

class Card;
class ToolbarButton;

// Schematic / map canvas of the current geometry (reach centrelines from
// the file, cross-section ticks, structures, storage / 2D areas, connections
// and BC lines). It is a schematic; the georeferenced map is RAS Mapper.
class GeometryCanvas : public QWidget {
    Q_OBJECT
public:
    explicit GeometryCanvas(ProjectSession* s, QWidget* parent = nullptr);
    void setDrawMode(bool on);                 // reach
    // M6 shapes: 1 storage area polygon, 2 2D area polygon, 3 connection line, 4 BC line (0 = off)
    void setDrawShape(int kind);
    int drawShape() const { return drawing_ ? drawKind_ : 0; }
    bool drawMode() const { return drawing_; }
    void resetView();
signals:
    void reachDrawn(const QVector<QPointF>& worldPts);  // upstream -> downstream
    void reachSelected(int reachUid);
    void crossSectionActivated(int reachUid, int xsUid);  // double-click
    void structureActivated(int reachUid, int kind);  // double-click on a structure: 0 bridge/culvert, 1 inline, 2 lateral
    void drawModeChanged(bool on);
    void shapeDrawn(int kind, const QVector<QPointF>& worldPts);
    void areaActivated(const QString& name);       // double-click inside a storage / 2D area
    void lineActivated(int kind, const QString& name);  // 3 connection, 4 BC line
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
private:
    struct Tick { int reachUid, xsUid; QPointF at, normal; QString label; bool structure; QString kind; };
    void rebuild();
    QTransform toScreen() const;
    const Tick* hitTick(const QPointF& pos) const;
    int hitReach(const QPointF& pos) const;
    void finishDraft();
    int drawKind_ = 0;
    ProjectSession* s_;
    QVector<QVector<QPointF>> lines_;
    QVector<int> lineUids_;
    QStringList lineNames_;
    QVector<Tick> ticks_;
    QRectF bounds_;
    bool synthetic_ = false;
    double zoom_ = 1;
    QPointF pan_, lastMouse_, pressPos_;
    QPointF hover_{-1, -1};
    bool panning_ = false;
    bool drawing_ = false;
    QVector<QPointF> draft_;  // world coordinates of the reach being drawn
    int selReach_ = 0;
};

class GeometryView : public View {
    Q_OBJECT
public:
    explicit GeometryView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("geometry"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.geometry"); }
    void reload() override;
    void activated() override;
protected:
    QWidget* createActions() override;
private:
    void fillFiles();
    void showSelection();
    void onReachDrawn(const QVector<QPointF>& pts);
    void reachSheet();
    void junctionSheet();
    bool editJunction(Junction& j, bool isNew);
    void lengthsTable();
    void newGeometryFile();
    void palette(const QString& id);
    void onShapeDrawn(int kind, const QVector<QPointF>& pts);
    QList<QPushButton*> shapeBtns_;
    QComboBox* file_;
    GeometryCanvas* canvas_;
    QPushButton* drawBtn_;
    Card* props_;
    QVBoxLayout* propsBody_;
    QLabel* empty_;
    int selReach_ = 0;
    bool loading_ = false;
};

}  // namespace deras::ui

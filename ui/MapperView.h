// De-RAS - ui/MapperView.h
// RAS Mapper (M3, Preview): projection, GeoTIFF terrains, georeferenced
// background pictures, reach centrelines and cross-section cut lines on a
// map, new cross sections cut from the terrain, and steady inundation maps
// (depth and water surface) with GeoTIFF/PNG export.
#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QWidget>
#include <vector>

#include "View.h"
#include "engines/mapping/Mapping.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;

namespace deras::ui {

class Card;

struct MapImage {
    QImage img;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // map extent (left, top, right, bottom)
};

class MapCanvas : public QWidget {
    Q_OBJECT
public:
    explicit MapCanvas(QWidget* parent = nullptr);
    void setGeometry(const Geometry* g) { geo_ = g; update(); }
    void setSelection(int reachUid, int xsUid) { selReach_ = reachUid; selXs_ = xsUid; update(); }
    void setTerrainImages(QList<MapImage> imgs) { terrain_ = std::move(imgs); update(); }
    void setPictures(QList<MapImage> imgs) { pictures_ = std::move(imgs); update(); }
    void setMapImage(const MapImage& m, bool show) { map_ = m; showMap_ = show; update(); }
    void setShowMap(bool on) { showMap_ = on; update(); }
    void setShowGeometry(bool on) { showGeo_ = on; update(); }
    void setCutMode(bool on);
    bool cutMode() const { return cutMode_; }
    void fit();
    bool hasContent() const;
    // map <-> screen
    QPointF toScreen(double x, double y) const;
    mapping::Pt toMap(const QPointF& p) const;
signals:
    void cutFinished(const deras::mapping::Line& line);
    void cutModeChanged(bool on);
    void xsClicked(int reachUid, int xsUid);
    void hovered(double x, double y, bool inside);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    void extent(double& x0, double& y0, double& x1, double& y1) const;
    void drawImage(QPainter& p, const MapImage& m, double opacity);
    bool pickXs(const QPointF& pos, int& reachUid, int& xsUid) const;
    const Geometry* geo_ = nullptr;
    QList<MapImage> terrain_, pictures_;
    MapImage map_;
    bool showMap_ = true, showGeo_ = true;
    int selReach_ = 0, selXs_ = 0;
    double scale_ = 1, cx_ = 0, cy_ = 0;  // pixels per map unit, map centre
    bool fitted_ = false;
    bool cutMode_ = false;
    mapping::Line cut_;
    QPointF mouse_;
    bool panning_ = false, moved_ = false;
    QPointF panFrom_;
};

class MapperView : public View {
    Q_OBJECT
public:
    explicit MapperView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("mapper"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("view.mapper"); }
    QString status() const override { return QStringLiteral("preview"); }
    void reload() override;
    void activated() override;
    // GIS menu entries
    void projectionSheet();
    void addTerrain();
    void computeMap();
    // 2D area result map of the current plan (first area, current variable)
    void show2DResults() { fill2D(); show2D(); }
protected:
    QWidget* createActions() override;
private:
    void loadLayers(bool force);
    void fillLists();
    void fillProfiles();
    void fill2D();
    void show2D();
    void updateCrsInfo();
    void removeTerrain();
    void addPicture();
    void removePicture();
    void clearMap();
    void exportRaster(bool depth);
    void exportPng();
    void onCut(const mapping::Line& line);
    void refreshFromTerrain();
    void updateSelectionTools();
    QString relativeOrAbsolute(const QString& path) const;
    QString absolutePath(const QString& stored) const;

    MapCanvas* canvas_;
    QLabel* crsLabel_;
    QLabel* crsWarn_;
    QListWidget* terrainList_;
    QListWidget* pictureList_;
    QLabel* geoLabel_;
    QCheckBox* showGeo_;
    QComboBox* profile_;
    QLabel* planLabel_;
    QLabel* mapStats_;
    QCheckBox* showMap_;
    QPushButton* computeBtn_;
    QComboBox* area2d_;
    QComboBox* var2d_;
    QSlider* time2d_;
    QLabel* time2dLabel_;
    QPushButton* exportDepth_;
    QPushButton* exportWse_;
    QPushButton* cutBtn_;
    QPushButton* refreshBtn_;
    QLabel* coord_;

    std::vector<io::Raster> terrains_;
    QStringList terrainNames_, terrainSources_;  // display name, absolute path
    QStringList loadedKey_;                        // what terrains_ was loaded from
    QStringList pictureKey_;
    mapping::MapResult map_;
    bool hasMap_ = false;
    QString mapLabel_;  // "p01 · Q150"
};

// Colour images of rasters (shared with the snapshot tool).
QImage terrainImage(const io::Raster& r, double zmin, double zmax, int maxSide = 2048);
QImage depthImage(const io::Raster& depth, double maxDepth);

}  // namespace deras::ui

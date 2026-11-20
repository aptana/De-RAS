// De-RAS - ui/Icons.h
// Simple line icons drawn with QPainter on a 24×24 grid (no image files,
// no third-party icon set). Every toolbar button also carries a short text
// label, so an icon never has to be guessed.
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QRectF>
#include <QString>

class QPainter;

namespace deras::ui {

// Names: folder, save, geometry, steady, unsteady, quasi, sediment, water,
// run, run-unsteady, run-sediment, run-water, design, mapper, xs, profile,
// genprofile, rating, cube, hydrograph, table-detail, table-grid, warning,
// timeseries, lock, info, back, forward, chevron-right, chevron-down, arrow-right,
// close, check, junction, bridge, inline, lateral, storage, grid2d, connect,
// pump, htab, picture, draw, line, breakline, region, pan, zoom, measure,
// identify, plus, copy, trash, import, export, doctor, play, stop, home,
// options, help, layers, dot, circle, search, file, description, table.
void paintIcon(QPainter* p, const QRectF& r, const QString& name, const QColor& color, double stroke = 1.7);
QPixmap iconPixmap(const QString& name, int px, const QColor& color, double dpr = 2.0);
QIcon lineIcon(const QString& name, const QColor& color);

}  // namespace deras::ui

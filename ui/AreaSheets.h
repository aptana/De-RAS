// De-RAS - ui/AreaSheets.h
// Editors of M6 (Preview): storage areas, 2D flow areas, storage-area / 2D
// connections, 2D boundary-condition lines and pump stations. Opened from
// the Data Geometri palette and after drawing a shape on the canvas.
#pragma once

#include <QString>
#include <vector>

#include "core/Model.h"

class QWidget;

namespace deras::ui {

class ProjectSession;

// List of storage areas (twoD = false) or 2D flow areas with Add / Edit / Delete.
void areasSheet(QWidget* host, ProjectSession* s, bool twoD, const QString& select = {});
// A polygon drawn on the canvas becomes a new area (its editor opens).
void addDrawnArea(QWidget* host, ProjectSession* s, bool twoD, const std::vector<XY>& polygon);
// Opens the editor of an existing area by name (double-click on the canvas).
void editAreaByName(QWidget* host, ProjectSession* s, const QString& name);

void connectionsSheet(QWidget* host, ProjectSession* s);
void addDrawnConnection(QWidget* host, ProjectSession* s, const std::vector<XY>& line);

void bcLinesSheet(QWidget* host, ProjectSession* s);
void addDrawnBcLine(QWidget* host, ProjectSession* s, const std::vector<XY>& line);

void pumpsSheet(QWidget* host, ProjectSession* s);

}  // namespace deras::ui

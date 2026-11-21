// De-RAS - ui/StructureSheets.h
// Editors for hydraulic structures (M4, Preview): bridges, culverts and
// inline structures of a reach, and lateral structures (M6), opened from
// the Data Geometri palette.
// The data follow the HEC-RAS Bridge/Culvert and Inline Structure editors;
// the file format of these blocks is not yet verified against HEC-RAS.
#pragma once

class QWidget;

namespace deras::ui {

class ProjectSession;

// List of the structures of one kind in a reach with Add / Edit / Delete.
// kind 0: bridges and culverts; 1: inline structures; 2: lateral structures (M6).
void structuresSheet(QWidget* host, ProjectSession* s, int reachUid, int kind);

}  // namespace deras::ui

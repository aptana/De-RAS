// De-RAS - ui/ProjectSheets.h
// "Proyek baru" as one in-window sheet (replaces the 0.1.0 four-page
// wizard): name and folder, units / coordinates / time zone, river and
// reach, and the template (empty or prismatic channel).
#pragma once

#include <QString>
#include <QWidget>

namespace deras::ui {

struct NewProjectSpec {
    QString folder, baseName, title;
    bool si = true;
    QString crs, timeZone;
    QString river, reach;
    bool prismatic = true;
    // prismatic template parameters (project units)
    int count = 11;
    double spacing = 100, bottomWidth = 10, sideSlope = 2, bankHeight = 4, bedSlope = 0.001, invert = 100;
    double nChannel = 0.03, nOverbank = 0.06;
};

// Returns false when cancelled.
bool newProjectSheet(QWidget* win, NewProjectSpec& spec);

// Edit the project description (.prj "BEGIN DESCRIPTION"). Returns false when cancelled.
bool descriptionSheet(QWidget* win, QString& text);

}  // namespace deras::ui

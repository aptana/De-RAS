// De-RAS - ui/Dialogs.h
// First-run language choice, About and the feature matrix. Since 0.2.0 they
// are sheets inside the main window, not separate dialog windows.
#pragma once

#include <QVector>
#include <QWidget>

namespace deras::ui {

// Shown once on first start over the main window. Returns true when the
// chosen language differs from the start-up language (window rebuild needed).
bool firstRunLanguageSheet(QWidget* win);

void showAboutSheet(QWidget* win);
void showFeatureMatrixSheet(QWidget* win);

// Feature status shown in the matrix. status: done | preview | partial | unavailable.
struct FeatureRow { const char* areaId; const char* areaEn; const char* id; const char* en; const char* status; const char* noteId; const char* noteEn; };
const QVector<FeatureRow>& featureRows();

}  // namespace deras::ui

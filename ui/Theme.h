// De-RAS - ui/Theme.h
// Design tokens for the 0.2.0 UI, in one place (BASE - PROMPT.md, "Gaya
// visual"). Two themes: Dark (default) and Light (light blue). The
// accent colours come from the De-RAS logo. Every widget reads colours from
// Theme::current(); the application style sheet is generated from the same
// tokens, so changing a value here changes it everywhere.
#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace deras::ui {

enum class ThemeName { Dark, Light };

struct Theme {
    ThemeName name = ThemeName::Dark;
    bool dark = true;
    // surfaces
    QColor base;          // window base
    QColor workA, workB;  // work-area radial gradient (centre -> edge)
    QColor cardA, cardB;  // card / panel gradient (top -> bottom)
    QColor bar;           // title strip, toolbar capsule, status bar
    QColor input;         // text fields, combo boxes
    QColor hover;         // hover overlay
    QColor selected;      // selected row / active button fill
    QColor line, lineStrong, focus;
    // text
    QColor text, text2, text3;
    QColor heading;       // "navy" role: titles, active icons
    // actions and accents
    QColor primary, primaryHover, onPrimary;
    QColor accent, accent2;
    QColor water;         // water in plots and maps
    QColor ground;        // ground line in plots
    // status (always shown with an icon or text, never colour only)
    QColor success, warning, error;
    // geometry
    int radiusCard = 14, radiusControl = 10, radiusSmall = 6;
    int basePx = 14;

    static const Theme& current();
    // Applies palette, fonts and the application style sheet.
    static void apply(ThemeName t, int basePx);
    static void loadFonts();  // bundled Inter and JetBrains Mono (SIL OFL)
    static QString uiFamily();
    static QString monoFamily();
};

QString cssColor(const QColor& c);       // rgba(...) for style sheets
QColor withAlpha(QColor c, double a);    // a in 0..1
QFont monoFont(int px = 0);              // JetBrains Mono (fallback Consolas)
QFont uiFont(int px = 0, int weight = QFont::Normal);

}  // namespace deras::ui

// De-RAS - ui/Common.h
// Shared UI helpers: bilingual text and persistent application settings.
// Design tokens (colours, radii, fonts) live in Theme.h.
#pragma once

#include <QIcon>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "core/Messages.h"
#include "Theme.h"

namespace deras::ui {

// ---------------- language ----------------
Language currentLanguage();
void setCurrentLanguage(Language l);
inline bool isID() { return currentLanguage() == Language::ID; }
// Every visible string is written in both languages at the call site.
inline QString tr2(const char* id, const char* en) { return QString::fromUtf8(isID() ? id : en); }
QString msgText(const Message& m);  // core message in the current language
QString qs(const std::string& s);   // UTF-8 std::string -> QString
std::string ss(const QString& s);   // QString -> UTF-8 std::string

// ---------------- settings ----------------
struct AppSettings {
    static bool firstRunDone();
    static void setFirstRunDone();
    static Language language();
    static void setLanguage(Language l);
    // "dark" (default since 0.2.0) or "light"; the 0.2.0 values "malam"/"siang" are still read
    static ThemeName theme();
    static void setTheme(ThemeName t);
    static int baseFontPx();
    static void setBaseFontPx(int px);
    static QStringList recentProjects();
    static void addRecentProject(const QString& prjPath);
    static void removeRecentProject(const QString& prjPath);
    // Autosave interval in minutes (M8); 0 = off. Default 5.
    static int autosaveMinutes();
    static void setAutosaveMinutes(int m);
};

QIcon appIcon();                     // multi-size icon from branding/icons (no runtime downscaling)
QString severityPrefix(Severity s);  // text marker + label (status never by colour only)

}  // namespace deras::ui

// De-RAS - ui/Common.cpp
#include "Common.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace deras::ui {

static Language g_lang = Language::EN;  // English is the default since 0.2.1 (review R7)

Language currentLanguage() { return g_lang; }
void setCurrentLanguage(Language l) { g_lang = l; }

QString msgText(const Message& m) { return QString::fromStdString(m.text(g_lang)); }
QString qs(const std::string& s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
std::string ss(const QString& s) { return s.toUtf8().toStdString(); }

static QSettings settings() { return QSettings(QSettings::defaultFormat(), QSettings::UserScope, QStringLiteral("De-RAS"), QStringLiteral("De-RAS")); }

bool AppSettings::firstRunDone() { return settings().value("firstRunDone", false).toBool(); }
void AppSettings::setFirstRunDone() { settings().setValue("firstRunDone", true); }
Language AppSettings::language() { return settings().value("language", "en").toString() == "id" ? Language::ID : Language::EN; }
void AppSettings::setLanguage(Language l) { settings().setValue("language", l == Language::EN ? "en" : "id"); }
// Key "theme" since 0.2.0 (the 0.1.0 light default does not carry over): dark is the default.
// 0.2.0 stored "malam"/"siang"; those values are still read (review R6 of build 0.2.0).
ThemeName AppSettings::theme() {
    const QString v = settings().value("theme", "dark").toString();
    return (v == "light" || v == "siang") ? ThemeName::Light : ThemeName::Dark;
}
void AppSettings::setTheme(ThemeName t) { settings().setValue("theme", t == ThemeName::Light ? "light" : "dark"); }
int AppSettings::autosaveMinutes() { return std::clamp(settings().value("autosaveMinutes", 5).toInt(), 0, 120); }
void AppSettings::setAutosaveMinutes(int m) { settings().setValue("autosaveMinutes", std::clamp(m, 0, 120)); }
int AppSettings::baseFontPx() { return settings().value("baseFontPx", 14).toInt(); }
void AppSettings::setBaseFontPx(int px) { settings().setValue("baseFontPx", px); }
// One entry per file: C:/a/b.prj, C:\a\b.prj and c:\A\B.prj are the same
// file on Windows (0.2.0 listed the same project twice).
static QString recentKey(const QString& p) {
    const QString c = QDir::cleanPath(QFileInfo(p).absoluteFilePath());
#ifdef Q_OS_WIN
    return c.toLower();
#else
    return c;
#endif
}
static void removeSame(QStringList& l, const QString& p) {
    const QString k = recentKey(p);
    l.erase(std::remove_if(l.begin(), l.end(), [&](const QString& e) { return recentKey(e) == k; }), l.end());
}
QStringList AppSettings::recentProjects() {
    QStringList out;
    for (const QString& p : settings().value("recentProjects").toStringList()) {
        QStringList tmp = out;
        removeSame(tmp, p);
        if (tmp.size() == out.size()) out << QDir::toNativeSeparators(QDir::cleanPath(p));
    }
    return out;
}
void AppSettings::addRecentProject(const QString& p) {
    QStringList l = recentProjects();
    removeSame(l, p);
    l.prepend(QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(p).absoluteFilePath())));
    while (l.size() > 10) l.removeLast();
    settings().setValue("recentProjects", l);
}
void AppSettings::removeRecentProject(const QString& p) {
    QStringList l = recentProjects();
    removeSame(l, p);
    settings().setValue("recentProjects", l);
}

QIcon appIcon() {
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) ic.addFile(QStringLiteral(":/branding/De-RAS-%1.png").arg(s), QSize(s, s));
    return ic;
}

QString severityPrefix(Severity s) {
    switch (s) {
        case Severity::Error: return QStringLiteral("⛔ ") + qs(severityLabel(s, currentLanguage()));
        case Severity::Warning: return QStringLiteral("⚠ ") + qs(severityLabel(s, currentLanguage()));
        default: return QStringLiteral("ℹ ") + qs(severityLabel(s, currentLanguage()));
    }
}

}  // namespace deras::ui

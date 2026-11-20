// De-RAS - ui/Theme.cpp
#include "Theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>

namespace deras::ui {

QString cssColor(const QColor& c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(QString::number(c.alphaF(), 'f', 3));
}

QColor withAlpha(QColor c, double a) {
    c.setAlphaF(static_cast<float>(a));
    return c;
}

static QString g_ui = QStringLiteral("Segoe UI");
static QString g_mono = QStringLiteral("Consolas");

void Theme::loadFonts() {
    static bool done = false;
    if (done) return;
    done = true;
    bool inter = false, mono = false;
    for (const char* f : {":/fonts/Inter-Regular.otf", ":/fonts/Inter-Medium.otf", ":/fonts/Inter-SemiBold.otf", ":/fonts/Inter-Bold.otf"})
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(f)) >= 0) inter = true;
    for (const char* f : {":/fonts/JetBrainsMono-Regular.ttf", ":/fonts/JetBrainsMono-Medium.ttf"})
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(f)) >= 0) mono = true;
    if (inter) g_ui = QStringLiteral("Inter");
    else if (QFontDatabase::families().contains(QStringLiteral("Segoe UI Variable Text"))) g_ui = QStringLiteral("Segoe UI Variable Text");
    if (mono) g_mono = QStringLiteral("JetBrains Mono");
}

QString Theme::uiFamily() { return g_ui; }
QString Theme::monoFamily() { return g_mono; }

QFont monoFont(int px) {
    QFont f(g_mono);
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px > 0 ? px : Theme::current().basePx - 1);
    return f;
}

QFont uiFont(int px, int weight) {
    QFont f(g_ui);
    f.setPixelSize(px > 0 ? px : Theme::current().basePx);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

static Theme make(ThemeName n, int px) {
    Theme t;
    t.name = n;
    t.basePx = px;
    if (n == ThemeName::Dark) {
        t.dark = true;
        t.base = QColor("#060B14");
        t.workA = QColor("#0B1526");
        t.workB = QColor("#060B14");
        t.cardA = QColor("#0D182C");
        t.cardB = QColor("#080F1D");
        t.bar = withAlpha(QColor("#060B14"), 0.86);
        t.input = withAlpha(QColor("#060B14"), 0.72);
        t.hover = withAlpha(QColor("#03A4FC"), 0.10);
        t.selected = withAlpha(QColor("#03A4FC"), 0.18);
        t.line = withAlpha(QColor("#03A4FC"), 0.15);
        t.lineStrong = withAlpha(QColor("#03A4FC"), 0.30);
        t.focus = withAlpha(QColor("#03A4FC"), 0.55);
        t.text = QColor("#E2E8F0");
        t.text2 = QColor("#94A3B8");
        t.text3 = QColor("#64748B");
        t.heading = QColor("#6BD4FC");
        t.primary = QColor("#0077E8");
        t.primaryHover = QColor("#03A4FC");
        t.onPrimary = QColor("#FFFFFF");
        t.accent = QColor("#03A4FC");
        t.accent2 = QColor("#6BD4FC");
        t.water = QColor("#03A4FC");
        t.ground = QColor("#E2E8F0");
        t.success = QColor("#3DBE8B");
        t.warning = QColor("#F2B84B");
        t.error = QColor("#F06A6A");
    } else {
        t.dark = false;
        // Light Theme: light blue, a light version of the dark theme's navy
        // (BASE - PROMPT.md token table, review R9 of build 0.2.0).
        t.base = QColor("#E6EFFA");
        t.workA = QColor("#F3F8FE");
        t.workB = QColor("#DCE8F7");
        t.cardA = QColor("#F7FAFF");
        t.cardB = QColor("#EAF2FC");
        t.bar = withAlpha(QColor("#E6EFFA"), 0.90);
        t.input = QColor("#FBFDFF");
        t.hover = withAlpha(QColor("#0151B7"), 0.07);
        t.selected = withAlpha(QColor("#0151B7"), 0.14);
        t.line = withAlpha(QColor("#01215F"), 0.14);
        t.lineStrong = withAlpha(QColor("#01215F"), 0.26);
        t.focus = QColor("#0151B7");
        t.text = QColor("#0B1F3A");
        t.text2 = QColor("#3D5675");
        t.text3 = QColor("#6A819E");
        t.heading = QColor("#01215F");
        t.primary = QColor("#0151B7");
        t.primaryHover = QColor("#01378A");
        t.onPrimary = QColor("#FFFFFF");
        t.accent = QColor("#0077E8");
        t.accent2 = QColor("#03A4FC");
        t.water = QColor("#0077E8");
        t.ground = QColor("#0B1F3A");
        t.success = QColor("#17704B");
        t.warning = QColor("#8A6100");
        t.error = QColor("#B83232");
    }
    return t;
}

static Theme g_theme = make(ThemeName::Dark, 14);
const Theme& Theme::current() { return g_theme; }

void Theme::apply(ThemeName n, int px) {
    loadFonts();
    px = std::clamp(px, 12, 20);
    g_theme = make(n, px);
    const Theme& t = g_theme;
    QPalette pal;
    const QColor solidCard = t.dark ? QColor("#0B1424") : QColor("#F7FAFF");
    pal.setColor(QPalette::Window, t.base);
    pal.setColor(QPalette::Base, solidCard);
    pal.setColor(QPalette::AlternateBase, t.dark ? QColor("#0E1A30") : QColor("#EEF4FC"));
    pal.setColor(QPalette::Text, t.text);
    pal.setColor(QPalette::WindowText, t.text);
    pal.setColor(QPalette::ButtonText, t.text);
    pal.setColor(QPalette::Button, solidCard);
    pal.setColor(QPalette::Highlight, t.primary);
    pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    pal.setColor(QPalette::ToolTipBase, solidCard);
    pal.setColor(QPalette::ToolTipText, t.text);
    pal.setColor(QPalette::PlaceholderText, t.text3);
    pal.setColor(QPalette::Link, t.accent);
    pal.setColor(QPalette::Disabled, QPalette::Text, t.text3);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, t.text3);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, t.text3);
    qApp->setPalette(pal);
    QFont f(g_ui);
    f.setPixelSize(px);
    qApp->setFont(f);

    const QString TX = cssColor(t.text), T2 = cssColor(t.text2), T3 = cssColor(t.text3), HD = cssColor(t.heading),
                  LN = cssColor(t.line), LS = cssColor(t.lineStrong), FC = cssColor(t.focus), IN = cssColor(t.input),
                  HV = cssColor(t.hover), SE = cssColor(t.selected), PR = cssColor(t.primary), PH = cssColor(t.primaryHover),
                  AC = cssColor(t.accent), SC = cssColor(solidCard), WA = cssColor(t.warning), ER = cssColor(t.error),
                  OK = cssColor(t.success), BASE = cssColor(t.base);
    const QString UI = g_ui, MONO = g_mono;
    QString q;
    q += QStringLiteral("* { font-family: '%1','Segoe UI Variable Text','Segoe UI',sans-serif; font-size: %2px; color: %3; }").arg(UI).arg(px).arg(TX);
    q += QStringLiteral("QMainWindow { background: %1; }").arg(BASE);
    q += QStringLiteral("QToolTip { background: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 6px 8px; }").arg(SC, TX, LS);
    // typography roles
    q += QStringLiteral("QLabel#Caption { color: %1; font-size: %2px; font-weight: 700; letter-spacing: 1.6px; }").arg(HD).arg(px - 3);
    q += QStringLiteral("QLabel#ViewTitle { color: %1; font-size: %2px; font-weight: 600; }").arg(TX).arg(px + 4);
    q += QStringLiteral("QLabel#CardTitle { color: %1; font-size: %2px; font-weight: 600; }").arg(TX).arg(px + 2);
    q += QStringLiteral("QLabel#Crumb { color: %1; font-size: %2px; }").arg(T2).arg(px - 1);
    q += QStringLiteral("QLabel#Muted { color: %1; }").arg(T2);
    q += QStringLiteral("QLabel#Dim { color: %1; }").arg(T3);
    q += QStringLiteral("QLabel#Mono, QLineEdit#Mono { font-family: '%1','Consolas',monospace; font-size: %2px; }").arg(MONO).arg(px - 1);
    q += QStringLiteral("QLabel#Strong { font-weight: 600; }");
    q += QStringLiteral("QLabel#Hint { color: %1; border: 1px dashed %2; border-radius: 10px; padding: 10px 12px; background: %3; }").arg(T2, LS, HV);
    q += QStringLiteral("QLabel#Banner { color: %1; border: 1px solid %2; border-radius: 10px; padding: 8px 12px; background: %3; }")
             .arg(t.dark ? "#F8E3B5" : "#5C4300", cssColor(withAlpha(t.warning, 0.55)), cssColor(withAlpha(t.warning, t.dark ? 0.12 : 0.14)));
    q += QStringLiteral("QLabel#ErrorBanner { color: %1; border: 1px solid %2; border-radius: 10px; padding: 8px 12px; background: %3; }")
             .arg(t.dark ? "#FFD1D1" : "#7A1717", cssColor(withAlpha(t.error, 0.55)), cssColor(withAlpha(t.error, 0.12)));
    // badges
    auto badge = [&](const char* name, const QColor& c, double bgA) {
        q += QStringLiteral("QLabel#%1 { color: %2; border: 1px solid %3; background: %4; border-radius: 6px; padding: 1px 7px; font-size: %5px; font-weight: 700; }")
                 .arg(QString::fromLatin1(name), cssColor(c), cssColor(withAlpha(c, 0.55)), cssColor(withAlpha(c, bgA)))
                 .arg(px - 3);
    };
    badge("BadgeAccent", t.dark ? t.accent2 : t.primary, 0.14);
    badge("BadgePreview", t.warning, 0.14);
    badge("BadgeLocked", t.text3, 0.10);
    badge("BadgeExample", t.dark ? t.accent2 : t.primary, 0.10);
    badge("BadgeOk", t.success, 0.12);
    badge("BadgeError", t.error, 0.12);
    // buttons
    q += QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 10px; padding: 7px 14px; color: %3; }").arg(IN, LS, TX);
    q += QStringLiteral("QPushButton:hover { border-color: %1; background: %2; }").arg(FC, HV);
    q += QStringLiteral("QPushButton:pressed { background: %1; }").arg(SE);
    q += QStringLiteral("QPushButton:focus { border: 2px solid %1; padding: 6px 13px; }").arg(FC);
    q += QStringLiteral("QPushButton:disabled { color: %1; border-color: %2; background: transparent; }").arg(T3, LN);
    q += QStringLiteral("QPushButton#Primary { background: %1; color: #FFFFFF; border: 1px solid %1; font-weight: 600; }").arg(PR);
    q += QStringLiteral("QPushButton#Primary:hover { background: %1; border-color: %1; }").arg(PH);
    q += QStringLiteral("QPushButton#Primary:focus { border: 2px solid %1; }").arg(t.dark ? "#BFE9FF" : "#01215F");
    q += QStringLiteral("QPushButton#Primary:disabled { background: %1; color: %2; border-color: %1; }").arg(cssColor(withAlpha(t.primary, 0.35)), T2);
    q += QStringLiteral("QPushButton#Danger { color: %1; border-color: %2; }").arg(ER, cssColor(withAlpha(t.error, 0.6)));
    q += QStringLiteral("QPushButton#Danger:disabled { color: %1; border-color: %2; }").arg(T3, LN);
    q += QStringLiteral("QPushButton#Link { background: transparent; border: none; color: %1; padding: 2px 4px; text-decoration: none; }").arg(AC);
    q += QStringLiteral("QPushButton#Link:hover { text-decoration: underline; }");
    q += QStringLiteral("QPushButton#Chip { border-radius: 9px; padding: 5px 11px; background: transparent; border: 1px solid %1; color: %2; }").arg(LN, TX);
    q += QStringLiteral("QPushButton#Chip:hover { background: %1; border-color: %2; }").arg(HV, LS);
    q += QStringLiteral("QPushButton#Chip:checked { background: %1; border-color: %2; color: %3; }").arg(SE, FC, HD);
    q += QStringLiteral("QPushButton#Chip:disabled { color: %1; border-color: %2; }").arg(T3, cssColor(withAlpha(t.line, t.line.alphaF() * 0.6)));
    q += QStringLiteral("QToolButton { border: 1px solid transparent; border-radius: 8px; padding: 4px; background: transparent; }");
    q += QStringLiteral("QToolButton:hover { background: %1; border-color: %2; }").arg(HV, LN);
    q += QStringLiteral("QToolButton:focus { border: 2px solid %1; }").arg(FC);
    // inputs
    const QString inputSel = QStringLiteral("QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit");
    q += inputSel + QStringLiteral(" { background: %1; border: 1px solid %2; border-radius: 10px; padding: 6px 10px; selection-background-color: %3; selection-color: #FFFFFF; }").arg(IN, LS, PR);
    q += QStringLiteral("QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QPlainTextEdit:hover { border-color: %1; }").arg(FC);
    q += QStringLiteral("QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %1; }").arg(FC);
    q += QStringLiteral("QLineEdit:read-only { color: %1; }").arg(T2);
    q += QStringLiteral("QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: %1; border-color: %2; }").arg(T3, LN);
    q += QStringLiteral("QComboBox::drop-down { border: none; width: 26px; }");
    q += QStringLiteral("QComboBox::down-arrow { image: url(:/ui/chevron-down-%1.png); width: 12px; height: 12px; }").arg(t.dark ? "dark" : "light");
    q += QStringLiteral("QComboBox QAbstractItemView { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; selection-background-color: %3; outline: none; }").arg(SC, LS, SE);
    q += QStringLiteral("QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0px; border: none; }");
    // tables and lists
    q += QStringLiteral("QTableView, QTableWidget { background: transparent; alternate-background-color: %1; border: none; gridline-color: %2; selection-background-color: %3; selection-color: %4; outline: none; }")
             .arg(cssColor(withAlpha(t.dark ? QColor("#03A4FC") : QColor("#0151B7"), 0.035)), LN, SE, TX);
    q += QStringLiteral("QTableView::item { padding: 2px 8px; border: none; }");
    q += QStringLiteral("QHeaderView { background: transparent; border: none; }");
    q += QStringLiteral("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; padding: 7px 8px; font-weight: 600; font-size: %4px; }")
             .arg(cssColor(withAlpha(t.dark ? QColor("#03A4FC") : QColor("#0151B7"), 0.05)), T2, LN).arg(px - 1);
    q += QStringLiteral("QTableCornerButton::section { background: transparent; border: none; }");
    q += QStringLiteral("QListWidget, QListView, QTreeView { background: transparent; border: none; outline: none; }");
    q += QStringLiteral("QListWidget::item { padding: 7px 10px; border-radius: 8px; margin: 1px 0px; }");
    q += QStringLiteral("QListWidget::item:hover { background: %1; }").arg(HV);
    q += QStringLiteral("QListWidget::item:selected { background: %1; color: %2; }").arg(SE, HD);
    q += QStringLiteral("QListWidget#NavList::item { padding: 9px 12px; }");
    // scroll bars
    q += QStringLiteral("QScrollBar:vertical { background: transparent; width: 9px; margin: 2px; }");
    q += QStringLiteral("QScrollBar:horizontal { background: transparent; height: 9px; margin: 2px; }");
    q += QStringLiteral("QScrollBar::handle { background: %1; border-radius: 4px; min-height: 24px; min-width: 24px; }").arg(LS);
    q += QStringLiteral("QScrollBar::handle:hover { background: %1; }").arg(FC);
    q += QStringLiteral("QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }");
    q += QStringLiteral("QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }");
    q += QStringLiteral("QScrollArea { background: transparent; border: none; }");
    q += QStringLiteral("QScrollArea > QWidget > QWidget { background: transparent; }");
    // menus
    q += QStringLiteral("QMenuBar { background: transparent; border: none; padding: 0px; }");
    q += QStringLiteral("QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 7px; color: %1; }").arg(TX);
    q += QStringLiteral("QMenuBar::item:selected { background: %1; }").arg(HV);
    q += QStringLiteral("QMenuBar::item:pressed { background: %1; }").arg(SE);
    q += QStringLiteral("QMenu { background: %1; border: 1px solid %2; border-radius: 10px; padding: 6px; }").arg(SC, LS);
    q += QStringLiteral("QMenu::item { padding: 6px 28px 6px 12px; border-radius: 6px; }");
    q += QStringLiteral("QMenu::item:selected { background: %1; color: %2; }").arg(SE, HD);
    q += QStringLiteral("QMenu::item:disabled { color: %1; }").arg(T3);
    q += QStringLiteral("QMenu::separator { height: 1px; background: %1; margin: 5px 8px; }").arg(LN);
    q += QStringLiteral("QMenu::icon { padding-left: 6px; }");
    // checks
    q += QStringLiteral("QCheckBox, QRadioButton { spacing: 8px; background: transparent; }");
    q += QStringLiteral("QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; border: 1px solid %1; background: %2; }").arg(FC, IN);
    q += QStringLiteral("QCheckBox::indicator { border-radius: 4px; } QRadioButton::indicator { border-radius: 8px; }");
    q += QStringLiteral("QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: %1; border-color: %1; }").arg(PR);
    // progress
    q += QStringLiteral("QProgressBar { background: %1; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; text-align: center; color: transparent; }").arg(cssColor(withAlpha(t.text3, 0.25)));
    q += QStringLiteral("QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2); border-radius: 3px; }").arg(cssColor(t.primary), cssColor(t.accent));
    q += QStringLiteral("QSplitter::handle { background: transparent; }");
    q += QStringLiteral("QTextBrowser { background: transparent; border: none; }");
    q += QStringLiteral("QAbstractScrollArea { background: transparent; }");
    (void)WA; (void)OK;
    qApp->setStyleSheet(q);
}

}  // namespace deras::ui

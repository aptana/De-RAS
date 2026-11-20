// De-RAS - ui/Widgets.h
// Building blocks of the 0.2.0 single-window UI: cards, badges, icon
// buttons, segmented controls, the grouped toolbar capsule, the view bar
// (back + breadcrumb + title + main action), the thin status bar, toasts and
// in-window sheets. Sheets replace separate dialog windows: only the Compute
// window and the Help window are separate top-level windows.
#pragma once

#include <QFrame>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>
#include <functional>

class QVBoxLayout;
class QHBoxLayout;
class QEventLoop;
class QStackedWidget;
class QScrollArea;

namespace deras::ui {

// Feature availability shown on buttons and in the feature matrix.
enum class Availability { Available, Preview, Locked };

// ---------------- Card ----------------
class Card : public QFrame {
    Q_OBJECT
public:
    explicit Card(const QString& caption = {}, const QString& helpTopic = {}, QWidget* parent = nullptr);
    QVBoxLayout* body() const { return body_; }
    QHBoxLayout* headerRight() const { return headerRight_; }
    void setCaption(const QString& c);
    void setCaptionDot(bool on);
    void setPadding(int px);
    void setTransparent(bool t) { transparent_ = t; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent* e) override;
private:
    QVBoxLayout* outer_;
    QVBoxLayout* body_;
    QHBoxLayout* headerRight_;
    QWidget* header_;
    QLabel* caption_;
    QLabel* dot_;
    bool transparent_ = false;
    bool settled_ = false;
};

QLabel* makeBadge(const QString& text, const char* kind);  // kind: BadgeAccent, BadgePreview, BadgeLocked, BadgeExample, BadgeOk, BadgeError
QLabel* makeCaption(const QString& text);                   // small upper-case section caption
QLabel* makeHint(const QString& text);                      // dashed hint box
QLabel* makeMuted(const QString& text, bool wrap = true);
QPushButton* makeButton(const QString& text, bool primary = false, const QString& icon = {});
QFrame* makeHLine();

// ---------------- IconButton ----------------
class IconButton : public QToolButton {
    Q_OBJECT
public:
    IconButton(const QString& icon, const QString& tooltip, QWidget* parent = nullptr, int iconPx = 18);
    void setIconName(const QString& icon);
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }
private:
    QString icon_;
    int px_;
    bool hover_ = false;
};

// ---------------- Segmented control ----------------
class Segmented : public QWidget {
    Q_OBJECT
public:
    explicit Segmented(QWidget* parent = nullptr);
    void addOption(const QString& text, int value, bool enabled = true, const QString& tooltip = {});
    void setValue(int v);
    int value() const { return value_; }
signals:
    void changed(int value);
private:
    QHBoxLayout* lay_;
    QList<QPushButton*> buttons_;
    int value_ = -1;
};

// ---------------- Toolbar ----------------
class ToolbarButton : public QWidget {
    Q_OBJECT
public:
    ToolbarButton(const QString& id, const QString& icon, const QString& label, const QString& tooltip, Availability a, QWidget* parent = nullptr);
    QString id() const { return id_; }
    void setActive(bool a) { active_ = a; update(); }
    void setDirty(bool d) { dirty_ = d; update(); }
    void setCompact(bool c);
    void setAvailability(Availability a, const QString& tooltip);
    Availability availability() const { return avail_; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
signals:
    void clicked(const QString& id);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override { keyFocus_ = false; QWidget::focusOutEvent(e); update(); }
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }
private:
    QString id_, icon_, label_;
    Availability avail_;
    bool active_ = false, dirty_ = false, compact_ = false, hover_ = false, keyFocus_ = false;
};

class ToolbarCapsule : public QWidget {
    Q_OBJECT
public:
    explicit ToolbarCapsule(QWidget* parent = nullptr);
    void beginGroup(const QString& caption);
    ToolbarButton* add(const QString& id, const QString& icon, const QString& label, const QString& tooltip, Availability a = Availability::Available);
    ToolbarButton* button(const QString& id) const;
    void setActive(const QString& id);
    QList<ToolbarButton*> buttons() const { return buttons_; }
    QSize minimumSizeHint() const override;
signals:
    void triggered(const QString& id);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void updateCompact();
    QHBoxLayout* row_;
    QHBoxLayout* group_ = nullptr;
    QList<QWidget*> separators_;
    QList<QLabel*> captions_;
    QList<ToolbarButton*> buttons_;
    int level_ = 0;  // 0 all labels, 1 locked buttons icon-only, 2 all icon-only
};

// ---------------- Toast ----------------
void showToast(QWidget* anyWidgetInWindow, const QString& text, const QString& icon = QStringLiteral("check"));

// ---------------- Sheet (in-window modal panel) ----------------
class Sheet : public QWidget {
    Q_OBJECT
public:
    Sheet(QWidget* anyWidgetInWindow, const QString& title, const QString& helpTopic = {}, int width = 560);
    ~Sheet() override;
    QVBoxLayout* body() const { return body_; }
    QHBoxLayout* footerLeft() const { return footerLeft_; }
    QPushButton* addButton(const QString& text, int result, bool primary = false);
    // Called before closing with a non-zero result; return false to keep the sheet open.
    void setAcceptCheck(std::function<bool(int)> f) { check_ = std::move(f); }
    void setTitle(const QString& t);
    int exec();
    void done(int result);
    void setError(const QString& text);  // inline error line above the buttons (empty hides it)
protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
private:
    void place();
    QWidget* host_;
    QFrame* card_;
    QLabel* title_;
    QVBoxLayout* body_;
    QHBoxLayout* footerLeft_;
    QHBoxLayout* buttons_;
    QLabel* error_;
    QScrollArea* scroll_;
    QEventLoop* loop_ = nullptr;
    std::function<bool(int)> check_;
    int result_ = 0;
    int width_;
};

bool sheetGetText(QWidget* from, const QString& title, const QString& label, QString& value, int maxLen = 0,
                  const QString& helpTopic = {}, const QString& note = {});
bool sheetConfirm(QWidget* from, const QString& title, const QString& text, const QString& okText, bool danger = false);
// Returns the index of the pressed button (0-based) or -1 when cancelled with Esc.
int sheetChoice(QWidget* from, const QString& title, const QString& text, const QStringList& buttons, int primaryIndex);
void sheetInfo(QWidget* from, const QString& title, const QString& text);

// Dark Windows title bar (Dark Theme) or light (Light Theme) on a top-level window; no-op elsewhere.
void applyWindowChrome(QWidget* topLevel);

// ---------------- animation helpers ----------------
bool animationsEnabled();  // honours the Windows "show animations" setting
void fadeIn(QWidget* w, int ms = 170);

}  // namespace deras::ui

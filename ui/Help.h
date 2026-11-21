// De-RAS - ui/Help.h
// Contextual help: the ⓘ button placed next to every feature, the short
// pop-up and the separate, resizable help window. Content comes from
// resources/help/help.json (bilingual, offline), which is also the source
// for the help-topic chapters of the user manual.
#pragma once

#include <QFocusEvent>
#include <QFrame>
#include <QHeaderView>
#include <QMap>
#include <QPointer>
#include <QToolButton>
#include <QWidget>

class QTextBrowser;
class QListWidget;
class QLineEdit;

namespace deras::ui {

struct HelpTopic {
    QString id;
    QString titleId, titleEn, shortId, shortEn, bodyId, bodyEn;
    QStringList related;
    QString status;  // "available" | "partial" | "unavailable"
    QString title() const;
    QString shortText() const;
    QString body() const;
};

class HelpCatalog {
public:
    static HelpCatalog& instance();
    const HelpTopic* topic(const QString& id) const;
    QStringList ids() const { return order_; }
private:
    HelpCatalog();
    QMap<QString, HelpTopic> topics_;
    QStringList order_;
};

// The ⓘ button. Hover shows the short text; click opens a small pop-up with
// the short text and a button to the full help window.
class InfoButton : public QToolButton {
    Q_OBJECT
public:
    explicit InfoButton(const QString& topicId, QWidget* parent = nullptr);
    QString topicId() const { return topic_; }
    QSize sizeHint() const override { return {18, 18}; }
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }
    void focusInEvent(QFocusEvent* e) override {
        keyFocus_ = e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason;
        QToolButton::focusInEvent(e);
    }
    void focusOutEvent(QFocusEvent* e) override { keyFocus_ = false; QToolButton::focusOutEvent(e); }
private:
    void showPopup();
    bool hover_ = false, keyFocus_ = false;
    QString topic_;
};

// A label with an ⓘ button beside it (for fields, groups and columns).
QWidget* withInfo(QWidget* w, const QString& topicId);
QWidget* labelWithInfo(const QString& text, const QString& topicId, QWidget* parent = nullptr);

// Table header that draws an ⓘ marker in every section that has a topic;
// clicking the marker opens the help for that column.
class InfoHeader : public QHeaderView {
    Q_OBJECT
public:
    explicit InfoHeader(Qt::Orientation o, QWidget* parent = nullptr);
    void setTopics(const QStringList& topicPerSection) { topics_ = topicPerSection; }
protected:
    void paintSection(QPainter* p, const QRect& rect, int logicalIndex) const override;
    void mousePressEvent(QMouseEvent* e) override;
    QSize sectionSizeFromContents(int logicalIndex) const override;
    bool event(QEvent* e) override;
private:
    QRect markerRect(const QRect& section) const;
    QStringList topics_;
};

class HelpWindow : public QWidget {
    Q_OBJECT
public:
    static void showTopic(const QString& id);
    static void closeWindow();  // before the main window is rebuilt (theme/language)
private:
    HelpWindow();
    void open(const QString& id);
    void filter(const QString& text);
    QListWidget* list_;
    QTextBrowser* view_;
    QLineEdit* search_;
    static QPointer<HelpWindow> s_window;
};

}  // namespace deras::ui

// De-RAS - ui/Widgets.cpp
#include "Widgets.h"

#include <QApplication>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "Common.h"
#include "Help.h"
#include "Icons.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace deras::ui {

bool animationsEnabled() {
#ifdef _WIN32
    BOOL on = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0)) return on != FALSE;
    return true;
#else
    return qEnvironmentVariableIsEmpty("DERAS_NO_ANIMATION");
#endif
}

void applyWindowChrome(QWidget* w) {
#ifdef _WIN32
    if (!w) return;
    using DwmSet = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static DwmSet fn = []() -> DwmSet {
        HMODULE m = LoadLibraryW(L"dwmapi.dll");
        return m ? reinterpret_cast<DwmSet>(reinterpret_cast<void*>(GetProcAddress(m, "DwmSetWindowAttribute"))) : nullptr;
    }();
    if (!fn) return;
    HWND h = reinterpret_cast<HWND>(w->winId());
    const Theme& t = Theme::current();
    BOOL dark = t.dark ? TRUE : FALSE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 2004+ / 11); 19 on older Windows 10 builds.
    if (FAILED(fn(h, 20, &dark, sizeof dark))) fn(h, 19, &dark, sizeof dark);
    // Windows 11: caption and border colours from the theme (ignored on Windows 10).
    const QColor cap = t.dark ? QColor("#060B14") : QColor("#E6EFFA");
    const COLORREF capRef = RGB(cap.red(), cap.green(), cap.blue());
    fn(h, 35, &capRef, sizeof capRef);  // DWMWA_CAPTION_COLOR
    const QColor txt = t.text;
    const COLORREF txtRef = RGB(txt.red(), txt.green(), txt.blue());
    fn(h, 36, &txtRef, sizeof txtRef);  // DWMWA_TEXT_COLOR
#else
    (void)w;
#endif
}

void fadeIn(QWidget* w, int ms) {
    if (!w || !animationsEnabled()) return;
    auto* eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(0.0);
    w->setGraphicsEffect(eff);
    auto* a = new QPropertyAnimation(eff, "opacity", w);
    a->setDuration(ms);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QPointer<QWidget> guard(w);
    QObject::connect(a, &QPropertyAnimation::finished, w, [guard]() { if (guard) guard->setGraphicsEffect(nullptr); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================ Card ============================
Card::Card(const QString& caption, const QString& helpTopic, QWidget* parent) : QFrame(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    outer_ = new QVBoxLayout(this);
    outer_->setContentsMargins(16, 14, 16, 16);
    outer_->setSpacing(10);
    header_ = new QWidget;
    auto* h = new QHBoxLayout(header_);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    dot_ = new QLabel;
    dot_->setFixedSize(8, 8);
    dot_->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;").arg(cssColor(Theme::current().accent)));
    dot_->hide();
    caption_ = makeCaption(caption);
    h->addWidget(dot_);
    h->addWidget(caption_);
    if (!helpTopic.isEmpty()) h->addWidget(new InfoButton(helpTopic));
    h->addStretch();
    headerRight_ = new QHBoxLayout;
    headerRight_->setSpacing(8);
    h->addLayout(headerRight_);
    outer_->addWidget(header_);
    header_->setVisible(!caption.isEmpty() || !helpTopic.isEmpty());
    body_ = new QVBoxLayout;
    body_->setContentsMargins(0, 0, 0, 0);
    body_->setSpacing(8);
    outer_->addLayout(body_, 1);
}

void Card::setCaption(const QString& c) {
    caption_->setText(c.toUpper());
    header_->setVisible(true);
}
void Card::setCaptionDot(bool on) { dot_->setVisible(on); }
void Card::setPadding(int px) { outer_->setContentsMargins(px, px, px, px); }

void Card::showEvent(QShowEvent* e) {
    // Cards whose content does not grow keep it at the top: extra height
    // goes below the content instead of between rows.
    if (!settled_) {
        settled_ = true;
        bool grows = false;
        for (int i = 0; i < body_->count(); ++i) {
            if (body_->stretch(i) > 0) grows = true;
            QLayoutItem* it = body_->itemAt(i);
            if (it->spacerItem() && (it->expandingDirections() & Qt::Vertical)) grows = true;
        }
        if (!grows) body_->addStretch(1);
    }
    QFrame::showEvent(e);
}

void Card::paintEvent(QPaintEvent*) {
    if (transparent_) return;
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    QColor a = t.cardA, b = t.cardB;
    if (t.dark) { a.setAlphaF(0.88f); b.setAlphaF(0.92f); }
    g.setColorAt(0, a);
    g.setColorAt(1, b);
    p.setPen(QPen(t.dark ? withAlpha(t.accent, 0.20) : t.line, 1));
    p.setBrush(g);
    p.drawRoundedRect(r, t.radiusCard, t.radiusCard);
    if (t.dark) {  // faint top highlight (glass edge)
        p.setPen(QPen(withAlpha(QColor("#7DD3FC"), 0.10), 1));
        p.drawLine(QPointF(r.left() + t.radiusCard, r.top() + 1), QPointF(r.right() - t.radiusCard, r.top() + 1));
    }
}

QLabel* makeBadge(const QString& text, const char* kind) {
    auto* l = new QLabel(text);
    l->setObjectName(QString::fromLatin1(kind));
    l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return l;
}

QLabel* makeCaption(const QString& text) {
    auto* l = new QLabel(text.toUpper());
    l->setObjectName("Caption");
    return l;
}

QLabel* makeHint(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("Hint");
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    return l;
}

QLabel* makeMuted(const QString& text, bool wrap) {
    auto* l = new QLabel(text);
    l->setObjectName("Muted");
    l->setWordWrap(wrap);
    return l;
}

QPushButton* makeButton(const QString& text, bool primary, const QString& icon) {
    auto* b = new QPushButton(text);
    if (primary) b->setObjectName("Primary");
    if (!icon.isEmpty()) {
        b->setIcon(lineIcon(icon, primary ? QColor("#FFFFFF") : Theme::current().text));
        b->setIconSize(QSize(16, 16));
    }
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QFrame* makeHLine() {
    auto* f = new QFrame;
    f->setFixedHeight(1);
    f->setStyleSheet(QStringLiteral("background:%1;").arg(cssColor(Theme::current().line)));
    return f;
}

// ============================ IconButton ============================
IconButton::IconButton(const QString& icon, const QString& tooltip, QWidget* parent, int iconPx)
    : QToolButton(parent), icon_(icon), px_(iconPx) {
    setToolTip(tooltip);
    setAccessibleName(tooltip);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(iconPx + 18, iconPx + 18);
    setFocusPolicy(Qt::TabFocus);
}

void IconButton::setIconName(const QString& icon) { icon_ = icon; update(); }

void IconButton::paintEvent(QPaintEvent*) {
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool on = isEnabled();
    p.setPen(QPen(hasFocus() ? t.focus : (hover_ && on ? t.lineStrong : t.line), hasFocus() ? 2 : 1));
    p.setBrush(isDown() ? t.selected : (hover_ && on ? t.hover : t.input));
    p.drawRoundedRect(r, t.radiusControl, t.radiusControl);
    const QColor c = !on ? t.text3 : (hover_ ? t.heading : t.text2);
    paintIcon(&p, QRectF((width() - px_) / 2.0, (height() - px_) / 2.0, px_, px_), icon_, c);
}

// ============================ Segmented ============================
Segmented::Segmented(QWidget* parent) : QWidget(parent) {
    lay_ = new QHBoxLayout(this);
    lay_->setContentsMargins(0, 0, 0, 0);
    lay_->setSpacing(6);
    lay_->addStretch();
}

void Segmented::addOption(const QString& text, int value, bool enabled, const QString& tooltip) {
    auto* b = new QPushButton(text);
    b->setObjectName("Chip");
    b->setCheckable(true);
    b->setEnabled(enabled);
    b->setProperty("value", value);
    b->setCursor(Qt::PointingHandCursor);
    if (!tooltip.isEmpty()) b->setToolTip(tooltip);
    if (!enabled) b->setIcon(lineIcon("lock", Theme::current().text3));
    lay_->insertWidget(lay_->count() - 1, b);
    buttons_ << b;
    connect(b, &QPushButton::clicked, this, [this, b]() {
        const int v = b->property("value").toInt();
        setValue(v);
        emit changed(v);
    });
}

void Segmented::setValue(int v) {
    value_ = v;
    for (auto* b : buttons_) b->setChecked(b->property("value").toInt() == v);
}

// ============================ ToolbarButton ============================
ToolbarButton::ToolbarButton(const QString& id, const QString& icon, const QString& label, const QString& tooltip, Availability a, QWidget* parent)
    : QWidget(parent), id_(id), icon_(icon), label_(label), avail_(a) {
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setAvailability(a, tooltip);
    setAccessibleName(QString(label).replace('\n', ' '));
}

void ToolbarButton::setAvailability(Availability a, const QString& tooltip) {
    avail_ = a;
    QString tip = tooltip;
    if (a == Availability::Locked) tip = QString(label_).replace('\n', ' ') + QStringLiteral(" — ") + tooltip;
    setToolTip(tip);
    setAccessibleDescription(tip);
    update();
}

void ToolbarButton::setCompact(bool c) {
    if (c == compact_) return;
    compact_ = c;
    updateGeometry();
    update();
}

QSize ToolbarButton::sizeHint() const {
    if (compact_) return {36, 40};
    const QFontMetrics fm(uiFont(Theme::current().basePx - 3));
    int w = 0;
    for (const QString& l : label_.split('\n')) w = std::max(w, fm.horizontalAdvance(l));
    return {std::max(50, w + 12), 62};
}

void ToolbarButton::paintEvent(QPaintEvent*) {
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool locked = avail_ == Availability::Locked;
    const double lift = (hover_ && !locked && !active_ && animationsEnabled()) ? -1.5 : 0.0;
    QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1).translated(0, lift);
    if (active_) {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, withAlpha(QColor("#0EA5E9"), t.dark ? 0.25 : 0.14));
        g.setColorAt(1, withAlpha(QColor("#2563EB"), t.dark ? 0.24 : 0.12));
        p.setBrush(g);
        p.setPen(QPen(t.dark ? withAlpha(QColor("#38BDF8"), 0.65) : t.primary, 1));
        p.drawRoundedRect(r, 9, 9);
    } else if (hover_ && !locked) {
        p.setBrush(withAlpha(QColor("#0EA5E9"), t.dark ? 0.15 : 0.08));
        p.setPen(QPen(t.dark ? withAlpha(QColor("#38BDF8"), 0.45) : t.lineStrong, 1));
        p.drawRoundedRect(r, 9, 9);
    }
    if (hasFocus() && keyFocus_) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(t.focus, 2));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 8, 8);
    }
    QColor ic = locked ? withAlpha(t.text3, 0.75) : (active_ ? (t.dark ? QColor("#38BDF8") : t.primary) : (hover_ ? t.text : t.text2));
    const double iconPx = 19;
    const double iy = compact_ ? (height() - iconPx) / 2.0 : 9;
    paintIcon(&p, QRectF((width() - iconPx) / 2.0, iy + lift, iconPx, iconPx), icon_, ic, 1.6);
    if (!compact_) {
        p.setFont(uiFont(t.basePx - 3, active_ ? QFont::DemiBold : QFont::Normal));
        p.setPen(locked ? withAlpha(t.text3, 0.85) : (active_ ? (t.dark ? QColor("#7DD3FC") : t.primary) : t.text2));
        const QStringList lines = label_.split('\n');
        const QFontMetrics fm(p.font());
        double y = 31 + lift;
        for (const QString& l : lines) {
            p.drawText(QRectF(0, y, width(), fm.height()), Qt::AlignHCenter | Qt::AlignTop, l);
            y += fm.height() - 1;
        }
    }
    // markers: lock (not available), yellow dot (preview), blue dot (unsaved edits)
    const QPointF corner(width() / 2.0 + iconPx / 2.0 + 2, iy + 2 + lift);
    if (locked) {
        paintIcon(&p, QRectF(corner.x() - 3, corner.y() - 5, 9, 9), "lock", withAlpha(t.text3, 0.9), 2.2);
    } else if (avail_ == Availability::Preview) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.warning);
        p.drawEllipse(corner, 3, 3);
    }
    if (dirty_) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        p.drawEllipse(QPointF(width() / 2.0 - iconPx / 2.0 - 3, iy + 2 + lift), 3, 3);
    }
}

void ToolbarButton::focusInEvent(QFocusEvent* e) {
    keyFocus_ = e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason;
    QWidget::focusInEvent(e);
    update();
}

void ToolbarButton::mouseReleaseEvent(QMouseEvent* e) {
    if (rect().contains(e->position().toPoint())) emit clicked(id_);
    QWidget::mouseReleaseEvent(e);
}

void ToolbarButton::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { emit clicked(id_); return; }
    QWidget::keyPressEvent(e);
}

// ============================ ToolbarCapsule ============================
ToolbarCapsule::ToolbarCapsule(QWidget* parent) : QWidget(parent) {
    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(14, 8, 14, 8);
    row_->setSpacing(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFixedHeight(96);
}

void ToolbarCapsule::beginGroup(const QString& caption) {
    if (group_) {
        auto* sep = new QWidget;
        sep->setFixedWidth(17);
        separators_ << sep;
        row_->addWidget(sep);
    }
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    auto* cap = makeCaption(caption);
    cap->setIndent(10);
    captions_ << cap;
    v->addWidget(cap);
    group_ = new QHBoxLayout;
    group_->setSpacing(2);
    v->addLayout(group_);
    row_->addWidget(box);
}

ToolbarButton* ToolbarCapsule::add(const QString& id, const QString& icon, const QString& label, const QString& tooltip, Availability a) {
    if (!group_) beginGroup(QString());
    auto* b = new ToolbarButton(id, icon, label, tooltip, a);
    b->setProperty("fullWidth", b->sizeHint().width());
    group_->addWidget(b);
    buttons_ << b;
    connect(b, &ToolbarButton::clicked, this, &ToolbarCapsule::triggered);
    return b;
}

ToolbarButton* ToolbarCapsule::button(const QString& id) const {
    for (auto* b : buttons_) if (b->id() == id) return b;
    return nullptr;
}

void ToolbarCapsule::setActive(const QString& id) {
    for (auto* b : buttons_) b->setActive(b->id() == id);
}

QSize ToolbarCapsule::minimumSizeHint() const { return {200, level_ == 2 ? 56 : 96}; }

void ToolbarCapsule::updateCompact() {
    const int base = row_->contentsMargins().left() + row_->contentsMargins().right() + 17 * separators_.size();
    int full = base, mid = base;
    for (auto* b : buttons_) {
        const int fw = b->property("fullWidth").toInt() + 2;
        full += fw;
        mid += b->availability() == Availability::Locked ? 38 : fw;
    }
    const int want = width() >= full ? 0 : width() >= mid ? 1 : 2;
    if (want == level_) return;
    level_ = want;
    for (auto* b : buttons_) b->setCompact(want == 2 || (want == 1 && b->availability() == Availability::Locked));
    for (auto* c : captions_) c->setVisible(want < 2);
    setFixedHeight(want == 2 ? 56 : 96);
    updateGeometry();
}

void ToolbarCapsule::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updateCompact();
}

void ToolbarCapsule::paintEvent(QPaintEvent*) {
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    if (t.dark) {
        g.setColorAt(0, QColor(13, 24, 44, 220));
        g.setColorAt(1, QColor(8, 15, 29, 230));
    } else {
        g.setColorAt(0, QColor("#FFFFFF"));
        g.setColorAt(1, QColor("#FBFCFE"));
    }
    p.setBrush(g);
    p.setPen(QPen(t.dark ? withAlpha(t.accent, 0.20) : t.line, 1));
    p.drawRoundedRect(r, 16, 16);
    p.setPen(QPen(t.line, 1));
    for (auto* s : separators_) {
        const QRect sr = s->geometry();
        const double x = sr.center().x() + 0.5;
        p.drawLine(QPointF(x, 10), QPointF(x, height() - 10));
    }
}

// ============================ Toast ============================
void showToast(QWidget* any, const QString& text, const QString& icon) {
    if (!any) return;
    QWidget* host = any->window();
    auto* f = new QFrame(host);
    f->setAttribute(Qt::WA_DeleteOnClose);
    f->setAttribute(Qt::WA_TransparentForMouseEvents);
    const Theme& t = Theme::current();
    f->setStyleSheet(QStringLiteral("QFrame { background:%1; border:1px solid %2; border-radius:12px; } QLabel { background: transparent; border: none; }")
                         .arg(cssColor(t.dark ? QColor(13, 24, 44, 245) : QColor("#F7FAFF")), cssColor(t.lineStrong)));
    auto* h = new QHBoxLayout(f);
    h->setContentsMargins(12, 9, 14, 9);
    h->setSpacing(8);
    auto* ic = new QLabel;
    const QColor c = icon == "warning" ? t.warning : (icon == "close" ? t.error : t.success);
    ic->setPixmap(iconPixmap(icon, 16, c));
    auto* l = new QLabel(text);
    h->addWidget(ic);
    h->addWidget(l);
    f->adjustSize();
    // stack above existing toasts
    int y = host->height() - f->height() - 40;
    for (QObject* o : host->children())
        if (auto* other = qobject_cast<QFrame*>(o); other && other != f && other->property("deras-toast").toBool()) y = std::min(y, other->y() - f->height() - 8);
    f->setProperty("deras-toast", true);
    f->move(host->width() - f->width() - 18, y);
    f->show();
    f->raise();
    fadeIn(f, 160);
    QTimer::singleShot(2800, f, &QFrame::close);
}

// ============================ Sheet ============================
Sheet::Sheet(QWidget* any, const QString& title, const QString& helpTopic, int width)
    : QWidget(any ? any->window() : nullptr), host_(any ? any->window() : nullptr), width_(width) {
    setAttribute(Qt::WA_NoSystemBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    card_ = new QFrame(this);
    card_->setObjectName("SheetCard");
    const Theme& t = Theme::current();
    card_->setStyleSheet(QStringLiteral("QFrame#SheetCard { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2); border: 1px solid %3; border-radius: 16px; }")
                             .arg(cssColor(t.dark ? QColor(15, 27, 48, 250) : QColor("#F7FAFF")), cssColor(t.dark ? QColor(9, 17, 32, 252) : QColor("#EAF2FC")),
                                  cssColor(t.dark ? withAlpha(t.accent, 0.30) : t.lineStrong)));
    auto* v = new QVBoxLayout(card_);
    v->setContentsMargins(22, 18, 22, 18);
    v->setSpacing(12);
    auto* head = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/branding/De-RAS-24.png"));
    title_ = new QLabel(title);
    title_->setObjectName("CardTitle");
    head->addWidget(logo);
    head->addWidget(title_);
    if (!helpTopic.isEmpty()) head->addWidget(new InfoButton(helpTopic));
    head->addStretch();
    auto* x = new IconButton("close", tr2("Tutup (Esc)", "Close (Esc)"), nullptr, 14);
    connect(x, &QToolButton::clicked, this, [this]() { done(0); });
    head->addWidget(x);
    v->addLayout(head);
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget;
    body_ = new QVBoxLayout(inner);
    body_->setContentsMargins(0, 0, 4, 0);
    body_->setSpacing(10);
    scroll_->setWidget(inner);
    v->addWidget(scroll_, 1);
    error_ = new QLabel;
    error_->setObjectName("ErrorBanner");
    error_->setWordWrap(true);
    error_->hide();
    v->addWidget(error_);
    auto* foot = new QHBoxLayout;
    footerLeft_ = new QHBoxLayout;
    foot->addLayout(footerLeft_);
    foot->addStretch();
    buttons_ = new QHBoxLayout;
    buttons_->setSpacing(8);
    foot->addLayout(buttons_);
    v->addLayout(foot);
    if (host_) host_->installEventFilter(this);
    hide();
}

Sheet::~Sheet() {
    if (host_) host_->removeEventFilter(this);
}

void Sheet::setTitle(const QString& t) { title_->setText(t); }

void Sheet::setError(const QString& text) {
    error_->setText(text.isEmpty() ? QString() : QStringLiteral("⛔ ") + text);
    error_->setVisible(!text.isEmpty());
    place();
}

QPushButton* Sheet::addButton(const QString& text, int result, bool primary) {
    auto* b = makeButton(text, primary);
    buttons_->addWidget(b);
    connect(b, &QPushButton::clicked, this, [this, result]() { done(result); });
    if (primary) b->setDefault(true);
    return b;
}

void Sheet::done(int r) {
    if (r != 0 && check_ && !check_(r)) return;
    result_ = r;
    if (loop_) loop_->quit();
}

void Sheet::place() {
    if (!host_) return;
    setGeometry(host_->rect());
    const int w = std::min(width_, host_->width() - 48);
    card_->setFixedWidth(w);
    card_->layout()->activate();
    const int inner = scroll_->widget()->sizeHint().height();
    const int chrome = card_->sizeHint().height() - scroll_->sizeHint().height();
    const int h = std::min(host_->height() - 64, chrome + inner + 6);
    card_->setFixedHeight(std::max(160, h));
    card_->move((width() - card_->width()) / 2, std::max(24, (height() - card_->height()) / 2 - 20));
}

int Sheet::exec() {
    place();
    show();
    raise();
    fadeIn(card_, 150);
    QWidget* first = nullptr;
    for (auto* w : scroll_->widget()->findChildren<QWidget*>())
        if ((qobject_cast<QLineEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) && w->isEnabled()) { first = w; break; }
    if (first) first->setFocus(); else setFocus();
    QEventLoop loop;
    loop_ = &loop;
    loop.exec();
    loop_ = nullptr;
    hide();
    return result_;
}

bool Sheet::eventFilter(QObject* o, QEvent* e) {
    if (o == host_ && e->type() == QEvent::Resize && isVisible()) place();
    return QWidget::eventFilter(o, e);
}

void Sheet::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { done(0); return; }
    QWidget::keyPressEvent(e);
}

void Sheet::mousePressEvent(QMouseEvent* e) { e->accept(); }  // the backdrop blocks the window behind

void Sheet::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::current().dark ? QColor(2, 5, 10, 165) : QColor(16, 30, 54, 90));
}

bool sheetGetText(QWidget* from, const QString& title, const QString& label, QString& value, int maxLen, const QString& topic, const QString& note) {
    Sheet s(from, title, topic, 460);
    auto* l = new QLabel(label);
    l->setWordWrap(true);
    s.body()->addWidget(l);
    auto* e = new QLineEdit(value);
    if (maxLen > 0) e->setMaxLength(maxLen);
    s.body()->addWidget(e);
    if (!note.isEmpty()) s.body()->addWidget(makeMuted(note));
    s.addButton(tr2("Batal", "Cancel"), 0);
    s.addButton(tr2("OK", "OK"), 1, true);
    QObject::connect(e, &QLineEdit::returnPressed, &s, [&s]() { s.done(1); });
    s.setAcceptCheck([&](int) {
        if (e->text().trimmed().isEmpty()) { s.setError(tr2("Isian tidak boleh kosong.", "The field must not be empty.")); return false; }
        return true;
    });
    if (s.exec() != 1) return false;
    value = e->text().trimmed();
    return true;
}

bool sheetConfirm(QWidget* from, const QString& title, const QString& text, const QString& okText, bool danger) {
    Sheet s(from, title, {}, 460);
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    s.body()->addWidget(l);
    s.addButton(tr2("Batal", "Cancel"), 0);
    auto* ok = s.addButton(okText, 1, !danger);
    if (danger) ok->setObjectName("Danger");
    return s.exec() == 1;
}

int sheetChoice(QWidget* from, const QString& title, const QString& text, const QStringList& buttons, int primary) {
    Sheet s(from, title, {}, 480);
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    s.body()->addWidget(l);
    for (int i = 0; i < buttons.size(); ++i) s.addButton(buttons[i], i + 1, i == primary);
    return s.exec() - 1;
}

void sheetInfo(QWidget* from, const QString& title, const QString& text) {
    Sheet s(from, title, {}, 480);
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextFormat(Qt::AutoText);
    s.body()->addWidget(l);
    s.addButton(tr2("Tutup", "Close"), 1, true);
    s.exec();
}

}  // namespace deras::ui

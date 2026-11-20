// De-RAS - ui/Help.cpp
#include "Help.h"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolTip>
#include <QVBoxLayout>

#include "Common.h"
#include "Icons.h"
#include "Widgets.h"

namespace deras::ui {

QString HelpTopic::title() const { return isID() ? titleId : titleEn; }
QString HelpTopic::shortText() const { return isID() ? shortId : shortEn; }
QString HelpTopic::body() const { return isID() ? bodyId : bodyEn; }

HelpCatalog& HelpCatalog::instance() {
    static HelpCatalog c;
    return c;
}

HelpCatalog::HelpCatalog() {
    QFile f(QStringLiteral(":/help/help.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value("topics").toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        HelpTopic t;
        t.id = o.value("id").toString();
        t.titleId = o.value("title").toObject().value("id").toString();
        t.titleEn = o.value("title").toObject().value("en").toString();
        t.shortId = o.value("short").toObject().value("id").toString();
        t.shortEn = o.value("short").toObject().value("en").toString();
        t.bodyId = o.value("body").toObject().value("id").toString();
        t.bodyEn = o.value("body").toObject().value("en").toString();
        t.status = o.value("status").toString("available");
        for (const auto& r : o.value("related").toArray()) t.related << r.toString();
        topics_.insert(t.id, t);
        order_ << t.id;
    }
}

const HelpTopic* HelpCatalog::topic(const QString& id) const {
    auto it = topics_.find(id);
    return it == topics_.end() ? nullptr : &it.value();
}

// ---------------- InfoButton ----------------
InfoButton::InfoButton(const QString& topicId, QWidget* parent) : QToolButton(parent), topic_(topicId) {
    setObjectName("Info");
    setAutoRaise(true);
    setFixedSize(18, 18);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    const HelpTopic* t = HelpCatalog::instance().topic(topicId);
    const QString title = t ? t->title() : topicId;
    setToolTip(t ? QStringLiteral("<b>%1</b><br>%2").arg(t->title().toHtmlEscaped(), t->shortText().toHtmlEscaped()) : topicId);
    setAccessibleName(tr2("Info: ", "Info: ") + title);
    setAccessibleDescription(t ? t->shortText() : QString());
    connect(this, &QToolButton::clicked, this, &InfoButton::showPopup);
}

// Small and dim (BASE - PROMPT: "ⓘ tetap ada di setiap fitur tetapi kecil dan redup"); brighter on hover/focus.
void InfoButton::paintEvent(QPaintEvent*) {
    const Theme& t = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool kf = hasFocus() && keyFocus_;
    const QColor c = (hover_ || kf) ? t.accent : withAlpha(t.text3, 0.9);
    if (kf) {
        p.setPen(QPen(t.focus, 1.5));
        p.drawEllipse(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
    paintIcon(&p, QRectF(2, 2, width() - 4, height() - 4), "info", c, 1.6);
}

void InfoButton::showPopup() {
    const HelpTopic* t = HelpCatalog::instance().topic(topic_);
    auto* pop = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    pop->setAttribute(Qt::WA_DeleteOnClose);
    pop->setObjectName("InfoPopup");
    {
        const Theme& th = Theme::current();
        pop->setStyleSheet(QStringLiteral("QFrame#InfoPopup { background: %1; border: 1px solid %2; border-radius: 12px; }")
                               .arg(cssColor(th.dark ? QColor("#0D182C") : QColor("#F7FAFF")), cssColor(th.lineStrong)));
    }
    auto* lay = new QVBoxLayout(pop);
    lay->setContentsMargins(14, 12, 14, 12);
    auto* title = new QLabel(t ? t->title() : topic_);
    title->setObjectName("Strong");
    auto* body = new QLabel(t ? t->shortText() : tr2("Bantuan untuk topik ini belum ditulis.", "Help for this topic has not been written yet."));
    body->setWordWrap(true);
    body->setMaximumWidth(360);
    lay->addWidget(title);
    lay->addWidget(body);
    if (t && t->status == "unavailable") {
        auto* u = new QLabel(tr2("Status: belum tersedia", "Status: not available yet"));
        u->setObjectName("Muted");
        lay->addWidget(u);
    }
    auto* more = new QPushButton(tr2("Buka bantuan lengkap…", "Open full help…"));
    more->setObjectName("Primary");
    lay->addWidget(more, 0, Qt::AlignRight);
    const QString id = topic_;
    connect(more, &QPushButton::clicked, pop, [pop, id]() {
        pop->close();
        HelpWindow::showTopic(id);
    });
    pop->adjustSize();
    QPoint pos = mapToGlobal(QPoint(0, height()));
    if (QScreen* sc = screen()) {
        QRect avail = sc->availableGeometry();
        if (pos.x() + pop->width() > avail.right()) pos.setX(avail.right() - pop->width());
        if (pos.y() + pop->height() > avail.bottom()) pos.setY(mapToGlobal(QPoint(0, 0)).y() - pop->height());
    }
    pop->move(pos);
    pop->show();
    more->setFocus();
}

QWidget* withInfo(QWidget* w, const QString& topicId) {
    auto* box = new QWidget;
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    h->addWidget(w, 1);
    h->addWidget(new InfoButton(topicId));
    return box;
}

QWidget* labelWithInfo(const QString& text, const QString& topicId, QWidget* parent) {
    auto* box = new QWidget(parent);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto* l = new QLabel(text);
    h->addWidget(l);
    h->addWidget(new InfoButton(topicId));
    h->addStretch();
    return box;
}

// ---------------- InfoHeader ----------------
InfoHeader::InfoHeader(Qt::Orientation o, QWidget* parent) : QHeaderView(o, parent) {
    setSectionsClickable(true);
    setMouseTracking(true);
}

QRect InfoHeader::markerRect(const QRect& r) const { return QRect(r.right() - 20, r.center().y() - 8, 16, 16); }

void InfoHeader::paintSection(QPainter* p, const QRect& rect, int idx) const {
    p->save();
    QHeaderView::paintSection(p, rect, idx);
    p->restore();
    if (idx < topics_.size() && !topics_[idx].isEmpty()) {
        p->save();
        const QRect m = markerRect(rect);
        paintIcon(p, QRectF(m).adjusted(2, 2, -2, -2), "info", withAlpha(Theme::current().text3, 0.9), 1.6);
        p->restore();
    }
}

QSize InfoHeader::sectionSizeFromContents(int idx) const {
    QSize s = QHeaderView::sectionSizeFromContents(idx);
    s.rwidth() += 22;
    return s;
}

void InfoHeader::mousePressEvent(QMouseEvent* e) {
    int idx = logicalIndexAt(e->pos());
    if (idx >= 0 && idx < topics_.size() && !topics_[idx].isEmpty()) {
        QRect r(sectionViewportPosition(idx), 0, sectionSize(idx), height());
        if (markerRect(r).adjusted(-3, -3, 3, 3).contains(e->pos())) {
            HelpWindow::showTopic(topics_[idx]);
            return;
        }
    }
    QHeaderView::mousePressEvent(e);
}

bool InfoHeader::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        int idx = logicalIndexAt(he->pos());
        if (idx >= 0 && idx < topics_.size() && !topics_[idx].isEmpty()) {
            if (const HelpTopic* t = HelpCatalog::instance().topic(topics_[idx])) {
                QToolTip::showText(he->globalPos(), QStringLiteral("<b>%1</b><br>%2<br><i>%3</i>")
                                                        .arg(t->title().toHtmlEscaped(), t->shortText().toHtmlEscaped(),
                                                             tr2("Klik ⓘ untuk bantuan lengkap.", "Click ⓘ for full help.")),
                                   this);
                return true;
            }
        }
    }
    return QHeaderView::event(e);
}

// ---------------- HelpWindow ----------------
QPointer<HelpWindow> HelpWindow::s_window;

HelpWindow::HelpWindow() : QWidget(nullptr, Qt::Window) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr2("Bantuan De-RAS", "De-RAS Help"));
    setWindowIcon(appIcon());
    resize(860, 620);
    applyWindowChrome(this);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 16);
    lay->setSpacing(12);
    auto* head = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/branding/De-RAS-32.png"));
    logo->setAccessibleName("De-RAS");
    auto* title = new QLabel(tr2("Bantuan De-RAS", "De-RAS Help"));
    title->setObjectName("CardTitle");
    auto* off = makeBadge(tr2("OFFLINE", "OFFLINE"), "BadgeAccent");
    search_ = new QLineEdit;
    search_->setPlaceholderText(tr2("Cari topik bantuan…", "Search help topics…"));
    search_->setAccessibleName(tr2("Cari topik bantuan", "Search help topics"));
    search_->setMinimumWidth(260);
    head->addWidget(logo);
    head->addWidget(title);
    head->addWidget(off);
    head->addStretch();
    head->addWidget(search_);
    lay->addLayout(head);
    auto* split = new QSplitter;
    auto* lc = new Card(tr2("Topik", "Topics"));
    list_ = new QListWidget;
    list_->setObjectName("NavList");
    list_->setAccessibleName(tr2("Daftar topik", "Topic list"));
    lc->body()->addWidget(list_, 1);
    auto* vc = new Card;
    view_ = new QTextBrowser;
    view_->setOpenLinks(false);
    vc->body()->addWidget(view_, 1);
    split->addWidget(lc);
    split->addWidget(vc);
    split->setStretchFactor(1, 3);
    split->setSizes({260, 640});
    split->setHandleWidth(12);
    lay->addWidget(split, 1);
    for (const QString& id : HelpCatalog::instance().ids()) {
        const HelpTopic* t = HelpCatalog::instance().topic(id);
        auto* it = new QListWidgetItem(t->title() + (t->status == "unavailable" ? tr2("  (belum tersedia)", "  (not available)") : QString()));
        it->setData(Qt::UserRole, id);
        list_->addItem(it);
    }
    connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it) {
        if (it) open(it->data(Qt::UserRole).toString());
    });
    connect(view_, &QTextBrowser::anchorClicked, this, [this](const QUrl& u) {
        if (u.scheme() == "topic") showTopic(u.path().isEmpty() ? u.host() : u.path());
    });
    connect(search_, &QLineEdit::textChanged, this, &HelpWindow::filter);
}

void HelpWindow::filter(const QString& text) {
    for (int i = 0; i < list_->count(); ++i) {
        auto* it = list_->item(i);
        const HelpTopic* t = HelpCatalog::instance().topic(it->data(Qt::UserRole).toString());
        bool show = text.isEmpty() || it->text().contains(text, Qt::CaseInsensitive) ||
                    (t && t->body().contains(text, Qt::CaseInsensitive));
        it->setHidden(!show);
    }
}

void HelpWindow::open(const QString& id) {
    const HelpTopic* t = HelpCatalog::instance().topic(id);
    const Theme& th = Theme::current();
    QString html = QStringLiteral("<html><body style='font-size:%2px;color:%1;line-height:140%'>").arg(th.text.name()).arg(th.basePx);
    if (!t) {
        html += "<p>" + tr2("Topik tidak ditemukan: ", "Topic not found: ") + id.toHtmlEscaped() + "</p>";
    } else {
        html += QStringLiteral("<h2 style='color:%1'>%2</h2>").arg(th.heading.name(), t->title().toHtmlEscaped());
        const QString warn = th.warning.name();
        if (t->status == "unavailable")
            html += "<p style='color:" + warn + "'><b>🔒 " + tr2("Belum tersedia di versi ini.", "Not available in this version.") + "</b></p>";
        else if (t->status == "partial")
            html += "<p style='color:" + warn + "'><b>⚠ " + tr2("Fitur ini baru tersedia sebagian.", "This feature is only partially available.") + "</b></p>";
        else if (t->status == "preview")
            html += "<p style='color:" + warn + "'><b>● " + tr2("Pratinjau — belum divalidasi.", "Preview — not validated yet.") + "</b></p>";
        html += "<p><i>" + t->shortText().toHtmlEscaped() + "</i></p>" + t->body();
        if (!t->related.isEmpty()) {
            html += "<h3>" + tr2("Topik terkait", "Related topics") + "</h3><ul>";
            for (const QString& r : t->related)
                if (const HelpTopic* rt = HelpCatalog::instance().topic(r))
                    html += QStringLiteral("<li><a href='topic:%1'>%2</a></li>").arg(r, rt->title().toHtmlEscaped());
            html += "</ul>";
        }
    }
    html += "</body></html>";
    view_->setHtml(html);
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).toString() == id && list_->currentRow() != i) {
            list_->blockSignals(true);
            list_->setCurrentRow(i);
            list_->blockSignals(false);
        }
}

void HelpWindow::showTopic(const QString& id) {
    if (!s_window) s_window = new HelpWindow;
    s_window->open(id);
    s_window->show();
    s_window->raise();
    s_window->activateWindow();
}

void HelpWindow::closeWindow() {
    if (s_window) {
        s_window->close();
        s_window->deleteLater();
        s_window = nullptr;
    }
}

}  // namespace deras::ui

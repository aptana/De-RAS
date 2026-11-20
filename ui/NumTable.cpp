// De-RAS - ui/NumTable.cpp
#include "NumTable.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>

#include "Common.h"
#include "Help.h"

namespace deras::ui {

std::optional<double> parseUserNumber(const QString& s) {
    QString t = s.trimmed();
    if (t.isEmpty()) return std::nullopt;
    if (t.count(',') == 1 && !t.contains('.')) t.replace(',', '.');
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok || !std::isfinite(v)) return std::nullopt;
    return v;
}

NumTable::NumTable(const QStringList& headers, const QStringList& topics, QWidget* parent) : QTableWidget(parent) {
    auto* hh = new InfoHeader(Qt::Horizontal, this);
    hh->setTopics(topics);
    setHorizontalHeader(hh);
    setColumnCount(headers.size());
    setHorizontalHeaderLabels(headers);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setDefaultSectionSize(26);
    decimals_.assign(static_cast<size_t>(headers.size()), -1);
    setRowCount(1);
    connect(this, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        if (loading_) return;
        ensureTrailingEmptyRow();
        emit edited();
    });
}

void NumTable::setDecimals(int col, int d) {
    if (col >= 0 && col < static_cast<int>(decimals_.size())) decimals_[static_cast<size_t>(col)] = d;
}

void NumTable::ensureTrailingEmptyRow() {
    loading_ = true;
    bool lastEmpty = true;
    int last = rowCount() - 1;
    if (last >= 0)
        for (int c = 0; c < columnCount(); ++c)
            if (item(last, c) && !item(last, c)->text().trimmed().isEmpty()) lastEmpty = false;
    if (last < 0 || !lastEmpty) setRowCount(rowCount() + 1);
    loading_ = false;
}

bool NumTable::values(std::vector<std::vector<double>>& rows, int* badCount) {
    rows.clear();
    int bad = 0;
    loading_ = true;
    const Theme& th = Theme::current();
    for (int r = 0; r < rowCount(); ++r) {
        std::vector<double> row;
        bool any = false, rowBad = false;
        for (int c = 0; c < columnCount(); ++c) {
            QTableWidgetItem* it = item(r, c);
            QString t = it ? it->text() : QString();
            if (!t.trimmed().isEmpty()) any = true;
        }
        if (!any) continue;
        for (int c = 0; c < columnCount(); ++c) {
            QTableWidgetItem* it = item(r, c);
            if (!it) { it = new QTableWidgetItem; setItem(r, c, it); }
            auto v = parseUserNumber(it->text());
            if (!v) {
                ++bad;
                rowBad = true;
                QColor e = th.error;
                e.setAlpha(60);
                it->setBackground(e);
                it->setToolTip(tr2("Bukan angka yang valid. Nilai ini tidak dipakai.", "Not a valid number. This value is not used."));
            } else {
                it->setBackground(QBrush());
                it->setToolTip(QString());
                row.push_back(*v);
            }
        }
        if (!rowBad) rows.push_back(row);
    }
    loading_ = false;
    if (badCount) *badCount = bad;
    return bad == 0;
}

void NumTable::setValues(const std::vector<std::vector<double>>& rows) {
    loading_ = true;
    clearContents();
    setRowCount(static_cast<int>(rows.size()) + 1);
    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
        for (int c = 0; c < columnCount() && c < static_cast<int>(rows[static_cast<size_t>(r)].size()); ++c) {
            double v = rows[static_cast<size_t>(r)][static_cast<size_t>(c)];
            int d = decimals_[static_cast<size_t>(c)];
            auto* it = new QTableWidgetItem(d >= 0 ? QString::number(v, 'f', d) : QString::number(v, 'g', 10));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            setItem(r, c, it);
        }
    loading_ = false;
}

void NumTable::pasteClipboard() {
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;
    int r0 = std::max(0, currentRow()), c0 = std::max(0, currentColumn());
    QStringList lines = text.split(QRegularExpression("\r?\n"));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
    loading_ = true;
    if (r0 + lines.size() >= rowCount()) setRowCount(r0 + lines.size() + 1);
    for (int i = 0; i < lines.size(); ++i) {
        QStringList cells = lines[i].split(QRegularExpression(lines[i].contains('\t') ? "\t" : (lines[i].contains(';') ? ";" : "[ ,]+")));
        for (int j = 0; j < cells.size() && c0 + j < columnCount(); ++j) {
            auto* it = new QTableWidgetItem(cells[j].trimmed());
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            setItem(r0 + i, c0 + j, it);
        }
    }
    loading_ = false;
    ensureTrailingEmptyRow();
    emit edited();
}

void NumTable::keyPressEvent(QKeyEvent* e) {
    if (e->matches(QKeySequence::Paste)) { pasteClipboard(); return; }
    if (e->key() == Qt::Key_Delete && selectionModel()->hasSelection()) {
        loading_ = true;
        for (auto* it : selectedItems()) it->setText(QString());
        loading_ = false;
        emit edited();
        return;
    }
    QTableWidget::keyPressEvent(e);
}

}  // namespace deras::ui

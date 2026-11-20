// De-RAS - ui/NumTable.h
// Numeric input table: Excel-style paste (Ctrl+V), delete rows, always one
// empty row at the end for new data, invalid cells highlighted (never
// silently converted to zero), and an ⓘ marker in every column header.
#pragma once

#include <QTableWidget>
#include <optional>
#include <vector>

namespace deras::ui {

class NumTable : public QTableWidget {
    Q_OBJECT
public:
    NumTable(const QStringList& headers, const QStringList& helpTopics, QWidget* parent = nullptr);
    // Rows of numbers; returns false if any non-empty cell is not a number
    // (bad cells are highlighted, `badCount` reports how many).
    bool values(std::vector<std::vector<double>>& rows, int* badCount = nullptr);
    void setValues(const std::vector<std::vector<double>>& rows);
    void setDecimals(int col, int d);

signals:
    void edited();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void ensureTrailingEmptyRow();
    void pasteClipboard();
    bool loading_ = false;
    std::vector<int> decimals_;
};

std::optional<double> parseUserNumber(const QString& s);  // accepts '.' or ',' decimal

}  // namespace deras::ui

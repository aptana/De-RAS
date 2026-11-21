// De-RAS - ui/OptionsView.h
// Opsi: application appearance (Dark/Light theme, font size, language)
// and the open project's settings stored in the De-RAS sidecar
// (coordinate system, time zone, target HEC-RAS version, notes).
#pragma once

#include "View.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace deras::ui {

class Card;
class Segmented;

class OptionsView : public View {
    Q_OBJECT
public:
    explicit OptionsView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("options"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("options"); }
    void reload() override;
    void activated() override { reload(); }
    // Copies the open project and converts the copy SI <-> US (M8, Preview).
    void convertUnits();
signals:
    void openProjectRequested(const QString& prjPath);
    // Theme, font size or language changed: the main window is rebuilt.
    void appearanceChanged();
private:
    void commitProject();
    Segmented *theme_, *lang_;
    QSpinBox *font_, *autosave_;
    Card* prjCard_;
    QLineEdit *crs_, *tz_;
    QComboBox* target_;
    QPlainTextEdit* notes_;
    bool loading_ = false;
};

}  // namespace deras::ui

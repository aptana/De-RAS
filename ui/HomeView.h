// De-RAS - ui/HomeView.h
// Beranda: the "Kontrol Proyek" card, a modern version of the HEC-RAS main
// window (Project, Plan, Geometry, Steady Flow, Unsteady Flow, Description,
// unit badge), and the recent-projects list. Without a project the card
// offers New project, Open project and Example project.
#pragma once

#include "View.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QGridLayout;
class QLabel;
class QPushButton;

namespace deras::ui {

class Card;
class IconButton;

class HomeView : public View {
    Q_OBJECT
public:
    explicit HomeView(ProjectSession* s, QWidget* parent = nullptr);
    QString viewId() const override { return QStringLiteral("home"); }
    QString title() const override;
    QString helpTopic() const override { return QStringLiteral("home.control"); }
    void reload() override;
    void activated() override;
signals:
    void newProjectRequested();
    void openRequested();
    void exampleRequested();
    void openPath(const QString& prj);
    void editDescriptionRequested();
protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void timerEvent(QTimerEvent*) override;
private:
    void fillRecent();
    QWidget* recentCard(const QString& path);
    Card* card_;
    QLabel* headInfo_;
    QLineEdit *prjTitle_, *prjFile_, *planFile_, *geomFile_, *flowFile_, *unsFile_;
    QComboBox *plan_, *geom_, *flow_, *uns_;
    IconButton *folderBtn_, *planGo_, *geomGo_, *flowGo_, *unsGo_, *descBtn_;
    QPlainTextEdit* desc_;
    QLabel* units_;
    QWidget* emptyActions_;
    QGridLayout* recentGrid_;
    QLabel* recentCaption_;
    bool loading_ = false;
    int timer_ = 0;
    double phase_ = 0;
};

}  // namespace deras::ui

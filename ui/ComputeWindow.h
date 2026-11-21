// De-RAS - ui/ComputeWindow.h
// Jendela Komputasi: a separate, non-modal window (one of the only two
// separate windows, with Help). Runs plans in the separate engine process
// (deras-steady), one at a time from a queue, so the UI never freezes and an
// engine crash cannot take the project down. Shows the real steps reported
// by the engine (STEP lines), progress, elapsed time and the log; safe
// cancellation (cancel file, then kill after 5 s; results are written
// atomically by the engine).
#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "ProjectSession.h"

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace deras::ui {

class ComputeWindow : public QWidget {
    Q_OBJECT
public:
    explicit ComputeWindow(ProjectSession* s, QWidget* parent = nullptr);
    ~ComputeWindow() override;
    bool isRunning() const { return proc_ != nullptr; }
    QString runningPlan() const { return running_; }
    int progress() const { return pct_; }
    static QString enginePath();  // deras-steady next to the application
    void popUp();                 // show, raise, activate
public slots:
    void enqueue(const QString& planExt);
    void cancel();
signals:
    void runFinished(const QString& planExt, bool succeeded);
    void progressChanged(int percent);       // -1 = idle
    void runningChanged(bool running);
    void viewResultsRequested();
protected:
    void closeEvent(QCloseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
private:
    enum class StepState { Pending, Running, Done, Failed, Locked };
    void startNext();
    void onOutput();
    void onFinished(int code, QProcess::ExitStatus st);
    void log(const QString& line);
    void setStep(const QString& id, StepState st);
    void resetSteps();
    QString cancelFile(const QString& planExt) const;
    ProjectSession* s_;
    QStringList queue_;
    QProcess* proc_ = nullptr;
    QString running_, finalStatus_, currentStep_;
    QByteArray buffer_;
    QElapsedTimer clock_;
    QTimer tick_;
    int pct_ = -1;
    QLabel *title_, *state_, *badge_, *elapsed_, *percent_;
    QProgressBar* bar_;
    QPushButton *cancel_, *results_, *close_;
    QListWidget* queueList_;
    QPlainTextEdit* log_;
    struct StepRow { QString id; QLabel* icon; QLabel* text; StepState state; };
    QList<StepRow> steps_;
};

}  // namespace deras::ui

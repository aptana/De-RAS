// De-RAS - ui/ComputeWindow.cpp
#include "ComputeWindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common.h"
#include "Features.h"
#include "Help.h"
#include "Icons.h"
#include "Widgets.h"

namespace deras::ui {

ComputeWindow::ComputeWindow(ProjectSession* s, QWidget* parent) : QWidget(parent, Qt::Window), s_(s) {
    setWindowTitle(tr2("Jendela Komputasi — De-RAS", "Compute Window — De-RAS"));
    setWindowIcon(appIcon());
    setObjectName("ComputeWindow");
    resize(760, 620);
    setMinimumSize(560, 460);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    // ---- header ----
    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/branding/De-RAS-64.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    head->addWidget(logo);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(0);
    auto* t = new QLabel(tr2("Jendela Komputasi", "Compute Window"));
    t->setObjectName("ViewTitle");
    tv->addWidget(t);
    title_ = new QLabel(tr2("Tidak ada run berjalan.", "No run in progress."));
    title_->setObjectName("Mono");
    tv->addWidget(title_);
    head->addLayout(tv, 1);
    badge_ = new QLabel(tr2("SIAP", "READY"));
    badge_->setObjectName("BadgeAccent");
    head->addWidget(badge_, 0, Qt::AlignTop);
    head->addWidget(new InfoButton("compute.window"), 0, Qt::AlignTop);
    root->addLayout(head);

    // ---- steps + progress ----
    auto* sc = new Card(tr2("Tahapan", "Steps"), "compute.steps");
    auto addStep = [&](const QString& id, const QString& text, bool locked) {
        auto* h = new QHBoxLayout;
        h->setSpacing(10);
        auto* ic = new QLabel;
        ic->setFixedSize(20, 20);
        auto* tx = new QLabel(text);
        h->addWidget(ic);
        h->addWidget(tx, 1);
        if (locked) {
            auto* lb = makeBadge(QStringLiteral("RAS Mapper"), "BadgeAccent");
            lb->setToolTip(tr2("Peta genangan dibuat di RAS Mapper (Ctrl+6) dari hasil run ini, bukan di jendela ini.",
                               "Inundation maps are made in RAS Mapper (Ctrl+6) from this run's results, not in this window."));
            h->addWidget(lb);
        }
        sc->body()->addLayout(h);
        steps_.push_back({id, ic, tx, locked ? StepState::Locked : StepState::Pending});
    };
    addStep("preprocess", tr2("Persiapan: baca dan periksa geometri, aliran, dan plan", "Pre-process: read and check geometry, flow, plan"), false);
    addStep("simulate", tr2("Simulasi steady (standard step)", "Steady simulation (standard step)"), false);
    addStep("write", tr2("Tulis hasil (atomik)", "Write results (atomic)"), false);
    addStep("map", tr2("Peta genangan: dibuat di RAS Mapper", "Inundation map: made in RAS Mapper"), true);
    sc->body()->addSpacing(6);
    auto* pr = new QHBoxLayout;
    bar_ = new QProgressBar;
    bar_->setRange(0, 100);
    bar_->setValue(0);
    bar_->setTextVisible(false);
    bar_->setAccessibleName(tr2("Kemajuan run", "Run progress"));
    percent_ = new QLabel(QStringLiteral("0%"));
    percent_->setObjectName("Mono");
    percent_->setMinimumWidth(44);
    percent_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pr->addWidget(bar_, 1);
    pr->addWidget(percent_);
    sc->body()->addLayout(pr);
    auto* st = new QHBoxLayout;
    state_ = makeMuted(tr2("Siap. Tekan Hitung (F5) di jendela utama untuk menjalankan plan aktif.", "Ready. Press Compute (F5) in the main window to run the current plan."));
    elapsed_ = new QLabel;
    elapsed_->setObjectName("Mono");
    st->addWidget(state_, 1);
    st->addWidget(elapsed_);
    sc->body()->addLayout(st);
    root->addWidget(sc);

    // ---- queue + log ----
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* qc = new Card(tr2("Antrean", "Queue"), "compute.queue");
    qc->setFixedWidth(170);
    queueList_ = new QListWidget;
    queueList_->setObjectName("NavList");
    queueList_->setFont(monoFont());
    qc->body()->addWidget(queueList_, 1);
    row->addWidget(qc);
    auto* lc = new Card(tr2("Log", "Log"), "compute.log");
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);
    log_->setFont(monoFont(Theme::current().basePx - 2));
    log_->setAccessibleName(tr2("Log run", "Run log"));
    lc->body()->addWidget(log_, 1);
    row->addWidget(lc, 1);
    root->addLayout(row, 1);

    // ---- footer ----
    auto* foot = new QHBoxLayout;
    cancel_ = makeButton(tr2("Batalkan", "Cancel"), false, "stop");
    cancel_->setObjectName("Danger");
    cancel_->setEnabled(false);
    foot->addWidget(cancel_);
    foot->addStretch();
    results_ = makeButton(tr2("Lihat hasil", "View results"), true, "profile");
    results_->setEnabled(false);
    close_ = makeButton(tr2("Tutup", "Close"));
    foot->addWidget(close_);
    foot->addWidget(results_);
    root->addLayout(foot);

    connect(cancel_, &QPushButton::clicked, this, &ComputeWindow::cancel);
    connect(close_, &QPushButton::clicked, this, &QWidget::close);
    connect(results_, &QPushButton::clicked, this, [this]() { emit viewResultsRequested(); hide(); });
    tick_.setInterval(250);
    connect(&tick_, &QTimer::timeout, this, [this]() {
        elapsed_->setText(QStringLiteral("%1 s").arg(clock_.elapsed() / 1000.0, 0, 'f', 1));
    });
    resetSteps();
    applyWindowChrome(this);
}

ComputeWindow::~ComputeWindow() {
    if (proc_) {
        proc_->disconnect();
        proc_->kill();
        proc_->waitForFinished(2000);
    }
}

QString ComputeWindow::enginePath() {
#ifdef Q_OS_WIN
    return QDir(QCoreApplication::applicationDirPath()).filePath("deras-steady.exe");
#else
    return QDir(QCoreApplication::applicationDirPath()).filePath("deras-steady");
#endif
}

void ComputeWindow::popUp() {
    if (isMinimized()) showNormal();
    show();
    raise();
    activateWindow();
}

QString ComputeWindow::cancelFile(const QString& planExt) const {
    auto* p = s_->project();
    if (!p) return {};
    return qs(io::fromPath(p->dir() / io::toPath(p->baseName() + "." + ss(planExt) + ".deras-cancel")));
}

void ComputeWindow::log(const QString& line) {
    log_->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + line);
    emit s_->logMessage(line);
}

void ComputeWindow::resetSteps() {
    for (auto& r : steps_) setStep(r.id, r.state == StepState::Locked ? StepState::Locked : StepState::Pending);
}

void ComputeWindow::setStep(const QString& id, StepState st) {
    const Theme& th = Theme::current();
    for (auto& r : steps_) {
        if (r.id != id) continue;
        r.state = st;
        QString icon = "circle";
        QColor c = th.text3;
        switch (st) {
            case StepState::Pending: icon = "circle"; c = th.text3; break;
            case StepState::Running: icon = "play"; c = th.accent; break;
            case StepState::Done: icon = "check"; c = th.success; break;
            case StepState::Failed: icon = "close"; c = th.error; break;
            case StepState::Locked: icon = "lock"; c = th.text3; break;
        }
        r.icon->setPixmap(iconPixmap(icon, 18, c));
        r.text->setStyleSheet(QStringLiteral("color:%1;%2").arg(cssColor(st == StepState::Pending || st == StepState::Locked ? th.text3 : th.text),
                                                               st == StepState::Running ? QStringLiteral("font-weight:600;") : QString()));
    }
}

void ComputeWindow::enqueue(const QString& planExt) {
    if (planExt.isEmpty() || queue_.contains(planExt) || running_ == planExt) return;
    queue_ << planExt;
    queueList_->addItem(planExt);
    log(tr2("Plan %1 masuk antrean.", "Plan %1 queued.").arg(planExt));
    if (!proc_) QTimer::singleShot(0, this, &ComputeWindow::startNext);
}

void ComputeWindow::startNext() {
    if (proc_ || queue_.isEmpty()) return;
    auto* p = s_->project();
    if (!p) { queue_.clear(); queueList_->clear(); return; }
    // The engine reads the files on disk, so save first.
    MessageList msgs;
    if (p->isModified() && !s_->save(msgs)) {
        for (const auto& m : msgs) log(severityPrefix(m.severity) + " " + msgText(m));
        state_->setText(QStringLiteral("⛔ ") + tr2("Proyek gagal disimpan, jadi run dibatalkan. Lihat log.", "The project could not be saved, so the run was cancelled. See the log."));
        queue_.clear();
        queueList_->clear();
        return;
    }
    const QString exe = enginePath();
    if (!QFileInfo::exists(exe)) {
        const QString t = QStringLiteral("⛔ ") + tr2("Mesin hitung tidak ditemukan: %1. Pasang ulang De-RAS dengan installer.", "Compute engine not found: %1. Reinstall De-RAS with the installer.").arg(QDir::toNativeSeparators(exe));
        log(t);
        state_->setText(t);
        queue_.clear();
        queueList_->clear();
        return;
    }
    running_ = queue_.takeFirst();
    delete queueList_->takeItem(0);
    QFile::remove(cancelFile(running_));
    finalStatus_.clear();
    currentStep_.clear();
    buffer_.clear();
    resetSteps();
    {
        const auto it = p->plans.find(ss(running_));
        const bool uns = it != p->plans.end() && it->second.model.isUnsteady();
        for (auto& r : steps_)
            if (r.id == QLatin1String("simulate"))
                r.text->setText(uns ? tr2("Simulasi unsteady 1D (Preissmann implisit, Pratinjau)", "1D unsteady simulation (Preissmann implicit, Preview)")
                                    : tr2("Simulasi steady (standard step)", "Steady simulation (standard step)"));
    }
    proc_ = new QProcess(this);
    proc_->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc_, &QProcess::readyReadStandardOutput, this, &ComputeWindow::onOutput);
    connect(proc_, &QProcess::finished, this, &ComputeWindow::onFinished);
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            log(QStringLiteral("⛔ ") + tr2("Mesin hitung gagal dijalankan: %1", "Compute engine failed to start: %1").arg(proc_->errorString()));
            onFinished(-1, QProcess::CrashExit);
        }
    });
    bar_->setValue(0);
    percent_->setText(QStringLiteral("0%"));
    pct_ = 0;
    emit progressChanged(0);
    emit runningChanged(true);
    cancel_->setEnabled(true);
    results_->setEnabled(false);
    auto* pl = s_->currentPlan();
    title_->setText(QStringLiteral("%1 · %2").arg(qs(p->baseName()) + ".prj", running_ + (pl && qs(p->info().currentPlan) == running_ ? QStringLiteral(" · ") + qs(pl->model.title) : QString())));
    badge_->setText(tr2("BERJALAN", "RUNNING"));
    badge_->setObjectName("BadgePreview");
    badge_->style()->unpolish(badge_);
    badge_->style()->polish(badge_);
    state_->setText(tr2("Menjalankan plan %1…", "Running plan %1…").arg(running_));
    clock_.start();
    tick_.start();
    log(tr2("Mulai run plan %1 (%2).", "Starting run of plan %1 (%2).").arg(running_, QFileInfo(exe).fileName()));
    proc_->start(exe, {s_->prjPath(), "--plan", running_, "--lang", isID() ? "id" : "en", "--cancel-file", cancelFile(running_)});
}

void ComputeWindow::onOutput() {
    if (!proc_) return;
    buffer_ += proc_->readAllStandardOutput();
    int nl;
    while ((nl = buffer_.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(buffer_.left(nl)).trimmed();
        buffer_.remove(0, nl + 1);
        if (line.startsWith("PROGRESS ")) {
            const auto parts = line.split(' ');
            if (parts.size() >= 3 && parts[2].toInt() > 0) {
                pct_ = parts[1].toInt() * 100 / parts[2].toInt();
                bar_->setValue(pct_);
                percent_->setText(QStringLiteral("%1%").arg(pct_));
                emit progressChanged(pct_);
            }
        } else if (line.startsWith("STEP ")) {
            const QString id = line.section(' ', 1, 1);
            if (!currentStep_.isEmpty()) setStep(currentStep_, StepState::Done);
            currentStep_ = id;
            setStep(id, StepState::Running);
        } else if (line.startsWith("MSG ")) {
            const int sp = line.indexOf(' ', 4);
            const QString sev = line.mid(4, sp - 4);
            const QString txt = line.mid(sp + 1);
            log((sev == "error" ? QStringLiteral("⛔ ") : sev == "warning" ? QStringLiteral("⚠ ") : QStringLiteral("ℹ ")) + txt);
        } else if (line.startsWith("STATUS ")) {
            finalStatus_ = line.section(' ', 1, 1);
        } else if (!line.isEmpty()) {
            log(line);
        }
    }
}

void ComputeWindow::onFinished(int code, QProcess::ExitStatus st) {
    if (!proc_) return;
    onOutput();
    const QString plan = running_;
    proc_->deleteLater();
    proc_ = nullptr;
    running_.clear();
    tick_.stop();
    elapsed_->setText(QStringLiteral("%1 s").arg(clock_.elapsed() / 1000.0, 0, 'f', 2));
    cancel_->setEnabled(false);
    QFile::remove(cancelFile(plan));
    const bool ok = st == QProcess::NormalExit && code == 0 && finalStatus_ == "succeeded";
    const bool cancelled = code == 3 || finalStatus_ == "cancelled";
    if (!currentStep_.isEmpty()) setStep(currentStep_, ok ? StepState::Done : StepState::Failed);
    QString text;
    if (ok) {
        bar_->setValue(100);
        percent_->setText(QStringLiteral("100%"));
        text = QStringLiteral("✓ ") + tr2("Plan %1 selesai dihitung.", "Plan %1 computed successfully.").arg(plan);
    } else if (cancelled) {
        text = QStringLiteral("⏹ ") + tr2("Plan %1 dibatalkan. Hasil lama tidak diubah.", "Plan %1 cancelled. Previous results were not changed.").arg(plan);
    } else if (st == QProcess::CrashExit) {
        text = QStringLiteral("⛔ ") + tr2("Mesin hitung berhenti mendadak saat menghitung plan %1. Proyek Anda aman. Kirimkan isi log ke pengembang.", "The compute engine stopped abnormally on plan %1. The project is safe; please report the log.").arg(plan);
    } else {
        text = QStringLiteral("⛔ ") + tr2("Plan %1 gagal. Periksa log dan jalankan Model Doctor (F7).", "Plan %1 failed. Check the log and run Model Doctor (F7).").arg(plan);
    }
    badge_->setText(ok ? tr2("SELESAI", "DONE") : cancelled ? tr2("DIBATALKAN", "CANCELLED") : tr2("GAGAL", "FAILED"));
    badge_->setObjectName(ok ? "BadgeOk" : cancelled ? "BadgeLocked" : "BadgeError");
    badge_->style()->unpolish(badge_);
    badge_->style()->polish(badge_);
    state_->setText(text);
    results_->setEnabled(ok);
    log(text);
    pct_ = -1;
    emit progressChanged(-1);
    emit runningChanged(false);
    emit runFinished(plan, ok);
    emit s_->resultsUpdated(plan);
    QTimer::singleShot(0, this, &ComputeWindow::startNext);
}

void ComputeWindow::cancel() {
    if (!proc_) return;
    log(tr2("Membatalkan plan %1…", "Cancelling plan %1…").arg(running_));
    QFile f(cancelFile(running_));
    if (f.open(QIODevice::WriteOnly)) f.close();
    // If the engine does not stop by itself within 5 s, terminate it. Results
    // are written atomically, so the project files are never half-written.
    QPointer<QProcess> p = proc_;
    QTimer::singleShot(5000, this, [p]() { if (p && p->state() != QProcess::NotRunning) p->kill(); });
}

void ComputeWindow::closeEvent(QCloseEvent* e) {
    if (!proc_) { e->accept(); return; }
    const int c = sheetChoice(this, tr2("Run masih berjalan", "A run is in progress"),
                              tr2("Plan %1 masih dihitung. Jendela ini boleh ditutup; run tetap jalan dan kemajuannya terlihat di bilah status jendela utama.",
                                  "Plan %1 is still computing. You can close this window and the run keeps going; its progress shows in the main window status bar.").arg(running_),
                              {tr2("Kembali", "Back"), tr2("Batalkan run", "Cancel run"), tr2("Tutup, tetap jalan", "Close, keep running")}, 2);
    if (c == 1) { cancel(); e->ignore(); return; }
    if (c == 2) { e->accept(); return; }
    e->ignore();
}

void ComputeWindow::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { close(); return; }
    QWidget::keyPressEvent(e);
}

}  // namespace deras::ui

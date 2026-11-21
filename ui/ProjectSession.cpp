// De-RAS - ui/ProjectSession.cpp
#include "ProjectSession.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <filesystem>

#include "Common.h"
#include "app/ModelDoctor.h"
#include "io/ResultsFile.h"
#include "Version.h"

namespace deras::ui {

ProjectSession::ProjectSession(QObject* parent) : QObject(parent) {
    // drop the cached results of a plan as soon as a run finishes (before views reload)
    connect(this, &ProjectSession::resultsUpdated, this, [this](const QString& e) { results_.erase(e); resultsErr_.erase(e); });
}

void ProjectSession::setProject(std::unique_ptr<io::Project> p) {
    prj_ = std::move(p);
    editGeom_.clear();
    editUnsteady_.clear();
    editFlow_.clear();
    results_.clear();
    resultsErr_.clear();
    selReach_ = selXs_ = 0;
    emit projectChanged();
}

void ProjectSession::close() {
    prj_.reset();
    editGeom_.clear();
    editUnsteady_.clear();
    editFlow_.clear();
    results_.clear();
    resultsErr_.clear();
    emit projectChanged();
}

bool ProjectSession::isExample() const {
    if (!prj_) return false;
    const auto& t = prj_->info().title;
    return t.rfind("Contoh", 0) == 0 || t.rfind("Example", 0) == 0 || prj_->baseName() == "Contoh" || prj_->baseName() == "DerasUji1km";
}

QString ProjectSession::installExample(const QString& dir) {
    // The example is the standard test project (review R5 of build 0.2.0),
    // bundled as a resource and copied once; an existing copy is kept as is.
    const QString prj = QDir(dir).filePath("DerasUji1km.prj");
    if (QFileInfo::exists(prj)) return prj;
    if (!QDir().mkpath(dir)) return {};
    QDir(dir).mkpath(QStringLiteral("Terrain"));
    for (const char* ext : {"prj", "g01", "g02", "f01", "p01", "p02", "p03", "u01", "u02", "deras.json", "terrain.tif"}) {
        const QString name = QString::fromLatin1(ext) == QLatin1String("terrain.tif") ? QStringLiteral("Terrain/DerasUji1km_terrain.tif")
                                                                                     : QStringLiteral("DerasUji1km.%1").arg(QLatin1String(ext));
        const QString to = QDir(dir).filePath(name);
        if (!QFile::copy(QStringLiteral(":/example/") + name, to)) return {};
        QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
    }
    QFile mark(QDir(dir).filePath(QStringLiteral("versi-contoh.txt")));
    if (mark.open(QIODevice::WriteOnly | QIODevice::Truncate)) mark.write(QByteArrayLiteral(DERAS_VERSION "\n"));
    return prj;
}

QString ProjectSession::exampleVersion(const QString& dir) {
    QFile mark(QDir(dir).filePath(QStringLiteral("versi-contoh.txt")));
    if (!mark.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(mark.readLine()).trimmed();
}

QString ProjectSession::restoreExample(const QString& dir, QString* movedTo) {
    if (QFileInfo::exists(dir)) {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
        QString to = dir + QStringLiteral("-lama-") + stamp;
        for (int i = 2; QFileInfo::exists(to); ++i) to = dir + QStringLiteral("-lama-") + stamp + QStringLiteral("-%1").arg(i);
        if (!QDir().rename(dir, to)) return {};
        if (movedTo) *movedTo = to;
    }
    return installExample(dir);
}

QString ProjectSession::prjPath() const { return prj_ ? qs(io::fromPath(prj_->prjPath())) : QString(); }
QString ProjectSession::title() const { return prj_ ? qs(prj_->info().title) : QString(); }

QString ProjectSession::currentPlanExt() const { return prj_ ? qs(prj_->info().currentPlan) : QString(); }

void ProjectSession::setCurrentPlan(const QString& ext) {
    if (!prj_ || qs(prj_->info().currentPlan) == ext) return;
    prj_->info().currentPlan = ss(ext);
    editGeom_.clear();
    editFlow_.clear();
    editUnsteady_.clear();
    emit modelEdited();
    emit projectChanged();
}

io::PlanFile* ProjectSession::currentPlan() const {
    if (!prj_) return nullptr;
    auto it = prj_->plans.find(prj_->info().currentPlan);
    return it == prj_->plans.end() ? nullptr : &it->second;
}

QString ProjectSession::currentGeometryExt() const {
    if (!prj_) return {};
    if (!editGeom_.isEmpty() && prj_->geometries.count(ss(editGeom_))) return editGeom_;
    if (auto* p = currentPlan(); p && prj_->geometries.count(p->model.geomFile)) return qs(p->model.geomFile);
    return prj_->geometries.empty() ? QString() : qs(prj_->geometries.begin()->first);
}

QString ProjectSession::currentFlowExt() const {
    if (!prj_) return {};
    if (!editFlow_.isEmpty() && prj_->steadyFlows.count(ss(editFlow_))) return editFlow_;
    if (auto* p = currentPlan(); p && prj_->steadyFlows.count(p->model.flowFile)) return qs(p->model.flowFile);
    return prj_->steadyFlows.empty() ? QString() : qs(prj_->steadyFlows.begin()->first);
}

void ProjectSession::setEditGeometry(const QString& ext) {
    if (editGeom_ == ext) return;
    editGeom_ = ext;
    emit projectChanged();
}

void ProjectSession::setEditFlow(const QString& ext) {
    if (editFlow_ == ext) return;
    editFlow_ = ext;
    emit projectChanged();
}

io::GeometryFile* ProjectSession::currentGeometry() const {
    QString e = currentGeometryExt();
    if (e.isEmpty()) return nullptr;
    return &prj_->geometries.at(ss(e));
}

io::SteadyFlowFile* ProjectSession::currentFlow() const {
    QString e = currentFlowExt();
    if (e.isEmpty()) return nullptr;
    return &prj_->steadyFlows.at(ss(e));
}

QString ProjectSession::currentUnsteadyExt() const {
    if (!prj_) return {};
    if (!editUnsteady_.isEmpty() && prj_->unsteadyFlows.count(ss(editUnsteady_))) return editUnsteady_;
    if (auto* p = currentPlan(); p && prj_->unsteadyFlows.count(p->model.flowFile)) return qs(p->model.flowFile);
    return prj_->unsteadyFlows.empty() ? QString() : qs(prj_->unsteadyFlows.begin()->first);
}

io::UnsteadyFlowFile* ProjectSession::currentUnsteady() const {
    const QString e = currentUnsteadyExt();
    return e.isEmpty() ? nullptr : &prj_->unsteadyFlows.at(ss(e));
}

void ProjectSession::setEditUnsteady(const QString& ext) {
    if (editUnsteady_ == ext) return;
    editUnsteady_ = ext;
    emit projectChanged();
}

QString ProjectSession::unsteadyTitle(const QString& ext) const {
    if (!prj_) return {};
    if (auto it = prj_->unsteadyFlows.find(ss(ext)); it != prj_->unsteadyFlows.end()) return qs(it->second.model.title);
    QFile f(qs(io::fromPath(prj_->pathFor(ss(ext)))));
    if (!f.open(QIODevice::ReadOnly)) return {};
    for (int i = 0; i < 40 && !f.atEnd(); ++i) {
        const QString line = QString::fromLatin1(f.readLine()).trimmed();
        if (line.startsWith("Flow Title=")) return line.mid(11).trimmed();
    }
    return {};
}

bool ProjectSession::save(MessageList& msgs) {
    if (!prj_) return false;
    bool ok = prj_->save(msgs);
    if (ok) {
        AppSettings::addRecentProject(prjPath());
        emit saved();
    }
    return ok;
}

const RunResult* ProjectSession::results(const QString& planExt) {
    if (!prj_ || planExt.isEmpty()) return nullptr;
    auto it = results_.find(planExt);
    if (it != results_.end()) return &it->second;
    if (resultsErr_.count(planExt)) return nullptr;
    auto path = io::resultsPathFor(prj_->dir(), prj_->baseName(), ss(planExt));
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) { resultsErr_[planExt] = QString(); return nullptr; }
    RunResult r;
    std::string err;
    if (!io::readResults(path, r, err)) { resultsErr_[planExt] = qs(err); return nullptr; }
    return &(results_[planExt] = std::move(r));
}

QString ProjectSession::resultsError(const QString& planExt) const {
    auto it = resultsErr_.find(planExt);
    return it == resultsErr_.end() ? QString() : it->second;
}

bool ProjectSession::findLocation(const QString& location, int& reachUid, int& xsUid) const {
    auto* g = currentGeometry();
    if (!g) return false;
    static const QRegularExpression re(QStringLiteral("^(.+?) / (.+?) RS (\\S+)"));
    const auto m = re.match(location);
    if (!m.hasMatch()) return false;
    for (const auto& r : g->model.reaches) {
        if (qs(r.river) != m.captured(1).trimmed() || qs(r.reach) != m.captured(2).trimmed()) continue;
        for (const auto& n : r.nodes)
            if (qs(n.rs) == m.captured(3)) { reachUid = r.uid; xsUid = n.uid; return true; }
    }
    return false;
}

MessageList ProjectSession::doctor() const {
    auto* pl = currentPlan();
    if (!prj_ || !pl) return {makeMsg(Severity::Error, "PLAN_NONE")};
    const auto gi = prj_->geometries.find(pl->model.geomFile);
    if (pl->model.isUnsteady()) {
        const auto ui = prj_->unsteadyFlows.find(pl->model.flowFile);
        return app::checkUnsteadyRun(pl->model, gi == prj_->geometries.end() ? nullptr : &gi->second.model,
                                     ui == prj_->unsteadyFlows.end() ? nullptr : &ui->second.model, prj_->units());
    }
    const auto fi = prj_->steadyFlows.find(pl->model.flowFile);
    return app::checkSteadyRun(pl->model, gi == prj_->geometries.end() ? nullptr : &gi->second.model,
                               fi == prj_->steadyFlows.end() ? nullptr : &fi->second.model, prj_->units());
}

}  // namespace deras::ui

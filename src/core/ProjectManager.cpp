#include "core/ProjectManager.h"

#include "shared/Logging.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/StorageService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QUuid>

namespace qtcode::core {

ProjectManager::ProjectManager(storage::StorageService &storageService)
    : m_storageService(storageService)
{
}

bool ProjectManager::restoreState(QString *errorMessage)
{
    if (!refreshProjects(errorMessage)) {
        return false;
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!settingsRepository.loadJson(settings::kActiveProjectSettingKey, &json, &found, errorMessage)) {
        return false;
    }

    if (!found) {
        qCInfo(qtcodeCore) << "No active project saved yet";
        return true;
    }

    const QString projectId = json.value(QStringLiteral("projectId")).toString();
    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Active project setting is missing projectId.");
        }
        return false;
    }

    settings::ProjectRecord project;
    if (!openProject(projectId, &project, errorMessage)) {
        qCWarning(qtcodeCore) << "Saved active project was not found:" << projectId;
        m_activeProjectId.clear();
        return true;
    }

    qCInfo(qtcodeCore) << "Restored active project" << project.name << "at" << project.rootPath;
    return true;
}

bool ProjectManager::addLocalRepository(
    const QString &path,
    settings::ProjectRecord *project,
    QString *errorMessage)
{
    const QString normalizedPath = normalizeLocalPath(path, errorMessage);
    if (normalizedPath.isEmpty()) {
        return false;
    }

    storage::ProjectRepository repository(m_storageService);
    settings::ProjectRecord existingProject;
    bool found = false;
    if (!repository.findProjectByRootPath(normalizedPath, &existingProject, &found, errorMessage)) {
        return false;
    }

    if (found) {
        if (!openProject(existingProject.id, project, errorMessage)) {
            return false;
        }

        qCInfo(qtcodeCore) << "Opened existing project" << existingProject.name;
        return true;
    }

    const QString timestamp = currentTimestamp();
    settings::ProjectRecord newProject;
    newProject.id = createId();
    newProject.name = projectNameFromPath(normalizedPath);
    newProject.rootPath = normalizedPath;
    newProject.createdAt = timestamp;
    newProject.updatedAt = timestamp;
    newProject.lastOpenedAt = timestamp;

    settings::RepositoryRecord newRepository;
    newRepository.id = createId();
    newRepository.projectId = newProject.id;
    newRepository.localPath = normalizedPath;
    newRepository.createdAt = timestamp;
    newRepository.updatedAt = timestamp;

    if (!m_storageService.beginTransaction(errorMessage)) {
        return false;
    }

    if (!repository.insertProject(newProject, errorMessage)
        || !repository.insertRepository(newRepository, errorMessage)) {
        if (!m_storageService.rollbackTransaction()) {
            // Best-effort rollback after a failed project insert.
        }
        return false;
    }

    if (!m_storageService.commitTransaction(errorMessage)) {
        if (!m_storageService.rollbackTransaction()) {
            // Best-effort rollback after a failed project commit.
        }
        return false;
    }

    if (!refreshProjects(errorMessage) || !setActiveProject(newProject.id, errorMessage)) {
        return false;
    }

    if (project != nullptr) {
        *project = newProject;
    }

    qCInfo(qtcodeCore) << "Added local repository project" << newProject.name << "at" << normalizedPath;
    return true;
}

bool ProjectManager::openProject(
    const QString &projectId,
    settings::ProjectRecord *project,
    QString *errorMessage)
{
    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project id must not be empty.");
        }
        return false;
    }

    storage::ProjectRepository repository(m_storageService);
    settings::ProjectRecord loadedProject;
    bool found = false;
    if (!repository.findProjectById(projectId, &loadedProject, &found, errorMessage)) {
        return false;
    }

    if (!found) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project '%1' was not found.").arg(projectId);
        }
        return false;
    }

    if (!touchProject(projectId, errorMessage) || !setActiveProject(projectId, errorMessage)) {
        return false;
    }

    if (project != nullptr) {
        *project = loadedProject;
    }

    qCInfo(qtcodeCore) << "Opened project" << loadedProject.name;
    return true;
}

bool ProjectManager::setActiveProject(const QString &projectId, QString *errorMessage)
{
    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project id must not be empty.");
        }
        return false;
    }

    storage::ProjectRepository repository(m_storageService);
    settings::ProjectRecord project;
    bool found = false;
    if (!repository.findProjectById(projectId, &project, &found, errorMessage)) {
        return false;
    }

    if (!found) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project '%1' was not found.").arg(projectId);
        }
        return false;
    }

    m_activeProjectId = projectId;
    if (!persistActiveProjectId(errorMessage)) {
        return false;
    }

    qCInfo(qtcodeCore) << "Active project set to" << project.name;
    return true;
}

QList<settings::ProjectRecord> ProjectManager::projects() const
{
    return m_projects;
}

bool ProjectManager::hasActiveProject() const
{
    return !m_activeProjectId.isEmpty();
}

QString ProjectManager::activeProjectId() const
{
    return m_activeProjectId;
}

bool ProjectManager::activeProject(settings::ProjectRecord *project) const
{
    if (project == nullptr || m_activeProjectId.isEmpty()) {
        return false;
    }

    for (const settings::ProjectRecord &record : m_projects) {
        if (record.id == m_activeProjectId) {
            *project = record;
            return true;
        }
    }

    return false;
}

bool ProjectManager::refreshProjects(QString *errorMessage)
{
    storage::ProjectRepository repository(m_storageService);
    if (!repository.listProjects(&m_projects, errorMessage)) {
        return false;
    }

    qCDebug(qtcodeCore) << "Loaded" << m_projects.size() << "project(s)";
    return true;
}

bool ProjectManager::persistActiveProjectId(QString *errorMessage)
{
    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    json.insert(QStringLiteral("projectId"), m_activeProjectId);
    return settingsRepository.upsertJson(settings::kActiveProjectSettingKey, json, errorMessage);
}

bool ProjectManager::touchProject(const QString &projectId, QString *errorMessage)
{
    storage::ProjectRepository repository(m_storageService);
    if (!repository.updateProjectLastOpened(projectId, currentTimestamp(), errorMessage)) {
        return false;
    }

    return refreshProjects(errorMessage);
}

QString ProjectManager::normalizeLocalPath(const QString &path, QString *errorMessage)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Repository path must not be empty.");
        }
        return {};
    }

    QFileInfo fileInfo(trimmedPath);
    if (!fileInfo.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Repository path does not exist: %1").arg(trimmedPath);
        }
        return {};
    }

    if (!fileInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Repository path is not a directory: %1").arg(trimmedPath);
        }
        return {};
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Repository path could not be resolved: %1").arg(trimmedPath);
        }
        return {};
    }

    return QDir::cleanPath(canonicalPath);
}

QString ProjectManager::currentTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString ProjectManager::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString ProjectManager::projectNameFromPath(const QString &path)
{
    const QString baseName = QFileInfo(path).fileName();
    return baseName.isEmpty() ? path : baseName;
}

} // namespace qtcode::core

#include "storage/repositories/ProjectRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

settings::ProjectRecord projectFromQuery(const QSqlQuery &query)
{
    settings::ProjectRecord project;
    project.id = query.value(QStringLiteral("id")).toString();
    project.name = query.value(QStringLiteral("name")).toString();
    project.rootPath = query.value(QStringLiteral("root_path")).toString();
    project.createdAt = query.value(QStringLiteral("created_at")).toString();
    project.updatedAt = query.value(QStringLiteral("updated_at")).toString();
    project.lastOpenedAt = query.value(QStringLiteral("last_opened_at")).toString();
    return project;
}

settings::RepositoryRecord repositoryFromQuery(const QSqlQuery &query)
{
    settings::RepositoryRecord repository;
    repository.id = query.value(QStringLiteral("id")).toString();
    repository.projectId = query.value(QStringLiteral("project_id")).toString();
    repository.localPath = query.value(QStringLiteral("local_path")).toString();
    repository.remoteUrl = query.value(QStringLiteral("remote_url")).toString();
    repository.defaultBranch = query.value(QStringLiteral("default_branch")).toString();
    repository.githubOwner = query.value(QStringLiteral("github_owner")).toString();
    repository.githubName = query.value(QStringLiteral("github_name")).toString();
    repository.createdAt = query.value(QStringLiteral("created_at")).toString();
    repository.updatedAt = query.value(QStringLiteral("updated_at")).toString();
    return repository;
}

} // namespace

ProjectRepository::ProjectRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool ProjectRepository::insertProject(
    const settings::ProjectRecord &project,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO projects (id, name, root_path, created_at, updated_at, last_opened_at) "
        "VALUES (:id, :name, :root_path, :created_at, :updated_at, :last_opened_at)"));
    query.bindValue(QStringLiteral(":id"), project.id);
    query.bindValue(QStringLiteral(":name"), project.name);
    query.bindValue(QStringLiteral(":root_path"), project.rootPath);
    query.bindValue(QStringLiteral(":created_at"), project.createdAt);
    query.bindValue(QStringLiteral(":updated_at"), project.updatedAt);
    query.bindValue(QStringLiteral(":last_opened_at"), project.lastOpenedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert project: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert project" << message;
        return false;
    }

    return true;
}

bool ProjectRepository::insertRepository(
    const settings::RepositoryRecord &repository,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO repositories ("
        "id, project_id, local_path, remote_url, default_branch, "
        "github_owner, github_name, created_at, updated_at) "
        "VALUES ("
        ":id, :project_id, :local_path, :remote_url, :default_branch, "
        ":github_owner, :github_name, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":id"), repository.id);
    query.bindValue(QStringLiteral(":project_id"), repository.projectId);
    query.bindValue(QStringLiteral(":local_path"), repository.localPath);
    query.bindValue(QStringLiteral(":remote_url"), repository.remoteUrl);
    query.bindValue(QStringLiteral(":default_branch"), repository.defaultBranch);
    query.bindValue(QStringLiteral(":github_owner"), repository.githubOwner);
    query.bindValue(QStringLiteral(":github_name"), repository.githubName);
    query.bindValue(QStringLiteral(":created_at"), repository.createdAt);
    query.bindValue(QStringLiteral(":updated_at"), repository.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert repository: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert repository" << message;
        return false;
    }

    return true;
}

bool ProjectRepository::updateProjectLastOpened(
    const QString &projectId,
    const QString &timestamp,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE projects "
        "SET last_opened_at = :last_opened_at, updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), projectId);
    query.bindValue(QStringLiteral(":last_opened_at"), timestamp);
    query.bindValue(QStringLiteral(":updated_at"), timestamp);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update project last opened: %1").arg(message);
        }
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool ProjectRepository::findProjectByRootPath(
    const QString &rootPath,
    settings::ProjectRecord *project,
    bool *found,
    QString *errorMessage) const
{
    if (found != nullptr) {
        *found = false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, name, root_path, created_at, updated_at, last_opened_at "
        "FROM projects WHERE root_path = :root_path"));
    query.bindValue(QStringLiteral(":root_path"), rootPath);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query project by root path: %1").arg(message);
        }
        return false;
    }

    if (!query.next()) {
        return true;
    }

    if (project != nullptr) {
        *project = projectFromQuery(query);
    }
    if (found != nullptr) {
        *found = true;
    }

    return true;
}

bool ProjectRepository::findProjectById(
    const QString &projectId,
    settings::ProjectRecord *project,
    bool *found,
    QString *errorMessage) const
{
    if (found != nullptr) {
        *found = false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, name, root_path, created_at, updated_at, last_opened_at "
        "FROM projects WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), projectId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query project by id: %1").arg(message);
        }
        return false;
    }

    if (!query.next()) {
        return true;
    }

    if (project != nullptr) {
        *project = projectFromQuery(query);
    }
    if (found != nullptr) {
        *found = true;
    }

    return true;
}

bool ProjectRepository::listProjects(
    QList<settings::ProjectRecord> *projects,
    QString *errorMessage) const
{
    if (projects != nullptr) {
        projects->clear();
    }

    QSqlQuery query(m_storageService.database());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, root_path, created_at, updated_at, last_opened_at "
            "FROM projects "
            "ORDER BY "
            "CASE WHEN last_opened_at IS NULL THEN 1 ELSE 0 END, "
            "last_opened_at DESC, "
            "updated_at DESC"))) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list projects: %1").arg(message);
        }
        return false;
    }

    if (projects == nullptr) {
        return true;
    }

    while (query.next()) {
        projects->append(projectFromQuery(query));
    }

    return true;
}

bool ProjectRepository::updatePrimaryRepositoryBranch(
    const QString &projectId,
    const QString &branchName,
    const QString &timestamp,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE repositories "
        "SET default_branch = :default_branch, updated_at = :updated_at "
        "WHERE id = ("
        "SELECT id FROM repositories WHERE project_id = :project_id "
        "ORDER BY created_at ASC LIMIT 1"
        ")"));
    query.bindValue(QStringLiteral(":project_id"), projectId);
    query.bindValue(QStringLiteral(":default_branch"), branchName);
    query.bindValue(QStringLiteral(":updated_at"), timestamp);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update repository branch: %1").arg(message);
        }
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool ProjectRepository::updatePrimaryRepositoryRemoteMetadata(
    const QString &projectId,
    const QString &remoteUrl,
    const QString &githubOwner,
    const QString &githubName,
    const QString &timestamp,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE repositories "
        "SET remote_url = :remote_url, "
        "github_owner = :github_owner, "
        "github_name = :github_name, "
        "updated_at = :updated_at "
        "WHERE id = ("
        "SELECT id FROM repositories WHERE project_id = :project_id "
        "ORDER BY created_at ASC LIMIT 1"
        ")"));
    query.bindValue(QStringLiteral(":project_id"), projectId);
    query.bindValue(QStringLiteral(":remote_url"), remoteUrl);
    query.bindValue(QStringLiteral(":github_owner"), githubOwner);
    query.bindValue(QStringLiteral(":github_name"), githubName);
    query.bindValue(QStringLiteral(":updated_at"), timestamp);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update repository remote metadata: %1").arg(message);
        }
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool ProjectRepository::findPrimaryRepository(
    const QString &projectId,
    settings::RepositoryRecord *repository,
    bool *found,
    QString *errorMessage) const
{
    if (found != nullptr) {
        *found = false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, project_id, local_path, remote_url, default_branch, "
        "github_owner, github_name, created_at, updated_at "
        "FROM repositories WHERE project_id = :project_id "
        "ORDER BY created_at ASC LIMIT 1"));
    query.bindValue(QStringLiteral(":project_id"), projectId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query repository: %1").arg(message);
        }
        return false;
    }

    if (!query.next()) {
        return true;
    }

    if (repository != nullptr) {
        *repository = repositoryFromQuery(query);
    }
    if (found != nullptr) {
        *found = true;
    }

    return true;
}

} // namespace qtcode::storage

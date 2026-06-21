#pragma once

#include "settings/ProjectModels.h"

#include <QList>
#include <QString>

#include <QObject>

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class ProjectManager final : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(
        storage::StorageService &storageService,
        git::GitService &gitService,
        QObject *parent = nullptr);

    [[nodiscard]] bool restoreState(QString *errorMessage = nullptr);
    [[nodiscard]] bool addLocalRepository(
        const QString &path,
        settings::ProjectRecord *project,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool openProject(
        const QString &projectId,
        settings::ProjectRecord *project,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool setActiveProject(
        const QString &projectId,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool removeLocalRepository(
        const QString &projectId,
        QString *errorMessage = nullptr);

    [[nodiscard]] QList<settings::ProjectRecord> projects() const;
    [[nodiscard]] bool hasActiveProject() const;
    [[nodiscard]] QString activeProjectId() const;
    [[nodiscard]] bool activeProject(settings::ProjectRecord *project) const;

signals:
    void projectsChanged();
    void activeProjectChanged(const QString &projectId);

private:
    [[nodiscard]] bool refreshProjects(QString *errorMessage);
    [[nodiscard]] bool persistActiveProjectId(QString *errorMessage);
    [[nodiscard]] bool clearActiveProject(QString *errorMessage);
    [[nodiscard]] bool touchProject(
        const QString &projectId,
        QString *errorMessage);
    [[nodiscard]] static QString normalizeLocalPath(
        const QString &path,
        QString *errorMessage);
    [[nodiscard]] static QString currentTimestamp();
    [[nodiscard]] static QString createId();
    [[nodiscard]] bool syncGitMetadata(
        const QString &projectId,
        const QString &path,
        QString *errorMessage);
    [[nodiscard]] static QString projectNameFromPath(const QString &path);

    storage::StorageService &m_storageService;
    git::GitService &m_gitService;
    QList<settings::ProjectRecord> m_projects;
    QString m_activeProjectId;
};

} // namespace qtcode::core

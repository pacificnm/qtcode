#pragma once

#include "settings/ProjectModels.h"

#include <QList>
#include <QString>

namespace qtcode::storage {

class StorageService;

class ProjectRepository
{
public:
    explicit ProjectRepository(StorageService &storageService);

    [[nodiscard]] bool insertProject(
        const settings::ProjectRecord &project,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool insertRepository(
        const settings::RepositoryRecord &repository,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool updateProjectLastOpened(
        const QString &projectId,
        const QString &timestamp,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool findProjectByRootPath(
        const QString &rootPath,
        settings::ProjectRecord *project,
        bool *found,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool findProjectById(
        const QString &projectId,
        settings::ProjectRecord *project,
        bool *found,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool listProjects(
        QList<settings::ProjectRecord> *projects,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool updatePrimaryRepositoryBranch(
        const QString &projectId,
        const QString &branchName,
        const QString &timestamp,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool findPrimaryRepository(
        const QString &projectId,
        settings::RepositoryRecord *repository,
        bool *found,
        QString *errorMessage = nullptr) const;

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

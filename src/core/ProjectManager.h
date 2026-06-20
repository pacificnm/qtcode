#pragma once

#include "settings/ProjectModels.h"

#include <QList>
#include <QString>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class ProjectManager
{
public:
    explicit ProjectManager(storage::StorageService &storageService);

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

    [[nodiscard]] QList<settings::ProjectRecord> projects() const;
    [[nodiscard]] bool hasActiveProject() const;
    [[nodiscard]] QString activeProjectId() const;
    [[nodiscard]] bool activeProject(settings::ProjectRecord *project) const;

private:
    [[nodiscard]] bool refreshProjects(QString *errorMessage);
    [[nodiscard]] bool persistActiveProjectId(QString *errorMessage);
    [[nodiscard]] bool touchProject(
        const QString &projectId,
        QString *errorMessage);
    [[nodiscard]] static QString normalizeLocalPath(
        const QString &path,
        QString *errorMessage);
    [[nodiscard]] static QString currentTimestamp();
    [[nodiscard]] static QString createId();
    [[nodiscard]] static QString projectNameFromPath(const QString &path);

    storage::StorageService &m_storageService;
    QList<settings::ProjectRecord> m_projects;
    QString m_activeProjectId;
};

} // namespace qtcode::core

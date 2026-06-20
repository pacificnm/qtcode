#include "ui/models/RepositoryListModel.h"

#include "core/ProjectManager.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

namespace qtcode::ui {

RepositoryListModel::RepositoryListModel(
    qtcode::core::ProjectManager *projectManager,
    QObject *parent)
    : QAbstractListModel(parent)
    , m_projectManager(projectManager)
{
    if (m_projectManager != nullptr) {
        connect(m_projectManager, &qtcode::core::ProjectManager::projectsChanged, this, &RepositoryListModel::reload);
        connect(m_projectManager, &qtcode::core::ProjectManager::activeProjectChanged, this, &RepositoryListModel::reload);
    }

    reload();
}

int RepositoryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || m_projectManager == nullptr) {
        return 0;
    }

    return m_projectManager->projects().size();
}

QVariant RepositoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_projectManager == nullptr) {
        return {};
    }

    const QList<qtcode::settings::ProjectRecord> projects = m_projectManager->projects();
    if (index.row() < 0 || index.row() >= projects.size()) {
        return {};
    }

    const qtcode::settings::ProjectRecord &project = projects.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return project.name;
    case Qt::ToolTipRole:
    case RootPathRole:
        return project.rootPath;
    case ProjectIdRole:
        return project.id;
    case IsActiveRole:
        return project.id == m_projectManager->activeProjectId();
    default:
        return {};
    }
}

QHash<int, QByteArray> RepositoryListModel::roleNames() const
{
    return {
        {ProjectIdRole, "projectId"},
        {NameRole, "name"},
        {RootPathRole, "rootPath"},
        {IsActiveRole, "isActive"},
    };
}

void RepositoryListModel::reload()
{
    beginResetModel();
    endResetModel();

    qCDebug(qtcodeUi) << "Repository list model refreshed with" << rowCount() << "project(s)";
}

} // namespace qtcode::ui

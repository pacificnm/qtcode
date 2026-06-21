#include "ui/models/RepositoryListModel.h"

#include "core/ProjectManager.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

namespace qtcode::ui {

namespace {

bool projectListsMatch(
    const QList<qtcode::settings::ProjectRecord> &left,
    const QList<qtcode::settings::ProjectRecord> &right)
{
    if (left.size() != right.size()) {
        return false;
    }

    for (int i = 0; i < left.size(); ++i) {
        const qtcode::settings::ProjectRecord &leftProject = left.at(i);
        const qtcode::settings::ProjectRecord &rightProject = right.at(i);
        if (leftProject.id != rightProject.id
            || leftProject.name != rightProject.name
            || leftProject.rootPath != rightProject.rootPath) {
            return false;
        }
    }

    return true;
}

} // namespace

RepositoryListModel::RepositoryListModel(
    qtcode::core::ProjectManager *projectManager,
    QObject *parent)
    : QAbstractListModel(parent)
    , m_projectManager(projectManager)
{
    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::projectsChanged,
            this,
            &RepositoryListModel::onProjectsChanged);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &RepositoryListModel::updateActiveState);
    }

    onProjectsChanged();
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

void RepositoryListModel::onProjectsChanged()
{
    if (m_projectManager == nullptr) {
        return;
    }

    const QList<qtcode::settings::ProjectRecord> projects = m_projectManager->projects();
    if (projectListsMatch(projects, m_cachedProjects)) {
        return;
    }

    m_cachedProjects = projects;
    reload();
}

void RepositoryListModel::updateActiveState()
{
    if (rowCount() == 0) {
        return;
    }

    const QModelIndex topLeft = index(0);
    const QModelIndex bottomRight = index(rowCount() - 1);
    emit dataChanged(topLeft, bottomRight, {IsActiveRole});
}

} // namespace qtcode::ui

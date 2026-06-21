#pragma once

#include <QDialog>

class QListView;
class QLabel;

namespace qtcode::core {
class CliCapabilityService;
class ProjectManager;
class StatusService;
} // namespace qtcode::core

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::ui {

class RepositoryListModel;

class ChangeRepositoryDialog final : public QDialog
{
    Q_OBJECT

public:
    ChangeRepositoryDialog(
        qtcode::core::ProjectManager *projectManager,
        qtcode::git::GitService *gitService,
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

signals:
    void activeRepositoryChanged();

private slots:
    void onRepositorySelected(const QModelIndex &current, const QModelIndex &previous);
    void syncRepositorySelection();
    void refreshCapabilityState();
    void showRepositoryContextMenu(const QPoint &position);

private:
    void configureLayout();
    void updateEmptyState();
    [[nodiscard]] QString resolveGitExecutable() const;
    void selectBranchForRepository(const QString &projectId, const QString &repositoryPath);
    void createBranchForRepository(const QString &projectId, const QString &repositoryPath);
    void checkoutBranchForRepository(
        const QString &projectId,
        const QString &repositoryPath,
        const QString &branchName,
        bool createBranch = false);
    void removeRepositoryAtIndex(const QModelIndex &index);

    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    RepositoryListModel *m_repositoryModel = nullptr;
    QListView *m_repositoryList = nullptr;
    QLabel *m_emptyStateLabel = nullptr;
    bool m_gitAvailable = false;
};

} // namespace qtcode::ui

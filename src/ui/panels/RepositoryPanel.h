#pragma once

#include <QWidget>

#include "git/GitCommitSummary.h"
#include "git/GitStatus.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::core {
class ProjectManager;
} // namespace qtcode::core

template <typename T>
class QFutureWatcher;

namespace qtcode::ui {

class RepositoryPanel final : public QWidget
{
    Q_OBJECT

public:
    RepositoryPanel(
        qtcode::git::GitService *gitService,
        qtcode::core::ProjectManager *projectManager,
        QWidget *parent = nullptr);
    ~RepositoryPanel() override;

public slots:
    void refreshStatus();

private slots:
    void onRefreshFinished();

private:
    void configureLayout();
    void startRefresh(const QString &repositoryPath);
    void setRefreshing(bool refreshing);
    void showEmptyState(const QString &message);
    void showErrorState(const QString &message);
    void applySnapshot(const qtcode::git::RepositoryGitSnapshot &snapshot);
    void showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status);
    void showRecentCommits(const QList<qtcode::git::GitCommitSummary> &commits);

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QLabel *m_projectLabel = nullptr;
    QLabel *m_changedFilesStateLabel = nullptr;
    QLabel *m_commitsStateLabel = nullptr;
    QListWidget *m_changedFilesList = nullptr;
    QListWidget *m_commitsList = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QFutureWatcher<qtcode::git::RepositoryGitSnapshot> *m_refreshWatcher = nullptr;
};

} // namespace qtcode::ui

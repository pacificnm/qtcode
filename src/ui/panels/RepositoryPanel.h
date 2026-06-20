#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "git/GitCommitSummary.h"
#include "git/GitStatus.h"
#include "github/GitHubModels.h"

class QLabel;
class QListView;
class QListWidget;
class QPushButton;

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::core {
class ProjectManager;
class CliCapabilityService;
} // namespace qtcode::core

namespace qtcode::github {
class GitHubService;
} // namespace qtcode::github

template <typename T>
class QFutureWatcher;

namespace qtcode::ui {

class RepositoryListModel;
class GitHubDetailView;

struct RepositoryRefreshBundle
{
    qtcode::git::RepositoryGitSnapshot git;
    qtcode::github::GitHubIssueListResult issues;
    qtcode::github::GitHubPullRequestListResult pullRequests;
};

class RepositoryPanel final : public QWidget
{
    Q_OBJECT

public:
    RepositoryPanel(
        qtcode::git::GitService *gitService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::github::GitHubService *gitHubService,
        QWidget *parent = nullptr);
    ~RepositoryPanel() override;

public slots:
    void refreshStatus();

signals:
    void issueContextSelected(const qtcode::github::GitHubIssueDetail &detail);
    void pullRequestContextSelected(const qtcode::github::GitHubPullRequestDetail &detail);

private slots:
    void onRefreshFinished();
    void addRepository();
    void onRepositorySelected(const QModelIndex &current, const QModelIndex &previous);
    void syncRepositorySelection();

private:
    void configureLayout();
    void startRefresh(const QString &projectId, const QString &repositoryPath);
    void setRefreshing(bool refreshing);
    void showEmptyState(const QString &message);
    void showErrorState(const QString &message);
    void applySnapshot(const qtcode::git::RepositoryGitSnapshot &snapshot);
    void showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status);
    void showRecentCommits(const QList<qtcode::git::GitCommitSummary> &commits);
    void showGitHubIssues(const qtcode::github::GitHubIssueListResult &result);
    void showGitHubPullRequests(const qtcode::github::GitHubPullRequestListResult &result);
    void onIssueSelected();
    void onPullRequestSelected();
    void refreshCapabilityState();

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::github::GitHubService *m_gitHubService = nullptr;
    RepositoryListModel *m_repositoryModel = nullptr;
    QListView *m_repositoryList = nullptr;
    QLabel *m_projectLabel = nullptr;
    QLabel *m_capabilityStateLabel = nullptr;
    QLabel *m_changedFilesStateLabel = nullptr;
    QLabel *m_commitsStateLabel = nullptr;
    QListWidget *m_changedFilesList = nullptr;
    QListWidget *m_commitsList = nullptr;
    QLabel *m_issuesStateLabel = nullptr;
    QListWidget *m_issuesList = nullptr;
    QLabel *m_pullRequestsStateLabel = nullptr;
    QListWidget *m_pullRequestsList = nullptr;
    QPushButton *m_addRepositoryButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QFutureWatcher<RepositoryRefreshBundle> *m_refreshWatcher = nullptr;
    QElapsedTimer m_refreshTimer;
    GitHubDetailView *m_detailView = nullptr;
    QString m_activeProjectId;
};

} // namespace qtcode::ui

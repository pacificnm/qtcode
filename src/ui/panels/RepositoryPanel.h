#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include "git/GitCommitSummary.h"
#include "git/GitOperationResult.h"
#include "git/GitStatus.h"
#include "github/GitHubCachePolicy.h"
#include "github/GitHubModels.h"

#include <functional>

class QLabel;
class QListView;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::core {
class ProjectManager;
class CliCapabilityService;
class StatusService;
} // namespace qtcode::core

namespace qtcode::github {
class GitHubService;
} // namespace qtcode::github

template <typename T>
class QFutureWatcher;

namespace qtcode::ui {

class RepositoryListModel;

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
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);
    ~RepositoryPanel() override;

public slots:
    void refreshStatus(bool showStatusFeedback = true);
    void addRepository();

signals:
    void issueContextRequested(const qtcode::github::GitHubIssueDetail &detail);
    void issueOpenRequested(
        const qtcode::github::GitHubIssueDetail &detail,
        const qtcode::github::GitHubCacheMetadata &cacheMetadata);
    void pullRequestOpenRequested(
        const qtcode::github::GitHubPullRequestDetail &detail,
        const qtcode::github::GitHubCacheMetadata &cacheMetadata);
    void fileOpenRequested(const QString &absolutePath);

private slots:
    void onChangedFileClicked(QListWidgetItem *item);
    void onRefreshFinished();
    void onGitOperationFinished();
    void onRepositorySelected(const QModelIndex &current, const QModelIndex &previous);
    void onActiveProjectChanged();
    void syncRepositorySelection();
    void onAutoRefreshTimer();
    void onCommitMessageChanged();
    void onCommitClicked();
    void onPushClicked();
    void onStageAllClicked();
    void onUnstageAllClicked();

private:
    void configureLayout();
    void startRefresh(const QString &projectId, const QString &repositoryPath);
    void setRefreshing(bool refreshing, bool showLoadingUi);
    void showEmptyState(const QString &message);
    void showErrorState(const QString &message);
    void applySnapshot(const qtcode::git::RepositoryGitSnapshot &snapshot);
    void applyWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status);
    void updateSourceControlActions(const qtcode::git::GitWorkingTreeStatus &status);
    [[nodiscard]] QString resolveChangedFilePath(const QString &relativePath) const;
    [[nodiscard]] QString resolveGitExecutable() const;
    [[nodiscard]] bool activeRepositoryPath(QString *repositoryPath) const;
    void startGitOperation(
        const std::function<qtcode::git::GitOperationResult()> &operation,
        const QString &successMessage);
    void showGitOperationResult(
        const qtcode::git::GitOperationResult &result,
        const QString &successMessage);
    void showGitHubIssues(const qtcode::github::GitHubIssueListResult &result);
    void showGitHubPullRequests(const qtcode::github::GitHubPullRequestListResult &result);
    void onIssueSelected();
    void onPullRequestSelected();
    void refreshCapabilityState();
    void updateAutoRefreshTimer();
    void showRepositoryContextMenu(const QPoint &position);
    void showIssuesContextMenu(const QPoint &position);
    void showStagedFilesContextMenu(const QPoint &position);
    void showUnstagedFilesContextMenu(const QPoint &position);
    void attachIssueToContext(int issueNumber);
    void removeRepositoryAtIndex(const QModelIndex &index);
    void stageRelativePaths(const QStringList &relativePaths);
    void unstageRelativePaths(const QStringList &relativePaths);
    [[nodiscard]] QStringList selectedRelativePaths(QListWidget *list) const;

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::github::GitHubService *m_gitHubService = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    RepositoryListModel *m_repositoryModel = nullptr;
    QListView *m_repositoryList = nullptr;
    QLabel *m_projectLabel = nullptr;
    QLabel *m_capabilityStateLabel = nullptr;
    QLabel *m_sourceControlStateLabel = nullptr;
    QPlainTextEdit *m_commitMessageEdit = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_pushButton = nullptr;
    QLabel *m_stagedSectionLabel = nullptr;
    QPushButton *m_unstageAllButton = nullptr;
    QListWidget *m_stagedFilesList = nullptr;
    QLabel *m_changesSectionLabel = nullptr;
    QPushButton *m_stageAllButton = nullptr;
    QListWidget *m_unstagedFilesList = nullptr;
    QLabel *m_issuesStateLabel = nullptr;
    QListWidget *m_issuesList = nullptr;
    QLabel *m_pullRequestsStateLabel = nullptr;
    QListWidget *m_pullRequestsList = nullptr;
    QFutureWatcher<RepositoryRefreshBundle> *m_refreshWatcher = nullptr;
    QFutureWatcher<qtcode::git::GitOperationResult> *m_gitOperationWatcher = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;
    QElapsedTimer m_refreshTimer;
    QString m_activeProjectId;
    bool m_showStatusFeedback = true;
    bool m_hasLoadedSnapshot = false;
    bool m_gitAvailable = false;
    int m_commitsAhead = 0;
    QString m_pendingGitSuccessMessage;
    bool m_clearCommitMessageOnSuccess = false;
    bool m_pendingRefreshAfterGitOperation = false;
};

} // namespace qtcode::ui

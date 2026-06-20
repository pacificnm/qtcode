#include "ui/panels/RepositoryPanel.h"

#include "core/ProjectManager.h"
#include "core/CliCapabilityService.h"
#include "core/CliCapabilityModels.h"
#include "git/GitCommitSummary.h"
#include "git/GitService.h"
#include "github/GitHubService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "ui/models/RepositoryListModel.h"

#include <KLocalizedString>

#include <QFileDialog>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace qtcode::ui {

RepositoryPanel::RepositoryPanel(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::github::GitHubService *gitHubService,
    QWidget *parent)
    : QWidget(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
    , m_cliCapabilityService(cliCapabilityService)
    , m_gitHubService(gitHubService)
    , m_refreshWatcher(new QFutureWatcher<RepositoryRefreshBundle>(this))
{
    configureLayout();
    refreshCapabilityState();

    if (m_cliCapabilityService != nullptr) {
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            &RepositoryPanel::refreshCapabilityState);
    }

    if (m_projectManager != nullptr) {
        m_repositoryModel = new RepositoryListModel(m_projectManager, this);
        m_repositoryList->setModel(m_repositoryModel);

        connect(
            m_repositoryList->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &RepositoryPanel::onRepositorySelected);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &RepositoryPanel::syncRepositorySelection);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::projectsChanged,
            this,
            &RepositoryPanel::syncRepositorySelection);

        syncRepositorySelection();
    }

    connect(m_refreshWatcher, &QFutureWatcher<RepositoryRefreshBundle>::finished, this, &RepositoryPanel::onRefreshFinished);
    refreshStatus();
}

RepositoryPanel::~RepositoryPanel()
{
    if (m_refreshWatcher->isRunning()) {
        m_refreshWatcher->waitForFinished();
    }
}

void RepositoryPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("Repositories"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_projectLabel = new QLabel(this);
    m_projectLabel->setWordWrap(true);
    m_projectLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_capabilityStateLabel = new QLabel(this);
    m_capabilityStateLabel->setWordWrap(true);
    m_capabilityStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QFont sectionFont = m_projectLabel->font();
    sectionFont.setBold(true);

    auto *localRepositoriesTitle = new QLabel(i18n("Local repositories"), this);
    localRepositoriesTitle->setFont(sectionFont);

    m_repositoryList = new QListView(this);
    m_repositoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_repositoryList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_addRepositoryButton = new QPushButton(i18n("Add repository"), this);
    connect(m_addRepositoryButton, &QPushButton::clicked, this, &RepositoryPanel::addRepository);

    m_refreshButton = new QPushButton(i18n("Refresh status"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &RepositoryPanel::refreshStatus);

    auto *changedFilesTitle = new QLabel(i18n("Changed files"), this);
    changedFilesTitle->setFont(sectionFont);

    m_changedFilesStateLabel = new QLabel(this);
    m_changedFilesStateLabel->setWordWrap(true);
    m_changedFilesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_changedFilesList = new QListWidget(this);
    m_changedFilesList->setSelectionMode(QAbstractItemView::NoSelection);

    auto *commitsTitle = new QLabel(i18n("Recent commits"), this);
    commitsTitle->setFont(sectionFont);

    m_commitsStateLabel = new QLabel(this);
    m_commitsStateLabel->setWordWrap(true);
    m_commitsStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_commitsList = new QListWidget(this);
    m_commitsList->setSelectionMode(QAbstractItemView::NoSelection);

    auto *issuesTitle = new QLabel(i18n("GitHub issues"), this);
    issuesTitle->setFont(sectionFont);

    m_issuesStateLabel = new QLabel(this);
    m_issuesStateLabel->setWordWrap(true);
    m_issuesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_issuesList = new QListWidget(this);
    m_issuesList->setSelectionMode(QAbstractItemView::NoSelection);

    layout->addWidget(titleLabel);
    layout->addWidget(m_capabilityStateLabel);
    layout->addWidget(m_projectLabel);
    layout->addWidget(localRepositoriesTitle);
    layout->addWidget(m_repositoryList);
    layout->addWidget(m_addRepositoryButton);
    layout->addWidget(m_refreshButton);
    layout->addWidget(changedFilesTitle);
    layout->addWidget(m_changedFilesStateLabel);
    layout->addWidget(m_changedFilesList);
    layout->addWidget(commitsTitle);
    layout->addWidget(m_commitsStateLabel);
    layout->addWidget(m_commitsList);
    layout->addWidget(issuesTitle);
    layout->addWidget(m_issuesStateLabel);
    layout->addWidget(m_issuesList, 1);
}

void RepositoryPanel::refreshStatus()
{
    if (m_refreshWatcher->isRunning()) {
        return;
    }

    if (m_gitService == nullptr || m_projectManager == nullptr) {
        showErrorState(i18n("Repository services are unavailable."));
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        showEmptyState(i18n("Add a local repository to browse branches, changes, and GitHub context."));
        return;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        showErrorState(i18n("The active project could not be loaded."));
        return;
    }

    startRefresh(activeProject.id, activeProject.rootPath);
}

void RepositoryPanel::startRefresh(const QString &projectId, const QString &repositoryPath)
{
    m_projectLabel->setText(i18n("Active project loading…"));
    setRefreshing(true);

    qtcode::github::GitHubService *gitHubService = m_gitHubService;
    m_refreshWatcher->setFuture(QtConcurrent::run(
        [projectId, repositoryPath, gitHubService]() {
            RepositoryRefreshBundle bundle;
            bundle.git = qtcode::git::loadRepositoryGitSnapshot(repositoryPath, 10);
            if (gitHubService != nullptr && bundle.git.success) {
                bundle.issues = gitHubService->listIssuesForProject(projectId);
            }
            return bundle;
        }));
}

void RepositoryPanel::onRefreshFinished()
{
    setRefreshing(false);

    settings::ProjectRecord activeProject;
    if (m_projectManager == nullptr || !m_projectManager->activeProject(&activeProject)) {
        showErrorState(i18n("The active project could not be loaded."));
        return;
    }

    const RepositoryRefreshBundle bundle = m_refreshWatcher->result();
    if (!bundle.git.success) {
        qCWarning(qtcodeUi) << "Failed to refresh repository snapshot:" << bundle.git.errorMessage;
        showErrorState(i18n("Could not load repository status: %1", bundle.git.errorMessage));
        return;
    }

    applySnapshot(bundle.git);
    showGitHubIssues(bundle.issues);

    qCInfo(qtcodeUi) << "Repository snapshot refreshed with"
                     << bundle.git.status.changedFiles.size() << "changed file(s),"
                     << bundle.git.commits.size() << "recent commit(s), and"
                     << bundle.issues.issues.size() << "GitHub issue(s)";
}

void RepositoryPanel::setRefreshing(bool refreshing)
{
    m_refreshButton->setEnabled(!refreshing);

    if (refreshing) {
        m_changedFilesStateLabel->setText(i18n("Loading changed files…"));
        m_changedFilesStateLabel->show();
        m_changedFilesList->hide();
        m_commitsStateLabel->setText(i18n("Loading recent commits…"));
        m_commitsStateLabel->show();
        m_commitsList->hide();
        m_issuesStateLabel->setText(i18n("Loading GitHub issues…"));
        m_issuesStateLabel->show();
        m_issuesList->hide();
    }
}

void RepositoryPanel::applySnapshot(const qtcode::git::RepositoryGitSnapshot &snapshot)
{
    settings::ProjectRecord activeProject;
    if (m_projectManager != nullptr && m_projectManager->activeProject(&activeProject)) {
        m_projectLabel->setText(
            i18n("Active project: %1\nBranch: %2", activeProject.name, snapshot.status.branchName));
    }

    showChangedFiles(snapshot.status);
    showRecentCommits(snapshot.commits);
}

void RepositoryPanel::showEmptyState(const QString &message)
{
    m_projectLabel->clear();
    m_changedFilesStateLabel->setText(message);
    m_changedFilesStateLabel->show();
    m_changedFilesList->clear();
    m_changedFilesList->hide();
    m_commitsStateLabel->hide();
    m_commitsList->clear();
    m_commitsList->hide();
    m_issuesStateLabel->setText(i18n("GitHub issues load after a repository is selected."));
    m_issuesStateLabel->show();
    m_issuesList->clear();
    m_issuesList->hide();
    m_refreshButton->setEnabled(m_projectManager != nullptr && m_projectManager->hasActiveProject());
    m_addRepositoryButton->setEnabled(m_projectManager != nullptr);
}

void RepositoryPanel::showErrorState(const QString &message)
{
    m_changedFilesStateLabel->setText(message);
    m_changedFilesStateLabel->show();
    m_changedFilesList->clear();
    m_changedFilesList->hide();
    m_commitsStateLabel->hide();
    m_commitsList->clear();
    m_commitsList->hide();
    m_issuesStateLabel->hide();
    m_issuesList->clear();
    m_issuesList->hide();
    m_refreshButton->setEnabled(true);
}

void RepositoryPanel::showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status)
{
    m_changedFilesList->clear();

    if (status.changedFiles.isEmpty()) {
        m_changedFilesStateLabel->setText(i18n("No local changes in the working tree."));
        m_changedFilesStateLabel->show();
        m_changedFilesList->hide();
    } else {
        m_changedFilesStateLabel->hide();

        for (const qtcode::git::ChangedFile &changedFile : status.changedFiles) {
            m_changedFilesList->addItem(
                QStringLiteral("%1 — %2").arg(changedFile.path, changedFile.statusLabel));
        }

        m_changedFilesList->show();
    }
}

void RepositoryPanel::showRecentCommits(const QList<qtcode::git::GitCommitSummary> &commits)
{
    m_commitsList->clear();

    if (commits.isEmpty()) {
        m_commitsStateLabel->setText(i18n("No commits in this repository yet."));
        m_commitsStateLabel->show();
        m_commitsList->hide();
        return;
    }

    m_commitsStateLabel->hide();

    for (const qtcode::git::GitCommitSummary &commit : commits) {
        m_commitsList->addItem(
            i18n("%1 — %2\n%3, %4",
                 commit.shortSha,
                 commit.subject,
                 commit.author,
                 commit.committedAt));
    }

    m_commitsList->show();
}

void RepositoryPanel::showGitHubIssues(const qtcode::github::GitHubIssueListResult &result)
{
    m_issuesList->clear();

    if (!result.success) {
        m_issuesStateLabel->setText(result.errorMessage);
        m_issuesStateLabel->show();
        m_issuesList->hide();
        return;
    }

    if (result.issues.isEmpty()) {
        const QString message = result.fromCache
            ? i18n("No cached GitHub issues are available for this repository.")
            : i18n("No open GitHub issues were returned for this repository.");
        m_issuesStateLabel->setText(message);
        m_issuesStateLabel->show();
        m_issuesList->hide();
        return;
    }

    m_issuesStateLabel->hide();

    for (const qtcode::github::GitHubIssue &issue : result.issues) {
        m_issuesList->addItem(
            i18n("#%1 — %2\n%3, %4",
                 issue.number,
                 issue.title,
                 issue.state,
                 issue.author));
    }

    m_issuesList->show();
}

void RepositoryPanel::addRepository()
{
    if (m_projectManager == nullptr) {
        showErrorState(i18n("Repository services are unavailable."));
        return;
    }

    const QString path = QFileDialog::getExistingDirectory(this, i18n("Add local repository"));
    if (path.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_projectManager->addLocalRepository(path, nullptr, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to add local repository:" << errorMessage;
        showErrorState(i18n("Could not add repository: %1", errorMessage));
        return;
    }

    refreshStatus();
}

void RepositoryPanel::onRepositorySelected(const QModelIndex &current, const QModelIndex &)
{
    if (!current.isValid() || m_projectManager == nullptr) {
        return;
    }

    const QString projectId = current.data(RepositoryListModel::ProjectIdRole).toString();
    if (projectId.isEmpty() || projectId == m_projectManager->activeProjectId()) {
        return;
    }

    QString errorMessage;
    if (!m_projectManager->openProject(projectId, nullptr, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to open selected repository:" << errorMessage;
        showErrorState(i18n("Could not open repository: %1", errorMessage));
        return;
    }

    refreshStatus();
}

void RepositoryPanel::syncRepositorySelection()
{
    if (m_repositoryModel == nullptr || m_repositoryList == nullptr || m_projectManager == nullptr) {
        return;
    }

    const QString activeProjectId = m_projectManager->activeProjectId();
    if (activeProjectId.isEmpty()) {
        m_repositoryList->clearSelection();
        return;
    }

    for (int row = 0; row < m_repositoryModel->rowCount(); ++row) {
        const QModelIndex index = m_repositoryModel->index(row);
        if (index.data(RepositoryListModel::ProjectIdRole).toString() == activeProjectId) {
            QSignalBlocker blocker(m_repositoryList->selectionModel());
            m_repositoryList->setCurrentIndex(index);
            return;
        }
    }
}

void RepositoryPanel::refreshCapabilityState()
{
    if (m_capabilityStateLabel == nullptr) {
        return;
    }

    if (m_cliCapabilityService == nullptr) {
        m_capabilityStateLabel->hide();
        return;
    }

    const qtcode::core::CliCapabilitiesSnapshot snapshot = m_cliCapabilityService->snapshot();
    QStringList messages;

    if (!snapshot.git.available) {
        messages.append(snapshot.git.unavailableMessage);
    }
    if (!snapshot.gh.available) {
        messages.append(snapshot.gh.unavailableMessage);
    } else if (!snapshot.gh.authenticated) {
        messages.append(snapshot.gh.authUnavailableMessage);
    }

    if (messages.isEmpty()) {
        m_capabilityStateLabel->hide();
        return;
    }

    m_capabilityStateLabel->setText(messages.join(QStringLiteral("\n\n")));
    m_capabilityStateLabel->show();
}

} // namespace qtcode::ui

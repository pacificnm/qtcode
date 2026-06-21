#include "ui/panels/RepositoryPanel.h"

#include "core/ProjectManager.h"
#include "core/CliCapabilityService.h"
#include "core/CliCapabilityModels.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "git/GitService.h"
#include "github/GitHubService.h"
#include "github/GitHubCachePolicy.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "ui/models/RepositoryListModel.h"
#include "ui/views/GitHubDetailView.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace qtcode::ui {

namespace {

constexpr int kAutoRefreshIntervalMs = 30 * 1000;

qtcode::github::GitHubCacheMetadata cacheMetadataFromListResult(
    bool fromCache,
    bool isStale,
    const QString &fetchedAt)
{
    qtcode::github::GitHubCacheMetadata metadata;
    metadata.fromCache = fromCache;
    metadata.isStale = isStale;
    metadata.fetchedAt = fetchedAt;
    return metadata;
}

struct NumberedListEntry
{
    int number = 0;
    QString text;
};

struct PathListEntry
{
    QString path;
    QString text;
};

bool numberedListWidgetMatches(QListWidget *list, const QList<NumberedListEntry> &entries)
{
    if (list == nullptr || list->count() != entries.size()) {
        return false;
    }

    for (int i = 0; i < entries.size(); ++i) {
        if (list->item(i)->data(Qt::UserRole).toInt() != entries.at(i).number
            || list->item(i)->text() != entries.at(i).text) {
            return false;
        }
    }

    return true;
}

bool pathListWidgetMatches(QListWidget *list, const QList<PathListEntry> &entries)
{
    if (list == nullptr || list->count() != entries.size()) {
        return false;
    }

    for (int i = 0; i < entries.size(); ++i) {
        if (list->item(i)->data(Qt::UserRole).toString() != entries.at(i).path
            || list->item(i)->text() != entries.at(i).text) {
            return false;
        }
    }

    return true;
}

void syncPathListWidget(QListWidget *list, const QList<PathListEntry> &entries)
{
    if (list == nullptr) {
        return;
    }

    if (pathListWidgetMatches(list, entries)) {
        return;
    }

    list->clear();
    for (const PathListEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.text);
        item->setData(Qt::UserRole, entry.path);
        list->addItem(item);
    }
}

void syncNumberedListWidget(QListWidget *list, const QList<NumberedListEntry> &entries)
{
    if (list == nullptr) {
        return;
    }

    const int selectedNumber = list->selectedItems().isEmpty()
        ? -1
        : list->selectedItems().first()->data(Qt::UserRole).toInt();

    if (numberedListWidgetMatches(list, entries)) {
        return;
    }

    list->clear();
    for (const NumberedListEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.text);
        item->setData(Qt::UserRole, entry.number);
        list->addItem(item);
    }

    if (selectedNumber <= 0) {
        return;
    }

    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->data(Qt::UserRole).toInt() == selectedNumber) {
            list->setCurrentRow(i);
            return;
        }
    }
}

void setLabelTextIfChanged(QLabel *label, const QString &text)
{
    if (label != nullptr && label->text() != text) {
        label->setText(text);
    }
}

void setWidgetVisibleIfChanged(QWidget *widget, bool visible)
{
    if (widget != nullptr && widget->isVisible() != visible) {
        widget->setVisible(visible);
    }
}

} // namespace

RepositoryPanel::RepositoryPanel(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::github::GitHubService *gitHubService,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QWidget(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
    , m_cliCapabilityService(cliCapabilityService)
    , m_gitHubService(gitHubService)
    , m_statusService(statusService)
    , m_refreshWatcher(new QFutureWatcher<RepositoryRefreshBundle>(this))
    , m_autoRefreshTimer(new QTimer(this))
{
    m_autoRefreshTimer->setInterval(kAutoRefreshIntervalMs);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &RepositoryPanel::onAutoRefreshTimer);

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
            &RepositoryPanel::onActiveProjectChanged);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::projectsChanged,
            this,
            &RepositoryPanel::syncRepositorySelection);

        syncRepositorySelection();
        updateAutoRefreshTimer();
    }

    connect(m_refreshWatcher, &QFutureWatcher<RepositoryRefreshBundle>::finished, this, &RepositoryPanel::onRefreshFinished);

    if (m_cliCapabilityService != nullptr) {
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            [this]() {
                if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
                    refreshStatus(false);
                }
            });
    }

    QTimer::singleShot(0, this, [this]() { refreshStatus(true); });
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

    auto *changedFilesTitle = new QLabel(i18n("Changed files"), this);
    changedFilesTitle->setFont(sectionFont);

    m_changedFilesStateLabel = new QLabel(this);
    m_changedFilesStateLabel->setWordWrap(true);
    m_changedFilesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_changedFilesList = new QListWidget(this);
    m_changedFilesList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_changedFilesList, &QListWidget::itemClicked, this, &RepositoryPanel::onChangedFileClicked);

    auto *issuesTitle = new QLabel(i18n("GitHub issues"), this);
    issuesTitle->setFont(sectionFont);

    m_issuesStateLabel = new QLabel(this);
    m_issuesStateLabel->setWordWrap(true);
    m_issuesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_issuesList = new QListWidget(this);
    m_issuesList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_issuesList, &QListWidget::itemSelectionChanged, this, &RepositoryPanel::onIssueSelected);

    auto *pullRequestsTitle = new QLabel(i18n("GitHub pull requests"), this);
    pullRequestsTitle->setFont(sectionFont);

    m_pullRequestsStateLabel = new QLabel(this);
    m_pullRequestsStateLabel->setWordWrap(true);
    m_pullRequestsStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_pullRequestsList = new QListWidget(this);
    m_pullRequestsList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(
        m_pullRequestsList,
        &QListWidget::itemSelectionChanged,
        this,
        &RepositoryPanel::onPullRequestSelected);

    m_detailView = new GitHubDetailView(this);
    connect(
        m_detailView,
        &GitHubDetailView::attachIssueRequested,
        this,
        [this](const qtcode::github::GitHubIssueDetail &detail) {
            emit issueContextSelected(detail);
        });
    connect(
        m_detailView,
        &GitHubDetailView::attachPullRequestRequested,
        this,
        [this](const qtcode::github::GitHubPullRequestDetail &detail) {
            emit pullRequestContextSelected(detail);
        });

    layout->addWidget(titleLabel);
    layout->addWidget(m_capabilityStateLabel);
    layout->addWidget(m_projectLabel);
    layout->addWidget(localRepositoriesTitle);
    layout->addWidget(m_repositoryList);
    layout->addWidget(changedFilesTitle);
    layout->addWidget(m_changedFilesStateLabel);
    layout->addWidget(m_changedFilesList);
    layout->addWidget(issuesTitle);
    layout->addWidget(m_issuesStateLabel);
    layout->addWidget(m_issuesList);
    layout->addWidget(pullRequestsTitle);
    layout->addWidget(m_pullRequestsStateLabel);
    layout->addWidget(m_pullRequestsList);
    layout->addWidget(m_detailView, 1);
}

void RepositoryPanel::refreshStatus(bool showStatusFeedback)
{
    if (m_refreshWatcher->isRunning()) {
        return;
    }

    m_showStatusFeedback = showStatusFeedback;

    if (m_gitService == nullptr || m_projectManager == nullptr) {
        showErrorState(i18n("Repository services are unavailable."));
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        showEmptyState(i18n("Add a local repository to browse branches, changes, and GitHub context."));
        updateAutoRefreshTimer();
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
    const bool showLoadingUi = m_showStatusFeedback || !m_hasLoadedSnapshot;

    if (showLoadingUi) {
        m_projectLabel->setText(i18n("Active project loading…"));
        setRefreshing(true, true);
    }

    m_refreshTimer.start();
    m_autoRefreshTimer->stop();

    if (m_showStatusFeedback && m_statusService != nullptr) {
        m_statusService->showProgress(i18n("Refreshing repository status…"));
    }

    qtcode::github::GitHubService *gitHubService = m_gitHubService;
    m_refreshWatcher->setFuture(QtConcurrent::run(
        [projectId, repositoryPath, gitHubService]() {
            RepositoryRefreshBundle bundle;
            bundle.git = qtcode::git::loadRepositoryGitSnapshot(repositoryPath, 0);
            if (gitHubService != nullptr && bundle.git.success) {
                bundle.issues = gitHubService->listIssuesForProject(projectId);
                bundle.pullRequests = gitHubService->listPullRequestsForProject(projectId);
            }
            return bundle;
        }));
}

void RepositoryPanel::onRefreshFinished()
{
    setRefreshing(false, false);

    settings::ProjectRecord activeProject;
    if (m_projectManager == nullptr || !m_projectManager->activeProject(&activeProject)) {
        showErrorState(i18n("The active project could not be loaded."));
        updateAutoRefreshTimer();
        return;
    }

    const RepositoryRefreshBundle bundle = m_refreshWatcher->result();
    if (!bundle.git.success) {
        qCWarning(qtcodeUi) << "Failed to refresh repository snapshot:" << bundle.git.errorMessage;
        showErrorState(i18n("Could not load repository status: %1", bundle.git.errorMessage));
        updateAutoRefreshTimer();
        return;
    }

    applySnapshot(bundle.git);
    showGitHubIssues(bundle.issues);
    showGitHubPullRequests(bundle.pullRequests);
    m_hasLoadedSnapshot = true;
    m_activeProjectId = activeProject.id;
    if (m_showStatusFeedback && m_detailView != nullptr) {
        m_detailView->clearDetail();
    }

    qCInfo(qtcodeUi) << "Repository snapshot refreshed with"
                     << bundle.git.status.changedFiles.size() << "changed file(s),"
                     << bundle.issues.issues.size() << "GitHub issue(s), and"
                     << bundle.pullRequests.pullRequests.size() << "pull request(s)"
                     << "in" << m_refreshTimer.elapsed() << "ms";

    if (m_showStatusFeedback && m_statusService != nullptr) {
        m_statusService->showMessage(i18n("Repository status refreshed."));
    }

    updateAutoRefreshTimer();
}

void RepositoryPanel::setRefreshing(bool refreshing, bool showLoadingUi)
{
    if (!refreshing || !showLoadingUi) {
        return;
    }

    setLabelTextIfChanged(m_changedFilesStateLabel, i18n("Loading changed files…"));
    setWidgetVisibleIfChanged(m_changedFilesStateLabel, true);
    setWidgetVisibleIfChanged(m_changedFilesList, false);
    setLabelTextIfChanged(m_issuesStateLabel, i18n("Loading GitHub issues…"));
    setWidgetVisibleIfChanged(m_issuesStateLabel, true);
    setWidgetVisibleIfChanged(m_issuesList, false);
    setLabelTextIfChanged(m_pullRequestsStateLabel, i18n("Loading GitHub pull requests…"));
    setWidgetVisibleIfChanged(m_pullRequestsStateLabel, true);
    setWidgetVisibleIfChanged(m_pullRequestsList, false);
}

void RepositoryPanel::applySnapshot(const qtcode::git::RepositoryGitSnapshot &snapshot)
{
    settings::ProjectRecord activeProject;
    if (m_projectManager != nullptr && m_projectManager->activeProject(&activeProject)) {
        setLabelTextIfChanged(
            m_projectLabel,
            i18n("Active project: %1\nBranch: %2", activeProject.name, snapshot.status.branchName));
    }

    showChangedFiles(snapshot.status);
}

void RepositoryPanel::showEmptyState(const QString &message)
{
    m_hasLoadedSnapshot = false;
    m_projectLabel->clear();
    setLabelTextIfChanged(m_changedFilesStateLabel, message);
    setWidgetVisibleIfChanged(m_changedFilesStateLabel, true);
    m_changedFilesList->clear();
    setWidgetVisibleIfChanged(m_changedFilesList, false);
    setLabelTextIfChanged(
        m_issuesStateLabel,
        i18n("GitHub issues load after a repository is selected."));
    setWidgetVisibleIfChanged(m_issuesStateLabel, true);
    m_issuesList->clear();
    setWidgetVisibleIfChanged(m_issuesList, false);
    setLabelTextIfChanged(
        m_pullRequestsStateLabel,
        i18n("GitHub pull requests load after a repository is selected."));
    setWidgetVisibleIfChanged(m_pullRequestsStateLabel, true);
    m_pullRequestsList->clear();
    setWidgetVisibleIfChanged(m_pullRequestsList, false);
    m_activeProjectId.clear();
}

void RepositoryPanel::showErrorState(const QString &message)
{
    if (m_statusService != nullptr) {
        m_statusService->showMessage(message, qtcode::core::StatusSeverity::Error);
    }

    m_changedFilesStateLabel->setText(message);
    m_changedFilesStateLabel->show();
    m_changedFilesList->clear();
    m_changedFilesList->hide();
    m_issuesStateLabel->hide();
    m_issuesList->clear();
    m_issuesList->hide();
    m_pullRequestsStateLabel->hide();
    m_pullRequestsList->clear();
    m_pullRequestsList->hide();
}

void RepositoryPanel::showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status)
{
    if (status.changedFiles.isEmpty()) {
        const QString emptyMessage = i18n("No local changes in the working tree.");
        setLabelTextIfChanged(m_changedFilesStateLabel, emptyMessage);
        if (m_changedFilesList->isVisible()) {
            m_changedFilesList->clear();
            setWidgetVisibleIfChanged(m_changedFilesList, false);
            setWidgetVisibleIfChanged(m_changedFilesStateLabel, true);
        } else if (!m_changedFilesStateLabel->isVisible()) {
            setWidgetVisibleIfChanged(m_changedFilesStateLabel, true);
        }
        return;
    }

    QList<PathListEntry> entries;
    entries.reserve(status.changedFiles.size());
    for (const qtcode::git::ChangedFile &changedFile : status.changedFiles) {
        PathListEntry entry;
        entry.text = QStringLiteral("%1 — %2").arg(changedFile.path, changedFile.statusLabel);
        entry.path = resolveChangedFilePath(changedFile.path);
        entries.append(entry);
    }

    syncPathListWidget(m_changedFilesList, entries);
    setWidgetVisibleIfChanged(m_changedFilesStateLabel, false);
    setWidgetVisibleIfChanged(m_changedFilesList, true);
}

void RepositoryPanel::onChangedFileClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit fileOpenRequested(path);
    }
}

QString RepositoryPanel::resolveChangedFilePath(const QString &relativePath) const
{
    const QString trimmedPath = relativePath.trimmed();
    if (trimmedPath.isEmpty() || m_projectManager == nullptr) {
        return {};
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject) || activeProject.rootPath.isEmpty()) {
        return {};
    }

    return QDir(activeProject.rootPath).absoluteFilePath(trimmedPath);
}

void RepositoryPanel::showGitHubIssues(const qtcode::github::GitHubIssueListResult &result)
{
    if (!result.success) {
        setLabelTextIfChanged(m_issuesStateLabel, result.errorMessage);
        setWidgetVisibleIfChanged(m_issuesStateLabel, true);
        if (m_issuesList->isVisible()) {
            m_issuesList->clear();
            setWidgetVisibleIfChanged(m_issuesList, false);
        }
        return;
    }

    if (result.issues.isEmpty()) {
        const QString message = result.fromCache
            ? i18n("No cached GitHub issues are available for this repository.")
            : i18n("No open GitHub issues were returned for this repository.");
        setLabelTextIfChanged(m_issuesStateLabel, message);
        setWidgetVisibleIfChanged(m_issuesStateLabel, true);
        if (m_issuesList->isVisible()) {
            m_issuesList->clear();
            setWidgetVisibleIfChanged(m_issuesList, false);
        }
        return;
    }

    if (result.fromCache) {
        setLabelTextIfChanged(
            m_issuesStateLabel,
            qtcode::github::GitHubCachePolicy::formatStatusLabel(
                result.fromCache,
                result.fetchedAt,
                result.isStale));
        setWidgetVisibleIfChanged(m_issuesStateLabel, true);
    } else {
        setWidgetVisibleIfChanged(m_issuesStateLabel, false);
    }

    QList<NumberedListEntry> entries;
    entries.reserve(result.issues.size());
    for (const qtcode::github::GitHubIssue &issue : result.issues) {
        entries.append(NumberedListEntry{
            issue.number,
            i18n("#%1 — %2\n%3, %4",
                 issue.number,
                 issue.title,
                 issue.state,
                 issue.author)});
    }

    syncNumberedListWidget(m_issuesList, entries);
    setWidgetVisibleIfChanged(m_issuesList, true);
}

void RepositoryPanel::showGitHubPullRequests(const qtcode::github::GitHubPullRequestListResult &result)
{
    if (!result.success) {
        setLabelTextIfChanged(m_pullRequestsStateLabel, result.errorMessage);
        setWidgetVisibleIfChanged(m_pullRequestsStateLabel, true);
        if (m_pullRequestsList->isVisible()) {
            m_pullRequestsList->clear();
            setWidgetVisibleIfChanged(m_pullRequestsList, false);
        }
        return;
    }

    if (result.pullRequests.isEmpty()) {
        const QString message = result.fromCache
            ? i18n("No cached GitHub pull requests are available for this repository.")
            : i18n("No open GitHub pull requests were returned for this repository.");
        setLabelTextIfChanged(m_pullRequestsStateLabel, message);
        setWidgetVisibleIfChanged(m_pullRequestsStateLabel, true);
        if (m_pullRequestsList->isVisible()) {
            m_pullRequestsList->clear();
            setWidgetVisibleIfChanged(m_pullRequestsList, false);
        }
        return;
    }

    if (result.fromCache) {
        setLabelTextIfChanged(
            m_pullRequestsStateLabel,
            qtcode::github::GitHubCachePolicy::formatStatusLabel(
                result.fromCache,
                result.fetchedAt,
                result.isStale));
        setWidgetVisibleIfChanged(m_pullRequestsStateLabel, true);
    } else {
        setWidgetVisibleIfChanged(m_pullRequestsStateLabel, false);
    }

    QList<NumberedListEntry> entries;
    entries.reserve(result.pullRequests.size());
    for (const qtcode::github::GitHubPullRequest &pullRequest : result.pullRequests) {
        entries.append(NumberedListEntry{
            pullRequest.number,
            i18n("#%1 — %2\n%3, %4",
                 pullRequest.number,
                 pullRequest.title,
                 pullRequest.state,
                 pullRequest.author)});
    }

    syncNumberedListWidget(m_pullRequestsList, entries);
    setWidgetVisibleIfChanged(m_pullRequestsList, true);
}

void RepositoryPanel::onPullRequestSelected()
{
    if (m_gitHubService == nullptr || m_pullRequestsList == nullptr || m_activeProjectId.isEmpty()) {
        return;
    }

    const QList<QListWidgetItem *> selectedItems = m_pullRequestsList->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    const int pullRequestNumber = selectedItems.first()->data(Qt::UserRole).toInt();
    if (pullRequestNumber <= 0) {
        return;
    }

    m_detailView->showLoadingMessage(i18n("Loading pull request #%1…", pullRequestNumber));

    const qtcode::github::GitHubPullRequestDetailResult detailResult =
        m_gitHubService->viewPullRequestForProject(m_activeProjectId, pullRequestNumber);
    if (!detailResult.success) {
        qCWarning(qtcodeUi) << "Failed to load pull request detail:" << detailResult.errorMessage;
        m_detailView->showErrorMessage(detailResult.errorMessage);
        return;
    }

    m_detailView->showPullRequest(
        detailResult.detail,
        cacheMetadataFromListResult(
            detailResult.fromCache,
            detailResult.isStale,
            detailResult.fetchedAt));
}

void RepositoryPanel::onIssueSelected()
{
    if (m_gitHubService == nullptr || m_issuesList == nullptr || m_activeProjectId.isEmpty()
        || m_detailView == nullptr) {
        return;
    }

    const QList<QListWidgetItem *> selectedItems = m_issuesList->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    const int issueNumber = selectedItems.first()->data(Qt::UserRole).toInt();
    if (issueNumber <= 0) {
        return;
    }

    m_detailView->showLoadingMessage(i18n("Loading issue #%1…", issueNumber));

    const qtcode::github::GitHubIssueDetailResult detailResult =
        m_gitHubService->viewIssueForProject(m_activeProjectId, issueNumber);
    if (!detailResult.success) {
        qCWarning(qtcodeUi) << "Failed to load issue detail:" << detailResult.errorMessage;
        m_detailView->showErrorMessage(detailResult.errorMessage);
        return;
    }

    m_detailView->showIssue(
        detailResult.detail,
        cacheMetadataFromListResult(
            detailResult.fromCache,
            detailResult.isStale,
            detailResult.fetchedAt));
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

    refreshStatus(true);
}

void RepositoryPanel::onActiveProjectChanged()
{
    m_hasLoadedSnapshot = false;
    if (m_detailView != nullptr) {
        m_detailView->clearDetail();
    }

    syncRepositorySelection();
    updateAutoRefreshTimer();

    if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
        refreshStatus(false);
    }
}

void RepositoryPanel::onAutoRefreshTimer()
{
    if (QGuiApplication::applicationState() != Qt::ApplicationActive) {
        return;
    }

    refreshStatus(false);
}

void RepositoryPanel::updateAutoRefreshTimer()
{
    if (m_autoRefreshTimer == nullptr) {
        return;
    }

    const bool shouldRun =
        m_projectManager != nullptr
        && m_projectManager->hasActiveProject()
        && !m_refreshWatcher->isRunning();

    if (shouldRun) {
        if (!m_autoRefreshTimer->isActive()) {
            m_autoRefreshTimer->start();
        }
        return;
    }

    m_autoRefreshTimer->stop();
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

    refreshStatus(true);
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

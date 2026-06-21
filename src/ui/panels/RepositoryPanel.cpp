#include "ui/panels/RepositoryPanel.h"

#include "core/ProjectManager.h"
#include "core/CliCapabilityService.h"
#include "core/CliCapabilityModels.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "core/WorkspaceInstaller.h"
#include "core/WorkspaceModels.h"
#include "git/GitService.h"
#include "github/GitHubService.h"
#include "github/GitHubCachePolicy.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "ui/dialogs/ChangeRepositoryDialog.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

#include <functional>

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
    QString url;
};

enum
{
    NumberRole = Qt::UserRole,
    UrlRole = Qt::UserRole + 1,
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
        if (list->item(i)->data(NumberRole).toInt() != entries.at(i).number
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
        : list->selectedItems().first()->data(NumberRole).toInt();

    if (numberedListWidgetMatches(list, entries)) {
        return;
    }

    list->clear();
    for (const NumberedListEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.text);
        item->setData(NumberRole, entry.number);
        item->setData(UrlRole, entry.url);
        list->addItem(item);
    }

    if (selectedNumber <= 0) {
        return;
    }

    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->data(NumberRole).toInt() == selectedNumber) {
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
    qtcode::core::WorkspaceInstaller *workspaceInstaller,
    QWidget *parent)
    : QWidget(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
    , m_cliCapabilityService(cliCapabilityService)
    , m_gitHubService(gitHubService)
    , m_statusService(statusService)
    , m_workspaceInstaller(workspaceInstaller)
    , m_refreshWatcher(new QFutureWatcher<RepositoryRefreshBundle>(this))
    , m_gitOperationWatcher(new QFutureWatcher<qtcode::git::GitOperationResult>(this))
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
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &RepositoryPanel::onActiveProjectChanged);

        updateAutoRefreshTimer();
    }

    if (m_workspaceInstaller != nullptr) {
        connect(
            m_workspaceInstaller,
            &qtcode::core::WorkspaceInstaller::workspaceInstalled,
            this,
            &RepositoryPanel::refreshWorkspaceSetupState);
    }

    refreshWorkspaceSetupState();

    connect(m_refreshWatcher, &QFutureWatcher<RepositoryRefreshBundle>::finished, this, &RepositoryPanel::onRefreshFinished);
    connect(m_gitOperationWatcher, &QFutureWatcher<qtcode::git::GitOperationResult>::finished, this, &RepositoryPanel::onGitOperationFinished);

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

    if (m_gitOperationWatcher->isRunning()) {
        m_gitOperationWatcher->waitForFinished();
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

    m_workspaceSetupWidget = new QWidget(this);
    auto *workspaceSetupLayout = new QVBoxLayout(m_workspaceSetupWidget);
    workspaceSetupLayout->setContentsMargins(0, 0, 0, 0);
    workspaceSetupLayout->setSpacing(6);

    m_workspaceSetupLabel = new QLabel(m_workspaceSetupWidget);
    m_workspaceSetupLabel->setWordWrap(true);
    m_workspaceSetupLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_workspaceSetupLabel->setText(
        i18n("This repository does not have QTCode memory scripts yet. Install the workspace bundle to add scripts, tools, and Cursor MCP config."));

    m_installWorkspaceButton = new QPushButton(i18n("Set up QTCode workspace"), m_workspaceSetupWidget);
    connect(
        m_installWorkspaceButton,
        &QPushButton::clicked,
        this,
        &RepositoryPanel::onInstallWorkspaceClicked);

    workspaceSetupLayout->addWidget(m_workspaceSetupLabel);
    workspaceSetupLayout->addWidget(m_installWorkspaceButton);
    m_workspaceSetupWidget->hide();

    QFont sectionFont = m_projectLabel->font();
    sectionFont.setBold(true);

    auto *sourceControlTitle = new QLabel(i18n("Source control"), this);
    sourceControlTitle->setFont(sectionFont);

    m_sourceControlStateLabel = new QLabel(this);
    m_sourceControlStateLabel->setWordWrap(true);
    m_sourceControlStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_commitMessageEdit = new QPlainTextEdit(this);
    m_commitMessageEdit->setPlaceholderText(i18n("Commit message"));
    m_commitMessageEdit->setMaximumHeight(72);
    m_commitMessageEdit->setTabChangesFocus(true);
    connect(m_commitMessageEdit, &QPlainTextEdit::textChanged, this, &RepositoryPanel::onCommitMessageChanged);

    m_commitButton = new QPushButton(i18n("Commit"), this);
    m_pushButton = new QPushButton(i18n("Push"), this);
    connect(m_commitButton, &QPushButton::clicked, this, &RepositoryPanel::onCommitClicked);
    connect(m_pushButton, &QPushButton::clicked, this, &RepositoryPanel::onPushClicked);

    auto *sourceControlActions = new QHBoxLayout();
    sourceControlActions->setContentsMargins(0, 0, 0, 0);
    sourceControlActions->addWidget(m_commitButton);
    sourceControlActions->addWidget(m_pushButton);
    sourceControlActions->addStretch();

    m_stagedSectionLabel = new QLabel(i18n("Staged changes"), this);
    m_stagedSectionLabel->setFont(sectionFont);

    m_unstageAllButton = new QPushButton(i18n("Unstage all"), this);
    m_unstageAllButton->setFlat(true);
    connect(m_unstageAllButton, &QPushButton::clicked, this, &RepositoryPanel::onUnstageAllClicked);

    auto *stagedHeader = new QHBoxLayout();
    stagedHeader->setContentsMargins(0, 0, 0, 0);
    stagedHeader->addWidget(m_stagedSectionLabel);
    stagedHeader->addStretch();
    stagedHeader->addWidget(m_unstageAllButton);

    m_stagedFilesList = new QListWidget(this);
    m_stagedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stagedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_stagedFilesList, &QListWidget::itemClicked, this, &RepositoryPanel::onChangedFileClicked);
    connect(
        m_stagedFilesList,
        &QListWidget::customContextMenuRequested,
        this,
        &RepositoryPanel::showStagedFilesContextMenu);

    m_changesSectionLabel = new QLabel(i18n("Changes"), this);
    m_changesSectionLabel->setFont(sectionFont);

    m_stageAllButton = new QPushButton(i18n("Stage all"), this);
    m_stageAllButton->setFlat(true);
    connect(m_stageAllButton, &QPushButton::clicked, this, &RepositoryPanel::onStageAllClicked);

    auto *changesHeader = new QHBoxLayout();
    changesHeader->setContentsMargins(0, 0, 0, 0);
    changesHeader->addWidget(m_changesSectionLabel);
    changesHeader->addStretch();
    changesHeader->addWidget(m_stageAllButton);

    m_unstagedFilesList = new QListWidget(this);
    m_unstagedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_unstagedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_unstagedFilesList, &QListWidget::itemClicked, this, &RepositoryPanel::onChangedFileClicked);
    connect(
        m_unstagedFilesList,
        &QListWidget::customContextMenuRequested,
        this,
        &RepositoryPanel::showUnstagedFilesContextMenu);

    auto *issuesTitle = new QLabel(i18n("GitHub issues"), this);
    issuesTitle->setFont(sectionFont);

    m_issuesStateLabel = new QLabel(this);
    m_issuesStateLabel->setWordWrap(true);
    m_issuesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_issuesList = new QListWidget(this);
    m_issuesList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_issuesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_issuesList, &QListWidget::itemSelectionChanged, this, &RepositoryPanel::onIssueSelected);
    connect(
        m_issuesList,
        &QListWidget::customContextMenuRequested,
        this,
        &RepositoryPanel::showIssuesContextMenu);

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

    layout->addWidget(titleLabel);
    layout->addWidget(m_capabilityStateLabel);
    layout->addWidget(m_workspaceSetupWidget);
    layout->addWidget(m_projectLabel);
    layout->addWidget(sourceControlTitle);
    layout->addWidget(m_sourceControlStateLabel);
    layout->addWidget(m_commitMessageEdit);
    layout->addLayout(sourceControlActions);
    layout->addLayout(stagedHeader);
    layout->addWidget(m_stagedFilesList);
    layout->addLayout(changesHeader);
    layout->addWidget(m_unstagedFilesList);
    layout->addWidget(issuesTitle);
    layout->addWidget(m_issuesStateLabel);
    layout->addWidget(m_issuesList);
    layout->addWidget(pullRequestsTitle);
    layout->addWidget(m_pullRequestsStateLabel);
    layout->addWidget(m_pullRequestsList);
}

void RepositoryPanel::refreshStatus(bool showStatusFeedback)
{
    if (m_refreshWatcher->isRunning()) {
        if (!showStatusFeedback) {
            m_pendingRefreshAfterGitOperation = true;
        }
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

    qCInfo(qtcodeUi) << "Repository snapshot refreshed with"
                     << bundle.git.status.stagedFiles.size() << "staged file(s),"
                     << bundle.git.status.unstagedFiles.size() << "unstaged file(s),"
                     << bundle.issues.issues.size() << "GitHub issue(s), and"
                     << bundle.pullRequests.pullRequests.size() << "pull request(s)"
                     << "in" << m_refreshTimer.elapsed() << "ms";

    if (m_showStatusFeedback && m_statusService != nullptr) {
        m_statusService->showMessage(i18n("Repository status refreshed."));
    }

    updateAutoRefreshTimer();

    if (m_pendingRefreshAfterGitOperation) {
        m_pendingRefreshAfterGitOperation = false;
        refreshStatus(false);
    }
}

void RepositoryPanel::setRefreshing(bool refreshing, bool showLoadingUi)
{
    if (!refreshing || !showLoadingUi) {
        return;
    }

    setLabelTextIfChanged(m_sourceControlStateLabel, i18n("Loading source control…"));
    setWidgetVisibleIfChanged(m_sourceControlStateLabel, true);
    setWidgetVisibleIfChanged(m_commitMessageEdit, false);
    setWidgetVisibleIfChanged(m_commitButton, false);
    setWidgetVisibleIfChanged(m_pushButton, false);
    setWidgetVisibleIfChanged(m_stagedSectionLabel, false);
    setWidgetVisibleIfChanged(m_unstageAllButton, false);
    setWidgetVisibleIfChanged(m_stagedFilesList, false);
    setWidgetVisibleIfChanged(m_changesSectionLabel, false);
    setWidgetVisibleIfChanged(m_stageAllButton, false);
    setWidgetVisibleIfChanged(m_unstagedFilesList, false);
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

    applyWorkingTreeStatus(snapshot.status);
}

void RepositoryPanel::applyWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status)
{
    m_commitsAhead = status.commitsAhead;

    const bool hasStagedFiles = !status.stagedFiles.isEmpty();
    const bool hasUnstagedFiles = !status.unstagedFiles.isEmpty();
    const bool hasWorkingTreeChanges = hasStagedFiles || hasUnstagedFiles;

    setWidgetVisibleIfChanged(m_commitMessageEdit, true);
    setWidgetVisibleIfChanged(m_commitButton, true);
    setWidgetVisibleIfChanged(m_pushButton, true);
    setWidgetVisibleIfChanged(m_stagedSectionLabel, true);
    setWidgetVisibleIfChanged(m_unstageAllButton, hasStagedFiles);
    setWidgetVisibleIfChanged(m_changesSectionLabel, true);
    setWidgetVisibleIfChanged(m_stageAllButton, hasUnstagedFiles);

    if (!hasWorkingTreeChanges) {
        const QString emptyMessage = i18n("No local changes in the working tree.");
        setLabelTextIfChanged(m_sourceControlStateLabel, emptyMessage);
        setWidgetVisibleIfChanged(m_sourceControlStateLabel, true);
        m_stagedFilesList->clear();
        m_unstagedFilesList->clear();
        setWidgetVisibleIfChanged(m_stagedFilesList, false);
        setWidgetVisibleIfChanged(m_unstagedFilesList, false);
    } else {
        setWidgetVisibleIfChanged(m_sourceControlStateLabel, false);

        QList<PathListEntry> stagedEntries;
        stagedEntries.reserve(status.stagedFiles.size());
        for (const qtcode::git::ChangedFile &changedFile : status.stagedFiles) {
            PathListEntry entry;
            entry.text = QStringLiteral("%1  %2").arg(changedFile.statusLabel, changedFile.path);
            entry.path = changedFile.path;
            stagedEntries.append(entry);
        }

        QList<PathListEntry> unstagedEntries;
        unstagedEntries.reserve(status.unstagedFiles.size());
        for (const qtcode::git::ChangedFile &changedFile : status.unstagedFiles) {
            PathListEntry entry;
            entry.text = QStringLiteral("%1  %2").arg(changedFile.statusLabel, changedFile.path);
            entry.path = changedFile.path;
            unstagedEntries.append(entry);
        }

        syncPathListWidget(m_stagedFilesList, stagedEntries);
        syncPathListWidget(m_unstagedFilesList, unstagedEntries);
        setWidgetVisibleIfChanged(m_stagedFilesList, hasStagedFiles);
        setWidgetVisibleIfChanged(m_unstagedFilesList, hasUnstagedFiles);
    }

    setLabelTextIfChanged(
        m_stagedSectionLabel,
        hasStagedFiles
            ? i18n("Staged changes (%1)", status.stagedFiles.size())
            : i18n("Staged changes"));
    setLabelTextIfChanged(
        m_changesSectionLabel,
        hasUnstagedFiles
            ? i18n("Changes (%1)", status.unstagedFiles.size())
            : i18n("Changes"));

    updateSourceControlActions(status);
}

void RepositoryPanel::showEmptyState(const QString &message)
{
    m_hasLoadedSnapshot = false;
    m_projectLabel->clear();
    setLabelTextIfChanged(m_sourceControlStateLabel, message);
    setWidgetVisibleIfChanged(m_sourceControlStateLabel, true);
    m_stagedFilesList->clear();
    m_unstagedFilesList->clear();
    setWidgetVisibleIfChanged(m_stagedFilesList, false);
    setWidgetVisibleIfChanged(m_unstagedFilesList, false);
    setWidgetVisibleIfChanged(m_commitMessageEdit, false);
    setWidgetVisibleIfChanged(m_commitButton, false);
    setWidgetVisibleIfChanged(m_pushButton, false);
    setWidgetVisibleIfChanged(m_stagedSectionLabel, false);
    setWidgetVisibleIfChanged(m_unstageAllButton, false);
    setWidgetVisibleIfChanged(m_changesSectionLabel, false);
    setWidgetVisibleIfChanged(m_stageAllButton, false);
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

    m_sourceControlStateLabel->setText(message);
    m_sourceControlStateLabel->show();
    m_stagedFilesList->clear();
    m_stagedFilesList->hide();
    m_unstagedFilesList->clear();
    m_unstagedFilesList->hide();
    m_commitMessageEdit->hide();
    m_commitButton->hide();
    m_pushButton->hide();
    m_issuesStateLabel->hide();
    m_issuesList->clear();
    m_issuesList->hide();
    m_pullRequestsStateLabel->hide();
    m_pullRequestsList->clear();
    m_pullRequestsList->hide();
}

void RepositoryPanel::onChangedFileClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const QString relativePath = item->data(Qt::UserRole).toString();
    const QString absolutePath = resolveChangedFilePath(relativePath);
    if (!absolutePath.isEmpty()) {
        emit fileOpenRequested(absolutePath);
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
                 issue.author),
            issue.url});
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
                 pullRequest.author),
            {}});
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

    if (m_statusService != nullptr) {
        m_statusService->showMessage(
            i18n("Loading pull request #%1…", pullRequestNumber),
            qtcode::core::StatusSeverity::Info);
    }

    const qtcode::github::GitHubPullRequestDetailResult detailResult =
        m_gitHubService->viewPullRequestForProject(m_activeProjectId, pullRequestNumber);
    if (!detailResult.success) {
        qCWarning(qtcodeUi) << "Failed to load pull request detail:" << detailResult.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                detailResult.errorMessage,
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    emit pullRequestOpenRequested(
        detailResult.detail,
        cacheMetadataFromListResult(
            detailResult.fromCache,
            detailResult.isStale,
            detailResult.fetchedAt));
}

void RepositoryPanel::onIssueSelected()
{
    if (m_gitHubService == nullptr || m_issuesList == nullptr || m_activeProjectId.isEmpty()) {
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

    if (m_statusService != nullptr) {
        m_statusService->showMessage(
            i18n("Loading issue #%1…", issueNumber),
            qtcode::core::StatusSeverity::Info);
    }

    const qtcode::github::GitHubIssueDetailResult detailResult =
        m_gitHubService->viewIssueForProject(m_activeProjectId, issueNumber);
    if (!detailResult.success) {
        qCWarning(qtcodeUi) << "Failed to load issue detail:" << detailResult.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                detailResult.errorMessage,
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    emit issueOpenRequested(
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

void RepositoryPanel::changeRepository()
{
    if (m_projectManager == nullptr) {
        showErrorState(i18n("Repository services are unavailable."));
        return;
    }

    ChangeRepositoryDialog dialog(
        m_projectManager,
        m_gitService,
        m_cliCapabilityService,
        m_statusService,
        this);
    connect(
        &dialog,
        &ChangeRepositoryDialog::activeRepositoryChanged,
        this,
        [this]() { refreshStatus(true); });

    dialog.exec();
}

void RepositoryPanel::onActiveProjectChanged()
{
    m_hasLoadedSnapshot = false;

    updateAutoRefreshTimer();
    refreshWorkspaceSetupState();

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
        m_gitAvailable = m_cliCapabilityService != nullptr && m_cliCapabilityService->isGitAvailable();
        return;
    }

    m_gitAvailable = snapshot.git.available;

    m_capabilityStateLabel->setText(messages.join(QStringLiteral("\n\n")));
    m_capabilityStateLabel->show();
}

void RepositoryPanel::refreshWorkspaceSetupState()
{
    if (m_workspaceSetupWidget == nullptr || m_workspaceInstaller == nullptr || m_projectManager == nullptr) {
        if (m_workspaceSetupWidget != nullptr) {
            m_workspaceSetupWidget->hide();
        }
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        m_workspaceSetupWidget->hide();
        return;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        m_workspaceSetupWidget->hide();
        return;
    }

    const bool installed = m_workspaceInstaller->isInstalled(activeProject.rootPath);
    m_workspaceSetupWidget->setVisible(!installed);
    if (m_installWorkspaceButton != nullptr) {
        m_installWorkspaceButton->setEnabled(true);
    }
}

void RepositoryPanel::onInstallWorkspaceClicked()
{
    if (m_workspaceInstaller == nullptr || m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        return;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return;
    }

    if (m_installWorkspaceButton != nullptr) {
        m_installWorkspaceButton->setEnabled(false);
    }

    qtcode::core::WorkspaceInstallContext context;
    context.projectName = activeProject.name;
    context.rootPath = activeProject.rootPath;
    context.scopeKey = activeProject.rootPath;

    qtcode::core::WorkspaceInstallResult result;
    QString errorMessage;
    if (!m_workspaceInstaller->install(context, &result, &errorMessage)) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(errorMessage, qtcode::core::StatusSeverity::Error);
        }
        QMessageBox::warning(this, i18n("Workspace setup failed"), errorMessage);
        if (m_installWorkspaceButton != nullptr) {
            m_installWorkspaceButton->setEnabled(true);
        }
        return;
    }

    refreshWorkspaceSetupState();
    if (m_statusService != nullptr) {
        m_statusService->showMessage(result.message);
    }
    QMessageBox::information(this, i18n("Workspace setup complete"), result.message);
}

void RepositoryPanel::updateSourceControlActions(const qtcode::git::GitWorkingTreeStatus &status)
{
    const bool hasStagedFiles = !status.stagedFiles.isEmpty();
    const bool hasCommitMessage = !m_commitMessageEdit->toPlainText().trimmed().isEmpty();
    const bool gitReady = m_gitAvailable && m_gitService != nullptr;
    const bool operationRunning = m_gitOperationWatcher != nullptr && m_gitOperationWatcher->isRunning();

    m_stageAllButton->setEnabled(gitReady && !status.unstagedFiles.isEmpty() && !operationRunning);
    m_unstageAllButton->setEnabled(gitReady && hasStagedFiles && !operationRunning);
    m_commitButton->setEnabled(gitReady && hasStagedFiles && hasCommitMessage && !operationRunning);

    if (status.commitsAhead > 0) {
        m_pushButton->setText(i18n("Push (%1)", status.commitsAhead));
    } else {
        m_pushButton->setText(i18n("Push"));
    }

    m_pushButton->setEnabled(gitReady && status.commitsAhead > 0 && !operationRunning);

    if (!gitReady) {
        m_commitButton->setToolTip(i18n("Install Git to commit changes."));
        m_pushButton->setToolTip(i18n("Install Git to push commits."));
    } else {
        m_commitButton->setToolTip(QString {});
        m_pushButton->setToolTip(QString {});
    }
}

void RepositoryPanel::onCommitMessageChanged()
{
    qtcode::git::GitWorkingTreeStatus status;
    status.stagedFiles = QList<qtcode::git::ChangedFile> {};
    for (int i = 0; i < m_stagedFilesList->count(); ++i) {
        qtcode::git::ChangedFile file;
        file.path = m_stagedFilesList->item(i)->data(Qt::UserRole).toString();
        status.stagedFiles.append(file);
    }
    status.commitsAhead = m_commitsAhead;
    updateSourceControlActions(status);
}

QString RepositoryPanel::resolveGitExecutable() const
{
    if (m_cliCapabilityService == nullptr) {
        return {};
    }

    return m_cliCapabilityService->snapshot().git.executablePath;
}

bool RepositoryPanel::activeRepositoryPath(QString *repositoryPath) const
{
    if (repositoryPath != nullptr) {
        repositoryPath->clear();
    }

    if (m_projectManager == nullptr) {
        return false;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject) || activeProject.rootPath.isEmpty()) {
        return false;
    }

    if (repositoryPath != nullptr) {
        *repositoryPath = activeProject.rootPath;
    }

    return true;
}

void RepositoryPanel::startGitOperation(
    const std::function<qtcode::git::GitOperationResult()> &operation,
    const QString &successMessage)
{
    if (m_gitService == nullptr || m_gitOperationWatcher == nullptr || !operation) {
        return;
    }

    if (m_gitOperationWatcher->isRunning()) {
        return;
    }

    m_pendingGitSuccessMessage = successMessage;

    if (m_statusService != nullptr) {
        m_statusService->showProgress(i18n("Running Git command…"));
    }

    m_commitButton->setEnabled(false);
    m_pushButton->setEnabled(false);
    m_stageAllButton->setEnabled(false);
    m_unstageAllButton->setEnabled(false);

    m_gitOperationWatcher->setFuture(QtConcurrent::run(operation));
}

void RepositoryPanel::showGitOperationResult(
    const qtcode::git::GitOperationResult &result,
    const QString &successMessage)
{
    if (!result.success) {
        qCWarning(qtcodeUi) << "Git operation failed:" << result.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(result.errorMessage, qtcode::core::StatusSeverity::Error);
        }
        refreshStatus(false);
        return;
    }

    if (m_statusService != nullptr) {
        m_statusService->showMessage(successMessage);
    }

    refreshStatus(false);
}

void RepositoryPanel::onGitOperationFinished()
{
    const qtcode::git::GitOperationResult result = m_gitOperationWatcher->result();
    if (result.success && m_clearCommitMessageOnSuccess) {
        const QSignalBlocker blocker(m_commitMessageEdit);
        m_commitMessageEdit->clear();
        m_clearCommitMessageOnSuccess = false;
    }

    showGitOperationResult(result, m_pendingGitSuccessMessage);
}

void RepositoryPanel::onStageAllClicked()
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    startGitOperation([this, repositoryPath, gitExecutable]() {
        return m_gitService->stageAll(repositoryPath, gitExecutable);
    }, i18n("All changes staged."));
}

void RepositoryPanel::onUnstageAllClicked()
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    startGitOperation([this, repositoryPath, gitExecutable]() {
        return m_gitService->unstageAll(repositoryPath, gitExecutable);
    }, i18n("All staged changes removed."));
}

void RepositoryPanel::stageRelativePaths(const QStringList &relativePaths)
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    startGitOperation([this, repositoryPath, gitExecutable, relativePaths]() {
        return m_gitService->stageFiles(repositoryPath, gitExecutable, relativePaths);
    }, i18n("Selected changes staged."));
}

void RepositoryPanel::unstageRelativePaths(const QStringList &relativePaths)
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    startGitOperation([this, repositoryPath, gitExecutable, relativePaths]() {
        return m_gitService->unstageFiles(repositoryPath, gitExecutable, relativePaths);
    }, i18n("Selected changes unstaged."));
}

QStringList RepositoryPanel::selectedRelativePaths(QListWidget *list) const
{
    QStringList paths;
    if (list == nullptr) {
        return paths;
    }

    const QList<QListWidgetItem *> selectedItems = list->selectedItems();
    paths.reserve(selectedItems.size());
    for (QListWidgetItem *item : selectedItems) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }

    return paths;
}

void RepositoryPanel::showStagedFilesContextMenu(const QPoint &position)
{
    if (m_stagedFilesList == nullptr) {
        return;
    }

    QListWidgetItem *item = m_stagedFilesList->itemAt(position);
    if (item == nullptr) {
        return;
    }

    if (!m_stagedFilesList->selectedItems().contains(item)) {
        m_stagedFilesList->setCurrentItem(item);
    }

    const QStringList relativePaths = selectedRelativePaths(m_stagedFilesList);
    if (relativePaths.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("vcs-removed")),
        i18n("Unstage"),
        this,
        [this, relativePaths]() {
            unstageRelativePaths(relativePaths);
        });
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("document-open")),
        i18n("Open file"),
        this,
        [this, relativePaths]() {
            const QString absolutePath = resolveChangedFilePath(relativePaths.first());
            if (!absolutePath.isEmpty()) {
                emit fileOpenRequested(absolutePath);
            }
        });

    menu.exec(m_stagedFilesList->viewport()->mapToGlobal(position));
}

void RepositoryPanel::showUnstagedFilesContextMenu(const QPoint &position)
{
    if (m_unstagedFilesList == nullptr) {
        return;
    }

    QListWidgetItem *item = m_unstagedFilesList->itemAt(position);
    if (item == nullptr) {
        return;
    }

    if (!m_unstagedFilesList->selectedItems().contains(item)) {
        m_unstagedFilesList->setCurrentItem(item);
    }

    const QStringList relativePaths = selectedRelativePaths(m_unstagedFilesList);
    if (relativePaths.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("vcs-added")),
        i18n("Stage"),
        this,
        [this, relativePaths]() {
            stageRelativePaths(relativePaths);
        });
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("document-open")),
        i18n("Open file"),
        this,
        [this, relativePaths]() {
            const QString absolutePath = resolveChangedFilePath(relativePaths.first());
            if (!absolutePath.isEmpty()) {
                emit fileOpenRequested(absolutePath);
            }
        });

    menu.exec(m_unstagedFilesList->viewport()->mapToGlobal(position));
}

void RepositoryPanel::onCommitClicked()
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    const QString message = m_commitMessageEdit->toPlainText().trimmed();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    if (message.isEmpty()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Enter a commit message before committing."),
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    m_clearCommitMessageOnSuccess = true;
    startGitOperation([this, repositoryPath, gitExecutable, message]() {
        return m_gitService->commit(repositoryPath, gitExecutable, message);
    }, i18n("Commit created."));
}

void RepositoryPanel::onPushClicked()
{
    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    if (m_commitsAhead <= 0) {
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        i18n("Push commits"),
        m_commitsAhead == 1
            ? i18n("Push 1 commit to the remote repository?")
            : i18n("Push %1 commits to the remote repository?", m_commitsAhead),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    startGitOperation([this, repositoryPath, gitExecutable]() {
        return m_gitService->push(repositoryPath, gitExecutable);
    }, i18n("Commits pushed."));
}

void RepositoryPanel::showIssuesContextMenu(const QPoint &position)
{
    if (m_issuesList == nullptr) {
        return;
    }

    QListWidgetItem *item = m_issuesList->itemAt(position);
    if (item == nullptr) {
        return;
    }

    const int issueNumber = item->data(NumberRole).toInt();
    if (issueNumber <= 0) {
        return;
    }

    const QString issueUrl = item->data(UrlRole).toString();

    QMenu menu(this);
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Add to Context"),
        this,
        [this, issueNumber]() {
            attachIssueToContext(issueNumber);
        });

    QAction *copyLinkAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("edit-copy")),
        i18n("Copy Link"));
    copyLinkAction->setEnabled(!issueUrl.isEmpty());
    connect(copyLinkAction, &QAction::triggered, this, [issueUrl]() {
        if (issueUrl.isEmpty()) {
            return;
        }

        QGuiApplication::clipboard()->setText(issueUrl);
    });

    menu.exec(m_issuesList->viewport()->mapToGlobal(position));
}

void RepositoryPanel::attachIssueToContext(int issueNumber)
{
    if (m_gitHubService == nullptr || m_activeProjectId.isEmpty() || issueNumber <= 0) {
        return;
    }

    if (m_statusService != nullptr) {
        m_statusService->showMessage(
            i18n("Loading issue #%1…", issueNumber),
            qtcode::core::StatusSeverity::Info);
    }

    const qtcode::github::GitHubIssueDetailResult detailResult =
        m_gitHubService->viewIssueForProject(m_activeProjectId, issueNumber);
    if (!detailResult.success) {
        qCWarning(qtcodeUi) << "Failed to load issue detail for context:" << detailResult.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                detailResult.errorMessage,
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    emit issueContextRequested(detailResult.detail);
}

} // namespace qtcode::ui

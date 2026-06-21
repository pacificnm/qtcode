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
#include "github/GitHubIssueBranchNaming.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "ui/dialogs/ChangeRepositoryDialog.h"
#include "ui/dialogs/CreateIssueBranchDialog.h"
#include "ui/dialogs/StageChangesDialog.h"
#include "ui/widgets/CollapsibleSection.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
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
    QString title;
};

enum
{
    NumberRole = Qt::UserRole,
    UrlRole = Qt::UserRole + 1,
    TitleRole = Qt::UserRole + 2,
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
        item->setData(TitleRole, entry.title);
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

    m_changesStateLabel = new QLabel(this);
    m_changesStateLabel->setWordWrap(true);
    m_changesStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_stageChangesButton = new QPushButton(i18n("Stage Changes"), this);
    m_stageChangesButton->setFlat(true);
    connect(m_stageChangesButton, &QPushButton::clicked, this, &RepositoryPanel::onStageChangesClicked);

    m_changesSection = new CollapsibleSection(i18n("Changes"), true, this);
    m_changesSection->headerTrailingLayout()->addWidget(m_stageChangesButton);
    m_changesSection->contentLayout()->addWidget(m_changesStateLabel);
    m_unstagedFilesList = new QListWidget(this);
    m_unstagedFilesList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_changesSection->contentLayout()->addWidget(m_unstagedFilesList, 1);
    m_unstagedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_unstagedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_unstagedFilesList, &QListWidget::itemClicked, this, &RepositoryPanel::onChangedFileClicked);
    connect(
        m_unstagedFilesList,
        &QListWidget::customContextMenuRequested,
        this,
        &RepositoryPanel::showUnstagedFilesContextMenu);

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

    m_issuesList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_issuesSection = new CollapsibleSection(i18n("GitHub issues"), false, this);
    m_issuesSection->contentLayout()->addWidget(m_issuesStateLabel);
    m_issuesSection->contentLayout()->addWidget(m_issuesList, 1);

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

    m_pullRequestsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_pullRequestsSection = new CollapsibleSection(i18n("GitHub pull requests"), false, this);
    m_pullRequestsSection->contentLayout()->addWidget(m_pullRequestsStateLabel);
    m_pullRequestsSection->contentLayout()->addWidget(m_pullRequestsList, 1);

    auto *headerWidget = new QWidget(this);
    headerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_projectLabel);
    headerLayout->addWidget(m_capabilityStateLabel);
    headerLayout->addWidget(m_workspaceSetupWidget);

    auto *headerSeparator = new QFrame(this);
    headerSeparator->setFrameShape(QFrame::HLine);
    headerSeparator->setFrameShadow(QFrame::Plain);
    headerSeparator->setLineWidth(1);

    layout->addWidget(headerWidget, 0, Qt::AlignTop);
    layout->addWidget(headerSeparator, 0, Qt::AlignTop);
    layout->addWidget(m_changesSection, 0);
    layout->addWidget(m_issuesSection, 0);
    layout->addWidget(m_pullRequestsSection, 0);
    layout->addStretch(1);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    connect(m_changesSection, &CollapsibleSection::expandedChanged, this, [this]() {
        updateAccordionStretch();
    });
    connect(m_issuesSection, &CollapsibleSection::expandedChanged, this, [this]() {
        updateAccordionStretch();
    });
    connect(m_pullRequestsSection, &CollapsibleSection::expandedChanged, this, [this]() {
        updateAccordionStretch();
    });
    updateAccordionStretch();
}

void RepositoryPanel::updateAccordionStretch()
{
    auto *panelLayout = qobject_cast<QVBoxLayout *>(layout());
    if (panelLayout == nullptr) {
        return;
    }

    const QList<CollapsibleSection *> sections = {
        m_changesSection,
        m_issuesSection,
        m_pullRequestsSection,
    };

    bool anyExpanded = false;
    for (CollapsibleSection *section : sections) {
        if (section == nullptr) {
            continue;
        }

        const int index = panelLayout->indexOf(section);
        if (index >= 0) {
            const int stretch = section->isExpanded() ? 1 : 0;
            panelLayout->setStretch(index, stretch);
            if (section->isExpanded()) {
                anyExpanded = true;
            }
        }
    }

    const int bottomSpacerIndex = panelLayout->count() - 1;
    if (bottomSpacerIndex >= 0) {
        panelLayout->setStretch(bottomSpacerIndex, anyExpanded ? 0 : 1);
    }
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

    setLabelTextIfChanged(m_changesStateLabel, i18n("Loading changes…"));
    setWidgetVisibleIfChanged(m_changesStateLabel, true);
    setWidgetVisibleIfChanged(m_stageChangesButton, false);
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
    m_lastWorkingTreeStatus = status;

    const bool hasStagedFiles = !status.stagedFiles.isEmpty();
    const bool hasUnstagedFiles = !status.unstagedFiles.isEmpty();
    const bool hasWorkingTreeChanges = hasStagedFiles || hasUnstagedFiles;

    setWidgetVisibleIfChanged(m_stageChangesButton, true);

    if (!hasWorkingTreeChanges) {
        const QString emptyMessage = i18n("No local changes in the working tree.");
        setLabelTextIfChanged(m_changesStateLabel, emptyMessage);
        setWidgetVisibleIfChanged(m_changesStateLabel, true);
        m_unstagedFilesList->clear();
        setWidgetVisibleIfChanged(m_unstagedFilesList, false);
    } else {
        if (hasUnstagedFiles) {
            setWidgetVisibleIfChanged(m_changesStateLabel, false);

            QList<PathListEntry> unstagedEntries;
            unstagedEntries.reserve(status.unstagedFiles.size());
            for (const qtcode::git::ChangedFile &changedFile : status.unstagedFiles) {
                PathListEntry entry;
                entry.text = QStringLiteral("%1  %2").arg(changedFile.statusLabel, changedFile.path);
                entry.path = changedFile.path;
                unstagedEntries.append(entry);
            }

            syncPathListWidget(m_unstagedFilesList, unstagedEntries);
            setWidgetVisibleIfChanged(m_unstagedFilesList, true);
        } else {
            setLabelTextIfChanged(
                m_changesStateLabel,
                i18n("All changes are staged. Use Stage Changes to commit or push."));
            setWidgetVisibleIfChanged(m_changesStateLabel, true);
            m_unstagedFilesList->clear();
            setWidgetVisibleIfChanged(m_unstagedFilesList, false);
        }
    }

    if (m_changesSection != nullptr) {
        m_changesSection->setTitle(
            hasUnstagedFiles
                ? i18n("Changes (%1)", status.unstagedFiles.size())
                : i18n("Changes"));
    }

    updateChangesActions(status);
}

void RepositoryPanel::showEmptyState(const QString &message)
{
    m_hasLoadedSnapshot = false;
    m_projectLabel->clear();
    setLabelTextIfChanged(m_changesStateLabel, message);
    setWidgetVisibleIfChanged(m_changesStateLabel, true);
    m_unstagedFilesList->clear();
    setWidgetVisibleIfChanged(m_unstagedFilesList, false);
    setWidgetVisibleIfChanged(m_stageChangesButton, false);
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

    m_changesStateLabel->setText(message);
    m_changesStateLabel->show();
    m_unstagedFilesList->clear();
    m_unstagedFilesList->hide();
    m_stageChangesButton->hide();
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
            issue.url,
            issue.title});
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
            {},
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

void RepositoryPanel::updateChangesActions(const qtcode::git::GitWorkingTreeStatus &status)
{
    const bool gitReady = m_gitAvailable && m_gitService != nullptr;
    const bool operationRunning = m_gitOperationWatcher != nullptr && m_gitOperationWatcher->isRunning();
    const bool hasWorkingTreeChanges =
        !status.stagedFiles.isEmpty() || !status.unstagedFiles.isEmpty();

    m_stageChangesButton->setEnabled(gitReady && hasWorkingTreeChanges && !operationRunning);

    if (!gitReady) {
        m_stageChangesButton->setToolTip(i18n("Install Git to stage and commit changes."));
    } else {
        m_stageChangesButton->setToolTip(QString {});
    }
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

    m_stageChangesButton->setEnabled(false);

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
    showGitOperationResult(result, m_pendingGitSuccessMessage);
}

void RepositoryPanel::onStageChangesClicked()
{
    if (m_gitService == nullptr || m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        return;
    }

    StageChangesDialog dialog(
        m_gitService,
        m_projectManager,
        m_cliCapabilityService,
        m_statusService,
        this);
    dialog.setWorkingTreeStatus(m_lastWorkingTreeStatus);
    connect(
        &dialog,
        &StageChangesDialog::changesUpdated,
        this,
        [this]() { refreshStatus(false); });
    connect(
        &dialog,
        &StageChangesDialog::fileOpenRequested,
        this,
        &RepositoryPanel::fileOpenRequested);

    dialog.exec();
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

    QStringList addablePaths;
    addablePaths.reserve(relativePaths.size());
    for (const QString &relativePath : relativePaths) {
        const QString absolutePath = resolveChangedFilePath(relativePath);
        if (!absolutePath.isEmpty() && QFileInfo(absolutePath).isFile()) {
            addablePaths.append(absolutePath);
        }
    }

    if (!addablePaths.isEmpty()) {
        menu.addAction(
            QIcon::fromTheme(QStringLiteral("bookmark-new")),
            i18n("Add to Context"),
            this,
            [this, addablePaths]() {
                for (const QString &absolutePath : addablePaths) {
                    emit fileContextRequested(absolutePath);
                }
            });
    }

    menu.exec(m_unstagedFilesList->viewport()->mapToGlobal(position));
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
    const QString issueTitle = item->data(TitleRole).toString();
    const QString existingBranch = resolveIssueBranchName(issueNumber);
    const bool gitReady = m_gitAvailable && m_gitService != nullptr;
    const bool ghReady =
        m_cliCapabilityService != nullptr
        && m_cliCapabilityService->snapshot().gh.available
        && m_cliCapabilityService->snapshot().gh.authenticated;

    QMenu menu(this);

    if (existingBranch.isEmpty()) {
        QAction *createBranchAction = menu.addAction(
            QIcon::fromTheme(QStringLiteral("list-add")),
            i18n("Create Branch"),
            this,
            [this, issueNumber, issueTitle]() {
                createIssueBranch(issueNumber, issueTitle);
            });
        createBranchAction->setEnabled(gitReady && ghReady);
    } else {
        QAction *changeBranchAction = menu.addAction(
            QIcon::fromTheme(QStringLiteral("vcs-branch")),
            i18n("Change Branch"),
            this,
            [this, existingBranch]() {
                checkoutIssueBranch(existingBranch);
            });
        changeBranchAction->setEnabled(gitReady);
    }

    menu.addSeparator();

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

QString RepositoryPanel::resolveIssueBranchName(int issueNumber) const
{
    if (issueNumber <= 0 || m_gitService == nullptr) {
        return {};
    }

    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty()) {
        return {};
    }

    QStringList branchReferences;
    QString errorMessage;
    if (!m_gitService->listRepositoryBranchReferences(
            repositoryPath,
            gitExecutable,
            &branchReferences,
            true,
            &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to list repository branches:" << errorMessage;
        return {};
    }

    QStringList linkedBranches;
    if (m_gitHubService != nullptr && m_cliCapabilityService != nullptr
        && m_cliCapabilityService->snapshot().gh.available
        && m_cliCapabilityService->snapshot().gh.authenticated
        && !m_activeProjectId.isEmpty()) {
        const qtcode::github::GitHubIssueBranchListResult linkedResult =
            m_gitHubService->listIssueLinkedBranchesForProject(m_activeProjectId, issueNumber);
        if (linkedResult.success) {
            linkedBranches = linkedResult.branchNames;
        }
    }

    return qtcode::github::resolveIssueBranchName(issueNumber, linkedBranches, branchReferences);
}

void RepositoryPanel::createIssueBranch(int issueNumber, const QString &issueTitle)
{
    if (m_activeProjectId.isEmpty() || issueNumber <= 0) {
        return;
    }

    CreateIssueBranchDialog dialog(
        m_gitService,
        m_gitHubService,
        m_projectManager,
        m_cliCapabilityService,
        m_statusService,
        m_activeProjectId,
        issueNumber,
        issueTitle,
        this);
    connect(
        &dialog,
        &CreateIssueBranchDialog::branchChanged,
        this,
        [this]() { refreshStatus(true); });

    dialog.exec();
}

void RepositoryPanel::checkoutIssueBranch(const QString &branchName)
{
    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedBranchName.isEmpty()) {
        return;
    }

    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    startGitOperation(
        [this, repositoryPath, gitExecutable, trimmedBranchName]() {
            return m_gitService->checkoutRemoteBranch(repositoryPath, gitExecutable, trimmedBranchName);
        },
        i18n("Checked out branch %1.", trimmedBranchName));
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

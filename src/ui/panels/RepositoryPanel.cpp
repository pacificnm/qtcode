#include "ui/panels/RepositoryPanel.h"

#include "core/ProjectManager.h"
#include "git/GitCommitSummary.h"
#include "git/GitService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace qtcode::ui {

RepositoryPanel::RepositoryPanel(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    QWidget *parent)
    : QWidget(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
    , m_refreshWatcher(new QFutureWatcher<qtcode::git::RepositoryGitSnapshot>(this))
{
    configureLayout();
    connect(m_refreshWatcher, &QFutureWatcher<qtcode::git::RepositoryGitSnapshot>::finished, this, &RepositoryPanel::onRefreshFinished);
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

    m_refreshButton = new QPushButton(i18n("Refresh status"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &RepositoryPanel::refreshStatus);

    auto *changedFilesTitle = new QLabel(i18n("Changed files"), this);
    QFont sectionFont = changedFilesTitle->font();
    sectionFont.setBold(true);
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

    layout->addWidget(titleLabel);
    layout->addWidget(m_projectLabel);
    layout->addWidget(m_refreshButton);
    layout->addWidget(changedFilesTitle);
    layout->addWidget(m_changedFilesStateLabel);
    layout->addWidget(m_changedFilesList);
    layout->addWidget(commitsTitle);
    layout->addWidget(m_commitsStateLabel);
    layout->addWidget(m_commitsList, 1);
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

    startRefresh(activeProject.rootPath);
}

void RepositoryPanel::startRefresh(const QString &repositoryPath)
{
    m_projectLabel->setText(i18n("Active project loading…"));
    setRefreshing(true);

    m_refreshWatcher->setFuture(QtConcurrent::run(
        [repositoryPath]() {
            return qtcode::git::loadRepositoryGitSnapshot(repositoryPath, 10);
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

    const qtcode::git::RepositoryGitSnapshot snapshot = m_refreshWatcher->result();
    if (!snapshot.success) {
        qCWarning(qtcodeUi) << "Failed to refresh repository snapshot:" << snapshot.errorMessage;
        showErrorState(i18n("Could not load repository status: %1", snapshot.errorMessage));
        return;
    }

    applySnapshot(snapshot);

    qCInfo(qtcodeUi) << "Repository snapshot refreshed with"
                     << snapshot.status.changedFiles.size() << "changed file(s) and"
                     << snapshot.commits.size() << "recent commit(s)";
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
    m_refreshButton->setEnabled(false);
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

} // namespace qtcode::ui

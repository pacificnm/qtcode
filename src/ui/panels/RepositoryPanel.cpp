#include "ui/panels/RepositoryPanel.h"

#include "core/ProjectManager.h"
#include "git/GitService.h"
#include "git/GitStatus.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace qtcode::ui {

RepositoryPanel::RepositoryPanel(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    QWidget *parent)
    : QWidget(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
{
    configureLayout();
    refreshStatus();
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

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_changedFilesList = new QListWidget(this);
    m_changedFilesList->setSelectionMode(QAbstractItemView::NoSelection);

    layout->addWidget(titleLabel);
    layout->addWidget(m_projectLabel);
    layout->addWidget(m_refreshButton);
    layout->addWidget(changedFilesTitle);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_changedFilesList, 1);
}

void RepositoryPanel::refreshStatus()
{
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

    m_projectLabel->setText(
        i18n("Active project: %1", activeProject.name));

    qtcode::git::GitWorkingTreeStatus status;
    QString errorMessage;
    if (!m_gitService->loadWorkingTreeStatus(activeProject.rootPath, &status, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to refresh repository status:" << errorMessage;
        showErrorState(
            i18n("Could not load repository status: %1", errorMessage));
        return;
    }

    m_projectLabel->setText(
        i18n("Active project: %1\nBranch: %2", activeProject.name, status.branchName));
    showChangedFiles(status);

    qCInfo(qtcodeUi) << "Repository status refreshed with"
                     << status.changedFiles.size() << "changed file(s)";
}

void RepositoryPanel::showEmptyState(const QString &message)
{
    m_projectLabel->clear();
    m_stateLabel->setText(message);
    m_stateLabel->show();
    m_changedFilesList->clear();
    m_changedFilesList->hide();
    m_refreshButton->setEnabled(m_projectManager != nullptr && m_projectManager->hasActiveProject());
}

void RepositoryPanel::showErrorState(const QString &message)
{
    m_stateLabel->setText(message);
    m_stateLabel->show();
    m_changedFilesList->clear();
    m_changedFilesList->hide();
    m_refreshButton->setEnabled(true);
}

void RepositoryPanel::showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status)
{
    m_changedFilesList->clear();

    if (status.changedFiles.isEmpty()) {
        m_stateLabel->setText(i18n("No local changes in the working tree."));
        m_stateLabel->show();
        m_changedFilesList->hide();
    } else {
        m_stateLabel->hide();

        for (const qtcode::git::ChangedFile &changedFile : status.changedFiles) {
            m_changedFilesList->addItem(
                QStringLiteral("%1 — %2").arg(changedFile.path, changedFile.statusLabel));
        }

        m_changedFilesList->show();
    }

    m_refreshButton->setEnabled(true);
}

} // namespace qtcode::ui

#include "ui/dialogs/ChangeRepositoryDialog.h"

#include "core/CliCapabilityService.h"
#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "git/GitService.h"
#include "shared/Logging.h"
#include "ui/models/RepositoryListModel.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace qtcode::ui {

ChangeRepositoryDialog::ChangeRepositoryDialog(
    qtcode::core::ProjectManager *projectManager,
    qtcode::git::GitService *gitService,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QDialog(parent)
    , m_projectManager(projectManager)
    , m_gitService(gitService)
    , m_cliCapabilityService(cliCapabilityService)
    , m_statusService(statusService)
{
    setWindowTitle(i18n("Change Repository"));
    setModal(true);
    setMinimumSize(420, 320);
    resize(480, 360);

    configureLayout();
    refreshCapabilityState();

    if (m_cliCapabilityService != nullptr) {
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            &ChangeRepositoryDialog::refreshCapabilityState);
    }

    if (m_projectManager != nullptr) {
        m_repositoryModel = new RepositoryListModel(m_projectManager, this);
        m_repositoryList->setModel(m_repositoryModel);

        connect(
            m_repositoryList->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &ChangeRepositoryDialog::onRepositorySelected);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::projectsChanged,
            this,
            &ChangeRepositoryDialog::updateEmptyState);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &ChangeRepositoryDialog::syncRepositorySelection);

        syncRepositorySelection();
        updateEmptyState();
    }
}

void ChangeRepositoryDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *descriptionLabel = new QLabel(
        i18n("Select a local repository to make it the active project."),
        this);
    descriptionLabel->setWordWrap(true);

    m_emptyStateLabel = new QLabel(
        i18n("No local repositories yet. Use File > Add Repository to add one."),
        this);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->hide();

    m_repositoryList = new QListView(this);
    m_repositoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_repositoryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_repositoryList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        m_repositoryList,
        &QListView::customContextMenuRequested,
        this,
        &ChangeRepositoryDialog::showRepositoryContextMenu);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(descriptionLabel);
    layout->addWidget(m_emptyStateLabel);
    layout->addWidget(m_repositoryList, 1);
    layout->addWidget(buttonBox);
}

void ChangeRepositoryDialog::updateEmptyState()
{
    const bool hasRepositories =
        m_projectManager != nullptr && !m_projectManager->projects().isEmpty();

    if (m_emptyStateLabel != nullptr) {
        m_emptyStateLabel->setVisible(!hasRepositories);
    }
    if (m_repositoryList != nullptr) {
        m_repositoryList->setVisible(hasRepositories);
    }
}

void ChangeRepositoryDialog::onRepositorySelected(const QModelIndex &current, const QModelIndex &)
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
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Could not open repository: %1", errorMessage),
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    emit activeRepositoryChanged();
}

void ChangeRepositoryDialog::syncRepositorySelection()
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

void ChangeRepositoryDialog::refreshCapabilityState()
{
    if (m_cliCapabilityService == nullptr) {
        m_gitAvailable = false;
        return;
    }

    m_gitAvailable = m_cliCapabilityService->isGitAvailable();
}

QString ChangeRepositoryDialog::resolveGitExecutable() const
{
    if (m_cliCapabilityService == nullptr) {
        return {};
    }

    return m_cliCapabilityService->snapshot().git.executablePath;
}

void ChangeRepositoryDialog::showRepositoryContextMenu(const QPoint &position)
{
    if (m_repositoryList == nullptr || m_repositoryModel == nullptr || m_projectManager == nullptr) {
        return;
    }

    const QModelIndex index = m_repositoryList->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    const QString projectId = index.data(RepositoryListModel::ProjectIdRole).toString();
    const QString repositoryPath = index.data(RepositoryListModel::RootPathRole).toString();
    const bool gitReady = m_gitAvailable && m_gitService != nullptr && !repositoryPath.isEmpty();

    QMenu menu(this);

    QAction *selectBranchAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("vcs-branch")),
        i18n("Select Branch"),
        this,
        [this, projectId, repositoryPath]() {
            selectBranchForRepository(projectId, repositoryPath);
        });
    selectBranchAction->setEnabled(gitReady);

    QAction *createBranchAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("list-add")),
        i18n("Create Branch"),
        this,
        [this, projectId, repositoryPath]() {
            createBranchForRepository(projectId, repositoryPath);
        });
    createBranchAction->setEnabled(gitReady);

    menu.addSeparator();

    menu.addAction(
        QIcon::fromTheme(QStringLiteral("list-remove")),
        i18n("Remove from list"),
        this,
        [this, index]() {
            removeRepositoryAtIndex(index);
        });

    menu.exec(m_repositoryList->viewport()->mapToGlobal(position));
}

void ChangeRepositoryDialog::selectBranchForRepository(
    const QString &projectId,
    const QString &repositoryPath)
{
    Q_UNUSED(projectId)

    if (m_gitService == nullptr || repositoryPath.isEmpty()) {
        return;
    }

    QStringList branches;
    QString currentBranch;
    QString errorMessage;
    if (!m_gitService->listLocalBranches(repositoryPath, &branches, &currentBranch, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to list branches:" << errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(errorMessage, qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    if (branches.isEmpty()) {
        QMessageBox::information(
            this,
            i18n("Select Branch"),
            i18n("This repository has no local branches yet."));
        return;
    }

    bool accepted = false;
    const int currentIndex = branches.indexOf(currentBranch);
    const QString selectedBranch = QInputDialog::getItem(
        this,
        i18n("Select Branch"),
        i18n("Branch:"),
        branches,
        currentIndex >= 0 ? currentIndex : 0,
        false,
        &accepted);
    if (!accepted || selectedBranch.isEmpty() || selectedBranch == currentBranch) {
        return;
    }

    checkoutBranchForRepository(projectId, repositoryPath, selectedBranch);
}

void ChangeRepositoryDialog::createBranchForRepository(
    const QString &projectId,
    const QString &repositoryPath)
{
    if (repositoryPath.isEmpty()) {
        return;
    }

    bool accepted = false;
    const QString branchName = QInputDialog::getText(
        this,
        i18n("Create Branch"),
        i18n("Branch name:"),
        QLineEdit::Normal,
        QString {},
        &accepted);
    if (!accepted) {
        return;
    }

    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedBranchName.isEmpty()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Branch name is required."),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    checkoutBranchForRepository(
        projectId,
        repositoryPath,
        trimmedBranchName,
        true);
}

void ChangeRepositoryDialog::checkoutBranchForRepository(
    const QString &projectId,
    const QString &repositoryPath,
    const QString &branchName,
    bool createBranch)
{
    const QString gitExecutable = resolveGitExecutable();
    if (gitExecutable.isEmpty() || m_gitService == nullptr || repositoryPath.isEmpty() || branchName.isEmpty()) {
        return;
    }

    const qtcode::git::GitOperationResult result = createBranch
        ? m_gitService->createBranch(repositoryPath, gitExecutable, branchName)
        : m_gitService->checkoutBranch(repositoryPath, gitExecutable, branchName);

    if (!result.success) {
        qCWarning(qtcodeUi) << "Git branch operation failed:" << result.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(result.errorMessage, qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    const QString successMessage = createBranch
        ? i18n("Created and checked out branch %1.", branchName)
        : i18n("Checked out branch %1.", branchName);
    if (m_statusService != nullptr) {
        m_statusService->showMessage(successMessage);
    }

    if (m_projectManager != nullptr && projectId == m_projectManager->activeProjectId()) {
        emit activeRepositoryChanged();
    }
}

void ChangeRepositoryDialog::removeRepositoryAtIndex(const QModelIndex &index)
{
    if (!index.isValid() || m_projectManager == nullptr) {
        return;
    }

    const QString projectId = index.data(RepositoryListModel::ProjectIdRole).toString();
    const QString projectName = index.data(RepositoryListModel::NameRole).toString();
    if (projectId.isEmpty()) {
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        i18n("Remove repository"),
        i18n(
            "Remove %1 from the local repository list?\n\n"
            "This only removes it from QTCode. Files on disk are not deleted.",
            projectName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_projectManager->removeLocalRepository(projectId, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to remove repository:" << errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Could not remove repository: %1", errorMessage),
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    updateEmptyState();

    if (projectId == m_projectManager->activeProjectId() || !m_projectManager->hasActiveProject()) {
        emit activeRepositoryChanged();
    }
}

} // namespace qtcode::ui

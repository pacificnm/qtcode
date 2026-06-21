#include "ui/dialogs/CreateIssueBranchDialog.h"

#include "core/CliCapabilityService.h"
#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "git/GitOperationResult.h"
#include "git/GitService.h"
#include "github/GitHubIssueBranchNaming.h"
#include "github/GitHubService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace qtcode::ui {

CreateIssueBranchDialog::CreateIssueBranchDialog(
    qtcode::git::GitService *gitService,
    qtcode::github::GitHubService *gitHubService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::core::StatusService *statusService,
    const QString &projectId,
    int issueNumber,
    const QString &issueTitle,
    QWidget *parent)
    : QDialog(parent)
    , m_gitService(gitService)
    , m_gitHubService(gitHubService)
    , m_projectManager(projectManager)
    , m_cliCapabilityService(cliCapabilityService)
    , m_statusService(statusService)
    , m_projectId(projectId)
    , m_issueNumber(issueNumber)
    , m_issueTitle(issueTitle)
    , m_createWatcher(new QFutureWatcher<qtcode::git::GitOperationResult>(this))
    , m_checkoutWatcher(new QFutureWatcher<qtcode::git::GitOperationResult>(this))
{
    setWindowTitle(i18n("Create Branch"));
    configureLayout();
    loadBranchOptions();

    connect(
        m_createWatcher,
        &QFutureWatcher<qtcode::git::GitOperationResult>::finished,
        this,
        &CreateIssueBranchDialog::onCreateOperationFinished);
    connect(
        m_checkoutWatcher,
        &QFutureWatcher<qtcode::git::GitOperationResult>::finished,
        this,
        &CreateIssueBranchDialog::onCheckoutOperationFinished);
}

void CreateIssueBranchDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_issueLabel = new QLabel(this);
    m_issueLabel->setWordWrap(true);
    m_issueLabel->setText(i18n("Issue #%1 — %2", m_issueNumber, m_issueTitle));

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->hide();

    m_branchNameEdit = new QLineEdit(this);
    m_branchNameEdit->setText(qtcode::github::suggestedIssueBranchName(m_issueNumber, m_issueTitle));

    m_baseBranchCombo = new QComboBox(this);
    m_baseBranchCombo->setEditable(false);

    auto *formLayout = new QFormLayout();
    formLayout->addRow(i18n("Branch name:"), m_branchNameEdit);
    formLayout->addRow(i18n("Create from:"), m_baseBranchCombo);

    m_changeBranchButton = new QPushButton(i18n("Change Branch"), this);
    m_changeBranchButton->hide();
    connect(m_changeBranchButton, &QPushButton::clicked, this, &CreateIssueBranchDialog::onChangeBranchClicked);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *createButton = m_buttonBox->addButton(i18n("Create Branch"), QDialogButtonBox::AcceptRole);
    m_createBranchButton = createButton;
    connect(createButton, &QPushButton::clicked, this, &CreateIssueBranchDialog::onCreateClicked);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(m_issueLabel);
    layout->addLayout(formLayout);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_changeBranchButton, 0, Qt::AlignLeft);
    layout->addWidget(m_buttonBox);

    if (m_cliCapabilityService != nullptr) {
        const auto snapshot = m_cliCapabilityService->snapshot();
        m_gitAvailable = snapshot.git.available;
        m_ghAvailable = snapshot.gh.available && snapshot.gh.authenticated;
    }

    createButton->setEnabled(m_gitAvailable && m_ghAvailable);
    if (!m_gitAvailable || !m_ghAvailable) {
        m_stateLabel->setText(i18n("Git and an authenticated GitHub CLI session are required to create issue branches."));
        m_stateLabel->show();
    }

    resize(480, 220);
}

void CreateIssueBranchDialog::loadBranchOptions()
{
    if (m_baseBranchCombo == nullptr || m_gitService == nullptr) {
        return;
    }

    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty()) {
        return;
    }

    QStringList branches;
    QString errorMessage;
    if (!m_gitService->listLocalBranches(repositoryPath, &branches, nullptr, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to load local branches:" << errorMessage;
        return;
    }

    if (branches.isEmpty()) {
        m_baseBranchCombo->addItem(QStringLiteral("main"));
        return;
    }

    m_baseBranchCombo->addItems(branches);
    m_baseBranchCombo->setCurrentIndex(defaultBaseBranchIndex(branches));
}

int CreateIssueBranchDialog::defaultBaseBranchIndex(const QStringList &branches) const
{
    const int mainIndex = branches.indexOf(QStringLiteral("main"));
    if (mainIndex >= 0) {
        return mainIndex;
    }

    const int masterIndex = branches.indexOf(QStringLiteral("master"));
    if (masterIndex >= 0) {
        return masterIndex;
    }

    return 0;
}

QString CreateIssueBranchDialog::resolveGitExecutable() const
{
    if (m_cliCapabilityService == nullptr) {
        return {};
    }

    return m_cliCapabilityService->snapshot().git.executablePath;
}

bool CreateIssueBranchDialog::activeRepositoryPath(QString *repositoryPath) const
{
    if (repositoryPath != nullptr) {
        repositoryPath->clear();
    }

    if (m_projectManager == nullptr) {
        return false;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject) || activeProject.rootPath.isEmpty()) {
        return false;
    }

    if (activeProject.id != m_projectId) {
        return false;
    }

    if (repositoryPath != nullptr) {
        *repositoryPath = activeProject.rootPath;
    }

    return true;
}

void CreateIssueBranchDialog::setBusy(bool busy)
{
    if (m_branchNameEdit != nullptr) {
        m_branchNameEdit->setEnabled(!busy);
    }
    if (m_baseBranchCombo != nullptr) {
        m_baseBranchCombo->setEnabled(!busy);
    }
    if (m_buttonBox != nullptr) {
        m_buttonBox->setEnabled(!busy);
    }
    if (m_changeBranchButton != nullptr && !m_createdBranchName.isEmpty()) {
        m_changeBranchButton->setEnabled(!busy);
    }

    if (busy && m_statusService != nullptr) {
        m_statusService->showProgress(i18n("Creating issue branch…"));
    }
}

void CreateIssueBranchDialog::onCreateClicked()
{
    if (m_createWatcher->isRunning() || m_gitHubService == nullptr) {
        return;
    }

    const QString branchName = m_branchNameEdit != nullptr ? m_branchNameEdit->text().trimmed() : QString {};
    const QString baseBranch =
        m_baseBranchCombo != nullptr ? m_baseBranchCombo->currentText().trimmed() : QString {};

    if (branchName.isEmpty()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Branch name is required."),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    if (baseBranch.isEmpty()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Base branch is required."),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty() || m_gitService == nullptr) {
        return;
    }

    setBusy(true);

    const QString projectId = m_projectId;
    const int issueNumber = m_issueNumber;

    m_createWatcher->setFuture(QtConcurrent::run([this,
                                                  projectId,
                                                  issueNumber,
                                                  baseBranch,
                                                  branchName,
                                                  repositoryPath,
                                                  gitExecutable]() {
        qtcode::git::GitOperationResult operationResult;

        const qtcode::github::GitHubIssueBranchDevelopResult developResult =
            m_gitHubService->developIssueBranchForProject(projectId, issueNumber, baseBranch, branchName, false);
        if (!developResult.success) {
            operationResult.errorMessage = developResult.errorMessage;
            return operationResult;
        }

        operationResult = m_gitService->fetchRemoteBranch(
            repositoryPath,
            gitExecutable,
            QStringLiteral("origin"),
            branchName);
        return operationResult;
    }));
}

void CreateIssueBranchDialog::onCreateOperationFinished()
{
    setBusy(false);

    const qtcode::git::GitOperationResult result = m_createWatcher->result();
    if (!result.success) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(result.errorMessage, qtcode::core::StatusSeverity::Error);
        }
        if (m_stateLabel != nullptr) {
            m_stateLabel->setText(i18n("Could not create the issue branch. Check the status bar for details."));
            m_stateLabel->show();
        }
        return;
    }

    const QString branchName = m_branchNameEdit != nullptr ? m_branchNameEdit->text().trimmed() : QString {};
    showCreatedState(branchName);

    if (m_statusService != nullptr) {
        m_statusService->showMessage(i18n("Created branch %1 and fetched it into the local repository.", branchName));
    }
}

void CreateIssueBranchDialog::showCreatedState(const QString &branchName)
{
    m_createdBranchName = branchName;

    if (m_branchNameEdit != nullptr) {
        m_branchNameEdit->setEnabled(false);
    }
    if (m_baseBranchCombo != nullptr) {
        m_baseBranchCombo->setEnabled(false);
    }
    if (m_buttonBox != nullptr) {
        m_buttonBox->button(QDialogButtonBox::Cancel)->setText(i18n("Close"));
        if (m_createBranchButton != nullptr) {
            m_createBranchButton->hide();
        }
    }
    if (m_stateLabel != nullptr) {
        m_stateLabel->setText(
            i18n("Branch %1 is ready in the local repository. Change branch to start working on this issue.",
                 branchName));
        m_stateLabel->show();
    }
    if (m_changeBranchButton != nullptr) {
        m_changeBranchButton->show();
        m_changeBranchButton->setEnabled(true);
    }
}

void CreateIssueBranchDialog::onChangeBranchClicked()
{
    startCheckout();
}

void CreateIssueBranchDialog::startCheckout()
{
    if (m_checkoutWatcher->isRunning() || m_createdBranchName.isEmpty() || m_gitService == nullptr) {
        return;
    }

    QString repositoryPath;
    const QString gitExecutable = resolveGitExecutable();
    if (!activeRepositoryPath(&repositoryPath) || gitExecutable.isEmpty()) {
        return;
    }

    if (m_statusService != nullptr) {
        m_statusService->showProgress(i18n("Switching branch…"));
    }

    const QString branchName = m_createdBranchName;
    m_checkoutWatcher->setFuture(QtConcurrent::run([this, repositoryPath, gitExecutable, branchName]() {
        return m_gitService->checkoutRemoteBranch(repositoryPath, gitExecutable, branchName);
    }));
}

void CreateIssueBranchDialog::onCheckoutOperationFinished()
{
    const qtcode::git::GitOperationResult result = m_checkoutWatcher->result();
    if (!result.success) {
        qCWarning(qtcodeUi) << "Failed to checkout issue branch:" << result.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(result.errorMessage, qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    if (m_statusService != nullptr) {
        m_statusService->showMessage(i18n("Checked out branch %1.", m_createdBranchName));
    }

    emit branchChanged();
    accept();
}

} // namespace qtcode::ui

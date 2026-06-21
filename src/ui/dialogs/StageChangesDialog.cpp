#include "ui/dialogs/StageChangesDialog.h"

#include "core/CliCapabilityService.h"
#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "git/GitCommitSummary.h"
#include "git/GitService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace qtcode::ui {

namespace {

struct PathListEntry
{
    QString path;
    QString text;
};

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

StageChangesDialog::StageChangesDialog(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QDialog(parent)
    , m_gitService(gitService)
    , m_projectManager(projectManager)
    , m_cliCapabilityService(cliCapabilityService)
    , m_statusService(statusService)
    , m_gitOperationWatcher(new QFutureWatcher<qtcode::git::GitOperationResult>(this))
{
    setWindowTitle(i18n("Stage Changes"));
    setModal(true);
    setMinimumSize(480, 520);
    resize(520, 560);

    configureLayout();

    if (m_cliCapabilityService != nullptr) {
        m_gitAvailable = m_cliCapabilityService->isGitAvailable();
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            [this]() {
                if (m_cliCapabilityService != nullptr) {
                    m_gitAvailable = m_cliCapabilityService->isGitAvailable();
                }
                reloadWorkingTreeStatus();
            });
    }

    connect(
        m_gitOperationWatcher,
        &QFutureWatcher<qtcode::git::GitOperationResult>::finished,
        this,
        &StageChangesDialog::onGitOperationFinished);
}

void StageChangesDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *descriptionLabel = new QLabel(
        i18n("Stage changes, write a commit message, and commit or push."),
        this);
    descriptionLabel->setWordWrap(true);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QFont sectionFont = descriptionLabel->font();
    sectionFont.setBold(true);

    m_unstagedSectionLabel = new QLabel(i18n("Changes"), this);
    m_unstagedSectionLabel->setFont(sectionFont);

    m_stageAllButton = new QPushButton(i18n("Stage all"), this);
    m_stageAllButton->setFlat(true);
    connect(m_stageAllButton, &QPushButton::clicked, this, &StageChangesDialog::onStageAllClicked);

    auto *unstagedHeader = new QHBoxLayout();
    unstagedHeader->setContentsMargins(0, 0, 0, 0);
    unstagedHeader->addWidget(m_unstagedSectionLabel);
    unstagedHeader->addStretch();
    unstagedHeader->addWidget(m_stageAllButton);

    m_unstagedFilesList = new QListWidget(this);
    m_unstagedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_unstagedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_unstagedFilesList, &QListWidget::itemClicked, this, &StageChangesDialog::onUnstagedFileClicked);
    connect(
        m_unstagedFilesList,
        &QListWidget::customContextMenuRequested,
        this,
        &StageChangesDialog::showUnstagedFilesContextMenu);

    m_commitMessageEdit = new QPlainTextEdit(this);
    m_commitMessageEdit->setPlaceholderText(i18n("Commit message"));
    m_commitMessageEdit->setMaximumHeight(96);
    m_commitMessageEdit->setTabChangesFocus(true);
    connect(m_commitMessageEdit, &QPlainTextEdit::textChanged, this, &StageChangesDialog::onCommitMessageChanged);

    m_commitButton = new QPushButton(i18n("Commit"), this);
    m_pushButton = new QPushButton(i18n("Push"), this);
    connect(m_commitButton, &QPushButton::clicked, this, &StageChangesDialog::onCommitClicked);
    connect(m_pushButton, &QPushButton::clicked, this, &StageChangesDialog::onPushClicked);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->addWidget(m_commitButton);
    actionsLayout->addWidget(m_pushButton);
    actionsLayout->addStretch();

    m_stagedSectionLabel = new QLabel(i18n("Staged changes"), this);
    m_stagedSectionLabel->setFont(sectionFont);

    m_unstageAllButton = new QPushButton(i18n("Unstage all"), this);
    m_unstageAllButton->setFlat(true);
    connect(m_unstageAllButton, &QPushButton::clicked, this, &StageChangesDialog::onUnstageAllClicked);

    auto *stagedHeader = new QHBoxLayout();
    stagedHeader->setContentsMargins(0, 0, 0, 0);
    stagedHeader->addWidget(m_stagedSectionLabel);
    stagedHeader->addStretch();
    stagedHeader->addWidget(m_unstageAllButton);

    m_stagedFilesList = new QListWidget(this);
    m_stagedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stagedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_stagedFilesList, &QListWidget::itemClicked, this, &StageChangesDialog::onStagedFileClicked);
    connect(
        m_stagedFilesList,
        &QListWidget::customContextMenuRequested,
        this,
        &StageChangesDialog::showStagedFilesContextMenu);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(descriptionLabel);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_commitMessageEdit);
    layout->addLayout(actionsLayout);
    layout->addLayout(unstagedHeader);
    layout->addWidget(m_unstagedFilesList);
    layout->addLayout(stagedHeader);
    layout->addWidget(m_stagedFilesList, 1);
    layout->addWidget(buttonBox);
}

void StageChangesDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    reloadWorkingTreeStatus();
}

void StageChangesDialog::setWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status)
{
    applyWorkingTreeStatus(status);
}

void StageChangesDialog::applyWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status)
{
    m_commitsAhead = status.commitsAhead;

    const bool hasStagedFiles = !status.stagedFiles.isEmpty();
    const bool hasUnstagedFiles = !status.unstagedFiles.isEmpty();
    const bool hasWorkingTreeChanges = hasStagedFiles || hasUnstagedFiles;

    setWidgetVisibleIfChanged(m_commitMessageEdit, true);
    setWidgetVisibleIfChanged(m_commitButton, true);
    setWidgetVisibleIfChanged(m_pushButton, true);
    setWidgetVisibleIfChanged(m_unstagedSectionLabel, true);
    setWidgetVisibleIfChanged(m_stageAllButton, hasUnstagedFiles);
    setWidgetVisibleIfChanged(m_stagedSectionLabel, true);
    setWidgetVisibleIfChanged(m_unstageAllButton, hasStagedFiles);

    if (!hasWorkingTreeChanges) {
        setLabelTextIfChanged(m_stateLabel, i18n("No local changes in the working tree."));
        setWidgetVisibleIfChanged(m_stateLabel, true);
        m_unstagedFilesList->clear();
        setWidgetVisibleIfChanged(m_unstagedFilesList, false);
        m_stagedFilesList->clear();
        setWidgetVisibleIfChanged(m_stagedFilesList, false);
    } else {
        setWidgetVisibleIfChanged(m_stateLabel, false);

        if (hasUnstagedFiles) {
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
            m_unstagedFilesList->clear();
            setWidgetVisibleIfChanged(m_unstagedFilesList, false);
        }

        if (hasStagedFiles) {
            QList<PathListEntry> stagedEntries;
            stagedEntries.reserve(status.stagedFiles.size());
            for (const qtcode::git::ChangedFile &changedFile : status.stagedFiles) {
                PathListEntry entry;
                entry.text = QStringLiteral("%1  %2").arg(changedFile.statusLabel, changedFile.path);
                entry.path = changedFile.path;
                stagedEntries.append(entry);
            }

            syncPathListWidget(m_stagedFilesList, stagedEntries);
            setWidgetVisibleIfChanged(m_stagedFilesList, true);
        } else {
            m_stagedFilesList->clear();
            setWidgetVisibleIfChanged(m_stagedFilesList, false);
        }
    }

    setLabelTextIfChanged(
        m_unstagedSectionLabel,
        hasUnstagedFiles
            ? i18n("Changes (%1)", status.unstagedFiles.size())
            : i18n("Changes"));

    setLabelTextIfChanged(
        m_stagedSectionLabel,
        hasStagedFiles
            ? i18n("Staged changes (%1)", status.stagedFiles.size())
            : i18n("Staged changes"));

    updateActions(status);
}

void StageChangesDialog::updateActions(const qtcode::git::GitWorkingTreeStatus &status)
{
    const bool hasStagedFiles = !status.stagedFiles.isEmpty();
    const bool hasUnstagedFiles = !status.unstagedFiles.isEmpty();
    const bool hasCommitMessage = !m_commitMessageEdit->toPlainText().trimmed().isEmpty();
    const bool gitReady = m_gitAvailable && m_gitService != nullptr;
    const bool operationRunning = m_gitOperationWatcher != nullptr && m_gitOperationWatcher->isRunning();

    m_stageAllButton->setEnabled(gitReady && hasUnstagedFiles && !operationRunning);
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

void StageChangesDialog::onCommitMessageChanged()
{
    qtcode::git::GitWorkingTreeStatus status;
    status.commitsAhead = m_commitsAhead;
    for (int i = 0; i < m_unstagedFilesList->count(); ++i) {
        qtcode::git::ChangedFile file;
        file.path = m_unstagedFilesList->item(i)->data(Qt::UserRole).toString();
        status.unstagedFiles.append(file);
    }
    for (int i = 0; i < m_stagedFilesList->count(); ++i) {
        qtcode::git::ChangedFile file;
        file.path = m_stagedFilesList->item(i)->data(Qt::UserRole).toString();
        status.stagedFiles.append(file);
    }
    updateActions(status);
}

void StageChangesDialog::onStagedFileClicked(QListWidgetItem *item)
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

void StageChangesDialog::onUnstagedFileClicked(QListWidgetItem *item)
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

QString StageChangesDialog::resolveChangedFilePath(const QString &relativePath) const
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

QString StageChangesDialog::resolveGitExecutable() const
{
    if (m_cliCapabilityService == nullptr) {
        return {};
    }

    return m_cliCapabilityService->snapshot().git.executablePath;
}

bool StageChangesDialog::activeRepositoryPath(QString *repositoryPath) const
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

void StageChangesDialog::startGitOperation(
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

void StageChangesDialog::showGitOperationResult(
    const qtcode::git::GitOperationResult &result,
    const QString &successMessage)
{
    if (!result.success) {
        qCWarning(qtcodeUi) << "Git operation failed:" << result.errorMessage;
        if (m_statusService != nullptr) {
            m_statusService->showMessage(result.errorMessage, qtcode::core::StatusSeverity::Error);
        }
        reloadWorkingTreeStatus();
        emit changesUpdated();
        return;
    }

    if (m_statusService != nullptr) {
        m_statusService->showMessage(successMessage);
    }

    reloadWorkingTreeStatus();
    emit changesUpdated();
}

void StageChangesDialog::reloadWorkingTreeStatus()
{
    QString repositoryPath;
    if (!activeRepositoryPath(&repositoryPath) || m_gitService == nullptr) {
        return;
    }

    const qtcode::git::RepositoryGitSnapshot snapshot =
        qtcode::git::loadRepositoryGitSnapshot(repositoryPath, 0);
    if (snapshot.success) {
        applyWorkingTreeStatus(snapshot.status);
    }
}

void StageChangesDialog::onGitOperationFinished()
{
    const qtcode::git::GitOperationResult result = m_gitOperationWatcher->result();
    if (result.success && m_clearCommitMessageOnSuccess) {
        const QSignalBlocker blocker(m_commitMessageEdit);
        m_commitMessageEdit->clear();
        m_clearCommitMessageOnSuccess = false;
    }

    showGitOperationResult(result, m_pendingGitSuccessMessage);
}

void StageChangesDialog::onStageAllClicked()
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

void StageChangesDialog::onUnstageAllClicked()
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

void StageChangesDialog::stageRelativePaths(const QStringList &relativePaths)
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

void StageChangesDialog::unstageRelativePaths(const QStringList &relativePaths)
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

QStringList StageChangesDialog::selectedRelativePaths(QListWidget *list) const
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

void StageChangesDialog::showUnstagedFilesContextMenu(const QPoint &position)
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

void StageChangesDialog::showStagedFilesContextMenu(const QPoint &position)
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

void StageChangesDialog::onCommitClicked()
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

void StageChangesDialog::onPushClicked()
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

} // namespace qtcode::ui

#pragma once

#include "git/GitOperationResult.h"
#include "git/GitStatus.h"

#include <QDialog>

#include <QShowEvent>

#include <functional>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;

namespace qtcode::core {
class CliCapabilityService;
class ProjectManager;
class StatusService;
} // namespace qtcode::core

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

template <typename T>
class QFutureWatcher;

namespace qtcode::ui {

class StageChangesDialog final : public QDialog
{
    Q_OBJECT

public:
    StageChangesDialog(
        qtcode::git::GitService *gitService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

    void setWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status);

signals:
    void changesUpdated();
    void fileOpenRequested(const QString &absolutePath);

private slots:
    void onCommitMessageChanged();
    void onCommitClicked();
    void onPushClicked();
    void onStageAllClicked();
    void onUnstageAllClicked();
    void onGitOperationFinished();
    void onStagedFileClicked(QListWidgetItem *item);
    void onUnstagedFileClicked(QListWidgetItem *item);

private:
    void configureLayout();
    void applyWorkingTreeStatus(const qtcode::git::GitWorkingTreeStatus &status);
    void updateActions(const qtcode::git::GitWorkingTreeStatus &status);
    void showStagedFilesContextMenu(const QPoint &position);
    void showUnstagedFilesContextMenu(const QPoint &position);
    void stageRelativePaths(const QStringList &relativePaths);
    void unstageRelativePaths(const QStringList &relativePaths);
    [[nodiscard]] QStringList selectedRelativePaths(QListWidget *list) const;
    [[nodiscard]] QString resolveChangedFilePath(const QString &relativePath) const;
    [[nodiscard]] QString resolveGitExecutable() const;
    [[nodiscard]] bool activeRepositoryPath(QString *repositoryPath) const;
    void startGitOperation(
        const std::function<qtcode::git::GitOperationResult()> &operation,
        const QString &successMessage);
    void showGitOperationResult(
        const qtcode::git::GitOperationResult &result,
        const QString &successMessage);
    void reloadWorkingTreeStatus();

protected:
    void showEvent(QShowEvent *event) override;

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    QLabel *m_stateLabel = nullptr;
    QPlainTextEdit *m_commitMessageEdit = nullptr;
    QPushButton *m_commitButton = nullptr;
    QPushButton *m_pushButton = nullptr;
    QLabel *m_unstagedSectionLabel = nullptr;
    QPushButton *m_stageAllButton = nullptr;
    QListWidget *m_unstagedFilesList = nullptr;
    QLabel *m_stagedSectionLabel = nullptr;
    QPushButton *m_unstageAllButton = nullptr;
    QListWidget *m_stagedFilesList = nullptr;
    QFutureWatcher<qtcode::git::GitOperationResult> *m_gitOperationWatcher = nullptr;
    int m_commitsAhead = 0;
    bool m_gitAvailable = false;
    QString m_pendingGitSuccessMessage;
    bool m_clearCommitMessageOnSuccess = false;
};

} // namespace qtcode::ui

#pragma once

#include "git/GitOperationResult.h"

#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace qtcode::core {
class CliCapabilityService;
class ProjectManager;
class StatusService;
} // namespace qtcode::core

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::github {
class GitHubService;
} // namespace qtcode::github

template <typename T>
class QFutureWatcher;

namespace qtcode::ui {

class CreateIssueBranchDialog final : public QDialog
{
    Q_OBJECT

public:
    CreateIssueBranchDialog(
        qtcode::git::GitService *gitService,
        qtcode::github::GitHubService *gitHubService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::core::StatusService *statusService,
        const QString &projectId,
        int issueNumber,
        const QString &issueTitle,
        QWidget *parent = nullptr);

signals:
    void branchChanged();

private slots:
    void onCreateClicked();
    void onChangeBranchClicked();
    void onCreateOperationFinished();
    void onCheckoutOperationFinished();

private:
    void configureLayout();
    void loadBranchOptions();
    [[nodiscard]] QString resolveGitExecutable() const;
    [[nodiscard]] bool activeRepositoryPath(QString *repositoryPath) const;
    [[nodiscard]] int defaultBaseBranchIndex(const QStringList &branches) const;
    void setBusy(bool busy);
    void showCreatedState(const QString &branchName);
    void startCheckout();

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::github::GitHubService *m_gitHubService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    QString m_projectId;
    int m_issueNumber = 0;
    QString m_issueTitle;
    QString m_createdBranchName;
    QLabel *m_issueLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLineEdit *m_branchNameEdit = nullptr;
    QComboBox *m_baseBranchCombo = nullptr;
    QPushButton *m_changeBranchButton = nullptr;
    QPushButton *m_createBranchButton = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QFutureWatcher<qtcode::git::GitOperationResult> *m_createWatcher = nullptr;
    QFutureWatcher<qtcode::git::GitOperationResult> *m_checkoutWatcher = nullptr;
    bool m_gitAvailable = false;
    bool m_ghAvailable = false;
};

} // namespace qtcode::ui

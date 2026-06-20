#pragma once

#include <QWidget>

#include "git/GitStatus.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace qtcode::core {
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::ui {

class RepositoryPanel final : public QWidget
{
    Q_OBJECT

public:
    RepositoryPanel(
        qtcode::git::GitService *gitService,
        qtcode::core::ProjectManager *projectManager,
        QWidget *parent = nullptr);

public slots:
    void refreshStatus();

private:
    void configureLayout();
    void showEmptyState(const QString &message);
    void showErrorState(const QString &message);
    void showChangedFiles(const qtcode::git::GitWorkingTreeStatus &status);

    qtcode::git::GitService *m_gitService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QLabel *m_projectLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QListWidget *m_changedFilesList = nullptr;
    QPushButton *m_refreshButton = nullptr;
};

} // namespace qtcode::ui

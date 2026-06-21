#pragma once

#include <QWidget>

class QTabWidget;

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::core {
class ProjectManager;
class CliCapabilityService;
class StatusService;
class WorkspaceInstaller;
} // namespace qtcode::core

namespace qtcode::github {
class GitHubService;
} // namespace qtcode::github

namespace qtcode::ui {

class FileTreePanel;
class RepositoryPanel;

class ProjectNavigationPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectNavigationPanel(
        qtcode::git::GitService *gitService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::github::GitHubService *gitHubService,
        qtcode::core::StatusService *statusService,
        qtcode::core::WorkspaceInstaller *workspaceInstaller,
        QWidget *parent = nullptr);

    [[nodiscard]] RepositoryPanel *repositoryPanel() const;
    [[nodiscard]] FileTreePanel *fileTreePanel() const;

public slots:
    void showRepositoryView();
    void showFilesView();

private:
    void configureLayout();

    QTabWidget *m_viewTabs = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    FileTreePanel *m_fileTreePanel = nullptr;
};

} // namespace qtcode::ui

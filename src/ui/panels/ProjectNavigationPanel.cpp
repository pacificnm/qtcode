#include "ui/panels/ProjectNavigationPanel.h"

#include "ui/panels/FileTreePanel.h"
#include "ui/panels/RepositoryPanel.h"

#include <KLocalizedString>

#include <QTabWidget>
#include <QVBoxLayout>

namespace qtcode::ui {

ProjectNavigationPanel::ProjectNavigationPanel(
    qtcode::git::GitService *gitService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::github::GitHubService *gitHubService,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QWidget(parent)
{
    m_repositoryPanel = new RepositoryPanel(
        gitService,
        projectManager,
        cliCapabilityService,
        gitHubService,
        statusService,
        this);
    m_fileTreePanel = new FileTreePanel(projectManager, statusService, this);

    configureLayout();
}

RepositoryPanel *ProjectNavigationPanel::repositoryPanel() const
{
    return m_repositoryPanel;
}

FileTreePanel *ProjectNavigationPanel::fileTreePanel() const
{
    return m_fileTreePanel;
}

void ProjectNavigationPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_viewTabs = new QTabWidget(this);
    m_viewTabs->setDocumentMode(true);
    m_viewTabs->addTab(m_repositoryPanel, i18n("Repository"));
    m_viewTabs->addTab(m_fileTreePanel, i18n("Files"));

    layout->addWidget(m_viewTabs, 1);
}

void ProjectNavigationPanel::showRepositoryView()
{
    if (m_viewTabs != nullptr) {
        m_viewTabs->setCurrentWidget(m_repositoryPanel);
    }
}

void ProjectNavigationPanel::showFilesView()
{
    if (m_viewTabs != nullptr) {
        m_viewTabs->setCurrentWidget(m_fileTreePanel);
    }
}

} // namespace qtcode::ui

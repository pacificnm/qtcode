#include "ui/MainWindow.h"

#include "core/ApplicationController.h"
#include "agents/AgentManager.h"
#include "core/SettingsService.h"
#include "settings/SettingsModels.h"
#include "shared/Logging.h"
#include "ui/panels/AgentPanel.h"
#include "ui/panels/McpServerPanel.h"
#include "ui/panels/RepositoryPanel.h"
#include "ui/panels/TerminalPanel.h"

#include <KActionCollection>
#include <KHelpMenu>
#include <KLocalizedString>
#include <KStandardAction>

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenuBar>
#include <QSplitter>
#include <QToolBar>

namespace qtcode::ui {

MainWindow::MainWindow(qtcode::core::ApplicationController *controller, QWidget *parent)
    : QMainWindow(parent)
    , m_controller(controller)
    , m_settingsService(controller != nullptr ? controller->settingsService() : nullptr)
{
    setWindowTitle(i18n("QTCode"));

    qtcode::settings::PanelLayoutSettings layout =
        m_settingsService != nullptr ? m_settingsService->defaultPanelLayout()
                                     : qtcode::settings::PanelLayoutSettings::defaults();

    if (m_settingsService != nullptr) {
        QString loadError;
        if (!m_settingsService->loadPanelLayout(&layout, &loadError)) {
            qCWarning(qtcodeUi) << "Failed to load panel layout settings:" << loadError;
            layout = m_settingsService->defaultPanelLayout();
        }
    }

    configureLayout();
    configureActions();
    configureMenus();
    configureToolBar();
    applyPanelLayout(layout);
}

MainWindow::~MainWindow()
{
    persistPanelLayout();
}

void MainWindow::configureLayout()
{
    m_repositoryPanel = new RepositoryPanel(
        m_controller != nullptr ? m_controller->gitService() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        m_controller != nullptr ? m_controller->cliCapabilityService() : nullptr,
        m_controller != nullptr ? m_controller->gitHubService() : nullptr,
        this);
    m_agentPanel = new AgentPanel(
        m_controller != nullptr ? m_controller->cliCapabilityService() : nullptr,
        m_controller != nullptr ? m_controller->agentManager() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        m_controller != nullptr ? m_controller->contextManager() : nullptr,
        this);
    m_mcpServerPanel = new McpServerPanel(
        m_controller != nullptr ? m_controller->mcpServerService() : nullptr,
        m_controller != nullptr ? m_controller->memoryService() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        this);
    m_terminalPanel = new TerminalPanel(
        m_controller != nullptr ? m_controller->terminalManager() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        this);

    m_repositoryPanel->setMinimumWidth(240);
    m_agentPanel->setMinimumWidth(320);
    m_mcpServerPanel->setMinimumWidth(280);
    m_terminalPanel->setMinimumHeight(120);

    m_horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_horizontalSplitter->addWidget(m_agentPanel);
    m_horizontalSplitter->addWidget(m_mcpServerPanel);
    m_horizontalSplitter->setStretchFactor(0, 2);
    m_horizontalSplitter->setStretchFactor(1, 1);
    m_horizontalSplitter->setCollapsible(0, false);
    m_horizontalSplitter->setCollapsible(1, false);

    m_rightVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_rightVerticalSplitter->addWidget(m_horizontalSplitter);
    m_rightVerticalSplitter->addWidget(m_terminalPanel);
    m_rightVerticalSplitter->setStretchFactor(0, 3);
    m_rightVerticalSplitter->setStretchFactor(1, 1);
    m_rightVerticalSplitter->setCollapsible(0, false);
    m_rightVerticalSplitter->setCollapsible(1, false);

    m_rootHorizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootHorizontalSplitter->addWidget(m_repositoryPanel);
    m_rootHorizontalSplitter->addWidget(m_rightVerticalSplitter);
    m_rootHorizontalSplitter->setStretchFactor(0, 1);
    m_rootHorizontalSplitter->setStretchFactor(1, 3);
    m_rootHorizontalSplitter->setCollapsible(0, false);
    m_rootHorizontalSplitter->setCollapsible(1, false);

    if (m_controller != nullptr && m_controller->agentManager() != nullptr) {
        connect(
            m_controller->agentManager(),
            &qtcode::agents::AgentManager::repositoryRefreshRequested,
            m_repositoryPanel,
            &RepositoryPanel::refreshStatus);
    }

    if (m_repositoryPanel != nullptr && m_agentPanel != nullptr) {
        connect(
            m_repositoryPanel,
            &RepositoryPanel::issueContextSelected,
            m_agentPanel,
            &AgentPanel::attachIssueContext);
        connect(
            m_repositoryPanel,
            &RepositoryPanel::pullRequestContextSelected,
            m_agentPanel,
            &AgentPanel::attachPullRequestContext);
    }

    setCentralWidget(m_rootHorizontalSplitter);

    qCInfo(qtcodeUi) << "Initialized repository, agent, MCP, and terminal panel layout";
}

void MainWindow::configureActions()
{
    m_actionCollection = new KActionCollection(this);
    m_actionCollection->addAssociatedWidget(this);

    KStandardAction::quit(qApp, &QApplication::quit, m_actionCollection);

    auto *addRepositoryAction = m_actionCollection->addAction(QStringLiteral("file_add_repository"));
    addRepositoryAction->setText(i18n("Add Repository"));
    addRepositoryAction->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    connect(
        addRepositoryAction,
        &QAction::triggered,
        m_repositoryPanel,
        &RepositoryPanel::addRepository);

    auto *refreshStatusAction = m_actionCollection->addAction(QStringLiteral("file_refresh_status"));
    refreshStatusAction->setText(i18n("Refresh Status"));
    refreshStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(
        refreshStatusAction,
        &QAction::triggered,
        m_repositoryPanel,
        &RepositoryPanel::refreshStatus);

    auto *newTerminalTabAction = m_actionCollection->addAction(QStringLiteral("view_new_terminal_tab"));
    newTerminalTabAction->setText(i18n("New Terminal Tab"));
    newTerminalTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    connect(
        newTerminalTabAction,
        &QAction::triggered,
        m_terminalPanel,
        &TerminalPanel::addTerminalTab);

    auto *resetPanelLayoutAction =
        m_actionCollection->addAction(QStringLiteral("view_reset_panel_layout"));
    resetPanelLayoutAction->setText(i18n("Reset Panel Layout"));
    connect(resetPanelLayoutAction, &QAction::triggered, this, &MainWindow::resetPanelLayout);

    m_toggleToolBarAction = m_actionCollection->addAction(QStringLiteral("view_toggle_toolbar"));
    m_toggleToolBarAction->setText(i18n("Main Toolbar"));
    m_toggleToolBarAction->setCheckable(true);
    m_toggleToolBarAction->setChecked(true);
}

void MainWindow::configureMenus()
{
    auto *fileMenu = menuBar()->addMenu(i18n("&File"));
    fileMenu->addAction(m_actionCollection->action(QStringLiteral("file_add_repository")));
    fileMenu->addAction(m_actionCollection->action(QStringLiteral("file_refresh_status")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionCollection->action(KStandardAction::name(KStandardAction::Quit)));

    auto *viewMenu = menuBar()->addMenu(i18n("&View"));
    viewMenu->addAction(m_actionCollection->action(QStringLiteral("view_reset_panel_layout")));
    viewMenu->addAction(m_toggleToolBarAction);

    m_helpMenu = new KHelpMenu(this);
    menuBar()->addMenu(m_helpMenu->menu());
}

void MainWindow::configureToolBar()
{
    m_mainToolBar = addToolBar(i18n("Main Toolbar"));
    m_mainToolBar->setObjectName(QStringLiteral("MainToolBar"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->addAction(m_actionCollection->action(QStringLiteral("file_add_repository")));
    m_mainToolBar->addAction(m_actionCollection->action(QStringLiteral("file_refresh_status")));
    m_mainToolBar->addAction(m_actionCollection->action(QStringLiteral("view_new_terminal_tab")));

    connect(m_toggleToolBarAction, &QAction::toggled, m_mainToolBar, &QWidget::setVisible);
}

void MainWindow::resetPanelLayout()
{
    const qtcode::settings::PanelLayoutSettings layout =
        m_settingsService != nullptr ? m_settingsService->defaultPanelLayout()
                                     : qtcode::settings::PanelLayoutSettings::defaults();
    applyPanelLayout(layout);
}

void MainWindow::applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout)
{
    resize(layout.windowWidth, layout.windowHeight);

    if (m_rootHorizontalSplitter != nullptr && layout.columnSizes.size() >= 2) {
        m_rootHorizontalSplitter->setSizes(layout.columnSizes);
    }

    if (m_horizontalSplitter != nullptr && layout.horizontalSizes.size() >= 2) {
        m_horizontalSplitter->setSizes(layout.horizontalSizes);
    }

    if (m_rightVerticalSplitter != nullptr && layout.verticalSizes.size() >= 2) {
        m_rightVerticalSplitter->setSizes(layout.verticalSizes);
    }
}

qtcode::settings::PanelLayoutSettings MainWindow::currentPanelLayout() const
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.windowWidth = width();
    layout.windowHeight = height();

    if (m_rootHorizontalSplitter != nullptr) {
        layout.columnSizes = m_rootHorizontalSplitter->sizes();
    }

    if (m_horizontalSplitter != nullptr) {
        layout.horizontalSizes = m_horizontalSplitter->sizes();
    }

    if (m_rightVerticalSplitter != nullptr) {
        layout.verticalSizes = m_rightVerticalSplitter->sizes();
    }

    return layout;
}

void MainWindow::persistPanelLayout()
{
    if (m_settingsService == nullptr) {
        return;
    }

    QString saveError;
    if (!m_settingsService->savePanelLayout(currentPanelLayout(), &saveError)) {
        qCWarning(qtcodeUi) << "Failed to save panel layout settings:" << saveError;
    }
}

} // namespace qtcode::ui

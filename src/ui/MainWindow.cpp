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
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
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
    configureActivityBar();
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
    m_agentPanel->conversationPanel()->setMinimumWidth(320);
    m_mcpServerPanel->setMinimumWidth(280);
    m_terminalPanel->setMinimumHeight(120);

    m_secondaryPanelStack = new QStackedWidget(this);
    m_secondaryPanelStack->setMinimumWidth(240);
    m_secondaryPanelStack->addWidget(m_agentPanel->contextPanel());
    m_secondaryPanelStack->addWidget(m_agentPanel->changesPanel());

    m_middleVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_middleVerticalSplitter->addWidget(m_agentPanel->conversationPanel());
    m_middleVerticalSplitter->addWidget(m_terminalPanel);
    m_middleVerticalSplitter->setStretchFactor(0, 3);
    m_middleVerticalSplitter->setStretchFactor(1, 1);
    m_middleVerticalSplitter->setCollapsible(0, false);
    m_middleVerticalSplitter->setCollapsible(1, false);

    m_rootHorizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootHorizontalSplitter->addWidget(m_repositoryPanel);
    m_rootHorizontalSplitter->addWidget(m_middleVerticalSplitter);
    m_rootHorizontalSplitter->addWidget(m_secondaryPanelStack);
    m_rootHorizontalSplitter->addWidget(m_mcpServerPanel);
    m_rootHorizontalSplitter->setStretchFactor(0, 1);
    m_rootHorizontalSplitter->setStretchFactor(1, 2);
    m_rootHorizontalSplitter->setStretchFactor(2, 1);
    m_rootHorizontalSplitter->setStretchFactor(3, 1);
    m_rootHorizontalSplitter->setCollapsible(0, false);
    m_rootHorizontalSplitter->setCollapsible(1, false);
    m_rootHorizontalSplitter->setCollapsible(2, false);
    m_rootHorizontalSplitter->setCollapsible(3, false);

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

    qCInfo(qtcodeUi) << "Initialized repository, agent, MCP, terminal, and secondary panel layout";
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

    m_agentPanelAction = m_actionCollection->addAction(QStringLiteral("view_agent_panel"));
    m_agentPanelAction->setText(i18n("AI Agent Panel"));
    m_agentPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    m_agentPanelAction->setCheckable(true);
    m_agentPanelAction->setChecked(true);
    connect(m_agentPanelAction, &QAction::toggled, this, &MainWindow::onAgentPanelActionToggled);

    m_contextPanelAction = m_actionCollection->addAction(QStringLiteral("view_context_panel"));
    m_contextPanelAction->setText(i18n("Retrieved Context Panel"));
    m_contextPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks-organize")));
    m_contextPanelAction->setCheckable(true);
    connect(m_contextPanelAction, &QAction::toggled, this, &MainWindow::onContextPanelActionToggled);

    m_changesPanelAction = m_actionCollection->addAction(QStringLiteral("view_changes_panel"));
    m_changesPanelAction->setText(i18n("Generated Changes Panel"));
    m_changesPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    m_changesPanelAction->setCheckable(true);
    connect(m_changesPanelAction, &QAction::toggled, this, &MainWindow::onChangesPanelActionToggled);

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
    viewMenu->addAction(m_agentPanelAction);
    viewMenu->addAction(m_contextPanelAction);
    viewMenu->addAction(m_changesPanelAction);
    viewMenu->addSeparator();
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

void MainWindow::configureActivityBar()
{
    m_activityToolBar = new QToolBar(i18n("Activity Bar"), this);
    m_activityToolBar->setObjectName(QStringLiteral("ActivityToolBar"));
    m_activityToolBar->setMovable(false);
    m_activityToolBar->setOrientation(Qt::Vertical);
    m_activityToolBar->setIconSize(QSize(22, 22));
    m_activityToolBar->addAction(m_agentPanelAction);
    m_activityToolBar->addAction(m_contextPanelAction);
    m_activityToolBar->addAction(m_changesPanelAction);
    addToolBar(Qt::RightToolBarArea, m_activityToolBar);
}

void MainWindow::onAgentPanelActionToggled(bool visible)
{
    Q_UNUSED(visible);
    syncAgentPanelVisibility();
}

void MainWindow::onContextPanelActionToggled(bool visible)
{
    if (visible && m_changesPanelAction != nullptr && m_changesPanelAction->isChecked()) {
        const QSignalBlocker blocker(m_changesPanelAction);
        m_changesPanelAction->setChecked(false);
    }

    if (visible && m_secondaryPanelStack != nullptr && m_agentPanel != nullptr) {
        m_secondaryPanelStack->setCurrentWidget(m_agentPanel->contextPanel());
    }

    syncSecondaryPanelVisibility();
}

void MainWindow::onChangesPanelActionToggled(bool visible)
{
    if (visible && m_contextPanelAction != nullptr && m_contextPanelAction->isChecked()) {
        const QSignalBlocker blocker(m_contextPanelAction);
        m_contextPanelAction->setChecked(false);
    }

    if (visible && m_secondaryPanelStack != nullptr && m_agentPanel != nullptr) {
        m_secondaryPanelStack->setCurrentWidget(m_agentPanel->changesPanel());
    }

    syncSecondaryPanelVisibility();
}

void MainWindow::syncAgentPanelVisibility()
{
    if (m_middleVerticalSplitter == nullptr || m_agentPanelAction == nullptr
        || m_agentPanel == nullptr) {
        return;
    }

    QList<int> sizes = m_middleVerticalSplitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    if (m_agentPanelAction->isChecked()) {
        if (sizes.at(0) < 120) {
            sizes[0] = m_storedAgentPanelHeight > 120 ? m_storedAgentPanelHeight : 560;
        }
        m_agentPanel->conversationPanel()->setVisible(true);
    } else {
        m_storedAgentPanelHeight = sizes.at(0);
        sizes[0] = 0;
        m_agentPanel->conversationPanel()->setVisible(false);
    }

    m_middleVerticalSplitter->setSizes(sizes);
}

void MainWindow::syncSecondaryPanelVisibility()
{
    if (m_rootHorizontalSplitter == nullptr || m_secondaryPanelStack == nullptr
        || m_contextPanelAction == nullptr || m_changesPanelAction == nullptr) {
        return;
    }

    QList<int> sizes = m_rootHorizontalSplitter->sizes();
    if (sizes.size() < 4) {
        return;
    }

    const bool showSecondary =
        m_contextPanelAction->isChecked() || m_changesPanelAction->isChecked();
    if (showSecondary) {
        if (sizes.at(2) < 120) {
            sizes[2] = m_storedSecondaryColumnWidth > 120 ? m_storedSecondaryColumnWidth : 320;
        }
        m_secondaryPanelStack->setVisible(true);
    } else {
        if (sizes.at(2) > 0) {
            m_storedSecondaryColumnWidth = sizes.at(2);
        }
        sizes[2] = 0;
        m_secondaryPanelStack->setVisible(false);
    }

    m_rootHorizontalSplitter->setSizes(sizes);
}

void MainWindow::resetPanelLayout()
{
    const qtcode::settings::PanelLayoutSettings layout =
        m_settingsService != nullptr ? m_settingsService->defaultPanelLayout()
                                     : qtcode::settings::PanelLayoutSettings::defaults();
    applyPanelLayout(layout);
}

void MainWindow::applyPanelVisibility(const qtcode::settings::PanelLayoutSettings &layout)
{
    if (m_agentPanelAction != nullptr) {
        const QSignalBlocker blocker(m_agentPanelAction);
        m_agentPanelAction->setChecked(layout.agentPanelVisible);
    }

    if (m_contextPanelAction != nullptr) {
        const QSignalBlocker blocker(m_contextPanelAction);
        m_contextPanelAction->setChecked(layout.contextPanelVisible);
    }

    if (m_changesPanelAction != nullptr) {
        const QSignalBlocker blocker(m_changesPanelAction);
        m_changesPanelAction->setChecked(layout.changesPanelVisible);
    }

    if (m_secondaryPanelStack != nullptr && m_agentPanel != nullptr) {
        if (layout.changesPanelVisible) {
            m_secondaryPanelStack->setCurrentWidget(m_agentPanel->changesPanel());
        } else if (layout.contextPanelVisible) {
            m_secondaryPanelStack->setCurrentWidget(m_agentPanel->contextPanel());
        }
    }

    syncAgentPanelVisibility();
    syncSecondaryPanelVisibility();
}

void MainWindow::applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout)
{
    resize(layout.windowWidth, layout.windowHeight);

    if (m_rootHorizontalSplitter != nullptr && layout.columnSizes.size() >= 4) {
        m_rootHorizontalSplitter->setSizes(layout.columnSizes);
        if (layout.columnSizes.at(2) > 120) {
            m_storedSecondaryColumnWidth = layout.columnSizes.at(2);
        }
    } else if (m_rootHorizontalSplitter != nullptr && layout.columnSizes.size() >= 3) {
        QList<int> sizes = layout.columnSizes;
        sizes.insert(2, 0);
        m_rootHorizontalSplitter->setSizes(sizes);
    }

    if (m_middleVerticalSplitter != nullptr && layout.verticalSizes.size() >= 2) {
        m_middleVerticalSplitter->setSizes(layout.verticalSizes);
        if (layout.verticalSizes.at(0) > 120) {
            m_storedAgentPanelHeight = layout.verticalSizes.at(0);
        }
    }

    applyPanelVisibility(layout);
}

qtcode::settings::PanelLayoutSettings MainWindow::currentPanelLayout() const
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.windowWidth = width();
    layout.windowHeight = height();

    if (m_rootHorizontalSplitter != nullptr) {
        layout.columnSizes = m_rootHorizontalSplitter->sizes();
    }

    if (m_middleVerticalSplitter != nullptr) {
        layout.verticalSizes = m_middleVerticalSplitter->sizes();
    }

    if (m_agentPanelAction != nullptr) {
        layout.agentPanelVisible = m_agentPanelAction->isChecked();
    }

    if (m_contextPanelAction != nullptr) {
        layout.contextPanelVisible = m_contextPanelAction->isChecked();
    }

    if (m_changesPanelAction != nullptr) {
        layout.changesPanelVisible = m_changesPanelAction->isChecked();
    }

    if (layout.columnSizes.size() >= 4
        && !layout.contextPanelVisible
        && !layout.changesPanelVisible) {
        layout.columnSizes[2] = 0;
    }

    if (layout.verticalSizes.size() >= 2 && !layout.agentPanelVisible) {
        layout.verticalSizes[0] = 0;
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

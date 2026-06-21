#include "ui/MainWindow.h"

#include "core/ApplicationController.h"
#include "core/StatusService.h"
#include "agents/AgentManager.h"
#include "core/SettingsService.h"
#include "settings/SettingsModels.h"
#include "shared/Logging.h"
#include "ui/panels/AgentPanel.h"
#include "ui/panels/McpServerPanel.h"
#include "ui/panels/FileTreePanel.h"
#include "ui/panels/ProjectNavigationPanel.h"
#include "ui/panels/RepositoryPanel.h"
#include "ui/panels/TerminalPanel.h"
#include "ui/panels/WorkspaceTabs.h"
#include "ui/StatusBar.h"

#include <KActionCollection>
#include <KHelpMenu>
#include <KLocalizedString>
#include <KStandardAction>
#include <KTextEditor/Application>
#include <KTextEditor/Editor>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>

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
    configureActivityBar();
    applyPanelLayout(layout);
}

MainWindow::~MainWindow()
{
    persistPanelLayout();
}

void MainWindow::configureLayout()
{
    m_projectNavigationPanel = new ProjectNavigationPanel(
        m_controller != nullptr ? m_controller->gitService() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        m_controller != nullptr ? m_controller->cliCapabilityService() : nullptr,
        m_controller != nullptr ? m_controller->gitHubService() : nullptr,
        m_controller != nullptr ? m_controller->statusService() : nullptr,
        this);
    m_agentPanel = new AgentPanel(
        m_controller != nullptr ? m_controller->cliCapabilityService() : nullptr,
        m_controller != nullptr ? m_controller->agentManager() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        m_controller != nullptr ? m_controller->contextManager() : nullptr,
        m_controller != nullptr ? m_controller->statusService() : nullptr,
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

    m_workspaceTabs = new WorkspaceTabs(
        m_controller != nullptr ? m_controller->statusService() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        this);
    m_workspaceTabs->setPermanentAiChatTab(m_agentPanel->conversationPanel());

    if (m_controller != nullptr) {
        m_ktextEditorApplication = std::make_unique<KTextEditor::Application>(this);
        KTextEditor::Editor::instance()->setApplication(m_ktextEditorApplication.get());
    }

    m_projectNavigationPanel->setMinimumWidth(240);
    m_workspaceTabs->setMinimumWidth(320);
    m_terminalPanel->setMinimumHeight(120);

    m_rightPanelStack = new QStackedWidget(this);
    m_rightPanelStack->setMinimumWidth(240);
    m_rightPanelStack->addWidget(m_agentPanel->sessionPanel());
    m_rightPanelStack->addWidget(m_agentPanel->contextPanel());
    m_rightPanelStack->addWidget(m_agentPanel->changesPanel());
    m_rightPanelStack->addWidget(m_mcpServerPanel);

    m_mainVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_mainVerticalSplitter->addWidget(m_workspaceTabs);
    m_mainVerticalSplitter->addWidget(m_terminalPanel);
    m_mainVerticalSplitter->setStretchFactor(0, 3);
    m_mainVerticalSplitter->setStretchFactor(1, 1);
    m_mainVerticalSplitter->setCollapsible(0, false);
    m_mainVerticalSplitter->setCollapsible(1, false);

    m_rootHorizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootHorizontalSplitter->addWidget(m_projectNavigationPanel);
    m_rootHorizontalSplitter->addWidget(m_mainVerticalSplitter);
    m_rootHorizontalSplitter->addWidget(m_rightPanelStack);
    m_rootHorizontalSplitter->setStretchFactor(0, 1);
    m_rootHorizontalSplitter->setStretchFactor(1, 2);
    m_rootHorizontalSplitter->setStretchFactor(2, 1);
    m_rootHorizontalSplitter->setCollapsible(0, false);
    m_rootHorizontalSplitter->setCollapsible(1, false);
    m_rootHorizontalSplitter->setCollapsible(2, false);

    if (m_controller != nullptr && m_controller->agentManager() != nullptr) {
        connect(
            m_controller->agentManager(),
            &qtcode::agents::AgentManager::repositoryRefreshRequested,
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::refreshStatus);
    }

    if (m_projectNavigationPanel != nullptr && m_agentPanel != nullptr) {
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::issueContextSelected,
            m_agentPanel,
            &AgentPanel::attachIssueContext);
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::pullRequestContextSelected,
            m_agentPanel,
            &AgentPanel::attachPullRequestContext);
    }

    if (m_projectNavigationPanel != nullptr && m_workspaceTabs != nullptr) {
        connect(
            m_projectNavigationPanel->fileTreePanel(),
            &FileTreePanel::fileOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenFile);
        connect(
            m_workspaceTabs,
            &WorkspaceTabs::activeEditorStateChanged,
            this,
            &MainWindow::refreshEditorActions);
    }

    auto *centralContainer = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_rootHorizontalSplitter, 1);

    m_statusBar = new StatusBar(
        m_controller != nullptr ? m_controller->statusService() : nullptr,
        centralContainer);
    centralLayout->addWidget(m_statusBar, 0);

    setCentralWidget(centralContainer);

    if (m_controller != nullptr && m_controller->statusService() != nullptr) {
        m_controller->statusService()->showMessage(i18n("Ready."));
    }

    qCInfo(qtcodeUi) << "Initialized three-column shell with workspace tabs and global status bar";
}

void MainWindow::configureActions()
{
    m_actionCollection = new KActionCollection(this);
    m_actionCollection->addAssociatedWidget(this);

    KStandardAction::quit(this, &QWidget::close, m_actionCollection);

    m_saveFileAction = KStandardAction::save(this, &MainWindow::saveCurrentFile, m_actionCollection);
    m_saveFileAction->setEnabled(false);

    m_closeFileTabAction = m_actionCollection->addAction(QStringLiteral("file_close_editor_tab"));
    m_closeFileTabAction->setText(i18n("Close File Tab"));
    m_closeFileTabAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    m_closeFileTabAction->setEnabled(false);
    connect(
        m_closeFileTabAction,
        &QAction::triggered,
        m_workspaceTabs,
        &WorkspaceTabs::closeCurrentEditorTab);

    auto *addRepositoryAction = m_actionCollection->addAction(QStringLiteral("file_add_repository"));
    addRepositoryAction->setText(i18n("Add Repository"));
    addRepositoryAction->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    connect(
        addRepositoryAction,
        &QAction::triggered,
        m_projectNavigationPanel->repositoryPanel(),
        &RepositoryPanel::addRepository);

    auto *refreshStatusAction = m_actionCollection->addAction(QStringLiteral("file_refresh_status"));
    refreshStatusAction->setText(i18n("Refresh Status"));
    refreshStatusAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(
        refreshStatusAction,
        &QAction::triggered,
        m_projectNavigationPanel->repositoryPanel(),
        &RepositoryPanel::refreshStatus);

    auto *newTerminalTabAction = m_actionCollection->addAction(QStringLiteral("file_new_terminal_tab"));
    newTerminalTabAction->setText(i18n("New Terminal Tab"));
    newTerminalTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    connect(
        newTerminalTabAction,
        &QAction::triggered,
        m_terminalPanel,
        &TerminalPanel::addTerminalTab);

    m_agentSessionsPanelAction = m_actionCollection->addAction(QStringLiteral("view_agent_sessions_panel"));
    m_agentSessionsPanelAction->setText(i18n("Agent Sessions"));
    m_agentSessionsPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("system-users")));
    m_agentSessionsPanelAction->setCheckable(true);
    connect(
        m_agentSessionsPanelAction,
        &QAction::toggled,
        this,
        &MainWindow::onAgentSessionsPanelActionToggled);

    m_contextPanelAction = m_actionCollection->addAction(QStringLiteral("view_context_panel"));
    m_contextPanelAction->setText(i18n("Retrieved Context"));
    m_contextPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks-organize")));
    m_contextPanelAction->setCheckable(true);
    connect(m_contextPanelAction, &QAction::toggled, this, &MainWindow::onContextPanelActionToggled);

    m_changesPanelAction = m_actionCollection->addAction(QStringLiteral("view_changes_panel"));
    m_changesPanelAction->setText(i18n("Generated Changes"));
    m_changesPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    m_changesPanelAction->setCheckable(true);
    connect(m_changesPanelAction, &QAction::toggled, this, &MainWindow::onChangesPanelActionToggled);

    m_mcpPanelAction = m_actionCollection->addAction(QStringLiteral("view_mcp_panel"));
    m_mcpPanelAction->setText(i18n("MCP Servers"));
    m_mcpPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("network-server")));
    m_mcpPanelAction->setCheckable(true);
    connect(m_mcpPanelAction, &QAction::toggled, this, &MainWindow::onMcpPanelActionToggled);

    auto *showRepositoryViewAction =
        m_actionCollection->addAction(QStringLiteral("view_show_repository"));
    showRepositoryViewAction->setText(i18n("Show Repository View"));
    connect(
        showRepositoryViewAction,
        &QAction::triggered,
        m_projectNavigationPanel,
        &ProjectNavigationPanel::showRepositoryView);

    auto *showFilesViewAction = m_actionCollection->addAction(QStringLiteral("view_show_files"));
    showFilesViewAction->setText(i18n("Show Files View"));
    connect(
        showFilesViewAction,
        &QAction::triggered,
        m_projectNavigationPanel,
        &ProjectNavigationPanel::showFilesView);

    auto *resetPanelLayoutAction =
        m_actionCollection->addAction(QStringLiteral("view_reset_panel_layout"));
    resetPanelLayoutAction->setText(i18n("Reset Panel Layout"));
    connect(resetPanelLayoutAction, &QAction::triggered, this, &MainWindow::resetPanelLayout);
}

void MainWindow::configureMenus()
{
    auto *fileMenu = menuBar()->addMenu(i18n("&File"));
    fileMenu->addAction(m_actionCollection->action(QStringLiteral("file_add_repository")));
    fileMenu->addAction(m_actionCollection->action(QStringLiteral("file_refresh_status")));
    fileMenu->addAction(m_actionCollection->action(QStringLiteral("file_new_terminal_tab")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveFileAction);
    fileMenu->addAction(m_closeFileTabAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionCollection->action(KStandardAction::name(KStandardAction::Quit)));

    auto *viewMenu = menuBar()->addMenu(i18n("&View"));
    viewMenu->addAction(m_agentSessionsPanelAction);
    viewMenu->addAction(m_contextPanelAction);
    viewMenu->addAction(m_changesPanelAction);
    viewMenu->addAction(m_mcpPanelAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionCollection->action(QStringLiteral("view_show_repository")));
    viewMenu->addAction(m_actionCollection->action(QStringLiteral("view_show_files")));
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionCollection->action(QStringLiteral("view_reset_panel_layout")));

    m_helpMenu = new KHelpMenu(this);
    menuBar()->addMenu(m_helpMenu->menu());
}

void MainWindow::configureActivityBar()
{
    m_activityToolBar = new QToolBar(i18n("Activity Bar"), this);
    m_activityToolBar->setObjectName(QStringLiteral("ActivityToolBar"));
    m_activityToolBar->setMovable(false);
    m_activityToolBar->setOrientation(Qt::Vertical);
    m_activityToolBar->setIconSize(QSize(22, 22));
    m_activityToolBar->addAction(m_agentSessionsPanelAction);
    m_activityToolBar->addAction(m_contextPanelAction);
    m_activityToolBar->addAction(m_changesPanelAction);
    m_activityToolBar->addAction(m_mcpPanelAction);
    addToolBar(Qt::RightToolBarArea, m_activityToolBar);
}

void MainWindow::onAgentSessionsPanelActionToggled(bool visible)
{
    if (visible) {
        setActiveRightPanelAction(QString::fromLatin1(qtcode::settings::kRightPanelSessions));
        return;
    }

    if (currentActiveRightPanel() == QString::fromLatin1(qtcode::settings::kRightPanelSessions)) {
        applyActiveRightPanel(QString::fromLatin1(qtcode::settings::kRightPanelNone));
    }
}

void MainWindow::onContextPanelActionToggled(bool visible)
{
    if (visible) {
        setActiveRightPanelAction(QString::fromLatin1(qtcode::settings::kRightPanelContext));
        return;
    }

    if (currentActiveRightPanel() == QString::fromLatin1(qtcode::settings::kRightPanelContext)) {
        applyActiveRightPanel(QString::fromLatin1(qtcode::settings::kRightPanelNone));
    }
}

void MainWindow::onChangesPanelActionToggled(bool visible)
{
    if (visible) {
        setActiveRightPanelAction(QString::fromLatin1(qtcode::settings::kRightPanelChanges));
        return;
    }

    if (currentActiveRightPanel() == QString::fromLatin1(qtcode::settings::kRightPanelChanges)) {
        applyActiveRightPanel(QString::fromLatin1(qtcode::settings::kRightPanelNone));
    }
}

void MainWindow::onMcpPanelActionToggled(bool visible)
{
    if (visible) {
        setActiveRightPanelAction(QString::fromLatin1(qtcode::settings::kRightPanelMcp));
        return;
    }

    if (currentActiveRightPanel() == QString::fromLatin1(qtcode::settings::kRightPanelMcp)) {
        applyActiveRightPanel(QString::fromLatin1(qtcode::settings::kRightPanelNone));
    }
}

void MainWindow::setActiveRightPanelAction(const QString &panelId)
{
    const QSignalBlocker sessionsBlocker(m_agentSessionsPanelAction);
    const QSignalBlocker contextBlocker(m_contextPanelAction);
    const QSignalBlocker changesBlocker(m_changesPanelAction);
    const QSignalBlocker mcpBlocker(m_mcpPanelAction);

    m_agentSessionsPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelSessions));
    m_contextPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelContext));
    m_changesPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelChanges));
    m_mcpPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelMcp));

    applyActiveRightPanel(panelId);
}

void MainWindow::applyActiveRightPanel(const QString &panelId)
{
    if (m_rightPanelStack == nullptr) {
        return;
    }

    if (panelId == QString::fromLatin1(qtcode::settings::kRightPanelSessions) && m_agentPanel != nullptr) {
        m_rightPanelStack->setCurrentWidget(m_agentPanel->sessionPanel());
    } else if (panelId == QString::fromLatin1(qtcode::settings::kRightPanelContext) && m_agentPanel != nullptr) {
        m_rightPanelStack->setCurrentWidget(m_agentPanel->contextPanel());
    } else if (panelId == QString::fromLatin1(qtcode::settings::kRightPanelChanges) && m_agentPanel != nullptr) {
        m_rightPanelStack->setCurrentWidget(m_agentPanel->changesPanel());
    } else if (panelId == QString::fromLatin1(qtcode::settings::kRightPanelMcp) && m_mcpServerPanel != nullptr) {
        m_rightPanelStack->setCurrentWidget(m_mcpServerPanel);
    }

    syncRightPanelVisibility();
}

QString MainWindow::currentActiveRightPanel() const
{
    if (m_agentSessionsPanelAction != nullptr && m_agentSessionsPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelSessions);
    }

    if (m_contextPanelAction != nullptr && m_contextPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelContext);
    }

    if (m_changesPanelAction != nullptr && m_changesPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelChanges);
    }

    if (m_mcpPanelAction != nullptr && m_mcpPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelMcp);
    }

    return QString::fromLatin1(qtcode::settings::kRightPanelNone);
}

void MainWindow::syncRightPanelVisibility()
{
    if (m_rootHorizontalSplitter == nullptr || m_rightPanelStack == nullptr) {
        return;
    }

    QList<int> sizes = m_rootHorizontalSplitter->sizes();
    if (sizes.size() < 3) {
        return;
    }

    const QString activePanel = currentActiveRightPanel();
    if (activePanel == QString::fromLatin1(qtcode::settings::kRightPanelNone)) {
        if (sizes.at(2) > 0) {
            m_storedRightColumnWidth = sizes.at(2);
        }
        sizes[2] = 0;
        m_rightPanelStack->setVisible(false);
    } else {
        if (sizes.at(2) < 120) {
            sizes[2] = m_storedRightColumnWidth > 120 ? m_storedRightColumnWidth : 320;
        }
        m_rightPanelStack->setVisible(true);
    }

    m_rootHorizontalSplitter->setSizes(sizes);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_workspaceTabs != nullptr && !m_workspaceTabs->closeAllEditorTabs(true)) {
        event->ignore();
        return;
    }

    persistPanelLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveCurrentFile()
{
    if (m_workspaceTabs != nullptr) {
        (void) m_workspaceTabs->saveCurrentEditorTab();
    }
}

void MainWindow::refreshEditorActions(bool hasActiveEditor, bool isModified)
{
    if (m_saveFileAction != nullptr) {
        m_saveFileAction->setEnabled(hasActiveEditor && isModified);
    }
    if (m_closeFileTabAction != nullptr) {
        m_closeFileTabAction->setEnabled(hasActiveEditor);
    }
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

    if (m_rootHorizontalSplitter != nullptr && layout.columnSizes.size() >= 3) {
        m_rootHorizontalSplitter->setSizes(layout.columnSizes);
        if (layout.columnSizes.at(2) > 120) {
            m_storedRightColumnWidth = layout.columnSizes.at(2);
        }
    }

    if (m_mainVerticalSplitter != nullptr && layout.verticalSizes.size() >= 2) {
        m_mainVerticalSplitter->setSizes(layout.verticalSizes);
    }

    setActiveRightPanelAction(layout.activeRightPanel);
}

qtcode::settings::PanelLayoutSettings MainWindow::currentPanelLayout() const
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.windowWidth = width();
    layout.windowHeight = height();
    layout.activeRightPanel = currentActiveRightPanel();

    if (m_rootHorizontalSplitter != nullptr) {
        layout.columnSizes = m_rootHorizontalSplitter->sizes();
        if (layout.columnSizes.size() >= 3
            && layout.activeRightPanel == QString::fromLatin1(qtcode::settings::kRightPanelNone)) {
            layout.columnSizes[2] = 0;
        }
    }

    if (m_mainVerticalSplitter != nullptr) {
        layout.verticalSizes = m_mainVerticalSplitter->sizes();
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

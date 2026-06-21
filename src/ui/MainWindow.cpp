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
#include "ui/views/ContextResultsView.h"
#include "ui/StatusBar.h"

#include <KActionCollection>
#include <KHelpMenu>
#include <KLocalizedString>
#include <KStandardAction>
#include <KTextEditor/Application>
#include <KTextEditor/Editor>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QIcon>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtGlobal>

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

    if (m_workspaceTabs != nullptr) {
        (void) m_workspaceTabs->closeAllEditorTabs(false);
    }

    if (KTextEditor::Editor::instance() != nullptr) {
        KTextEditor::Editor::instance()->setApplication(nullptr);
    }

    delete m_ktextEditorApplication;
    m_ktextEditorApplication = nullptr;
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
    connect(
        m_terminalPanel,
        &TerminalPanel::collapsedChanged,
        this,
        &MainWindow::onTerminalPanelCollapsedChanged);

    m_workspaceTabs = new WorkspaceTabs(
        m_controller != nullptr ? m_controller->statusService() : nullptr,
        m_controller != nullptr ? m_controller->projectManager() : nullptr,
        this);
    m_workspaceTabs->setPermanentAiChatTab(m_agentPanel->conversationPanel());

    if (m_controller != nullptr) {
        m_ktextEditorApplication = new KTextEditor::Application(&m_ktextEditorHost);
        KTextEditor::Editor::instance()->setApplication(m_ktextEditorApplication);
    }

    m_projectNavigationPanel->setMinimumWidth(240);
    m_workspaceTabs->setMinimumWidth(320);
    m_terminalPanel->setMinimumHeight(120);

    m_rightPanelStack = new QStackedWidget(this);
    m_rightPanelStack->setMinimumWidth(240);
    m_rightPanelStack->addWidget(m_agentPanel->sessionPanel());
    m_rightPanelStack->addWidget(m_agentPanel->contextPanel());
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
    connect(
        m_rootHorizontalSplitter,
        &QSplitter::splitterMoved,
        this,
        [this](int /*pos*/, int /*index*/) {
            if (m_rootHorizontalSplitter == nullptr || m_rightColumnCollapsed) {
                return;
            }

            const QList<int> sizes = m_rootHorizontalSplitter->sizes();
            if (sizes.size() >= 3 && sizes.at(2) > 120) {
                m_storedRightColumnWidth = sizes.at(2);
            }
        });

    if (m_controller != nullptr && m_controller->agentManager() != nullptr) {
        connect(
            m_controller->agentManager(),
            &qtcode::agents::AgentManager::repositoryRefreshRequested,
            m_projectNavigationPanel->repositoryPanel(),
            [panel = m_projectNavigationPanel->repositoryPanel()]() {
                panel->refreshStatus(false);
            });
    }

    if (m_projectNavigationPanel != nullptr && m_agentPanel != nullptr && m_workspaceTabs != nullptr) {
        connect(
            m_workspaceTabs,
            &WorkspaceTabs::issueContextSelected,
            m_agentPanel,
            &AgentPanel::attachIssueContext);
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::issueContextRequested,
            m_agentPanel,
            &AgentPanel::attachIssueContext);
        connect(
            m_workspaceTabs,
            &WorkspaceTabs::pullRequestContextSelected,
            m_agentPanel,
            &AgentPanel::attachPullRequestContext);
    }

    if (m_projectNavigationPanel != nullptr && m_workspaceTabs != nullptr) {
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::issueOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenIssue);
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::pullRequestOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenPullRequest);
        connect(
            m_projectNavigationPanel->repositoryPanel(),
            &RepositoryPanel::fileOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenFile);
        connect(
            m_projectNavigationPanel->fileTreePanel(),
            &FileTreePanel::fileOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenFile);
        connect(
            m_projectNavigationPanel->fileTreePanel(),
            &FileTreePanel::pathRenamed,
            m_workspaceTabs,
            &WorkspaceTabs::handleFileRenamed);
        connect(
            m_projectNavigationPanel->fileTreePanel(),
            &FileTreePanel::pathDeleted,
            m_workspaceTabs,
            &WorkspaceTabs::handlePathDeleted);
        connect(
            m_workspaceTabs,
            &WorkspaceTabs::activeEditorStateChanged,
            this,
            &MainWindow::refreshEditorActions);
    }

    if (m_agentPanel != nullptr && m_workspaceTabs != nullptr
        && m_agentPanel->contextResultsView() != nullptr) {
        connect(
            m_agentPanel->contextResultsView(),
            &ContextResultsView::fileOpenRequested,
            m_workspaceTabs,
            &WorkspaceTabs::requestOpenFile);
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

    m_newFileAction = m_actionCollection->addAction(QStringLiteral("file_new_file"));
    m_newFileAction->setText(i18n("New File"));
    m_newFileAction->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    connect(
        m_newFileAction,
        &QAction::triggered,
        m_projectNavigationPanel->fileTreePanel(),
        &FileTreePanel::createNewFile);

    m_newFolderAction = m_actionCollection->addAction(QStringLiteral("file_new_folder"));
    m_newFolderAction->setText(i18n("New Folder"));
    m_newFolderAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    connect(
        m_newFolderAction,
        &QAction::triggered,
        m_projectNavigationPanel->fileTreePanel(),
        &FileTreePanel::createNewFolder);

    m_renameEntryAction = m_actionCollection->addAction(QStringLiteral("file_rename"));
    m_renameEntryAction->setText(i18n("Rename"));
    connect(
        m_renameEntryAction,
        &QAction::triggered,
        m_projectNavigationPanel->fileTreePanel(),
        &FileTreePanel::renameSelectedEntry);

    m_deleteEntryAction = m_actionCollection->addAction(QStringLiteral("file_delete"));
    m_deleteEntryAction->setText(i18n("Delete"));
    m_deleteEntryAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    connect(
        m_deleteEntryAction,
        &QAction::triggered,
        m_projectNavigationPanel->fileTreePanel(),
        &FileTreePanel::deleteSelectedEntry);

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
    m_agentSessionsPanelAction->setChecked(true);

    m_contextPanelAction = m_actionCollection->addAction(QStringLiteral("view_context_panel"));
    m_contextPanelAction->setText(i18n("Retrieved Context"));
    m_contextPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks-organize")));
    m_contextPanelAction->setCheckable(true);

    m_mcpPanelAction = m_actionCollection->addAction(QStringLiteral("view_mcp_panel"));
    m_mcpPanelAction->setText(i18n("MCP Servers"));
    m_mcpPanelAction->setIcon(QIcon::fromTheme(QStringLiteral("network-server")));
    m_mcpPanelAction->setCheckable(true);

    auto *rightPanelActionGroup = new QActionGroup(this);
    rightPanelActionGroup->setExclusive(true);
    rightPanelActionGroup->addAction(m_agentSessionsPanelAction);
    rightPanelActionGroup->addAction(m_contextPanelAction);
    rightPanelActionGroup->addAction(m_mcpPanelAction);
    connect(
        rightPanelActionGroup,
        &QActionGroup::triggered,
        this,
        &MainWindow::onRightPanelActionTriggered);

    m_rightPanelToggleAction = m_actionCollection->addAction(QStringLiteral("view_toggle_right_panel"));
    m_rightPanelToggleAction->setText(i18n("Toggle Right Panel"));
    connect(
        m_rightPanelToggleAction,
        &QAction::triggered,
        this,
        &MainWindow::toggleRightColumnCollapsed);
    updateRightPanelToggleAction();

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
    fileMenu->addAction(m_newFileAction);
    fileMenu->addAction(m_newFolderAction);
    fileMenu->addAction(m_renameEntryAction);
    fileMenu->addAction(m_deleteEntryAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveFileAction);
    fileMenu->addAction(m_closeFileTabAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionCollection->action(KStandardAction::name(KStandardAction::Quit)));

    auto *viewMenu = menuBar()->addMenu(i18n("&View"));
    viewMenu->addAction(m_rightPanelToggleAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_agentSessionsPanelAction);
    viewMenu->addAction(m_contextPanelAction);
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
    m_activityToolBar->addAction(m_rightPanelToggleAction);
    m_activityToolBar->addSeparator();
    m_activityToolBar->addAction(m_agentSessionsPanelAction);
    m_activityToolBar->addAction(m_contextPanelAction);
    m_activityToolBar->addAction(m_mcpPanelAction);
    addToolBar(Qt::RightToolBarArea, m_activityToolBar);
}

void MainWindow::onRightPanelActionTriggered(QAction *action)
{
    QString panelId = QString::fromLatin1(qtcode::settings::kRightPanelSessions);
    if (action == m_contextPanelAction) {
        panelId = QString::fromLatin1(qtcode::settings::kRightPanelContext);
    } else if (action == m_mcpPanelAction) {
        panelId = QString::fromLatin1(qtcode::settings::kRightPanelMcp);
    } else if (action != m_agentSessionsPanelAction) {
        return;
    }

    if (m_rightColumnCollapsed) {
        setRightColumnCollapsed(false);
    }

    applyActiveRightPanel(panelId);
}

void MainWindow::toggleRightColumnCollapsed()
{
    setRightColumnCollapsed(!m_rightColumnCollapsed);
}

void MainWindow::setRightColumnCollapsed(bool collapsed)
{
    if (m_rightColumnCollapsed == collapsed) {
        return;
    }

    m_rightColumnCollapsed = collapsed;
    syncRightColumnVisibility();
    updateRightPanelToggleAction();
}

void MainWindow::updateRightPanelToggleAction()
{
    if (m_rightPanelToggleAction == nullptr) {
        return;
    }

    if (m_rightColumnCollapsed) {
        m_rightPanelToggleAction->setIcon(QIcon::fromTheme(QStringLiteral("arrow-left")));
        m_rightPanelToggleAction->setToolTip(i18n("Expand Right Panel"));
        m_rightPanelToggleAction->setText(i18n("Expand Right Panel"));
    } else {
        m_rightPanelToggleAction->setIcon(QIcon::fromTheme(QStringLiteral("arrow-right")));
        m_rightPanelToggleAction->setToolTip(i18n("Collapse Right Panel"));
        m_rightPanelToggleAction->setText(i18n("Collapse Right Panel"));
    }
}

void MainWindow::onTerminalPanelCollapsedChanged(bool collapsed)
{
    Q_UNUSED(collapsed);
    syncTerminalPanelHeight();
}

void MainWindow::syncTerminalPanelHeight()
{
    if (m_mainVerticalSplitter == nullptr || m_terminalPanel == nullptr) {
        return;
    }

    QList<int> sizes = m_mainVerticalSplitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const bool collapsed = m_terminalPanel->isCollapsed();
    const int collapsedHeight = m_terminalPanel->collapsedHeight();
    const int splitterHeight = m_mainVerticalSplitter->height();

    if (collapsed) {
        if (sizes.at(1) > collapsedHeight + 8) {
            m_storedTerminalHeight = sizes.at(1);
        }

        m_terminalPanel->setMinimumHeight(collapsedHeight);
        m_terminalPanel->setMaximumHeight(collapsedHeight);

        sizes[1] = collapsedHeight;
        sizes[0] = qMax(0, splitterHeight - collapsedHeight);
        m_mainVerticalSplitter->setSizes(sizes);
        return;
    }

    m_terminalPanel->setMaximumHeight(QWIDGETSIZE_MAX);
    m_terminalPanel->setMinimumHeight(120);

    const int restoredHeight = m_storedTerminalHeight > 120 ? m_storedTerminalHeight : 240;
    const int terminalHeight = qMin(restoredHeight, qMax(120, splitterHeight - 120));
    sizes[1] = terminalHeight;
    sizes[0] = qMax(0, splitterHeight - terminalHeight);
    m_mainVerticalSplitter->setSizes(sizes);
}

void MainWindow::setActiveRightPanelAction(const QString &panelId)
{
    const QSignalBlocker sessionsBlocker(m_agentSessionsPanelAction);
    const QSignalBlocker contextBlocker(m_contextPanelAction);
    const QSignalBlocker mcpBlocker(m_mcpPanelAction);

    m_agentSessionsPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelSessions));
    m_contextPanelAction->setChecked(panelId == QString::fromLatin1(qtcode::settings::kRightPanelContext));
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
    } else if (panelId == QString::fromLatin1(qtcode::settings::kRightPanelMcp) && m_mcpServerPanel != nullptr) {
        m_rightPanelStack->setCurrentWidget(m_mcpServerPanel);
    }

    syncRightColumnVisibility();
}

QString MainWindow::currentActiveRightPanel() const
{
    if (m_contextPanelAction != nullptr && m_contextPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelContext);
    }

    if (m_mcpPanelAction != nullptr && m_mcpPanelAction->isChecked()) {
        return QString::fromLatin1(qtcode::settings::kRightPanelMcp);
    }

    return QString::fromLatin1(qtcode::settings::kRightPanelSessions);
}

void MainWindow::syncRightColumnVisibility()
{
    if (m_rootHorizontalSplitter == nullptr || m_rightPanelStack == nullptr) {
        return;
    }

    QList<int> sizes = m_rootHorizontalSplitter->sizes();
    if (sizes.size() < 3) {
        return;
    }

    const int leftWidth = sizes.at(0);
    const int minCenterWidth = m_workspaceTabs != nullptr ? m_workspaceTabs->minimumWidth() : 320;
    const int splitterWidth = m_rootHorizontalSplitter->width();

    if (m_rightColumnCollapsed) {
        if (sizes.at(2) > 0) {
            m_storedRightColumnWidth = sizes.at(2);
            sizes[1] += sizes.at(2);
            sizes[2] = 0;
        }
        sizes[0] = leftWidth;
        m_rightPanelStack->setVisible(false);
    } else {
        m_rightPanelStack->setVisible(true);
        if (sizes.at(2) < 120) {
            const int restoreWidth = m_storedRightColumnWidth > 120 ? m_storedRightColumnWidth : 320;
            const int availableForRight = qMax(0, splitterWidth - leftWidth - minCenterWidth);
            const int appliedWidth = qMin(restoreWidth, availableForRight);
            sizes[0] = leftWidth;
            sizes[2] = appliedWidth;
            sizes[1] = qMax(minCenterWidth, splitterWidth - leftWidth - appliedWidth);
        }
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
        if (!layout.terminalCollapsed && layout.verticalSizes.at(1) > 120) {
            m_storedTerminalHeight = layout.verticalSizes.at(1);
        }
    }

    if (layout.storedTerminalHeight > 120) {
        m_storedTerminalHeight = layout.storedTerminalHeight;
    }

    if (m_terminalPanel != nullptr) {
        const QSignalBlocker blocker(m_terminalPanel);
        m_terminalPanel->setCollapsed(layout.terminalCollapsed);
    }
    syncTerminalPanelHeight();

    if (layout.storedRightColumnWidth > 120) {
        m_storedRightColumnWidth = layout.storedRightColumnWidth;
    }

    QString activePanel = layout.activeRightPanel;
    if (activePanel == QString::fromLatin1(qtcode::settings::kRightPanelNone)) {
        activePanel = QString::fromLatin1(qtcode::settings::kRightPanelSessions);
    }

    m_rightColumnCollapsed = layout.rightColumnCollapsed;
    updateRightPanelToggleAction();
    setActiveRightPanelAction(activePanel);
}

qtcode::settings::PanelLayoutSettings MainWindow::currentPanelLayout() const
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.windowWidth = width();
    layout.windowHeight = height();
    layout.activeRightPanel = currentActiveRightPanel();
    layout.rightColumnCollapsed = m_rightColumnCollapsed;

    if (m_rootHorizontalSplitter != nullptr) {
        layout.columnSizes = m_rootHorizontalSplitter->sizes();
        if (layout.columnSizes.size() >= 3) {
            if (m_rightColumnCollapsed) {
                layout.columnSizes[2] = 0;
            }

            if (m_storedRightColumnWidth > 120) {
                layout.storedRightColumnWidth = m_storedRightColumnWidth;
            } else if (!m_rightColumnCollapsed && layout.columnSizes.at(2) > 120) {
                layout.storedRightColumnWidth = layout.columnSizes.at(2);
            }
        }
    }

    if (m_mainVerticalSplitter != nullptr) {
        layout.verticalSizes = m_mainVerticalSplitter->sizes();
    }

    if (m_terminalPanel != nullptr) {
        layout.terminalCollapsed = m_terminalPanel->isCollapsed();
        if (layout.terminalCollapsed && m_storedTerminalHeight > 120) {
            layout.storedTerminalHeight = m_storedTerminalHeight;
        }
    }

    return layout;
}

bool MainWindow::runWorkspaceSmokeChecks(QString *errorMessage)
{
    if (m_workspaceTabs == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workspace tabs are unavailable.");
        }
        return false;
    }

    if (m_workspaceTabs->aiChatTabIndex() < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Permanent AI Chat tab is missing.");
        }
        return false;
    }

    const int initialTabCount = m_workspaceTabs->tabCount();
    if (initialTabCount < 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workspace has no tabs.");
        }
        return false;
    }

    const QByteArray workspaceFile = qgetenv("QTCODE_WORKSPACE_FILE");
    if (!workspaceFile.isEmpty()) {
        const QString filePath = QString::fromUtf8(workspaceFile);
        if (!QFileInfo::exists(filePath)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Workspace smoke file is missing.");
            }
            return false;
        }

        m_workspaceTabs->requestOpenFile(filePath);
        if (m_workspaceTabs->tabCount() <= initialTabCount) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Editor tab did not open.");
            }
            return false;
        }

        if (!m_workspaceTabs->hasActiveEditorTab()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Active editor tab unavailable after open.");
            }
            return false;
        }

        if (!m_workspaceTabs->saveCurrentEditorTab()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Save failed for workspace smoke file.");
            }
            return false;
        }
    }

    qCInfo(qtcodeUi) << "Workspace smoke check passed";
    return true;
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

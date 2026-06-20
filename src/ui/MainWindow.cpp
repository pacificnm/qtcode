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

#include <KLocalizedString>

#include <QSplitter>

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
    m_horizontalSplitter->addWidget(m_repositoryPanel);
    m_horizontalSplitter->addWidget(m_agentPanel);
    m_horizontalSplitter->addWidget(m_mcpServerPanel);
    m_horizontalSplitter->setStretchFactor(0, 1);
    m_horizontalSplitter->setStretchFactor(1, 2);
    m_horizontalSplitter->setStretchFactor(2, 1);
    m_horizontalSplitter->setCollapsible(0, false);
    m_horizontalSplitter->setCollapsible(1, false);
    m_horizontalSplitter->setCollapsible(2, false);

    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->addWidget(m_horizontalSplitter);
    m_verticalSplitter->addWidget(m_terminalPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 1);
    m_verticalSplitter->setCollapsible(0, false);
    m_verticalSplitter->setCollapsible(1, false);

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
            &RepositoryPanel::pullRequestContextSelected,
            m_agentPanel,
            &AgentPanel::attachPullRequestContext);
    }

    setCentralWidget(m_verticalSplitter);

    qCInfo(qtcodeUi) << "Initialized repository, agent, MCP, and terminal panel layout";
}

void MainWindow::applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout)
{
    resize(layout.windowWidth, layout.windowHeight);

    if (m_horizontalSplitter != nullptr && layout.horizontalSizes.size() >= 3) {
        m_horizontalSplitter->setSizes(layout.horizontalSizes);
    } else if (m_horizontalSplitter != nullptr && layout.horizontalSizes.size() >= 2) {
        QList<int> sizes = layout.horizontalSizes;
        sizes.append(320);
        m_horizontalSplitter->setSizes(sizes);
    }

    if (m_verticalSplitter != nullptr && layout.verticalSizes.size() >= 2) {
        m_verticalSplitter->setSizes(layout.verticalSizes);
    }
}

qtcode::settings::PanelLayoutSettings MainWindow::currentPanelLayout() const
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.windowWidth = width();
    layout.windowHeight = height();

    if (m_horizontalSplitter != nullptr) {
        layout.horizontalSizes = m_horizontalSplitter->sizes();
    }

    if (m_verticalSplitter != nullptr) {
        layout.verticalSizes = m_verticalSplitter->sizes();
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

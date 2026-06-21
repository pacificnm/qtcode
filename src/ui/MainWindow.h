#pragma once

#include <QMainWindow>

class QAction;
class QSplitter;
class QStackedWidget;
class QToolBar;

class KActionCollection;
class KHelpMenu;

namespace qtcode::core {
class ApplicationController;
class SettingsService;
} // namespace qtcode::core

namespace qtcode::settings {
struct PanelLayoutSettings;
} // namespace qtcode::settings

namespace qtcode::ui {

class AgentPanel;
class McpServerPanel;
class RepositoryPanel;
class StatusBar;
class TerminalPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(qtcode::core::ApplicationController *controller, QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void resetPanelLayout();
    void onAgentSessionsPanelActionToggled(bool visible);
    void onContextPanelActionToggled(bool visible);
    void onChangesPanelActionToggled(bool visible);
    void onMcpPanelActionToggled(bool visible);

private:
    void configureLayout();
    void configureActions();
    void configureMenus();
    void configureActivityBar();
    void applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout);
    void applyActiveRightPanel(const QString &panelId);
    void syncRightPanelVisibility();
    void setActiveRightPanelAction(const QString &panelId);
    [[nodiscard]] QString currentActiveRightPanel() const;
    [[nodiscard]] qtcode::settings::PanelLayoutSettings currentPanelLayout() const;
    void persistPanelLayout();

    qtcode::core::ApplicationController *m_controller = nullptr;
    qtcode::core::SettingsService *m_settingsService = nullptr;
    QSplitter *m_rootHorizontalSplitter = nullptr;
    QSplitter *m_mainVerticalSplitter = nullptr;
    QStackedWidget *m_rightPanelStack = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    McpServerPanel *m_mcpServerPanel = nullptr;
    TerminalPanel *m_terminalPanel = nullptr;
    StatusBar *m_statusBar = nullptr;
    KActionCollection *m_actionCollection = nullptr;
    KHelpMenu *m_helpMenu = nullptr;
    QToolBar *m_activityToolBar = nullptr;
    QAction *m_agentSessionsPanelAction = nullptr;
    QAction *m_contextPanelAction = nullptr;
    QAction *m_changesPanelAction = nullptr;
    QAction *m_mcpPanelAction = nullptr;
    int m_storedRightColumnWidth = 320;
};

} // namespace qtcode::ui

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
class TerminalPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(qtcode::core::ApplicationController *controller, QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void resetPanelLayout();
    void onAgentPanelActionToggled(bool visible);
    void onContextPanelActionToggled(bool visible);
    void onChangesPanelActionToggled(bool visible);

private:
    void configureLayout();
    void configureActions();
    void configureMenus();
    void configureToolBar();
    void configureActivityBar();
    void applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout);
    void applyPanelVisibility(const qtcode::settings::PanelLayoutSettings &layout);
    void syncSecondaryPanelVisibility();
    void syncAgentPanelVisibility();
    [[nodiscard]] qtcode::settings::PanelLayoutSettings currentPanelLayout() const;
    void persistPanelLayout();

    qtcode::core::ApplicationController *m_controller = nullptr;
    qtcode::core::SettingsService *m_settingsService = nullptr;
    QSplitter *m_rootHorizontalSplitter = nullptr;
    QSplitter *m_middleVerticalSplitter = nullptr;
    QStackedWidget *m_secondaryPanelStack = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    McpServerPanel *m_mcpServerPanel = nullptr;
    TerminalPanel *m_terminalPanel = nullptr;
    KActionCollection *m_actionCollection = nullptr;
    KHelpMenu *m_helpMenu = nullptr;
    QToolBar *m_mainToolBar = nullptr;
    QToolBar *m_activityToolBar = nullptr;
    QAction *m_toggleToolBarAction = nullptr;
    QAction *m_agentPanelAction = nullptr;
    QAction *m_contextPanelAction = nullptr;
    QAction *m_changesPanelAction = nullptr;
    int m_storedSecondaryColumnWidth = 320;
    int m_storedAgentPanelHeight = 560;
};

} // namespace qtcode::ui

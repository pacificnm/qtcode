#pragma once

#include <QMainWindow>

class QAction;
class QSplitter;
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

private:
    void configureLayout();
    void configureActions();
    void configureMenus();
    void configureToolBar();
    void applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout);
    [[nodiscard]] qtcode::settings::PanelLayoutSettings currentPanelLayout() const;
    void persistPanelLayout();

    qtcode::core::ApplicationController *m_controller = nullptr;
    qtcode::core::SettingsService *m_settingsService = nullptr;
    QSplitter *m_rootHorizontalSplitter = nullptr;
    QSplitter *m_rightVerticalSplitter = nullptr;
    QSplitter *m_horizontalSplitter = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    McpServerPanel *m_mcpServerPanel = nullptr;
    TerminalPanel *m_terminalPanel = nullptr;
    KActionCollection *m_actionCollection = nullptr;
    KHelpMenu *m_helpMenu = nullptr;
    QToolBar *m_mainToolBar = nullptr;
    QAction *m_toggleToolBarAction = nullptr;
};

} // namespace qtcode::ui

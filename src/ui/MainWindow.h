#pragma once

#include <QMainWindow>

#include <memory>

class QAction;
class QCloseEvent;
class QSplitter;
class QStackedWidget;
class QToolBar;

class KActionCollection;
class KHelpMenu;

namespace KTextEditor {
class Application;
} // namespace KTextEditor

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
class ProjectNavigationPanel;
class StatusBar;
class TerminalPanel;
class WorkspaceTabs;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(qtcode::core::ApplicationController *controller, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void resetPanelLayout();
    void saveCurrentFile();
    void refreshEditorActions(bool hasActiveEditor, bool isModified);
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
    ProjectNavigationPanel *m_projectNavigationPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    WorkspaceTabs *m_workspaceTabs = nullptr;
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
    QAction *m_saveFileAction = nullptr;
    QAction *m_closeFileTabAction = nullptr;
    std::unique_ptr<KTextEditor::Application> m_ktextEditorApplication;
    int m_storedRightColumnWidth = 320;
};

} // namespace qtcode::ui

#pragma once

#include <QMainWindow>

class QSplitter;

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

private:
    void configureLayout();
    void applyPanelLayout(const qtcode::settings::PanelLayoutSettings &layout);
    [[nodiscard]] qtcode::settings::PanelLayoutSettings currentPanelLayout() const;
    void persistPanelLayout();

    qtcode::core::ApplicationController *m_controller = nullptr;
    qtcode::core::SettingsService *m_settingsService = nullptr;
    QSplitter *m_verticalSplitter = nullptr;
    QSplitter *m_horizontalSplitter = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    McpServerPanel *m_mcpServerPanel = nullptr;
    TerminalPanel *m_terminalPanel = nullptr;
};

} // namespace qtcode::ui

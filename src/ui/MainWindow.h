#pragma once

#include <QMainWindow>

class QSplitter;

namespace qtcode::ui {

class AgentPanel;
class RepositoryPanel;
class TerminalPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void configureLayout();

    QSplitter *m_verticalSplitter = nullptr;
    QSplitter *m_horizontalSplitter = nullptr;
    RepositoryPanel *m_repositoryPanel = nullptr;
    AgentPanel *m_agentPanel = nullptr;
    TerminalPanel *m_terminalPanel = nullptr;
};

} // namespace qtcode::ui

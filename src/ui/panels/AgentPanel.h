#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

namespace qtcode::agents {
class AgentManager;
class AgentSession;
} // namespace qtcode::agents

namespace qtcode::core {
class CliCapabilityService;
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::ui {

class AgentPanel final : public QWidget
{
    Q_OBJECT

public:
    AgentPanel(
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::agents::AgentManager *agentManager,
        qtcode::core::ProjectManager *projectManager,
        QWidget *parent = nullptr);

private slots:
    void refreshCapabilityState();
    void sendPrompt();
    void onSessionUpdated(qtcode::agents::AgentSession *session);
    void onActiveProjectChanged();

private:
    void configureLayout();
    void refreshConversation();
    void setPromptEnabled(bool enabled);
    [[nodiscard]] bool ensureActiveSession(QString *errorMessage);

    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::agents::AgentManager *m_agentManager = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QLabel *m_stateLabel = nullptr;
    QTextEdit *m_conversationView = nullptr;
    QLineEdit *m_promptInput = nullptr;
    QPushButton *m_sendButton = nullptr;
    QString m_activeSessionId;
};

} // namespace qtcode::ui

#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
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
    void refreshAgentSelector();
    void refreshSessionList();
    void sendPrompt();
    void createNewSession();
    void cancelActiveRequest();
    void onSessionListSelectionChanged();
    void onSessionUpdated(qtcode::agents::AgentSession *session);
    void onActiveProjectChanged();

private:
    void configureLayout();
    void refreshConversation();
    void updateSessionStatusDisplay(const qtcode::agents::AgentSession *session);
    void updateRequestControls(const qtcode::agents::AgentSession *session);
    void setPromptEnabled(bool enabled);
    void selectSession(const QString &sessionId);
    [[nodiscard]] QString selectedAgentKey() const;
    [[nodiscard]] static QString sessionListLabel(const qtcode::agents::AgentSession *session);
    [[nodiscard]] bool ensureActiveSession(QString *errorMessage);

    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::agents::AgentManager *m_agentManager = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QComboBox *m_agentSelector = nullptr;
    QListWidget *m_sessionList = nullptr;
    QPushButton *m_newSessionButton = nullptr;
    QLabel *m_stateLabel = nullptr;
    QTextEdit *m_conversationView = nullptr;
    QLineEdit *m_promptInput = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QString m_activeSessionId;
    bool m_refreshingSessionList = false;
};

} // namespace qtcode::ui

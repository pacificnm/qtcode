#pragma once

#include <QWidget>

class QComboBox;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace qtcode::agents {
class AgentManager;
class AgentSession;
} // namespace qtcode::agents

namespace qtcode::core {
class CliCapabilityService;
class ContextManager;
class ProjectManager;
class StatusService;
} // namespace qtcode::core

#include "core/ContextManager.h"
#include "core/StatusModels.h"
#include "github/GitHubModels.h"

namespace qtcode::settings {
struct ProjectRecord;
} // namespace qtcode::settings

namespace qtcode::ui {

class ContextResultsView;
class ConversationView;

class AgentPanel final : public QWidget
{
    Q_OBJECT

public:
    AgentPanel(
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::agents::AgentManager *agentManager,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::ContextManager *contextManager,
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

    [[nodiscard]] QWidget *sessionPanel() const;
    [[nodiscard]] QWidget *conversationPanel() const;
    [[nodiscard]] QWidget *contextPanel() const;
    [[nodiscard]] ContextResultsView *contextResultsView() const;

public slots:
    void attachIssueContext(const qtcode::github::GitHubIssueDetail &detail);
    void attachPullRequestContext(const qtcode::github::GitHubPullRequestDetail &detail);
    void attachFileContext(const QString &absolutePath, const QString &contentOverride = {});
    void reloadAgentSelector();

private slots:
    void refreshCapabilityState();
    void refreshAgentSelector();
    void refreshSessionList();
    void sendPrompt();
    void onComposerActionClicked();
    void createNewSession();
    void removeSelectedSession();
    void showSessionContextMenu(const QPoint &position);
    void cancelActiveRequest();
    void onSessionListSelectionChanged();
    void onSessionUpdated(qtcode::agents::AgentSession *session);
    void onActiveProjectChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void configureLayout();
    void refreshConversation();
    void updateSessionStatusDisplay(const qtcode::agents::AgentSession *session);
    void updateRequestControls(const qtcode::agents::AgentSession *session);
    void setPromptEnabled(bool enabled);
    void syncPromptComposerState();
    void refreshComposerControls();
    [[nodiscard]] bool isActiveRequestRunning() const;
    void selectSession(const QString &sessionId);
    [[nodiscard]] QString selectedAgentKey() const;
    [[nodiscard]] QString selectedModelKey() const;
    [[nodiscard]] QString selectedExecutionModeKey() const;
    [[nodiscard]] static QString executionModeDisplayLabel(const QString &modeKey);
    [[nodiscard]] static QString sessionListLabel(const qtcode::agents::AgentSession *session);
    [[nodiscard]] bool ensureActiveSession(QString *errorMessage);
    [[nodiscard]] QString preferredAgentKeyForProject() const;
    void refreshSavedContextRetrieval();
    void refreshRequestOptionSelectors();
    void onRequestOptionsChanged();
    void dispatchPromptWithContext(
        const QString &prompt,
        const qtcode::settings::ProjectRecord &project,
        const QStringList &contextExcerpts,
        const QString &statusMessage = {},
        bool memoryUnavailable = false);
    void showStatus(const QString &text, qtcode::core::StatusSeverity severity = qtcode::core::StatusSeverity::Info);

    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::agents::AgentManager *m_agentManager = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::ContextManager *m_contextManager = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    QWidget *m_sessionPanel = nullptr;
    QWidget *m_conversationPanel = nullptr;
    QWidget *m_contextPanel = nullptr;
    QComboBox *m_agentSelector = nullptr;
    QComboBox *m_modelSelector = nullptr;
    QComboBox *m_executionModeSelector = nullptr;
    QListWidget *m_sessionList = nullptr;
    QPushButton *m_newSessionButton = nullptr;
    ContextResultsView *m_contextResultsView = nullptr;
    ConversationView *m_conversationView = nullptr;
    QPlainTextEdit *m_promptInput = nullptr;
    QPushButton *m_sendButton = nullptr;
    QString m_activeSessionId;
    bool m_refreshingSessionList = false;
    bool m_refreshingRequestOptions = false;
    bool m_contextRetrievalInFlight = false;
    bool m_promptComposerEnabled = false;
    QString m_pendingPrompt;
    QString m_lastRetrievalQuery;
    int m_lastTotalResultCount = 0;
    bool m_lastMemoryUnavailable = false;
    QString m_lastRetrievalStatusMessage;
    QStringList m_attachedGitHubContextExcerpts;
    int m_attachedPullRequestNumber = 0;
};

} // namespace qtcode::ui

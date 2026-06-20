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
class ContextManager;
class ProjectManager;
} // namespace qtcode::core

#include "core/ContextManager.h"
#include "github/GitHubModels.h"

namespace qtcode::settings {
struct ProjectRecord;
} // namespace qtcode::settings

namespace qtcode::ui {

class ContextResultsView;
class DiffReviewView;

class AgentPanel final : public QWidget
{
    Q_OBJECT

public:
    AgentPanel(
        qtcode::core::CliCapabilityService *cliCapabilityService,
        qtcode::agents::AgentManager *agentManager,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::ContextManager *contextManager,
        QWidget *parent = nullptr);

public slots:
    void attachPullRequestContext(const qtcode::github::GitHubPullRequestDetail &detail);

private slots:
    void refreshCapabilityState();
    void refreshAgentSelector();
    void refreshSessionList();
    void sendPrompt();
    void createNewSession();
    void approveSelectedArtifact(const QString &artifactId);
    void rejectSelectedArtifact(const QString &artifactId);
    void cancelActiveRequest();
    void onSessionListSelectionChanged();
    void onSessionUpdated(qtcode::agents::AgentSession *session);
    void onActiveProjectChanged();

private:
    void configureLayout();
    void refreshConversation();
    void refreshDiffReview();
    void updateSessionStatusDisplay(const qtcode::agents::AgentSession *session);
    void updateRequestControls(const qtcode::agents::AgentSession *session);
    void setPromptEnabled(bool enabled);
    void selectSession(const QString &sessionId);
    [[nodiscard]] QString selectedAgentKey() const;
    [[nodiscard]] static QString sessionListLabel(const qtcode::agents::AgentSession *session);
    [[nodiscard]] bool ensureActiveSession(QString *errorMessage);
    void refreshSavedContextRetrieval();
    void dispatchPromptWithContext(
        const QString &prompt,
        const qtcode::settings::ProjectRecord &project,
        const QStringList &contextExcerpts,
        const QString &statusMessage = {},
        bool memoryUnavailable = false);

    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    qtcode::agents::AgentManager *m_agentManager = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::ContextManager *m_contextManager = nullptr;
    QComboBox *m_agentSelector = nullptr;
    QListWidget *m_sessionList = nullptr;
    QPushButton *m_newSessionButton = nullptr;
    QLabel *m_stateLabel = nullptr;
    ContextResultsView *m_contextResultsView = nullptr;
    QTextEdit *m_conversationView = nullptr;
    QLineEdit *m_promptInput = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    DiffReviewView *m_diffReviewView = nullptr;
    QString m_activeSessionId;
    bool m_refreshingSessionList = false;
    bool m_contextRetrievalInFlight = false;
    QString m_pendingPrompt;
    QString m_reviewablePrompt;
    bool m_hasReviewableContext = false;
    QString m_lastRetrievalQuery;
    int m_lastTotalResultCount = 0;
    bool m_lastMemoryUnavailable = false;
    QString m_lastRetrievalStatusMessage;
    QStringList m_attachedGitHubContextExcerpts;
    int m_attachedPullRequestNumber = 0;
};

} // namespace qtcode::ui

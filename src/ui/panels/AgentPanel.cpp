#include "ui/panels/AgentPanel.h"

#include "agents/AgentManager.h"
#include "agents/AgentSession.h"
#include "core/CliCapabilityModels.h"
#include "core/CliCapabilityService.h"
#include "core/ProjectManager.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

AgentPanel::AgentPanel(
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::agents::AgentManager *agentManager,
    qtcode::core::ProjectManager *projectManager,
    QWidget *parent)
    : QWidget(parent)
    , m_cliCapabilityService(cliCapabilityService)
    , m_agentManager(agentManager)
    , m_projectManager(projectManager)
{
    configureLayout();
    refreshCapabilityState();
    setPromptEnabled(false);

    if (m_cliCapabilityService != nullptr) {
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            &AgentPanel::refreshCapabilityState);
    }

    if (m_agentManager != nullptr) {
        connect(
            m_agentManager,
            &qtcode::agents::AgentManager::sessionUpdated,
            this,
            &AgentPanel::onSessionUpdated);
    }

    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &AgentPanel::onActiveProjectChanged);
        onActiveProjectChanged();
    }
}

void AgentPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("AI Agent"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_conversationView = new QTextEdit(this);
    m_conversationView->setReadOnly(true);
    m_conversationView->setPlaceholderText(i18n("Agent responses will appear here."));

    m_promptInput = new QLineEdit(this);
    m_promptInput->setPlaceholderText(i18n("Ask the agent about the active repository…"));
    connect(m_promptInput, &QLineEdit::returnPressed, this, &AgentPanel::sendPrompt);

    m_sendButton = new QPushButton(i18n("Send prompt"), this);
    connect(m_sendButton, &QPushButton::clicked, this, &AgentPanel::sendPrompt);

    layout->addWidget(titleLabel);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_conversationView, 1);
    layout->addWidget(m_promptInput);
    layout->addWidget(m_sendButton);
}

void AgentPanel::refreshCapabilityState()
{
    if (m_stateLabel == nullptr) {
        return;
    }

    if (m_cliCapabilityService == nullptr) {
        m_stateLabel->setText(i18n("Agent services are unavailable."));
        setPromptEnabled(false);
        return;
    }

    const qtcode::core::CliCapabilitiesSnapshot snapshot = m_cliCapabilityService->snapshot();
    if (snapshot.agentCli.available) {
        m_stateLabel->setText(
            i18n("Detected %1 (%2). Send a prompt once a repository is active.",
                 snapshot.agentCli.displayName,
                 snapshot.agentCli.version));
        return;
    }

    m_stateLabel->setText(snapshot.agentCli.unavailableMessage);
    setPromptEnabled(false);
}

void AgentPanel::sendPrompt()
{
    if (m_agentManager == nullptr || m_projectManager == nullptr || m_promptInput == nullptr) {
        return;
    }

    const QString prompt = m_promptInput->text().trimmed();
    if (prompt.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!ensureActiveSession(&errorMessage)) {
        m_stateLabel->setText(errorMessage);
        return;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        m_stateLabel->setText(i18n("Select an active repository before sending a prompt."));
        return;
    }

    qtcode::agents::AgentRequest request;
    request.sessionId = m_activeSessionId;
    request.projectId = activeProject.id;
    request.prompt = prompt;
    request.workingDirectory = activeProject.rootPath;
    request.nonInteractive = true;

    if (!m_agentManager->dispatchRequest(m_activeSessionId, request, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to dispatch agent prompt:" << errorMessage;
        m_stateLabel->setText(i18n("Could not send prompt: %1", errorMessage));
        return;
    }

    m_promptInput->clear();
    setPromptEnabled(false);
    refreshConversation();
}

void AgentPanel::onSessionUpdated(qtcode::agents::AgentSession *session)
{
    if (session == nullptr || session->id() != m_activeSessionId) {
        return;
    }

    refreshConversation();

    const bool running = session->status() == qtcode::agents::AgentSessionStatus::Running;
    setPromptEnabled(!running && m_projectManager != nullptr && m_projectManager->hasActiveProject());

    if (session->status() == qtcode::agents::AgentSessionStatus::Failed) {
        m_stateLabel->setText(i18n("The last agent request failed."));
    } else if (session->status() == qtcode::agents::AgentSessionStatus::Completed) {
        m_stateLabel->setText(i18n("Agent response completed."));
    }
}

void AgentPanel::onActiveProjectChanged()
{
    m_activeSessionId.clear();
    if (m_conversationView != nullptr) {
        m_conversationView->clear();
    }

    const bool canPrompt = m_projectManager != nullptr && m_projectManager->hasActiveProject()
        && m_cliCapabilityService != nullptr && m_cliCapabilityService->isAgentCliAvailable();
    setPromptEnabled(canPrompt);

    if (!canPrompt && m_projectManager != nullptr && !m_projectManager->hasActiveProject()) {
        m_stateLabel->setText(i18n("Select a repository to start an agent conversation."));
    } else {
        refreshCapabilityState();
    }
}

void AgentPanel::refreshConversation()
{
    if (m_conversationView == nullptr || m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        return;
    }

    qtcode::agents::AgentSession *session = m_agentManager->session(m_activeSessionId);
    if (session == nullptr) {
        return;
    }

    QStringList lines;
    for (const qtcode::agents::AgentMessage &message : session->messages()) {
        const QString roleLabel = message.role == QStringLiteral("user")
            ? i18n("You")
            : i18n("Agent");
        lines.append(QStringLiteral("%1: %2").arg(roleLabel, message.content));
    }

    m_conversationView->setPlainText(lines.join(QStringLiteral("\n\n")));
}

void AgentPanel::setPromptEnabled(bool enabled)
{
    if (m_promptInput != nullptr) {
        m_promptInput->setEnabled(enabled);
    }
    if (m_sendButton != nullptr) {
        m_sendButton->setEnabled(enabled);
    }
}

bool AgentPanel::ensureActiveSession(QString *errorMessage)
{
    if (m_agentManager == nullptr || m_projectManager == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Agent services are unavailable.");
        }
        return false;
    }

    if (!m_activeSessionId.isEmpty() && m_agentManager->session(m_activeSessionId) != nullptr) {
        return true;
    }

    if (!m_projectManager->hasActiveProject()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Select an active repository before sending a prompt.");
        }
        return false;
    }

    qtcode::agents::AgentSession *session = m_agentManager->createSession(
        m_projectManager->activeProjectId(),
        QStringLiteral("codex"),
        {},
        errorMessage);
    if (session == nullptr) {
        return false;
    }

    m_activeSessionId = session->id();
    return true;
}

} // namespace qtcode::ui

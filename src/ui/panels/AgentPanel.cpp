#include "ui/panels/AgentPanel.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentManager.h"
#include "agents/AgentSession.h"
#include "core/CliCapabilityModels.h"
#include "core/CliCapabilityService.h"
#include "core/ContextManager.h"
#include "memory/ContextResult.h"
#include "storage/repositories/ContextRetrievalRepository.h"
#include "core/ProjectManager.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "ui/views/ContextResultsView.h"
#include "ui/views/DiffReviewView.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTextCursor>
#include <QVBoxLayout>

#include <QFutureWatcher>
#include <QtConcurrent>

#include <algorithm>

namespace {

constexpr int kSessionIdRole = Qt::UserRole;

} // namespace

namespace qtcode::ui {

AgentPanel::AgentPanel(
    qtcode::core::CliCapabilityService *cliCapabilityService,
    qtcode::agents::AgentManager *agentManager,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::ContextManager *contextManager,
    QWidget *parent)
    : QWidget(parent)
    , m_cliCapabilityService(cliCapabilityService)
    , m_agentManager(agentManager)
    , m_projectManager(projectManager)
    , m_contextManager(contextManager)
{
    configureLayout();
    refreshCapabilityState();
    refreshAgentSelector();
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
            &qtcode::agents::AgentManager::adaptersChanged,
            this,
            &AgentPanel::refreshAgentSelector);
        connect(
            m_agentManager,
            &qtcode::agents::AgentManager::sessionsChanged,
            this,
            &AgentPanel::refreshSessionList);
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

    auto *agentSelectorLabel = new QLabel(i18n("Agent"), this);
    m_agentSelector = new QComboBox(this);
    m_agentSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    auto *sessionHeaderLayout = new QHBoxLayout();
    auto *sessionListLabel = new QLabel(i18n("Sessions"), this);
    m_newSessionButton = new QPushButton(i18n("New session"), this);
    sessionHeaderLayout->addWidget(sessionListLabel);
    sessionHeaderLayout->addStretch();
    sessionHeaderLayout->addWidget(m_newSessionButton);

    m_sessionList = new QListWidget(this);
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_contextResultsView = new ContextResultsView(this);

    m_conversationView = new QTextEdit(this);
    m_conversationView->setReadOnly(true);
    m_conversationView->setPlaceholderText(i18n("Agent responses will appear here."));

    m_promptInput = new QLineEdit(this);
    m_promptInput->setPlaceholderText(i18n("Ask the agent about the active repository…"));
    connect(m_promptInput, &QLineEdit::returnPressed, this, &AgentPanel::sendPrompt);

    m_sendButton = new QPushButton(i18n("Send prompt"), this);
    connect(m_sendButton, &QPushButton::clicked, this, &AgentPanel::sendPrompt);

    m_cancelButton = new QPushButton(i18n("Cancel request"), this);
    m_cancelButton->setVisible(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &AgentPanel::cancelActiveRequest);

    connect(m_newSessionButton, &QPushButton::clicked, this, &AgentPanel::createNewSession);
    connect(m_sessionList, &QListWidget::currentRowChanged, this, [this]() {
        onSessionListSelectionChanged();
    });

    layout->addWidget(titleLabel);
    layout->addWidget(agentSelectorLabel);
    layout->addWidget(m_agentSelector);
    layout->addLayout(sessionHeaderLayout);
    layout->addWidget(m_sessionList);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_contextResultsView);
    layout->addWidget(m_conversationView, 1);

    m_diffReviewView = new DiffReviewView(this);
    connect(
        m_diffReviewView,
        &DiffReviewView::approveRequested,
        this,
        &AgentPanel::approveSelectedArtifact);
    connect(
        m_diffReviewView,
        &DiffReviewView::rejectRequested,
        this,
        &AgentPanel::rejectSelectedArtifact);
    layout->addWidget(m_diffReviewView);

    layout->addWidget(m_promptInput);
    layout->addWidget(m_sendButton);
    layout->addWidget(m_cancelButton);
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
    if (!snapshot.agentCli.available) {
        m_stateLabel->setText(snapshot.agentCli.unavailableMessage);
        setPromptEnabled(false);
    }
}

void AgentPanel::refreshAgentSelector()
{
    if (m_agentSelector == nullptr || m_agentManager == nullptr) {
        return;
    }

    const QString previousAgentKey = selectedAgentKey();
    m_agentSelector->clear();

    for (qtcode::agents::AgentAdapter *adapter : m_agentManager->adapters()) {
        if (adapter == nullptr || !adapter->isAvailable()) {
            continue;
        }

        m_agentSelector->addItem(adapter->displayName(), adapter->agentKey());
    }

    m_agentSelector->setEnabled(m_agentSelector->count() > 0);

    if (m_agentSelector->count() == 0) {
        return;
    }

    int selectedIndex = 0;
    if (!previousAgentKey.isEmpty()) {
        selectedIndex = m_agentSelector->findData(previousAgentKey);
        if (selectedIndex < 0) {
            selectedIndex = 0;
        }
    }

    m_agentSelector->setCurrentIndex(selectedIndex);
}

void AgentPanel::refreshSessionList()
{
    if (m_sessionList == nullptr || m_agentManager == nullptr || m_projectManager == nullptr) {
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        m_sessionList->clear();
        return;
    }

    m_refreshingSessionList = true;
    const QSignalBlocker blocker(m_sessionList);
    m_sessionList->clear();

    QList<qtcode::agents::AgentSession *> projectSessions =
        m_agentManager->sessionsForProject(m_projectManager->activeProjectId());
    std::sort(
        projectSessions.begin(),
        projectSessions.end(),
        [](const qtcode::agents::AgentSession *left, const qtcode::agents::AgentSession *right) {
            if (left == nullptr || right == nullptr) {
                return left != nullptr;
            }
            return left->updatedAt() > right->updatedAt();
        });

    for (qtcode::agents::AgentSession *session : projectSessions) {
        if (session == nullptr) {
            continue;
        }

        auto *item = new QListWidgetItem(sessionListLabel(session), m_sessionList);
        item->setData(kSessionIdRole, session->id());
    }

    int selectedRow = -1;
    if (!m_activeSessionId.isEmpty()) {
        for (int row = 0; row < m_sessionList->count(); ++row) {
            if (m_sessionList->item(row)->data(kSessionIdRole).toString() == m_activeSessionId) {
                selectedRow = row;
                break;
            }
        }
    }

    if (selectedRow >= 0) {
        m_sessionList->setCurrentRow(selectedRow);
    } else if (m_sessionList->count() > 0) {
        m_sessionList->setCurrentRow(0);
        m_activeSessionId = m_sessionList->item(0)->data(kSessionIdRole).toString();
    } else {
        m_activeSessionId.clear();
    }

    m_refreshingSessionList = false;
    refreshConversation();
}

void AgentPanel::sendPrompt()
{
    if (m_agentManager == nullptr || m_projectManager == nullptr || m_promptInput == nullptr) {
        return;
    }

    if (m_contextRetrievalInFlight) {
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

    if (m_contextManager == nullptr) {
        dispatchPromptWithContext(prompt, activeProject, {});
        return;
    }

    if (m_hasReviewableContext
        && m_reviewablePrompt == prompt
        && m_contextResultsView != nullptr) {
        dispatchPromptWithContext(
            prompt,
            activeProject,
            m_contextResultsView->attachedContextExcerpts());
        m_hasReviewableContext = false;
        m_reviewablePrompt.clear();
        return;
    }

    m_pendingPrompt = prompt;
    m_contextRetrievalInFlight = true;
    setPromptEnabled(false);
    m_stateLabel->setText(i18n("Retrieving project memory…"));

    auto *watcher = new QFutureWatcher<qtcode::core::ContextRetrievalOutcome>(this);
    connect(watcher, &QFutureWatcher<qtcode::core::ContextRetrievalOutcome>::finished, this, [this, watcher, activeProject]() {
        const qtcode::core::ContextRetrievalOutcome contextOutcome = watcher->result();
        m_lastRetrievalQuery = contextOutcome.retrievalQuery;
        m_lastTotalResultCount = contextOutcome.results.size();
        m_lastMemoryUnavailable = contextOutcome.memoryUnavailable;
        m_lastRetrievalStatusMessage = contextOutcome.statusMessage;

        if (m_contextResultsView != nullptr) {
            m_contextResultsView->setRetrievalOutcome(contextOutcome);
        }

        const QString prompt = m_pendingPrompt;
        m_pendingPrompt.clear();
        m_contextRetrievalInFlight = false;

        if (contextOutcome.results.isEmpty()) {
            dispatchPromptWithContext(
                prompt,
                activeProject,
                {},
                contextOutcome.statusMessage,
                contextOutcome.memoryUnavailable);
        } else {
            m_hasReviewableContext = true;
            m_reviewablePrompt = prompt;
            m_stateLabel->setText(
                i18n("Review retrieved context, attach or detach results, then send again."));
            setPromptEnabled(true);
        }

        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run(
        [contextManager = m_contextManager, prompt, activeProject]() {
            return contextManager->retrieveForAgentRequest(prompt, activeProject);
        }));
}

void AgentPanel::dispatchPromptWithContext(
    const QString &prompt,
    const settings::ProjectRecord &project,
    const QStringList &contextExcerpts,
    const QString &statusMessage,
    bool memoryUnavailable)
{
    if (m_agentManager == nullptr || prompt.trimmed().isEmpty()) {
        setPromptEnabled(true);
        return;
    }

    qtcode::agents::AgentRequest request;
    request.sessionId = m_activeSessionId;
    request.projectId = project.id;
    request.prompt = prompt;
    request.workingDirectory = project.rootPath;
    request.contextExcerpts = contextExcerpts;
    request.nonInteractive = true;

    QString errorMessage;
    if (!m_agentManager->dispatchRequest(m_activeSessionId, request, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to dispatch agent prompt:" << errorMessage;
        m_stateLabel->setText(i18n("Could not send prompt: %1", errorMessage));
        setPromptEnabled(true);
        return;
    }

    const QList<memory::ContextResult> attachedResults =
        m_contextResultsView != nullptr ? m_contextResultsView->attachedResults() : QList<memory::ContextResult> {};
    const QString retrievalQuery = m_lastRetrievalQuery.isEmpty()
        ? qtcode::core::ContextManager::buildRetrievalQuery(prompt, project)
        : m_lastRetrievalQuery;
    QString persistError;
    if (!m_agentManager->persistContextRetrievalMetadata(
            m_activeSessionId,
            project.id,
            retrievalQuery,
            QStringLiteral("qtcode-memory"),
            attachedResults,
            m_lastTotalResultCount > 0 ? m_lastTotalResultCount : attachedResults.size(),
            memoryUnavailable,
            statusMessage.isEmpty() ? m_lastRetrievalStatusMessage : statusMessage,
            &persistError)) {
        qCWarning(qtcodeUi) << "Failed to persist context retrieval metadata:" << persistError;
    } else {
        refreshSavedContextRetrieval();
    }

    if (memoryUnavailable) {
        m_stateLabel->setText(
            i18n("Sent prompt without project memory: %1", statusMessage));
    } else if (contextExcerpts.isEmpty() && !statusMessage.isEmpty()) {
        m_stateLabel->setText(statusMessage);
    } else if (!contextExcerpts.isEmpty()) {
        m_stateLabel->setText(
            i18n("Sent prompt with %1 attached context result(s).", contextExcerpts.size()));
    }

    if (m_promptInput != nullptr) {
        m_promptInput->clear();
    }
    setPromptEnabled(false);
    refreshConversation();
}

void AgentPanel::createNewSession()
{
    if (m_agentManager == nullptr || m_projectManager == nullptr) {
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        m_stateLabel->setText(i18n("Select a repository before creating a session."));
        return;
    }

    if (selectedAgentKey().isEmpty()) {
        m_stateLabel->setText(i18n("No agent is available for a new session."));
        return;
    }

    QString errorMessage;
    qtcode::agents::AgentSession *session = m_agentManager->createSession(
        m_projectManager->activeProjectId(),
        selectedAgentKey(),
        {},
        &errorMessage);
    if (session == nullptr) {
        m_stateLabel->setText(
            errorMessage.isEmpty() ? i18n("Could not create a new session.") : errorMessage);
        return;
    }

    selectSession(session->id());
}

void AgentPanel::onSessionListSelectionChanged()
{
    if (m_refreshingSessionList || m_sessionList == nullptr) {
        return;
    }

    QListWidgetItem *currentItem = m_sessionList->currentItem();
    if (currentItem == nullptr) {
        m_activeSessionId.clear();
        refreshConversation();
        setPromptEnabled(false);
        if (m_cancelButton != nullptr) {
            m_cancelButton->setVisible(false);
            m_cancelButton->setEnabled(false);
        }
        return;
    }

    selectSession(currentItem->data(kSessionIdRole).toString());
    refreshDiffReview();
}

void AgentPanel::onSessionUpdated(qtcode::agents::AgentSession *session)
{
    if (session == nullptr) {
        return;
    }

    if (m_projectManager != nullptr
        && session->projectId() != m_projectManager->activeProjectId()) {
        return;
    }

    if (m_sessionList != nullptr) {
        for (int row = 0; row < m_sessionList->count(); ++row) {
            QListWidgetItem *item = m_sessionList->item(row);
            if (item != nullptr && item->data(kSessionIdRole).toString() == session->id()) {
                item->setText(sessionListLabel(session));
                break;
            }
        }
    }

    if (session->id() != m_activeSessionId) {
        return;
    }

    refreshConversation();
    refreshDiffReview();

    const bool running = session->status() == qtcode::agents::AgentSessionStatus::Running;
    setPromptEnabled(!running && m_projectManager != nullptr && m_projectManager->hasActiveProject());
    updateSessionStatusDisplay(session);
    updateRequestControls(session);
}

void AgentPanel::cancelActiveRequest()
{
    if (m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_agentManager->cancelRequest(m_activeSessionId, &errorMessage)) {
        m_stateLabel->setText(
            errorMessage.isEmpty() ? i18n("Could not cancel the active agent request.") : errorMessage);
    }
}

void AgentPanel::onActiveProjectChanged()
{
    m_activeSessionId.clear();
    if (m_conversationView != nullptr) {
        m_conversationView->clear();
    }

    refreshSessionList();

    const bool canPrompt = m_projectManager != nullptr && m_projectManager->hasActiveProject()
        && m_agentManager != nullptr && !selectedAgentKey().isEmpty();
    setPromptEnabled(canPrompt);

    if (m_projectManager != nullptr && !m_projectManager->hasActiveProject()) {
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
        m_conversationView->clear();
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
    m_conversationView->moveCursor(QTextCursor::End);
}

void AgentPanel::refreshSavedContextRetrieval()
{
    if (m_contextResultsView == nullptr || m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        if (m_contextResultsView != nullptr) {
            m_contextResultsView->clearResults();
        }
        return;
    }

    storage::PersistedContextRetrieval retrieval;
    QList<storage::PersistedContextResult> results;
    QString loadError;
    if (!m_agentManager->latestContextRetrievalForSession(
            m_activeSessionId,
            &retrieval,
            &results,
            &loadError)) {
        qCWarning(qtcodeUi) << "Failed to load saved context retrieval:" << loadError;
        return;
    }

    if (retrieval.id.isEmpty()) {
        m_contextResultsView->clearResults();
        return;
    }

    m_contextResultsView->setPersistedRetrieval(retrieval, results);
}

void AgentPanel::refreshDiffReview()
{
    if (m_diffReviewView == nullptr || m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        if (m_diffReviewView != nullptr) {
            m_diffReviewView->clearSession();
        }
        return;
    }

    m_diffReviewView->setSession(m_agentManager->session(m_activeSessionId));
}

void AgentPanel::approveSelectedArtifact(const QString &artifactId)
{
    if (m_agentManager == nullptr || m_projectManager == nullptr || m_activeSessionId.isEmpty()) {
        return;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        m_stateLabel->setText(i18n("Select an active repository before approving changes."));
        return;
    }

    QString errorMessage;
    if (!m_agentManager->approveArtifact(
            m_activeSessionId,
            artifactId,
            activeProject.rootPath,
            &errorMessage)) {
        m_stateLabel->setText(
            errorMessage.isEmpty() ? i18n("Could not approve the selected change.") : errorMessage);
        return;
    }

    refreshDiffReview();
}

void AgentPanel::rejectSelectedArtifact(const QString &artifactId)
{
    if (m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_agentManager->rejectArtifact(m_activeSessionId, artifactId, &errorMessage)) {
        m_stateLabel->setText(
            errorMessage.isEmpty() ? i18n("Could not reject the selected change.") : errorMessage);
        return;
    }

    refreshDiffReview();
}

void AgentPanel::updateSessionStatusDisplay(const qtcode::agents::AgentSession *session)
{
    if (m_stateLabel == nullptr || session == nullptr) {
        return;
    }

    switch (session->status()) {
    case qtcode::agents::AgentSessionStatus::Running: {
        const QString statusUpdate = session->lastStatusUpdate().trimmed();
        if (statusUpdate.isEmpty()) {
            m_stateLabel->setText(i18n("Agent request is running…"));
        } else {
            m_stateLabel->setText(i18n("Agent request is running: %1", statusUpdate));
        }
        break;
    }
    case qtcode::agents::AgentSessionStatus::Completed:
        m_stateLabel->setText(i18n("Agent response completed."));
        break;
    case qtcode::agents::AgentSessionStatus::Failed: {
        const QString errorMessage = session->lastErrorMessage().trimmed();
        if (errorMessage.isEmpty()) {
            m_stateLabel->setText(i18n("The last agent request failed. You can send another prompt."));
        } else {
            m_stateLabel->setText(i18n("The last agent request failed: %1", errorMessage));
        }
        break;
    }
    case qtcode::agents::AgentSessionStatus::Canceled:
        m_stateLabel->setText(i18n("The last agent request was canceled."));
        break;
    case qtcode::agents::AgentSessionStatus::Idle:
    default:
        m_stateLabel->setText(i18n("Ready for the next prompt."));
        break;
    }
}

void AgentPanel::updateRequestControls(const qtcode::agents::AgentSession *session)
{
    const bool running = session != nullptr
        && session->status() == qtcode::agents::AgentSessionStatus::Running;
    if (m_cancelButton != nullptr) {
        m_cancelButton->setVisible(running);
        m_cancelButton->setEnabled(running);
    }
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

void AgentPanel::selectSession(const QString &sessionId)
{
    m_activeSessionId = sessionId;

    qtcode::agents::AgentSession *session = m_agentManager != nullptr
        ? m_agentManager->session(sessionId)
        : nullptr;
    if (session == nullptr) {
        refreshConversation();
        setPromptEnabled(false);
        return;
    }

    refreshConversation();
    updateSessionStatusDisplay(session);
    updateRequestControls(session);
    refreshSavedContextRetrieval();

    const bool running = session->status() == qtcode::agents::AgentSessionStatus::Running;
    setPromptEnabled(
        !running && m_projectManager != nullptr && m_projectManager->hasActiveProject());
}

QString AgentPanel::selectedAgentKey() const
{
    if (m_agentSelector == nullptr || m_agentSelector->currentIndex() < 0) {
        return {};
    }

    return m_agentSelector->currentData().toString();
}

QString AgentPanel::sessionListLabel(const qtcode::agents::AgentSession *session)
{
    if (session == nullptr) {
        return {};
    }

    const QString statusLabel = qtcode::agents::agentSessionStatusLabel(session->status());
    if (session->title().trimmed().isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(session->agentKey(), statusLabel);
    }

    return QStringLiteral("%1 (%2)").arg(session->title(), statusLabel);
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

    if (selectedAgentKey().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("No agent is available for this request.");
        }
        return false;
    }

    qtcode::agents::AgentSession *session = m_agentManager->createSession(
        m_projectManager->activeProjectId(),
        selectedAgentKey(),
        {},
        errorMessage);
    if (session == nullptr) {
        return false;
    }

    selectSession(session->id());
    refreshSessionList();
    return true;
}

} // namespace qtcode::ui

#include "ui/panels/AgentPanel.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentManager.h"
#include "agents/AgentOptions.h"
#include "agents/AgentSession.h"
#include "core/CliCapabilityModels.h"
#include "core/CliCapabilityService.h"
#include "core/ContextManager.h"
#include "memory/ContextResult.h"
#include "storage/repositories/ContextRetrievalRepository.h"
#include "core/ProjectManager.h"
#include "core/RepoConfigLoader.h"
#include "core/RepoConfigWriter.h"
#include "core/StatusService.h"
#include "github/GitHubContextFormatting.h"
#include "github/GitHubModels.h"
#include "settings/ProjectModels.h"
#include "settings/RepoConfig.h"
#include "shared/Logging.h"
#include "ui/views/ContextResultsView.h"
#include "ui/views/ConversationView.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
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
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QWidget(parent)
    , m_cliCapabilityService(cliCapabilityService)
    , m_agentManager(agentManager)
    , m_projectManager(projectManager)
    , m_contextManager(contextManager)
    , m_statusService(statusService)
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

QWidget *AgentPanel::sessionPanel() const
{
    return m_sessionPanel;
}

QWidget *AgentPanel::conversationPanel() const
{
    return m_conversationPanel;
}

QWidget *AgentPanel::contextPanel() const
{
    return m_contextPanel;
}

ContextResultsView *AgentPanel::contextResultsView() const
{
    return m_contextResultsView;
}

void AgentPanel::configureLayout()
{
    m_sessionPanel = new QWidget(this);
    auto *sessionLayout = new QVBoxLayout(m_sessionPanel);
    sessionLayout->setContentsMargins(12, 12, 12, 12);
    sessionLayout->setSpacing(8);

    auto *sessionTitleLabel = new QLabel(i18n("Agent Sessions"), m_sessionPanel);
    QFont sessionTitleFont = sessionTitleLabel->font();
    sessionTitleFont.setBold(true);
    sessionTitleLabel->setFont(sessionTitleFont);

    auto *agentSelectorLabel = new QLabel(i18n("Agent"), m_sessionPanel);
    m_agentSelector = new QComboBox(m_sessionPanel);
    m_agentSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    auto *sessionHeaderLayout = new QHBoxLayout();
    auto *sessionListLabel = new QLabel(i18n("Sessions"), m_sessionPanel);
    m_newSessionButton = new QPushButton(i18n("New session"), m_sessionPanel);
    sessionHeaderLayout->addWidget(sessionListLabel);
    sessionHeaderLayout->addStretch();
    sessionHeaderLayout->addWidget(m_newSessionButton);

    m_sessionList = new QListWidget(m_sessionPanel);
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_newSessionButton, &QPushButton::clicked, this, &AgentPanel::createNewSession);
    connect(m_sessionList, &QListWidget::currentRowChanged, this, [this]() {
        onSessionListSelectionChanged();
    });
    connect(
        m_sessionList,
        &QListWidget::customContextMenuRequested,
        this,
        &AgentPanel::showSessionContextMenu);

    sessionLayout->addWidget(sessionTitleLabel);
    sessionLayout->addWidget(agentSelectorLabel);
    sessionLayout->addWidget(m_agentSelector);
    sessionLayout->addLayout(sessionHeaderLayout);
    sessionLayout->addWidget(m_sessionList, 1);

    m_conversationPanel = new QWidget(this);
    auto *conversationLayout = new QVBoxLayout(m_conversationPanel);
    conversationLayout->setContentsMargins(12, 12, 12, 12);
    conversationLayout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("AI Chat"), m_conversationPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_conversationView = new ConversationView(m_conversationPanel);

    m_promptInput = new QPlainTextEdit(m_conversationPanel);
    m_promptInput->setPlaceholderText(i18n("Ask the agent about the active repository…"));
    m_promptInput->setMinimumHeight(80);
    m_promptInput->setMaximumHeight(160);
    m_promptInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_promptInput->setTabChangesFocus(true);
    m_promptInput->installEventFilter(this);
    connect(m_promptInput, &QPlainTextEdit::textChanged, this, &AgentPanel::refreshComposerControls);

    m_sendButton = new QPushButton(m_conversationPanel);
    m_sendButton->setToolTip(i18n("Send prompt"));
    const QIcon sendIcon = QIcon::fromTheme(QStringLiteral("document-send"));
    if (!sendIcon.isNull()) {
        m_sendButton->setIcon(sendIcon);
    } else {
        m_sendButton->setText(i18n("Send"));
    }
    m_sendButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_sendButton, &QPushButton::clicked, this, &AgentPanel::onComposerActionClicked);

    m_modelSelector = new QComboBox(m_conversationPanel);
    m_modelSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_modelSelector->setToolTip(i18n("Model for the active agent session"));
    connect(m_modelSelector, &QComboBox::currentIndexChanged, this, &AgentPanel::onRequestOptionsChanged);

    m_executionModeSelector = new QComboBox(m_conversationPanel);
    m_executionModeSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_executionModeSelector->setToolTip(i18n("Execution mode for the active agent session"));
    connect(
        m_executionModeSelector,
        &QComboBox::currentIndexChanged,
        this,
        &AgentPanel::onRequestOptionsChanged);

    auto *composerControlsLayout = new QHBoxLayout();
    composerControlsLayout->setSpacing(8);
    composerControlsLayout->addWidget(m_modelSelector, 0);
    composerControlsLayout->addWidget(m_executionModeSelector, 0);
    composerControlsLayout->addStretch(1);
    composerControlsLayout->addWidget(m_sendButton, 0, Qt::AlignRight);

    conversationLayout->addWidget(titleLabel);
    conversationLayout->addWidget(m_conversationView, 1);
    conversationLayout->addWidget(m_promptInput);
    conversationLayout->addLayout(composerControlsLayout);

    m_contextPanel = new QWidget(this);
    auto *contextLayout = new QVBoxLayout(m_contextPanel);
    contextLayout->setContentsMargins(12, 12, 12, 12);
    contextLayout->setSpacing(8);

    auto *contextTitleLabel = new QLabel(i18n("Retrieved Context"), m_contextPanel);
    QFont contextTitleFont = contextTitleLabel->font();
    contextTitleFont.setBold(true);
    contextTitleLabel->setFont(contextTitleFont);

    m_contextResultsView = new ContextResultsView(m_projectManager, m_contextPanel);
    contextLayout->addWidget(contextTitleLabel);
    contextLayout->addWidget(m_contextResultsView, 1);

    setVisible(false);
}

void AgentPanel::showStatus(const QString &text, qtcode::core::StatusSeverity severity)
{
    if (m_statusService == nullptr || text.trimmed().isEmpty()) {
        return;
    }

    if (severity == qtcode::core::StatusSeverity::Progress) {
        m_statusService->showProgress(text);
        return;
    }

    m_statusService->showMessage(text, severity);
}

void AgentPanel::refreshCapabilityState()
{
    if (m_cliCapabilityService == nullptr) {
        showStatus(i18n("Agent services are unavailable."), qtcode::core::StatusSeverity::Warning);
        syncPromptComposerState();
        return;
    }

    if (!m_cliCapabilityService->isDetectionRunning()) {
        const qtcode::core::CliCapabilitiesSnapshot snapshot = m_cliCapabilityService->snapshot();
        if (!snapshot.agentCli.available) {
            showStatus(snapshot.agentCli.unavailableMessage, qtcode::core::StatusSeverity::Warning);
        }
    }

    syncPromptComposerState();
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

    QString preferredAgentKey = previousAgentKey;
    if (preferredAgentKey.isEmpty()) {
        preferredAgentKey = preferredAgentKeyForProject();
    }

    int selectedIndex = 0;
    if (!preferredAgentKey.isEmpty()) {
        selectedIndex = m_agentSelector->findData(preferredAgentKey);
        if (selectedIndex < 0) {
            selectedIndex = 0;
        }
    }

    m_agentSelector->setCurrentIndex(selectedIndex);
}

void AgentPanel::reloadAgentSelector()
{
    refreshAgentSelector();
}

QString AgentPanel::preferredAgentKeyForProject() const
{
    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        return {};
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return {};
    }

    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(activeProject.rootPath);
    if (!repoConfig.hasDefaultAgentKey()) {
        return {};
    }

    return repoConfig.defaultAgentKey.trimmed();
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

    const QString projectId = m_projectManager->activeProjectId();
    if (m_activeSessionId.isEmpty()) {
        m_activeSessionId = m_agentManager->activeSessionIdForProject(projectId);
    }

    m_refreshingSessionList = true;
    const QSignalBlocker blocker(m_sessionList);
    m_sessionList->clear();

    QList<qtcode::agents::AgentSession *> projectSessions =
        m_agentManager->sessionsForProject(projectId);
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

    QString sessionIdToSelect;
    if (selectedRow >= 0) {
        m_sessionList->setCurrentRow(selectedRow);
        sessionIdToSelect = m_activeSessionId;
    } else if (m_sessionList->count() > 0) {
        m_sessionList->setCurrentRow(0);
        sessionIdToSelect = m_sessionList->item(0)->data(kSessionIdRole).toString();
    }

    m_refreshingSessionList = false;

    if (!sessionIdToSelect.isEmpty()) {
        selectSession(sessionIdToSelect);
        return;
    }

    m_activeSessionId.clear();
    refreshConversation();
    refreshRequestOptionSelectors();
    syncPromptComposerState();
}

void AgentPanel::sendPrompt()
{
    if (m_agentManager == nullptr || m_projectManager == nullptr || m_promptInput == nullptr) {
        return;
    }

    if (m_contextRetrievalInFlight) {
        return;
    }

    const QString prompt = m_promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!ensureActiveSession(&errorMessage)) {
        showStatus(errorMessage, qtcode::core::StatusSeverity::Warning);
        return;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        showStatus(i18n("Select an active repository before sending a prompt."), qtcode::core::StatusSeverity::Warning);
        return;
    }

    if (m_contextManager == nullptr) {
        dispatchPromptWithContext(prompt, activeProject, {});
        return;
    }

    m_pendingPrompt = prompt;
    m_contextRetrievalInFlight = true;
    refreshComposerControls();
    showStatus(i18n("Retrieving project memory…"), qtcode::core::StatusSeverity::Progress);

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

        const QStringList attachedExcerpts = m_contextResultsView != nullptr
            ? m_contextResultsView->attachedContextExcerpts()
            : QStringList {};
        dispatchPromptWithContext(
            prompt,
            activeProject,
            attachedExcerpts,
            contextOutcome.statusMessage,
            contextOutcome.memoryUnavailable);

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
    QStringList mergedContextExcerpts = m_attachedGitHubContextExcerpts;
    mergedContextExcerpts.append(contextExcerpts);
    request.contextExcerpts = mergedContextExcerpts;
    request.nonInteractive = true;
    request.modelKey = selectedModelKey();
    request.executionModeKey = selectedExecutionModeKey();

    QString errorMessage;
    if (!m_agentManager->dispatchRequest(m_activeSessionId, request, &errorMessage)) {
        qCWarning(qtcodeUi) << "Failed to dispatch agent prompt:" << errorMessage;
        showStatus(i18n("Could not send prompt: %1", errorMessage), qtcode::core::StatusSeverity::Error);
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
    }

    if (memoryUnavailable) {
        showStatus(
            i18n("Sent prompt without project memory: %1", statusMessage),
            qtcode::core::StatusSeverity::Warning);
    } else if (mergedContextExcerpts.isEmpty() && !statusMessage.isEmpty()) {
        showStatus(statusMessage);
    } else if (!mergedContextExcerpts.isEmpty()) {
        showStatus(i18n("Sent prompt with %1 attached context excerpt(s).", mergedContextExcerpts.size()));
    }

    m_attachedGitHubContextExcerpts.clear();
    m_attachedPullRequestNumber = 0;

    if (m_promptInput != nullptr) {
        m_promptInput->clear();
    }
    setPromptEnabled(false);
    refreshConversation();
}

void AgentPanel::attachIssueContext(const qtcode::github::GitHubIssueDetail &detail)
{
    m_attachedGitHubContextExcerpts = {qtcode::github::formatIssueContextExcerpt(detail)};
    m_attachedPullRequestNumber = 0;
    showStatus(i18n("Attached GitHub issue #%1 to the next agent prompt.", detail.number));
}

void AgentPanel::attachPullRequestContext(const qtcode::github::GitHubPullRequestDetail &detail)
{
    m_attachedGitHubContextExcerpts = {
        qtcode::github::formatPullRequestContextExcerpt(detail)};
    m_attachedPullRequestNumber = detail.number;
    showStatus(
        i18n("Attached GitHub pull request #%1 to the next agent prompt.", detail.number));
}

void AgentPanel::attachFileContext(const QString &absolutePath, const QString &contentOverride)
{
    if (m_contextResultsView == nullptr) {
        return;
    }

    QString errorMessage;
    if (!m_contextResultsView->addFileContext(absolutePath, contentOverride, &errorMessage)) {
        showStatus(
            errorMessage.isEmpty() ? i18n("Could not add the file to context.") : errorMessage,
            qtcode::core::StatusSeverity::Error);
        return;
    }

    showStatus(i18n("Added %1 to retrieved context.", QFileInfo(absolutePath).fileName()));
}

void AgentPanel::createNewSession()
{
    if (m_agentManager == nullptr || m_projectManager == nullptr) {
        return;
    }

    if (!m_projectManager->hasActiveProject()) {
        showStatus(i18n("Select a repository before creating a session."), qtcode::core::StatusSeverity::Warning);
        return;
    }

    if (selectedAgentKey().isEmpty()) {
        showStatus(i18n("No agent is available for a new session."), qtcode::core::StatusSeverity::Warning);
        return;
    }

    QString errorMessage;
    qtcode::agents::AgentSession *session = m_agentManager->createSession(
        m_projectManager->activeProjectId(),
        selectedAgentKey(),
        {},
        &errorMessage);
    if (session == nullptr) {
        showStatus(
            errorMessage.isEmpty() ? i18n("Could not create a new session.") : errorMessage,
            qtcode::core::StatusSeverity::Error);
        return;
    }

    selectSession(session->id());
}

void AgentPanel::showSessionContextMenu(const QPoint &position)
{
    if (m_sessionList == nullptr) {
        return;
    }

    QListWidgetItem *item = m_sessionList->itemAt(position);
    if (item == nullptr) {
        return;
    }

    if (!m_sessionList->selectedItems().contains(item)) {
        m_sessionList->setCurrentItem(item);
    }

    const QString sessionId = item->data(kSessionIdRole).toString();
    if (sessionId.isEmpty()) {
        return;
    }

    const qtcode::agents::AgentSession *session =
        m_agentManager != nullptr ? m_agentManager->session(sessionId) : nullptr;
    const bool running =
        session != nullptr
        && session->status() == qtcode::agents::AgentSessionStatus::Running;

    QMenu menu(this);
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("list-add")),
        i18n("New session"),
        this,
        &AgentPanel::createNewSession);

    menu.addSeparator();

    QAction *removeAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("list-remove")),
        i18n("Remove session"),
        this,
        &AgentPanel::removeSelectedSession);
    removeAction->setEnabled(!running);
    if (running) {
        removeAction->setToolTip(i18n("Cancel the running request before removing this session."));
    }

    menu.exec(m_sessionList->viewport()->mapToGlobal(position));
}

void AgentPanel::removeSelectedSession()
{
    if (m_sessionList == nullptr || m_agentManager == nullptr) {
        return;
    }

    QListWidgetItem *currentItem = m_sessionList->currentItem();
    if (currentItem == nullptr) {
        return;
    }

    const QString sessionId = currentItem->data(kSessionIdRole).toString();
    if (sessionId.isEmpty()) {
        return;
    }

    const qtcode::agents::AgentSession *session = m_agentManager->session(sessionId);
    const QString sessionLabel = session != nullptr ? sessionListLabel(session) : currentItem->text();

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        i18n("Remove session"),
        i18n("Remove the agent session \"%1\" and its conversation history?", sessionLabel),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_agentManager->deleteSession(sessionId, &errorMessage)) {
        showStatus(
            errorMessage.isEmpty() ? i18n("Could not remove the agent session.") : errorMessage,
            qtcode::core::StatusSeverity::Error);
        return;
    }

    if (sessionId == m_activeSessionId) {
        m_activeSessionId.clear();
        if (m_conversationView != nullptr) {
            m_conversationView->clearConversation();
        }
        if (m_contextResultsView != nullptr) {
            m_contextResultsView->clearResults();
        }
    }

    showStatus(i18n("Agent session removed."));
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
        refreshRequestOptionSelectors();
        setPromptEnabled(false);
        refreshComposerControls();
        return;
    }

    selectSession(currentItem->data(kSessionIdRole).toString());
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

    syncPromptComposerState();
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
        showStatus(
            errorMessage.isEmpty() ? i18n("Could not cancel the active agent request.") : errorMessage,
            qtcode::core::StatusSeverity::Error);
    }
}

void AgentPanel::onActiveProjectChanged()
{
    m_activeSessionId.clear();
    if (m_agentManager != nullptr && m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
        m_activeSessionId = m_agentManager->activeSessionIdForProject(m_projectManager->activeProjectId());
    }

    if (m_conversationView != nullptr) {
        m_conversationView->clearConversation();
    }

    refreshAgentSelector();
    refreshSessionList();

    if (m_projectManager != nullptr && m_projectManager->hasActiveProject() && m_activeSessionId.isEmpty()) {
        QString errorMessage;
        if (!ensureActiveSession(&errorMessage)) {
            qCWarning(qtcodeUi) << "Failed to ensure default agent session:" << errorMessage;
        }
    }

    if (m_projectManager != nullptr && !m_projectManager->hasActiveProject()) {
        setPromptEnabled(false);
        showStatus(i18n("Select a repository to start an agent conversation."), qtcode::core::StatusSeverity::Warning);
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
        m_conversationView->clearConversation();
        return;
    }

    m_conversationView->syncMessages(
        session->messages(),
        session->status() == qtcode::agents::AgentSessionStatus::Running);
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

void AgentPanel::updateSessionStatusDisplay(const qtcode::agents::AgentSession *session)
{
    if (session == nullptr) {
        return;
    }

    switch (session->status()) {
    case qtcode::agents::AgentSessionStatus::Running: {
        const QString statusUpdate = session->lastStatusUpdate().trimmed();
        if (statusUpdate.isEmpty()) {
            showStatus(i18n("Agent request is running…"), qtcode::core::StatusSeverity::Progress);
        } else {
            showStatus(
                i18n("Agent request is running: %1", statusUpdate),
                qtcode::core::StatusSeverity::Progress);
        }
        break;
    }
    case qtcode::agents::AgentSessionStatus::Completed:
        showStatus(i18n("Agent response completed."));
        break;
    case qtcode::agents::AgentSessionStatus::Failed: {
        const QString errorMessage = session->lastErrorMessage().trimmed();
        if (errorMessage.isEmpty()) {
            showStatus(
                i18n("The last agent request failed. You can send another prompt."),
                qtcode::core::StatusSeverity::Error);
        } else {
            showStatus(
                i18n("The last agent request failed: %1", errorMessage),
                qtcode::core::StatusSeverity::Error);
        }
        break;
    }
    case qtcode::agents::AgentSessionStatus::Canceled:
        showStatus(i18n("The last agent request was canceled."), qtcode::core::StatusSeverity::Warning);
        break;
    case qtcode::agents::AgentSessionStatus::Idle:
    default:
        showStatus(i18n("Ready for the next prompt."));
        break;
    }
}

void AgentPanel::updateRequestControls(const qtcode::agents::AgentSession *session)
{
    Q_UNUSED(session);
    refreshComposerControls();
}

void AgentPanel::setPromptEnabled(bool enabled)
{
    m_promptComposerEnabled = enabled;
    refreshComposerControls();
}

void AgentPanel::syncPromptComposerState()
{
    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        setPromptEnabled(false);
        return;
    }

    if (m_cliCapabilityService != nullptr
        && !m_cliCapabilityService->isDetectionRunning()
        && !m_cliCapabilityService->isAgentCliAvailable()) {
        setPromptEnabled(false);
        return;
    }

    if (m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        setPromptEnabled(false);
        return;
    }

    const qtcode::agents::AgentSession *session = m_agentManager->session(m_activeSessionId);
    if (session == nullptr) {
        setPromptEnabled(false);
        return;
    }

    const bool running = session->status() == qtcode::agents::AgentSessionStatus::Running;
    setPromptEnabled(!running);
}

bool AgentPanel::isActiveRequestRunning() const
{
    if (m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        return false;
    }

    const qtcode::agents::AgentSession *session = m_agentManager->session(m_activeSessionId);
    return session != nullptr
        && session->status() == qtcode::agents::AgentSessionStatus::Running;
}

void AgentPanel::onComposerActionClicked()
{
    if (isActiveRequestRunning()) {
        cancelActiveRequest();
        return;
    }

    sendPrompt();
}

void AgentPanel::refreshComposerControls()
{
    const bool running = isActiveRequestRunning();
    const bool hasText = m_promptInput != nullptr
        && !m_promptInput->toPlainText().trimmed().isEmpty();
    const bool inputEnabled = m_promptComposerEnabled && !m_contextRetrievalInFlight && !running;
    const bool sendEnabled = inputEnabled && hasText;
    const bool selectorsEnabled = inputEnabled;

    if (m_promptInput != nullptr) {
        m_promptInput->setEnabled(inputEnabled);
    }
    if (m_modelSelector != nullptr) {
        m_modelSelector->setEnabled(selectorsEnabled && m_modelSelector->count() > 0);
    }
    if (m_executionModeSelector != nullptr) {
        m_executionModeSelector->setEnabled(
            selectorsEnabled && m_executionModeSelector->count() > 0);
    }
    if (m_sendButton == nullptr) {
        return;
    }

    if (running) {
        const QIcon cancelIcon = QIcon::fromTheme(QStringLiteral("process-stop"));
        if (!cancelIcon.isNull()) {
            m_sendButton->setIcon(cancelIcon);
            m_sendButton->setText({});
        } else {
            m_sendButton->setIcon(QIcon());
            m_sendButton->setText(i18n("Cancel"));
        }
        m_sendButton->setToolTip(i18n("Cancel request"));
        m_sendButton->setEnabled(true);
        return;
    }

    const QIcon sendIcon = QIcon::fromTheme(QStringLiteral("document-send"));
    if (!sendIcon.isNull()) {
        m_sendButton->setIcon(sendIcon);
        m_sendButton->setText({});
    } else {
        m_sendButton->setIcon(QIcon());
        m_sendButton->setText(i18n("Send"));
    }
    m_sendButton->setToolTip(i18n("Send prompt"));
    m_sendButton->setEnabled(sendEnabled);
}

bool AgentPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_promptInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            if (m_sendButton != nullptr && m_sendButton->isEnabled() && !isActiveRequestRunning()) {
                sendPrompt();
            }
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void AgentPanel::selectSession(const QString &sessionId)
{
    m_activeSessionId = sessionId;

    qtcode::agents::AgentSession *session = m_agentManager != nullptr
        ? m_agentManager->session(sessionId)
        : nullptr;
    if (session == nullptr) {
        refreshConversation();
        refreshRequestOptionSelectors();
        syncPromptComposerState();
        return;
    }

    refreshConversation();
    updateSessionStatusDisplay(session);
    updateRequestControls(session);
    refreshSavedContextRetrieval();
    refreshRequestOptionSelectors();

    if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
        QString persistError;
        if (!m_agentManager->persistActiveSessionForProject(
                m_projectManager->activeProjectId(),
                sessionId,
                &persistError)) {
            qCWarning(qtcodeUi) << "Failed to persist active agent session:" << persistError;
        }
    }

    syncPromptComposerState();
}

QString AgentPanel::selectedAgentKey() const
{
    if (m_agentSelector == nullptr || m_agentSelector->currentIndex() < 0) {
        return {};
    }

    return m_agentSelector->currentData().toString();
}

QString AgentPanel::selectedModelKey() const
{
    if (m_modelSelector == nullptr || m_modelSelector->currentIndex() < 0) {
        return {};
    }

    return m_modelSelector->currentData().toString();
}

QString AgentPanel::selectedExecutionModeKey() const
{
    if (m_executionModeSelector == nullptr || m_executionModeSelector->currentIndex() < 0) {
        return QStringLiteral("agent");
    }

    const QString modeKey = m_executionModeSelector->currentData().toString();
    return modeKey.isEmpty() ? QStringLiteral("agent") : modeKey;
}

QString AgentPanel::executionModeDisplayLabel(const QString &modeKey)
{
    if (modeKey == QStringLiteral("agent")) {
        return i18n("Agent");
    }
    if (modeKey == QStringLiteral("plan")) {
        return i18n("Plan");
    }
    if (modeKey == QStringLiteral("debug")) {
        return i18n("Debug");
    }
    if (modeKey == QStringLiteral("multitask")) {
        return i18n("Multitask");
    }
    if (modeKey == QStringLiteral("ask")) {
        return i18n("Ask");
    }

    return modeKey;
}

void AgentPanel::refreshRequestOptionSelectors()
{
    if (m_modelSelector == nullptr || m_executionModeSelector == nullptr || m_agentManager == nullptr) {
        return;
    }

    m_refreshingRequestOptions = true;
    const QSignalBlocker modelBlocker(m_modelSelector);
    const QSignalBlocker modeBlocker(m_executionModeSelector);

    m_modelSelector->clear();
    m_executionModeSelector->clear();

    if (m_activeSessionId.isEmpty()) {
        m_modelSelector->setEnabled(false);
        m_executionModeSelector->setEnabled(false);
        m_refreshingRequestOptions = false;
        refreshComposerControls();
        return;
    }

    qtcode::agents::AgentSession *session = m_agentManager->session(m_activeSessionId);
    if (session == nullptr) {
        m_modelSelector->setEnabled(false);
        m_executionModeSelector->setEnabled(false);
        m_refreshingRequestOptions = false;
        refreshComposerControls();
        return;
    }

    qtcode::agents::AgentAdapter *adapter = m_agentManager->adapter(session->agentKey());
    const qtcode::agents::AgentSessionRequestOptions savedOptions =
        m_agentManager->requestOptionsForSession(m_activeSessionId);

    const QList<qtcode::agents::AgentOption> executionModes =
        qtcode::agents::executionModesForAgentKey(session->agentKey());
    for (const qtcode::agents::AgentOption &mode : executionModes) {
        m_executionModeSelector->addItem(
            executionModeDisplayLabel(mode.key),
            mode.key);
    }

    int executionModeIndex = m_executionModeSelector->findData(savedOptions.executionModeKey);
    if (executionModeIndex < 0) {
        executionModeIndex = m_executionModeSelector->findData(QStringLiteral("agent"));
    }
    if (executionModeIndex < 0 && m_executionModeSelector->count() > 0) {
        executionModeIndex = 0;
    }
    if (executionModeIndex >= 0) {
        m_executionModeSelector->setCurrentIndex(executionModeIndex);
    }

    if (adapter != nullptr) {
        const QList<qtcode::agents::AgentOption> models = adapter->supportedModels();
        for (const qtcode::agents::AgentOption &model : models) {
            m_modelSelector->addItem(model.label, model.key);
        }

        int modelIndex = m_modelSelector->findData(savedOptions.modelKey);
        if (modelIndex < 0 && !models.isEmpty()) {
            modelIndex = 0;
        }
        if (modelIndex >= 0) {
            m_modelSelector->setCurrentIndex(modelIndex);
        }

        if (session->agentKey() == QStringLiteral("cursor")) {
            auto *watcher = new QFutureWatcher<bool>(this);
            connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, savedOptions]() {
                const bool refreshed = watcher->result();
                watcher->deleteLater();

                if (!refreshed || m_activeSessionId.isEmpty() || m_agentManager == nullptr) {
                    return;
                }

                qtcode::agents::AgentSession *activeSession =
                    m_agentManager->session(m_activeSessionId);
                if (activeSession == nullptr
                    || activeSession->agentKey() != QStringLiteral("cursor")) {
                    return;
                }

                qtcode::agents::AgentAdapter *cursorAdapter =
                    m_agentManager->adapter(QStringLiteral("cursor"));
                if (cursorAdapter == nullptr) {
                    return;
                }

                m_refreshingRequestOptions = true;
                const QSignalBlocker blocker(m_modelSelector);
                const QString selectedModel = selectedModelKey();
                m_modelSelector->clear();

                const QList<qtcode::agents::AgentOption> refreshedModels =
                    cursorAdapter->supportedModels();
                for (const qtcode::agents::AgentOption &model : refreshedModels) {
                    m_modelSelector->addItem(model.label, model.key);
                }

                int modelIndex = m_modelSelector->findData(selectedModel);
                if (modelIndex < 0) {
                    modelIndex = m_modelSelector->findData(savedOptions.modelKey);
                }
                if (modelIndex < 0 && m_modelSelector->count() > 0) {
                    modelIndex = 0;
                }
                if (modelIndex >= 0) {
                    m_modelSelector->setCurrentIndex(modelIndex);
                }

                m_refreshingRequestOptions = false;
                refreshComposerControls();
            });

            watcher->setFuture(QtConcurrent::run([adapter]() {
                QString errorMessage;
                return adapter->refreshSupportedModels(&errorMessage);
            }));
        }
    }

    m_refreshingRequestOptions = false;
    refreshComposerControls();
}

void AgentPanel::onRequestOptionsChanged()
{
    if (m_refreshingRequestOptions || m_agentManager == nullptr || m_activeSessionId.isEmpty()) {
        return;
    }

    qtcode::agents::AgentSessionRequestOptions options;
    options.modelKey = selectedModelKey();
    options.executionModeKey = selectedExecutionModeKey();

    QString persistError;
    if (!m_agentManager->persistRequestOptionsForSession(
            m_activeSessionId,
            options,
            &persistError)) {
        qCWarning(qtcodeUi) << "Failed to persist agent session request options:" << persistError;
    }
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

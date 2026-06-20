#include "agents/AgentManager.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentSession.h"
#include "agents/adapters/CodexAgentAdapter.h"
#include "agents/DiffApplier.h"
#include "shared/Logging.h"
#include "storage/repositories/AgentSessionRepository.h"

#include <QDateTime>
#include <QUuid>

namespace qtcode::agents {

AgentManager::AgentManager(storage::StorageService &storageService, QObject *parent)
    : QObject(parent)
    , m_storageService(storageService)
{
    qRegisterMetaType<qtcode::agents::AgentEvent>("qtcode::agents::AgentEvent");
    qRegisterMetaType<qtcode::agents::AgentRequestStatus>("qtcode::agents::AgentRequestStatus");
    qRegisterMetaType<qtcode::agents::AgentMessage>("qtcode::agents::AgentMessage");
    qRegisterMetaType<qtcode::agents::AgentArtifact>("qtcode::agents::AgentArtifact");
    qRegisterMetaType<qtcode::agents::AgentSessionStatus>("qtcode::agents::AgentSessionStatus");
}

AgentManager::~AgentManager() = default;

bool AgentManager::restoreState(QString *errorMessage)
{
    storage::AgentSessionRepository repository(m_storageService);
    QList<storage::PersistedAgentSession> persistedSessions;
    if (!repository.listSessions(&persistedSessions, errorMessage)) {
        return false;
    }

    for (const storage::PersistedAgentSession &record : persistedSessions) {
        QList<storage::PersistedAgentMessage> persistedMessages;
        if (!repository.listMessagesForSession(record.id, &persistedMessages, errorMessage)) {
            return false;
        }

        auto session = std::make_unique<AgentSession>(
            record.id,
            record.projectId,
            record.agentKey,
            record.title);

        AgentSessionStatus status = agentSessionStatusFromLabel(record.status);
        if (status == AgentSessionStatus::Running) {
            status = AgentSessionStatus::Idle;
        }

        QList<AgentMessage> messages;
        messages.reserve(persistedMessages.size());
        for (const storage::PersistedAgentMessage &persistedMessage : persistedMessages) {
            AgentMessage message;
            message.id = persistedMessage.id;
            message.role = persistedMessage.role;
            message.content = persistedMessage.content;
            message.createdAt = persistedMessage.createdAt;
            messages.append(message);
        }

        session->restoreFromPersistence(status, messages, record.createdAt, record.updatedAt);
        m_sessions.push_back(std::move(session));
    }

    qCInfo(qtcodeAgents) << "Restored" << m_sessions.size() << "agent session(s) from SQLite";
    return true;
}

bool AgentManager::registerAdapter(
    std::unique_ptr<AgentAdapter> adapter,
    QString *errorMessage)
{
    if (adapter == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent adapter must not be null.");
        }
        return false;
    }

    const QString agentKey = adapter->agentKey();
    if (agentKey.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent adapter key must not be empty.");
        }
        return false;
    }

    if (m_adaptersByKey.contains(agentKey)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent adapter '%1' is already registered.").arg(agentKey);
        }
        return false;
    }

    AgentAdapter *rawAdapter = adapter.get();
    connectAdapter(rawAdapter);
    m_adaptersByKey.insert(agentKey, rawAdapter);
    m_adapters.push_back(std::move(adapter));

    qCInfo(qtcodeAgents) << "Registered agent adapter" << agentKey << rawAdapter->displayName()
                         << "available:" << rawAdapter->isAvailable();

    emit adaptersChanged();
    return true;
}

bool AgentManager::registerBuiltInAdapters(QString *errorMessage)
{
    return registerAdapter(std::make_unique<CodexAgentAdapter>(), errorMessage);
}

QList<AgentAdapter *> AgentManager::adapters() const
{
    QList<AgentAdapter *> adapters;
    adapters.reserve(m_adapters.size());
    for (const std::unique_ptr<AgentAdapter> &adapter : m_adapters) {
        adapters.append(adapter.get());
    }
    return adapters;
}

AgentAdapter *AgentManager::adapter(const QString &agentKey) const
{
    return m_adaptersByKey.value(agentKey, nullptr);
}

AgentAdapter *AgentManager::firstAvailableAdapter() const
{
    for (const std::unique_ptr<AgentAdapter> &adapter : m_adapters) {
        if (adapter->isAvailable()) {
            return adapter.get();
        }
    }

    return nullptr;
}

AgentSession *AgentManager::createSession(
    const QString &projectId,
    const QString &agentKey,
    const QString &title,
    QString *errorMessage)
{
    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project id must not be empty.");
        }
        return nullptr;
    }

    AgentAdapter *selectedAdapter = adapter(agentKey);
    if (selectedAdapter == nullptr) {
        selectedAdapter = firstAvailableAdapter();
    }

    if (selectedAdapter == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No agent adapter is available.");
        }
        return nullptr;
    }

    const QString sessionTitle =
        title.isEmpty() ? selectedAdapter->displayName() : title.trimmed();
    auto session = std::make_unique<AgentSession>(
        createId(),
        projectId,
        selectedAdapter->agentKey(),
        sessionTitle);

    AgentSession *rawSession = session.get();
    m_sessions.push_back(std::move(session));

    if (!persistSessionInsert(rawSession, errorMessage)) {
        m_sessions.pop_back();
        return nullptr;
    }

    qCInfo(qtcodeAgents) << "Created agent session" << rawSession->id() << "for project"
                         << projectId << "with adapter" << rawSession->agentKey();

    emit sessionsChanged();
    emit sessionUpdated(rawSession);
    return rawSession;
}

QList<AgentSession *> AgentManager::sessions() const
{
    QList<AgentSession *> sessions;
    sessions.reserve(m_sessions.size());
    for (const std::unique_ptr<AgentSession> &session : m_sessions) {
        sessions.append(session.get());
    }
    return sessions;
}

QList<AgentSession *> AgentManager::sessionsForProject(const QString &projectId) const
{
    QList<AgentSession *> sessions;
    if (projectId.isEmpty()) {
        return sessions;
    }

    for (const std::unique_ptr<AgentSession> &session : m_sessions) {
        if (session->projectId() == projectId) {
            sessions.append(session.get());
        }
    }

    return sessions;
}

AgentSession *AgentManager::session(const QString &sessionId) const
{
    for (const std::unique_ptr<AgentSession> &session : m_sessions) {
        if (session->id() == sessionId) {
            return session.get();
        }
    }

    return nullptr;
}

bool AgentManager::dispatchRequest(
    const QString &sessionId,
    const AgentRequest &request,
    QString *errorMessage)
{
    AgentSession *activeSession = session(sessionId);
    if (activeSession == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    AgentAdapter *selectedAdapter = adapter(activeSession->agentKey());
    if (selectedAdapter == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent adapter '%1' is not registered.")
                                .arg(activeSession->agentKey());
        }
        return false;
    }

    AgentRequest normalizedRequest = request;
    normalizedRequest.sessionId = activeSession->id();
    normalizedRequest.projectId = activeSession->projectId();

    AgentMessage userMessage;
    userMessage.id = createId();
    userMessage.role = QStringLiteral("user");
    userMessage.content = normalizedRequest.prompt;
    userMessage.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    activeSession->clearLastErrorMessage();
    activeSession->clearLastStatusUpdate();
    activeSession->addMessage(userMessage);
    if (!persistMessage(activeSession, userMessage, errorMessage)) {
        return false;
    }

    activeSession->setStatus(AgentSessionStatus::Running);
    if (!persistSessionUpdate(activeSession, errorMessage)) {
        return false;
    }

    m_activeSessionByAdapter.insert(selectedAdapter->agentKey(), activeSession);

    if (!selectedAdapter->startRequest(normalizedRequest, errorMessage)) {
        activeSession->setStatus(AgentSessionStatus::Failed);
        if (errorMessage != nullptr && !errorMessage->isEmpty()) {
            activeSession->setLastErrorMessage(*errorMessage);
        }
        QString persistError;
        if (!persistSessionUpdate(activeSession, &persistError)) {
            qCWarning(qtcodeAgents) << "Failed to persist failed agent session:" << persistError;
        }
        m_activeSessionByAdapter.remove(selectedAdapter->agentKey());
        emit sessionUpdated(activeSession);
        return false;
    }

    emit sessionUpdated(activeSession);
    return true;
}

bool AgentManager::cancelRequest(const QString &sessionId, QString *errorMessage)
{
    AgentSession *activeSession = session(sessionId);
    if (activeSession == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    if (activeSession->status() != AgentSessionStatus::Running) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No running agent request is active for this session.");
        }
        return false;
    }

    AgentAdapter *selectedAdapter = adapter(activeSession->agentKey());
    if (selectedAdapter == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent adapter '%1' is not registered.")
                                .arg(activeSession->agentKey());
        }
        return false;
    }

    selectedAdapter->cancelRequest();
    return true;
}

bool AgentManager::isRequestActive(const QString &sessionId) const
{
    AgentSession *activeSession = session(sessionId);
    return activeSession != nullptr && activeSession->status() == AgentSessionStatus::Running;
}

bool AgentManager::addArtifact(
    const QString &sessionId,
    const AgentArtifact &artifact,
    QString *errorMessage)
{
    AgentSession *activeSession = session(sessionId);
    if (activeSession == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    if (artifact.id.isEmpty() || artifact.kind.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact id and kind must not be empty.");
        }
        return false;
    }

    activeSession->addArtifact(artifact);
    emit sessionUpdated(activeSession);
    return true;
}

bool AgentManager::approveArtifact(
    const QString &sessionId,
    const QString &artifactId,
    const QString &projectRoot,
    QString *errorMessage)
{
    AgentSession *activeSession = session(sessionId);
    if (activeSession == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    AgentArtifact *artifact = activeSession->artifactById(artifactId);
    if (artifact == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact '%1' was not found.").arg(artifactId);
        }
        return false;
    }

    if (artifact->reviewState != ArtifactReviewState::Pending) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact '%1' is no longer pending review.").arg(artifactId);
        }
        return false;
    }

    if (!DiffApplier::applyArtifact(projectRoot, *artifact, errorMessage)) {
        return false;
    }

    if (!activeSession->updateArtifactReviewState(artifactId, ArtifactReviewState::Approved)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to mark artifact '%1' as approved.").arg(artifactId);
        }
        return false;
    }

    emit sessionUpdated(activeSession);
    emit repositoryRefreshRequested();
    return true;
}

bool AgentManager::rejectArtifact(
    const QString &sessionId,
    const QString &artifactId,
    QString *errorMessage)
{
    AgentSession *activeSession = session(sessionId);
    if (activeSession == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    AgentArtifact *artifact = activeSession->artifactById(artifactId);
    if (artifact == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact '%1' was not found.").arg(artifactId);
        }
        return false;
    }

    if (artifact->reviewState != ArtifactReviewState::Pending) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact '%1' is no longer pending review.").arg(artifactId);
        }
        return false;
    }

    if (!activeSession->updateArtifactReviewState(artifactId, ArtifactReviewState::Rejected)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to mark artifact '%1' as rejected.").arg(artifactId);
        }
        return false;
    }

    emit sessionUpdated(activeSession);
    return true;
}

void AgentManager::onAdapterRequestFinished(
    AgentRequestStatus status,
    const QString &errorMessage)
{
    AgentAdapter *senderAdapter = qobject_cast<AgentAdapter *>(sender());
    if (senderAdapter == nullptr) {
        return;
    }

    AgentSession *activeSession = m_activeSessionByAdapter.take(senderAdapter->agentKey());
    if (activeSession == nullptr) {
        return;
    }

    switch (status) {
    case AgentRequestStatus::Succeeded:
        activeSession->setStatus(AgentSessionStatus::Completed);
        break;
    case AgentRequestStatus::Canceled:
        activeSession->setStatus(AgentSessionStatus::Canceled);
        break;
    case AgentRequestStatus::Failed:
    default:
        activeSession->setStatus(AgentSessionStatus::Failed);
        if (!errorMessage.isEmpty()) {
            activeSession->setLastErrorMessage(errorMessage);
            qCWarning(qtcodeAgents) << "Agent request failed for session" << activeSession->id()
                                    << errorMessage;
        }
        break;
    }

    QString persistError;
    if (!persistSessionUpdate(activeSession, &persistError)) {
        qCWarning(qtcodeAgents) << "Failed to persist agent session update:" << persistError;
    }

    emit sessionUpdated(activeSession);
}

bool AgentManager::persistSessionInsert(const AgentSession *session, QString *errorMessage)
{
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session must not be null.");
        }
        return false;
    }

    storage::PersistedAgentSession record;
    record.id = session->id();
    record.projectId = session->projectId();
    record.agentKey = session->agentKey();
    record.title = session->title();
    record.status = agentSessionStatusLabel(session->status());
    record.createdAt = session->createdAt();
    record.updatedAt = session->updatedAt();

    storage::AgentSessionRepository repository(m_storageService);
    return repository.insertSession(record, errorMessage);
}

bool AgentManager::persistSessionUpdate(const AgentSession *session, QString *errorMessage)
{
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session must not be null.");
        }
        return false;
    }

    storage::PersistedAgentSession record;
    record.id = session->id();
    record.projectId = session->projectId();
    record.agentKey = session->agentKey();
    record.title = session->title();
    record.status = agentSessionStatusLabel(session->status());
    record.createdAt = session->createdAt();
    record.updatedAt = session->updatedAt();

    storage::AgentSessionRepository repository(m_storageService);
    return repository.updateSession(record, errorMessage);
}

bool AgentManager::persistMessage(
    const AgentSession *session,
    const AgentMessage &message,
    QString *errorMessage)
{
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session must not be null.");
        }
        return false;
    }

    storage::PersistedAgentMessage record;
    record.id = message.id;
    record.sessionId = session->id();
    record.role = message.role;
    record.content = message.content;
    record.metadataJson = QString();
    record.createdAt = message.createdAt;

    storage::AgentSessionRepository repository(m_storageService);
    return repository.insertMessage(record, errorMessage);
}

bool AgentManager::persistMessageUpdate(
    const AgentSession *session,
    const AgentMessage &message,
    QString *errorMessage)
{
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session must not be null.");
        }
        return false;
    }

    storage::PersistedAgentMessage record;
    record.id = message.id;
    record.sessionId = session->id();
    record.role = message.role;
    record.content = message.content;
    record.metadataJson = QString();
    record.createdAt = message.createdAt;

    storage::AgentSessionRepository repository(m_storageService);
    return repository.updateMessage(record, errorMessage);
}

bool AgentManager::appendOrPersistAssistantOutput(
    AgentSession *session,
    const QString &text,
    QString *errorMessage)
{
    if (session == nullptr || text.isEmpty()) {
        return false;
    }

    if (session->appendAssistantOutput(text)) {
        const QList<AgentMessage> messages = session->messages();
        for (int index = messages.size() - 1; index >= 0; --index) {
            if (messages.at(index).role == QStringLiteral("assistant")) {
                return persistMessageUpdate(session, messages.at(index), errorMessage);
            }
        }
        return false;
    }

    AgentMessage assistantMessage;
    assistantMessage.id = createId();
    assistantMessage.role = QStringLiteral("assistant");
    assistantMessage.content = text;
    assistantMessage.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    session->addMessage(assistantMessage);
    return persistMessage(session, assistantMessage, errorMessage);
}

QString AgentManager::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void AgentManager::connectAdapter(AgentAdapter *adapter)
{
    connect(
        adapter,
        &AgentAdapter::eventEmitted,
        this,
        [this](const AgentEvent &event) {
            AgentAdapter *sourceAdapter = qobject_cast<AgentAdapter *>(sender());
            if (sourceAdapter == nullptr) {
                return;
            }

            AgentSession *activeSession = m_activeSessionByAdapter.value(sourceAdapter->agentKey());
            if (activeSession == nullptr) {
                return;
            }

            if (event.type == AgentEventType::OutputText && !event.text.isEmpty()) {
                QString persistError;
                if (!appendOrPersistAssistantOutput(activeSession, event.text, &persistError)) {
                    qCWarning(qtcodeAgents) << "Failed to persist assistant output:" << persistError;
                }
            }

            if (event.type == AgentEventType::StatusUpdate && !event.text.isEmpty()) {
                activeSession->setLastStatusUpdate(event.text);
            }

            if (event.type == AgentEventType::Error && !event.error.message.isEmpty()) {
                activeSession->setLastErrorMessage(event.error.message);
            }

            if (event.type == AgentEventType::ArtifactReady && !event.artifact.id.isEmpty()) {
                activeSession->addArtifact(event.artifact);
            }

            emit sessionUpdated(activeSession);
        });

    connect(
        adapter,
        &AgentAdapter::requestFinished,
        this,
        &AgentManager::onAdapterRequestFinished);
}

} // namespace qtcode::agents

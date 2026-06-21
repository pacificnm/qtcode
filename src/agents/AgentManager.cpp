#include "agents/AgentManager.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentSession.h"
#include "agents/adapters/CodexAgentAdapter.h"
#include "agents/adapters/CursorAgentAdapter.h"
#include "agents/DiffApplier.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/repositories/AgentSessionRepository.h"
#include "storage/repositories/ContextRetrievalRepository.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QJsonObject>
#include <QTimer>
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

AgentManager::~AgentManager()
{
    for (QTimer *timer : std::as_const(m_messagePersistTimers)) {
        if (timer != nullptr) {
            timer->stop();
            delete timer;
        }
    }
}

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
    if (!registerAdapter(std::make_unique<CodexAgentAdapter>(), errorMessage)) {
        return false;
    }

    return registerAdapter(std::make_unique<CursorAgentAdapter>(), errorMessage);
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

QString AgentManager::activeSessionIdForProject(const QString &projectId) const
{
    if (projectId.isEmpty()) {
        return {};
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    QString errorMessage;
    if (!settingsRepository.loadJson(
            settings::kActiveAgentSessionByProjectSettingKey,
            &json,
            &found,
            &errorMessage)) {
        qCWarning(qtcodeAgents) << "Failed to load active agent session setting:" << errorMessage;
        return {};
    }

    if (!found) {
        return {};
    }

    return json.value(projectId).toString();
}

bool AgentManager::persistActiveSessionForProject(
    const QString &projectId,
    const QString &sessionId,
    QString *errorMessage)
{
    if (projectId.isEmpty() || sessionId.isEmpty()) {
        return true;
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!settingsRepository.loadJson(
            settings::kActiveAgentSessionByProjectSettingKey,
            &json,
            &found,
            errorMessage)) {
        return false;
    }

    json.insert(projectId, sessionId);
    return settingsRepository.upsertJson(
        settings::kActiveAgentSessionByProjectSettingKey,
        json,
        errorMessage);
}

AgentSessionRequestOptions AgentManager::requestOptionsForSession(const QString &sessionId) const
{
    AgentSessionRequestOptions options;
    options.executionModeKey = QStringLiteral("agent");

    if (sessionId.isEmpty()) {
        return options;
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    QString errorMessage;
    if (!settingsRepository.loadJson(
            settings::kAgentSessionRequestOptionsSettingKey,
            &json,
            &found,
            &errorMessage)) {
        qCWarning(qtcodeAgents) << "Failed to load agent session request options:" << errorMessage;
        return options;
    }

    if (!found || !json.contains(sessionId)) {
        return options;
    }

    const QJsonObject sessionOptions = json.value(sessionId).toObject();
    options.modelKey = sessionOptions.value(QStringLiteral("modelKey")).toString();
    options.executionModeKey = sessionOptions.value(QStringLiteral("executionModeKey"))
                                   .toString()
                                   .trimmed();
    if (options.executionModeKey.isEmpty()) {
        options.executionModeKey = QStringLiteral("agent");
    }

    return options;
}

bool AgentManager::persistRequestOptionsForSession(
    const QString &sessionId,
    const AgentSessionRequestOptions &options,
    QString *errorMessage)
{
    if (sessionId.isEmpty()) {
        return true;
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!settingsRepository.loadJson(
            settings::kAgentSessionRequestOptionsSettingKey,
            &json,
            &found,
            errorMessage)) {
        return false;
    }

    QJsonObject sessionOptions;
    sessionOptions.insert(QStringLiteral("modelKey"), options.modelKey);
    sessionOptions.insert(QStringLiteral("executionModeKey"), options.executionModeKey);
    json.insert(sessionId, sessionOptions);

    return settingsRepository.upsertJson(
        settings::kAgentSessionRequestOptionsSettingKey,
        json,
        errorMessage);
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
    flushPendingMessagePersist(activeSession, &persistError);

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

bool AgentManager::appendOrPersistOutput(
    AgentSession *session,
    const QString &text,
    const QString &role,
    bool startNewMessage,
    QString *errorMessage)
{
    if (session == nullptr || text.isEmpty()) {
        return false;
    }

    const QString normalizedRole =
        role.trimmed().isEmpty() ? QStringLiteral("assistant") : role.trimmed();

    if (!startNewMessage && session->appendRoleOutput(normalizedRole, text)) {
        scheduleMessagePersist(session);
        return true;
    }

    AgentMessage outputMessage;
    outputMessage.id = createId();
    outputMessage.role = normalizedRole;
    outputMessage.content = text;
    outputMessage.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    session->addMessage(outputMessage);
    return persistMessage(session, outputMessage, errorMessage);
}

void AgentManager::scheduleMessagePersist(const AgentSession *session)
{
    if (session == nullptr) {
        return;
    }

    QTimer *timer = m_messagePersistTimers.value(session->id());
    if (timer == nullptr) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(250);
        m_messagePersistTimers.insert(session->id(), timer);
        connect(timer, &QTimer::timeout, this, [this, sessionId = session->id()]() {
            AgentSession *activeSession = this->session(sessionId);
            if (activeSession == nullptr) {
                return;
            }

            QString persistError;
            flushPendingMessagePersist(activeSession, &persistError);
            if (!persistError.isEmpty()) {
                qCWarning(qtcodeAgents) << "Failed to persist streamed message:" << persistError;
            }
        });
    }

    timer->start();
}

void AgentManager::flushPendingMessagePersist(const AgentSession *session, QString *errorMessage)
{
    if (session == nullptr) {
        return;
    }

    QTimer *timer = m_messagePersistTimers.value(session->id());
    if (timer != nullptr) {
        timer->stop();
    }

    const QList<AgentMessage> messages = session->messages();
    if (messages.isEmpty()) {
        return;
    }

    if (!persistMessageUpdate(session, messages.constLast(), errorMessage)) {
        return;
    }
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
        [this, adapter](const AgentEvent &event) {
            if (adapter == nullptr) {
                return;
            }

            AgentSession *activeSession = m_activeSessionByAdapter.value(adapter->agentKey());
            if (activeSession == nullptr) {
                return;
            }

            if (event.type == AgentEventType::OutputText && !event.text.isEmpty()) {
                QString persistError;
                if (!appendOrPersistOutput(
                        activeSession,
                        event.text,
                        event.messageRole,
                        event.startNewMessage,
                        &persistError)) {
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

bool AgentManager::persistContextRetrievalMetadata(
    const QString &sessionId,
    const QString &projectId,
    const QString &query,
    const QString &providerKey,
    const QList<memory::ContextResult> &attachedResults,
    int totalResultCount,
    bool memoryUnavailable,
    const QString &statusMessage,
    QString *errorMessage)
{
    if (sessionId.isEmpty() || projectId.isEmpty() || query.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Context retrieval metadata requires a session, project, and query.");
        }
        return false;
    }

    storage::PersistedContextRetrieval retrieval;
    retrieval.id = createId();
    retrieval.sessionId = sessionId;
    retrieval.projectId = projectId;
    retrieval.query = query.trimmed();
    retrieval.providerKey = providerKey.trimmed().isEmpty() ? QStringLiteral("qtcode-memory") : providerKey.trimmed();
    retrieval.resultCount = attachedResults.size();
    retrieval.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    retrieval.metadataJson.insert(QStringLiteral("total_result_count"), totalResultCount);
    retrieval.metadataJson.insert(QStringLiteral("attached_result_count"), attachedResults.size());
    retrieval.metadataJson.insert(QStringLiteral("memory_unavailable"), memoryUnavailable);
    if (!statusMessage.isEmpty()) {
        retrieval.metadataJson.insert(QStringLiteral("status_message"), statusMessage);
    }

    QList<storage::PersistedContextResult> persistedResults;
    persistedResults.reserve(attachedResults.size());
    for (const memory::ContextResult &result : attachedResults) {
        storage::PersistedContextResult persisted;
        persisted.id = createId();
        persisted.retrievalId = retrieval.id;
        persisted.sourceType = memory::contextSourceTypeLabel(result.sourceType);
        persisted.sourceUri = result.sourceUri;
        persisted.title = result.title;
        persisted.excerpt = result.excerpt;
        persisted.score = result.score;
        persisted.metadataJson.insert(QStringLiteral("retrieved_at"), result.retrievedAt);
        persistedResults.append(persisted);
    }

    storage::ContextRetrievalRepository repository(m_storageService);
    if (!repository.insertRetrieval(retrieval, persistedResults, errorMessage)) {
        return false;
    }

    qCInfo(qtcodeAgents) << "Persisted context retrieval metadata for session" << sessionId
                         << "with" << persistedResults.size() << "attached result(s)";
    return true;
}

bool AgentManager::latestContextRetrievalForSession(
    const QString &sessionId,
    storage::PersistedContextRetrieval *retrieval,
    QList<storage::PersistedContextResult> *results,
    QString *errorMessage) const
{
    storage::ContextRetrievalRepository repository(m_storageService);
    return repository.latestRetrievalForSession(sessionId, retrieval, results, errorMessage);
}

} // namespace qtcode::agents

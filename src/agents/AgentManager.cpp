#include "agents/AgentManager.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentSession.h"
#include "agents/adapters/CodexAgentAdapter.h"
#include "shared/Logging.h"

#include <QDateTime>
#include <QUuid>

namespace qtcode::agents {

AgentManager::AgentManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<qtcode::agents::AgentEvent>("qtcode::agents::AgentEvent");
    qRegisterMetaType<qtcode::agents::AgentRequestStatus>("qtcode::agents::AgentRequestStatus");
    qRegisterMetaType<qtcode::agents::AgentMessage>("qtcode::agents::AgentMessage");
    qRegisterMetaType<qtcode::agents::AgentArtifact>("qtcode::agents::AgentArtifact");
    qRegisterMetaType<qtcode::agents::AgentSessionStatus>("qtcode::agents::AgentSessionStatus");
}

AgentManager::~AgentManager() = default;

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
    activeSession->addMessage(userMessage);
    activeSession->setStatus(AgentSessionStatus::Running);

    m_activeSessionByAdapter.insert(selectedAdapter->agentKey(), activeSession);

    if (!selectedAdapter->startRequest(normalizedRequest, errorMessage)) {
        activeSession->setStatus(AgentSessionStatus::Failed);
        m_activeSessionByAdapter.remove(selectedAdapter->agentKey());
        emit sessionUpdated(activeSession);
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
        activeSession->setStatus(AgentSessionStatus::Idle);
        break;
    case AgentRequestStatus::Failed:
    default:
        activeSession->setStatus(AgentSessionStatus::Failed);
        if (!errorMessage.isEmpty()) {
            qCWarning(qtcodeAgents) << "Agent request failed for session" << activeSession->id()
                                    << errorMessage;
        }
        break;
    }

    emit sessionUpdated(activeSession);
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
                AgentMessage assistantMessage;
                assistantMessage.id = createId();
                assistantMessage.role = QStringLiteral("assistant");
                assistantMessage.content = event.text;
                assistantMessage.createdAt =
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                activeSession->addMessage(assistantMessage);
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

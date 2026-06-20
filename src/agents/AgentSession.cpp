#include "agents/AgentSession.h"

#include <QDateTime>

namespace qtcode::agents {

AgentSession::AgentSession(
    const QString &id,
    const QString &projectId,
    const QString &agentKey,
    const QString &title,
    QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_projectId(projectId)
    , m_agentKey(agentKey)
    , m_title(title)
    , m_createdAt(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
    , m_updatedAt(m_createdAt)
{
}

QString AgentSession::id() const
{
    return m_id;
}

QString AgentSession::projectId() const
{
    return m_projectId;
}

QString AgentSession::agentKey() const
{
    return m_agentKey;
}

QString AgentSession::title() const
{
    return m_title;
}

AgentSessionStatus AgentSession::status() const
{
    return m_status;
}

QList<AgentMessage> AgentSession::messages() const
{
    return m_messages;
}

QList<AgentArtifact> AgentSession::artifacts() const
{
    return m_artifacts;
}

QString AgentSession::createdAt() const
{
    return m_createdAt;
}

QString AgentSession::updatedAt() const
{
    return m_updatedAt;
}

void AgentSession::setTitle(const QString &title)
{
    m_title = title;
    touchUpdatedAt();
}

void AgentSession::setStatus(AgentSessionStatus status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    touchUpdatedAt();
    emit statusChanged(status);
}

void AgentSession::addMessage(const AgentMessage &message)
{
    m_messages.append(message);
    touchUpdatedAt();
    emit messageAdded(message);
}

void AgentSession::addArtifact(const AgentArtifact &artifact)
{
    m_artifacts.append(artifact);
    touchUpdatedAt();
    emit artifactAdded(artifact);
}

void AgentSession::touchUpdatedAt()
{
    m_updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

void AgentSession::restoreFromPersistence(
    AgentSessionStatus status,
    const QList<AgentMessage> &messages,
    const QString &createdAt,
    const QString &updatedAt)
{
    m_status = status;
    m_messages = messages;
    m_createdAt = createdAt;
    m_updatedAt = updatedAt;
}

} // namespace qtcode::agents

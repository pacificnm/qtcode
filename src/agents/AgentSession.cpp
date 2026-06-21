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

QString AgentSession::createdAt() const
{
    return m_createdAt;
}

QString AgentSession::updatedAt() const
{
    return m_updatedAt;
}

QString AgentSession::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

QString AgentSession::lastStatusUpdate() const
{
    return m_lastStatusUpdate;
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

void AgentSession::setLastErrorMessage(const QString &message)
{
    m_lastErrorMessage = message.trimmed();
}

void AgentSession::clearLastErrorMessage()
{
    m_lastErrorMessage.clear();
}

void AgentSession::setLastStatusUpdate(const QString &statusUpdate)
{
    m_lastStatusUpdate = statusUpdate.trimmed();
}

void AgentSession::clearLastStatusUpdate()
{
    m_lastStatusUpdate.clear();
}

bool AgentSession::appendRoleOutput(const QString &role, const QString &text)
{
    if (text.isEmpty() || m_messages.isEmpty()) {
        return false;
    }

    const QString normalizedRole =
        role.isEmpty() ? QStringLiteral("assistant") : role.trimmed();
    if (m_messages.last().role != normalizedRole) {
        return false;
    }

    if (m_messages.last().content == text) {
        touchUpdatedAt();
        return true;
    }

    m_messages.last().content.append(text);
    touchUpdatedAt();
    return true;
}

bool AgentSession::updateAssistantMessage(const QString &messageId, const QString &content)
{
    for (AgentMessage &message : m_messages) {
        if (message.id != messageId) {
            continue;
        }

        message.content = content;
        touchUpdatedAt();
        return true;
    }

    return false;
}

void AgentSession::addMessage(const AgentMessage &message)
{
    m_messages.append(message);
    touchUpdatedAt();
    emit messageAdded(message);
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

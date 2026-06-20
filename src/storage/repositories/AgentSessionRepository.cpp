#include "storage/repositories/AgentSessionRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

PersistedAgentSession sessionFromQuery(const QSqlQuery &query)
{
    PersistedAgentSession session;
    session.id = query.value(QStringLiteral("id")).toString();
    session.projectId = query.value(QStringLiteral("project_id")).toString();
    session.agentKey = query.value(QStringLiteral("agent_key")).toString();
    session.title = query.value(QStringLiteral("title")).toString();
    session.status = query.value(QStringLiteral("status")).toString();
    session.createdAt = query.value(QStringLiteral("created_at")).toString();
    session.updatedAt = query.value(QStringLiteral("updated_at")).toString();
    return session;
}

PersistedAgentMessage messageFromQuery(const QSqlQuery &query)
{
    PersistedAgentMessage message;
    message.id = query.value(QStringLiteral("id")).toString();
    message.sessionId = query.value(QStringLiteral("session_id")).toString();
    message.role = query.value(QStringLiteral("role")).toString();
    message.content = query.value(QStringLiteral("content")).toString();
    message.metadataJson = query.value(QStringLiteral("metadata_json")).toString();
    message.createdAt = query.value(QStringLiteral("created_at")).toString();
    return message;
}

} // namespace

AgentSessionRepository::AgentSessionRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool AgentSessionRepository::insertSession(
    const PersistedAgentSession &session,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO agent_sessions ("
        "id, project_id, agent_key, title, status, created_at, updated_at) "
        "VALUES ("
        ":id, :project_id, :agent_key, :title, :status, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":id"), session.id);
    query.bindValue(QStringLiteral(":project_id"), session.projectId);
    query.bindValue(QStringLiteral(":agent_key"), session.agentKey);
    query.bindValue(QStringLiteral(":title"), session.title);
    query.bindValue(QStringLiteral(":status"), session.status);
    query.bindValue(QStringLiteral(":created_at"), session.createdAt);
    query.bindValue(QStringLiteral(":updated_at"), session.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert agent session: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert agent session" << message;
        return false;
    }

    return true;
}

bool AgentSessionRepository::updateSession(
    const PersistedAgentSession &session,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE agent_sessions SET "
        "project_id = :project_id, "
        "agent_key = :agent_key, "
        "title = :title, "
        "status = :status, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), session.id);
    query.bindValue(QStringLiteral(":project_id"), session.projectId);
    query.bindValue(QStringLiteral(":agent_key"), session.agentKey);
    query.bindValue(QStringLiteral(":title"), session.title);
    query.bindValue(QStringLiteral(":status"), session.status);
    query.bindValue(QStringLiteral(":updated_at"), session.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update agent session: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to update agent session" << message;
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool AgentSessionRepository::listSessions(
    QList<PersistedAgentSession> *sessions,
    QString *errorMessage) const
{
    if (sessions == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent session list output must not be null.");
        }
        return false;
    }

    sessions->clear();

    QSqlQuery query(m_storageService.database());
    if (!query.exec(QStringLiteral(
            "SELECT id, project_id, agent_key, title, status, created_at, updated_at "
            "FROM agent_sessions "
            "ORDER BY created_at ASC"))) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list agent sessions: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list agent sessions" << message;
        return false;
    }

    while (query.next()) {
        sessions->append(sessionFromQuery(query));
    }

    return true;
}

bool AgentSessionRepository::listMessagesForSession(
    const QString &sessionId,
    QList<PersistedAgentMessage> *messages,
    QString *errorMessage) const
{
    if (messages == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent message list output must not be null.");
        }
        return false;
    }

    messages->clear();

    if (sessionId.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, session_id, role, content, metadata_json, created_at "
        "FROM agent_messages "
        "WHERE session_id = :session_id "
        "ORDER BY created_at ASC"));
    query.bindValue(QStringLiteral(":session_id"), sessionId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list agent messages: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list agent messages" << message;
        return false;
    }

    while (query.next()) {
        messages->append(messageFromQuery(query));
    }

    return true;
}

bool AgentSessionRepository::insertMessage(
    const PersistedAgentMessage &message,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO agent_messages ("
        "id, session_id, role, content, metadata_json, created_at) "
        "VALUES ("
        ":id, :session_id, :role, :content, :metadata_json, :created_at)"));
    query.bindValue(QStringLiteral(":id"), message.id);
    query.bindValue(QStringLiteral(":session_id"), message.sessionId);
    query.bindValue(QStringLiteral(":role"), message.role);
    query.bindValue(QStringLiteral(":content"), message.content);
    query.bindValue(QStringLiteral(":metadata_json"), message.metadataJson);
    query.bindValue(QStringLiteral(":created_at"), message.createdAt);

    if (!query.exec()) {
        const QString messageText = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert agent message: %1").arg(messageText);
        }
        qCWarning(qtcodeStorage) << "Failed to insert agent message" << messageText;
        return false;
    }

    return true;
}

} // namespace qtcode::storage

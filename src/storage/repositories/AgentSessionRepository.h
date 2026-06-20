#pragma once

#include <QList>
#include <QString>

namespace qtcode::storage {

class StorageService;

struct PersistedAgentSession
{
    QString id;
    QString projectId;
    QString agentKey;
    QString title;
    QString status;
    QString createdAt;
    QString updatedAt;
};

struct PersistedAgentMessage
{
    QString id;
    QString sessionId;
    QString role;
    QString content;
    QString metadataJson;
    QString createdAt;
};

class AgentSessionRepository
{
public:
    explicit AgentSessionRepository(StorageService &storageService);

    [[nodiscard]] bool insertSession(
        const PersistedAgentSession &session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool updateSession(
        const PersistedAgentSession &session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool listSessions(
        QList<PersistedAgentSession> *sessions,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool listMessagesForSession(
        const QString &sessionId,
        QList<PersistedAgentMessage> *messages,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool updateMessage(
        const PersistedAgentMessage &message,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool insertMessage(
        const PersistedAgentMessage &message,
        QString *errorMessage = nullptr);

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

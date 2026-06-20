#pragma once

#include "agents/AgentModels.h"

#include <QHash>
#include <QList>
#include <QString>

#include <QObject>

#include <memory>
#include <vector>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::agents {

class AgentAdapter;
class AgentSession;

class AgentManager final : public QObject
{
    Q_OBJECT

public:
    explicit AgentManager(
        storage::StorageService &storageService,
        QObject *parent = nullptr);
    ~AgentManager() override;

    [[nodiscard]] bool restoreState(QString *errorMessage = nullptr);

    [[nodiscard]] bool registerAdapter(
        std::unique_ptr<AgentAdapter> adapter,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool registerBuiltInAdapters(QString *errorMessage = nullptr);

    [[nodiscard]] QList<AgentAdapter *> adapters() const;
    [[nodiscard]] AgentAdapter *adapter(const QString &agentKey) const;
    [[nodiscard]] AgentAdapter *firstAvailableAdapter() const;

    [[nodiscard]] AgentSession *createSession(
        const QString &projectId,
        const QString &agentKey,
        const QString &title,
        QString *errorMessage = nullptr);
    [[nodiscard]] QList<AgentSession *> sessions() const;
    [[nodiscard]] QList<AgentSession *> sessionsForProject(const QString &projectId) const;
    [[nodiscard]] AgentSession *session(const QString &sessionId) const;

    [[nodiscard]] bool dispatchRequest(
        const QString &sessionId,
        const AgentRequest &request,
        QString *errorMessage = nullptr);

signals:
    void adaptersChanged();
    void sessionsChanged();
    void sessionUpdated(qtcode::agents::AgentSession *session);

private slots:
    void onAdapterRequestFinished(
        qtcode::agents::AgentRequestStatus status,
        const QString &errorMessage);

private:
    [[nodiscard]] static QString createId();
    void connectAdapter(AgentAdapter *adapter);
    [[nodiscard]] bool persistSessionInsert(
        const AgentSession *session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool persistSessionUpdate(
        const AgentSession *session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool persistMessage(
        const AgentSession *session,
        const AgentMessage &message,
        QString *errorMessage = nullptr);

    storage::StorageService &m_storageService;
    std::vector<std::unique_ptr<AgentAdapter>> m_adapters;
    std::vector<std::unique_ptr<AgentSession>> m_sessions;
    QHash<QString, AgentAdapter *> m_adaptersByKey;
    QHash<QString, AgentSession *> m_activeSessionByAdapter;
};

} // namespace qtcode::agents

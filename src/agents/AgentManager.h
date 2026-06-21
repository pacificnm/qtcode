#pragma once

#include "agents/AgentModels.h"

#include "memory/ContextResult.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QTimer>

#include <QObject>

#include <memory>
#include <vector>

namespace qtcode::storage {
class StorageService;
struct PersistedContextRetrieval;
struct PersistedContextResult;
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
    [[nodiscard]] QString activeSessionIdForProject(const QString &projectId) const;
    [[nodiscard]] bool persistActiveSessionForProject(
        const QString &projectId,
        const QString &sessionId,
        QString *errorMessage = nullptr);
    [[nodiscard]] AgentSessionRequestOptions requestOptionsForSession(
        const QString &sessionId) const;
    [[nodiscard]] bool persistRequestOptionsForSession(
        const QString &sessionId,
        const AgentSessionRequestOptions &options,
        QString *errorMessage = nullptr);

    [[nodiscard]] bool dispatchRequest(
        const QString &sessionId,
        const AgentRequest &request,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool cancelRequest(
        const QString &sessionId,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool isRequestActive(const QString &sessionId) const;
    [[nodiscard]] bool addArtifact(
        const QString &sessionId,
        const AgentArtifact &artifact,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool approveArtifact(
        const QString &sessionId,
        const QString &artifactId,
        const QString &projectRoot,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool rejectArtifact(
        const QString &sessionId,
        const QString &artifactId,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool persistContextRetrievalMetadata(
        const QString &sessionId,
        const QString &projectId,
        const QString &query,
        const QString &providerKey,
        const QList<memory::ContextResult> &attachedResults,
        int totalResultCount,
        bool memoryUnavailable,
        const QString &statusMessage,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool latestContextRetrievalForSession(
        const QString &sessionId,
        storage::PersistedContextRetrieval *retrieval,
        QList<storage::PersistedContextResult> *results,
        QString *errorMessage = nullptr) const;

signals:
    void adaptersChanged();
    void sessionsChanged();
    void sessionUpdated(qtcode::agents::AgentSession *session);
    void repositoryRefreshRequested();

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
    [[nodiscard]] bool persistMessageUpdate(
        const AgentSession *session,
        const AgentMessage &message,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool appendOrPersistOutput(
        AgentSession *session,
        const QString &text,
        const QString &role,
        bool startNewMessage,
        QString *errorMessage = nullptr);
    void scheduleMessagePersist(const AgentSession *session);
    void flushPendingMessagePersist(const AgentSession *session, QString *errorMessage = nullptr);

    storage::StorageService &m_storageService;
    std::vector<std::unique_ptr<AgentAdapter>> m_adapters;
    std::vector<std::unique_ptr<AgentSession>> m_sessions;
    QHash<QString, AgentAdapter *> m_adaptersByKey;
    QHash<QString, AgentSession *> m_activeSessionByAdapter;
    QHash<QString, QTimer *> m_messagePersistTimers;
};

} // namespace qtcode::agents

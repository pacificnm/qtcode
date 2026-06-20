#pragma once

#include "agents/AgentModels.h"

#include <QList>
#include <QString>

#include <QObject>

namespace qtcode::agents {

class AgentSession final : public QObject
{
    Q_OBJECT

public:
    AgentSession(
        const QString &id,
        const QString &projectId,
        const QString &agentKey,
        const QString &title,
        QObject *parent = nullptr);

    [[nodiscard]] QString id() const;
    [[nodiscard]] QString projectId() const;
    [[nodiscard]] QString agentKey() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] AgentSessionStatus status() const;
    [[nodiscard]] QList<AgentMessage> messages() const;
    [[nodiscard]] QList<AgentArtifact> artifacts() const;
    [[nodiscard]] QString createdAt() const;
    [[nodiscard]] QString updatedAt() const;

    void setTitle(const QString &title);
    void setStatus(AgentSessionStatus status);
    void addMessage(const AgentMessage &message);
    void addArtifact(const AgentArtifact &artifact);
    void touchUpdatedAt();

signals:
    void messageAdded(const qtcode::agents::AgentMessage &message);
    void artifactAdded(const qtcode::agents::AgentArtifact &artifact);
    void statusChanged(qtcode::agents::AgentSessionStatus status);

private:
    QString m_id;
    QString m_projectId;
    QString m_agentKey;
    QString m_title;
    AgentSessionStatus m_status = AgentSessionStatus::Idle;
    QList<AgentMessage> m_messages;
    QList<AgentArtifact> m_artifacts;
    QString m_createdAt;
    QString m_updatedAt;
};

} // namespace qtcode::agents

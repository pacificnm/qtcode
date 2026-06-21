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
    [[nodiscard]] QString createdAt() const;
    [[nodiscard]] QString updatedAt() const;
    [[nodiscard]] QString lastErrorMessage() const;
    [[nodiscard]] QString lastStatusUpdate() const;

    void setTitle(const QString &title);
    void setStatus(AgentSessionStatus status);
    void setLastErrorMessage(const QString &message);
    void clearLastErrorMessage();
    void setLastStatusUpdate(const QString &statusUpdate);
    void clearLastStatusUpdate();
    void addMessage(const AgentMessage &message);
    [[nodiscard]] bool appendRoleOutput(const QString &role, const QString &text);
    [[nodiscard]] bool updateAssistantMessage(const QString &messageId, const QString &content);
    void touchUpdatedAt();
    void restoreFromPersistence(
        AgentSessionStatus status,
        const QList<AgentMessage> &messages,
        const QString &createdAt,
        const QString &updatedAt);

signals:
    void messageAdded(const qtcode::agents::AgentMessage &message);
    void statusChanged(qtcode::agents::AgentSessionStatus status);

private:
    QString m_id;
    QString m_projectId;
    QString m_agentKey;
    QString m_title;
    AgentSessionStatus m_status = AgentSessionStatus::Idle;
    QList<AgentMessage> m_messages;
    QString m_createdAt;
    QString m_updatedAt;
    QString m_lastErrorMessage;
    QString m_lastStatusUpdate;
};

} // namespace qtcode::agents

#pragma once

#include "agents/AgentModels.h"

#include <QObject>

namespace qtcode::agents {

class AgentAdapter : public QObject
{
    Q_OBJECT

public:
    explicit AgentAdapter(QObject *parent = nullptr);
    ~AgentAdapter() override;

    AgentAdapter(const AgentAdapter &) = delete;
    AgentAdapter &operator=(const AgentAdapter &) = delete;

    [[nodiscard]] virtual QString agentKey() const = 0;
    [[nodiscard]] virtual QString displayName() const = 0;
    [[nodiscard]] virtual AgentCapabilities capabilities() const = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual QString executablePath() const;

    [[nodiscard]] virtual bool validateConfiguration(QString *errorMessage = nullptr) const = 0;
    [[nodiscard]] virtual QList<AgentOption> supportedModels() const;
    [[nodiscard]] virtual bool refreshSupportedModels(QString *errorMessage = nullptr);
    [[nodiscard]] virtual bool startRequest(
        const AgentRequest &request,
        QString *errorMessage = nullptr) = 0;
    virtual void cancelRequest() = 0;

signals:
    void eventEmitted(const qtcode::agents::AgentEvent &event);
    void requestFinished(
        qtcode::agents::AgentRequestStatus status,
        const QString &errorMessage);
};

} // namespace qtcode::agents

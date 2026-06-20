#pragma once

#include "agents/AgentAdapter.h"

namespace qtcode::agents {

class CodexAgentAdapter final : public AgentAdapter
{
    Q_OBJECT

public:
    explicit CodexAgentAdapter(QObject *parent = nullptr);

    [[nodiscard]] QString agentKey() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] AgentCapabilities capabilities() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString executablePath() const override;

    [[nodiscard]] bool validateConfiguration(QString *errorMessage = nullptr) const override;
    [[nodiscard]] bool startRequest(
        const AgentRequest &request,
        QString *errorMessage = nullptr) override;
    void cancelRequest() override;

    void setExecutablePath(const QString &executablePath);

private:
    QString m_executablePath;
    bool m_requestInFlight = false;
};

} // namespace qtcode::agents

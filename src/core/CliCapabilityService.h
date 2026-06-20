#pragma once

#include "core/CliCapabilityModels.h"

#include <QObject>

namespace qtcode::core {

class CliCapabilityService final : public QObject
{
    Q_OBJECT

public:
    explicit CliCapabilityService(QObject *parent = nullptr);

    [[nodiscard]] bool detectCapabilities();
    [[nodiscard]] const CliCapabilitiesSnapshot &snapshot() const;
    [[nodiscard]] bool isGitAvailable() const;
    [[nodiscard]] bool isGhAvailable() const;
    [[nodiscard]] bool isAgentCliAvailable() const;

signals:
    void capabilitiesDetected();

private:
    [[nodiscard]] CliToolCapability detectGit() const;
    [[nodiscard]] CliToolCapability detectGh() const;
    [[nodiscard]] CliToolCapability detectFirstAgentCli() const;
    [[nodiscard]] CliToolCapability probeExecutable(
        const QString &toolId,
        const QString &displayName,
        const QString &executableName,
        const QString &unavailableMessage) const;
    [[nodiscard]] static QString firstOutputLine(const QString &output);

    CliCapabilitiesSnapshot m_snapshot;
};

} // namespace qtcode::core

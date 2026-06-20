#pragma once

#include "core/CliCapabilityModels.h"

#include <QObject>

template <typename T>
class QFutureWatcher;

namespace qtcode::core {

class CliCapabilityService final : public QObject
{
    Q_OBJECT

public:
    explicit CliCapabilityService(QObject *parent = nullptr);
    ~CliCapabilityService() override;

    [[nodiscard]] bool detectCapabilities();
    void scheduleDetection();
    [[nodiscard]] bool isDetectionRunning() const;

    [[nodiscard]] const CliCapabilitiesSnapshot &snapshot() const;
    [[nodiscard]] bool isGitAvailable() const;
    [[nodiscard]] bool isGhAvailable() const;
    [[nodiscard]] bool isGhAuthenticated() const;
    [[nodiscard]] bool isAgentCliAvailable() const;

signals:
    void capabilitiesDetected();

private slots:
    void onDetectionFinished();

private:
    [[nodiscard]] CliCapabilitiesSnapshot buildCapabilitiesSnapshot() const;
    [[nodiscard]] CliToolCapability detectGit() const;
    [[nodiscard]] CliToolCapability detectGh() const;
    [[nodiscard]] CliToolCapability detectFirstAgentCli() const;
    [[nodiscard]] static QString parseGhAuthAccount(const QString &authStatusOutput);
    [[nodiscard]] CliToolCapability probeExecutable(
        const QString &toolId,
        const QString &displayName,
        const QString &executableName,
        const QString &unavailableMessage) const;
    [[nodiscard]] static QString firstOutputLine(const QString &output);
    void logCapabilitiesSnapshot(const CliCapabilitiesSnapshot &snapshot) const;

    CliCapabilitiesSnapshot m_snapshot;
    QFutureWatcher<CliCapabilitiesSnapshot> *m_detectionWatcher = nullptr;
};

} // namespace qtcode::core

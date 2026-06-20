#include "core/CliCapabilityService.h"

#include "shared/Logging.h"
#include "shared/ProcessRunner.h"

#include <QStandardPaths>

namespace qtcode::core {

namespace {

struct AgentCliCandidate
{
    const char *executableName;
    const char *displayName;
};

constexpr AgentCliCandidate kAgentCliCandidates[] = {
    {"codex", "Codex CLI"},
    {"claude", "Claude CLI"},
    {"aider", "aider"},
    {"cursor", "Cursor CLI"},
};

constexpr int kDetectionTimeoutMs = 3000;

} // namespace

CliCapabilityService::CliCapabilityService(QObject *parent)
    : QObject(parent)
{
    m_snapshot.git.toolId = QStringLiteral("git");
    m_snapshot.git.displayName = QStringLiteral("Git");
    m_snapshot.gh.toolId = QStringLiteral("gh");
    m_snapshot.gh.displayName = QStringLiteral("GitHub CLI");
    m_snapshot.agentCli.toolId = QStringLiteral("agent");
    m_snapshot.agentCli.displayName = QStringLiteral("Agent CLI");
}

bool CliCapabilityService::detectCapabilities()
{
    m_snapshot.git = detectGit();
    m_snapshot.gh = detectGh();
    m_snapshot.agentCli = detectFirstAgentCli();

    qCInfo(qtcodeCore) << "CLI capabilities:"
                       << "git" << (m_snapshot.git.available ? m_snapshot.git.version : "missing")
                       << "gh" << (m_snapshot.gh.available ? m_snapshot.gh.version : "missing")
                       << "agent"
                       << (m_snapshot.agentCli.available ? m_snapshot.agentCli.displayName
                                                         : "missing");

    emit capabilitiesDetected();
    return true;
}

const CliCapabilitiesSnapshot &CliCapabilityService::snapshot() const
{
    return m_snapshot;
}

bool CliCapabilityService::isGitAvailable() const
{
    return m_snapshot.git.available;
}

bool CliCapabilityService::isGhAvailable() const
{
    return m_snapshot.gh.available;
}

bool CliCapabilityService::isAgentCliAvailable() const
{
    return m_snapshot.agentCli.available;
}

CliToolCapability CliCapabilityService::detectGit() const
{
    return probeExecutable(
        QStringLiteral("git"),
        QStringLiteral("Git"),
        QStringLiteral("git"),
        QStringLiteral(
            "Install Git to run repository command workflows. "
            "On Ubuntu/Debian: sudo apt install git"));
}

CliToolCapability CliCapabilityService::detectGh() const
{
    return probeExecutable(
        QStringLiteral("gh"),
        QStringLiteral("GitHub CLI"),
        QStringLiteral("gh"),
        QStringLiteral(
            "Install and authenticate GitHub CLI to browse issues and pull requests. "
            "On Ubuntu/Debian: sudo apt install gh, then run gh auth login"));
}

CliToolCapability CliCapabilityService::detectFirstAgentCli() const
{
    CliToolCapability unavailableCapability;
    unavailableCapability.toolId = QStringLiteral("agent");
    unavailableCapability.displayName = QStringLiteral("Agent CLI");
    unavailableCapability.available = false;
    unavailableCapability.unavailableMessage = QStringLiteral(
        "Install Codex CLI or another supported agent CLI to start AI sessions. "
        "Supported tools: Codex, Claude CLI, aider, and Cursor CLI.");

    for (const AgentCliCandidate &candidate : kAgentCliCandidates) {
        CliToolCapability capability = probeExecutable(
            QString::fromUtf8(candidate.executableName),
            QString::fromUtf8(candidate.displayName),
            QString::fromUtf8(candidate.executableName),
            unavailableCapability.unavailableMessage);
        if (capability.available) {
            return capability;
        }
    }

    return unavailableCapability;
}

CliToolCapability CliCapabilityService::probeExecutable(
    const QString &toolId,
    const QString &displayName,
    const QString &executableName,
    const QString &unavailableMessage) const
{
    CliToolCapability capability;
    capability.toolId = toolId;
    capability.displayName = displayName;
    capability.unavailableMessage = unavailableMessage;

    const QString executablePath =
        QStandardPaths::findExecutable(executableName);
    if (executablePath.isEmpty()) {
        return capability;
    }

    const shared::ProcessResult versionResult =
        shared::ProcessRunner::run(executablePath, {QStringLiteral("--version")}, kDetectionTimeoutMs);
    if (!versionResult.started || versionResult.exitCode != 0) {
        qCDebug(qtcodeCore) << "CLI probe failed for" << executableName << versionResult.standardError;
        return capability;
    }

    capability.available = true;
    capability.executablePath = executablePath;
    capability.version = firstOutputLine(versionResult.standardOutput);
    if (capability.version.isEmpty()) {
        capability.version = firstOutputLine(versionResult.standardError);
    }

    return capability;
}

QString CliCapabilityService::firstOutputLine(const QString &output)
{
    const int newlineIndex = output.indexOf(QLatin1Char('\n'));
    if (newlineIndex < 0) {
        return output.trimmed();
    }

    return output.left(newlineIndex).trimmed();
}

} // namespace qtcode::core

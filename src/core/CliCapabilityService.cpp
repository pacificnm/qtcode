#include "core/CliCapabilityService.h"

#include "shared/Logging.h"
#include "shared/PerformanceTimer.h"
#include "shared/ProcessRunner.h"

#include <QFutureWatcher>
#include <QStandardPaths>
#include <QtConcurrent>

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

CliCapabilityService::~CliCapabilityService()
{
    if (m_detectionWatcher != nullptr && m_detectionWatcher->isRunning()) {
        m_detectionWatcher->waitForFinished();
    }
}

bool CliCapabilityService::detectCapabilities()
{
    m_snapshot = buildCapabilitiesSnapshot();
    logCapabilitiesSnapshot(m_snapshot);
    emit capabilitiesDetected();
    return true;
}

void CliCapabilityService::scheduleDetection()
{
    if (m_detectionWatcher == nullptr) {
        m_detectionWatcher = new QFutureWatcher<CliCapabilitiesSnapshot>(this);
        connect(
            m_detectionWatcher,
            &QFutureWatcher<CliCapabilitiesSnapshot>::finished,
            this,
            &CliCapabilityService::onDetectionFinished);
    }

    if (m_detectionWatcher->isRunning()) {
        return;
    }

    m_detectionWatcher->setFuture(QtConcurrent::run([this]() {
        return buildCapabilitiesSnapshot();
    }));
}

bool CliCapabilityService::isDetectionRunning() const
{
    return m_detectionWatcher != nullptr && m_detectionWatcher->isRunning();
}

void CliCapabilityService::onDetectionFinished()
{
    if (m_detectionWatcher == nullptr) {
        return;
    }

    m_snapshot = m_detectionWatcher->result();
    logCapabilitiesSnapshot(m_snapshot);
    emit capabilitiesDetected();
}

CliCapabilitiesSnapshot CliCapabilityService::buildCapabilitiesSnapshot() const
{
    qtcode::shared::ScopedPerformanceTimer timer(qtcodeCore(), QStringLiteral("CLI capability detection"));

    CliCapabilitiesSnapshot snapshot;
    snapshot.git = detectGit();
    snapshot.gh = detectGh();
    snapshot.agentCli = detectFirstAgentCli();
    return snapshot;
}

void CliCapabilityService::logCapabilitiesSnapshot(const CliCapabilitiesSnapshot &snapshot) const
{
    qCInfo(qtcodeCore) << "CLI capabilities:"
                       << "git" << (snapshot.git.available ? snapshot.git.version : "missing")
                       << "gh"
                       << (snapshot.gh.available
                               ? (snapshot.gh.authenticated
                                      ? QStringLiteral("%1 authenticated as %2")
                                            .arg(snapshot.gh.version, snapshot.gh.authAccount)
                                      : QStringLiteral("%1 not authenticated").arg(snapshot.gh.version))
                               : QStringLiteral("missing"))
                       << "agent"
                       << (snapshot.agentCli.available ? snapshot.agentCli.displayName : "missing");
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

bool CliCapabilityService::isGhAuthenticated() const
{
    return m_snapshot.gh.available && m_snapshot.gh.authenticated;
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
    CliToolCapability capability = probeExecutable(
        QStringLiteral("gh"),
        QStringLiteral("GitHub CLI"),
        QStringLiteral("gh"),
        QStringLiteral(
            "Install GitHub CLI to browse issues and pull requests. "
            "On Ubuntu/Debian: sudo apt install gh"));

    if (!capability.available) {
        return capability;
    }

    capability.authUnavailableMessage = QStringLiteral(
        "GitHub CLI is installed but not authenticated. "
        "Run gh auth login to connect your GitHub account.");

    const shared::ProcessResult authResult = shared::ProcessRunner::run(
        capability.executablePath,
        {QStringLiteral("auth"), QStringLiteral("status")},
        kDetectionTimeoutMs);

    const QString authOutput =
        QStringLiteral("%1\n%2").arg(authResult.standardOutput, authResult.standardError).trimmed();

    if (authResult.started && authResult.exitCode == 0) {
        capability.authenticated = true;
        capability.authAccount = parseGhAuthAccount(authOutput);
        if (capability.authAccount.isEmpty()) {
            qCDebug(qtcodeCore) << "GitHub CLI authenticated but account name was not parsed";
        }
        return capability;
    }

    capability.authenticated = false;
    if (!authOutput.isEmpty()) {
        qCDebug(qtcodeCore) << "GitHub CLI auth status:" << authOutput;
    }

    return capability;
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

QString CliCapabilityService::parseGhAuthAccount(const QString &authStatusOutput)
{
    const QString accountPrefix = QStringLiteral("Logged in to github.com account ");
    for (const QString &line : authStatusOutput.split(QLatin1Char('\n'))) {
        const QString trimmedLine = line.trimmed();
        const int prefixIndex = trimmedLine.indexOf(accountPrefix);
        if (prefixIndex < 0) {
            continue;
        }

        QString account = trimmedLine.mid(prefixIndex + accountPrefix.size()).trimmed();
        const int suffixIndex = account.indexOf(QLatin1Char(' '));
        if (suffixIndex >= 0) {
            account = account.left(suffixIndex);
        }

        return account.trimmed();
    }

    return {};
}

} // namespace qtcode::core

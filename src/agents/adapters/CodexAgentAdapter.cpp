#include "agents/adapters/CodexAgentAdapter.h"

#include <QStandardPaths>

namespace qtcode::agents {

CodexAgentAdapter::CodexAgentAdapter(QObject *parent)
    : AgentAdapter(parent)
{
    m_executablePath = QStandardPaths::findExecutable(QStringLiteral("codex"));
}

QString CodexAgentAdapter::agentKey() const
{
    return QStringLiteral("codex");
}

QString CodexAgentAdapter::displayName() const
{
    return QStringLiteral("Codex CLI");
}

AgentCapabilities CodexAgentAdapter::capabilities() const
{
    return AgentCapability::StreamingText | AgentCapability::DiffOutput
        | AgentCapability::SupportsNonInteractiveMode | AgentCapability::SupportsProjectConfig;
}

bool CodexAgentAdapter::isAvailable() const
{
    return !m_executablePath.isEmpty();
}

QString CodexAgentAdapter::executablePath() const
{
    return m_executablePath;
}

bool CodexAgentAdapter::validateConfiguration(QString *errorMessage) const
{
    if (isAvailable()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Codex CLI was not found on PATH.");
    }
    return false;
}

bool CodexAgentAdapter::startRequest(const AgentRequest &request, QString *errorMessage)
{
    Q_UNUSED(request)

    if (m_requestInFlight) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Codex adapter already has a request in flight.");
        }
        return false;
    }

    if (!validateConfiguration(errorMessage)) {
        return false;
    }

    m_requestInFlight = true;

    AgentEvent event;
    event.type = AgentEventType::StatusUpdate;
    event.text = QStringLiteral("Codex adapter interfaces are defined; execution arrives in a later milestone.");
    emit eventEmitted(event);

    m_requestInFlight = false;
    emit requestFinished(AgentRequestStatus::Succeeded, {});
    return true;
}

void CodexAgentAdapter::cancelRequest()
{
    if (!m_requestInFlight) {
        return;
    }

    m_requestInFlight = false;
    emit requestFinished(AgentRequestStatus::Canceled, {});
}

void CodexAgentAdapter::setExecutablePath(const QString &executablePath)
{
    m_executablePath = executablePath;
}

} // namespace qtcode::agents

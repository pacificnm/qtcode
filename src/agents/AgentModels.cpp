#include "agents/AgentModels.h"

namespace qtcode::agents {

QString agentCapabilityLabel(AgentCapability capability)
{
    switch (capability) {
    case AgentCapability::StreamingText:
        return QStringLiteral("StreamingText");
    case AgentCapability::FileMutation:
        return QStringLiteral("FileMutation");
    case AgentCapability::DiffOutput:
        return QStringLiteral("DiffOutput");
    case AgentCapability::McpAware:
        return QStringLiteral("McpAware");
    case AgentCapability::RequiresTerminal:
        return QStringLiteral("RequiresTerminal");
    case AgentCapability::SupportsNonInteractiveMode:
        return QStringLiteral("SupportsNonInteractiveMode");
    case AgentCapability::SupportsProjectConfig:
        return QStringLiteral("SupportsProjectConfig");
    }

    return QStringLiteral("Unknown");
}

QString agentErrorKindLabel(AgentErrorKind kind)
{
    switch (kind) {
    case AgentErrorKind::MissingExecutable:
        return QStringLiteral("MissingExecutable");
    case AgentErrorKind::AuthenticationRequired:
        return QStringLiteral("AuthenticationRequired");
    case AgentErrorKind::InvalidConfiguration:
        return QStringLiteral("InvalidConfiguration");
    case AgentErrorKind::ProcessFailed:
        return QStringLiteral("ProcessFailed");
    case AgentErrorKind::RequestCanceled:
        return QStringLiteral("RequestCanceled");
    case AgentErrorKind::UnsupportedCapability:
        return QStringLiteral("UnsupportedCapability");
    case AgentErrorKind::RateLimited:
        return QStringLiteral("RateLimited");
    case AgentErrorKind::Unknown:
        return QStringLiteral("Unknown");
    }

    return QStringLiteral("Unknown");
}

QString agentSessionStatusLabel(AgentSessionStatus status)
{
    switch (status) {
    case AgentSessionStatus::Idle:
        return QStringLiteral("Idle");
    case AgentSessionStatus::Running:
        return QStringLiteral("Running");
    case AgentSessionStatus::Failed:
        return QStringLiteral("Failed");
    case AgentSessionStatus::Completed:
        return QStringLiteral("Completed");
    case AgentSessionStatus::Canceled:
        return QStringLiteral("Canceled");
    }

    return QStringLiteral("Unknown");
}

AgentSessionStatus agentSessionStatusFromLabel(const QString &label)
{
    const QString normalizedLabel = label.trimmed();
    if (normalizedLabel == QStringLiteral("Running")) {
        return AgentSessionStatus::Running;
    }
    if (normalizedLabel == QStringLiteral("Failed")) {
        return AgentSessionStatus::Failed;
    }
    if (normalizedLabel == QStringLiteral("Completed")) {
        return AgentSessionStatus::Completed;
    }
    if (normalizedLabel == QStringLiteral("Canceled")) {
        return AgentSessionStatus::Canceled;
    }

    return AgentSessionStatus::Idle;
}

} // namespace qtcode::agents

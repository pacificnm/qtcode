#pragma once

#include <QFlags>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace qtcode::agents {

enum class AgentCapability {
    StreamingText = 1 << 0,
    FileMutation = 1 << 1,
    DiffOutput = 1 << 2,
    McpAware = 1 << 3,
    RequiresTerminal = 1 << 4,
    SupportsNonInteractiveMode = 1 << 5,
    SupportsProjectConfig = 1 << 6,
};
Q_DECLARE_FLAGS(AgentCapabilities, AgentCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(AgentCapabilities)

enum class AgentSessionStatus {
    Idle,
    Running,
    Failed,
    Completed,
    Canceled,
};

enum class AgentRequestStatus {
    Succeeded,
    Failed,
    Canceled,
};

enum class AgentEventType {
    OutputText,
    StatusUpdate,
    ArtifactReady,
    Error,
};

enum class AgentErrorKind {
    MissingExecutable,
    AuthenticationRequired,
    InvalidConfiguration,
    ProcessFailed,
    RequestCanceled,
    UnsupportedCapability,
    RateLimited,
    Unknown,
};

struct AgentError
{
    AgentErrorKind kind = AgentErrorKind::Unknown;
    QString message;
};

struct AgentArtifact
{
    QString id;
    QString kind;
    QString title;
    QString content;
    QString filePath;
};

struct AgentMessage
{
    QString id;
    QString role;
    QString content;
    QString createdAt;
};

struct AgentRequest
{
    QString sessionId;
    QString projectId;
    QString prompt;
    QString workingDirectory;
    QStringList contextExcerpts;
    bool nonInteractive = false;
};

struct AgentEvent
{
    AgentEventType type = AgentEventType::OutputText;
    QString text;
    AgentArtifact artifact;
    AgentError error;
};

[[nodiscard]] QString agentCapabilityLabel(AgentCapability capability);
[[nodiscard]] QString agentErrorKindLabel(AgentErrorKind kind);
[[nodiscard]] QString agentSessionStatusLabel(AgentSessionStatus status);
[[nodiscard]] AgentSessionStatus agentSessionStatusFromLabel(const QString &label);

} // namespace qtcode::agents

Q_DECLARE_METATYPE(qtcode::agents::AgentEvent)
Q_DECLARE_METATYPE(qtcode::agents::AgentMessage)
Q_DECLARE_METATYPE(qtcode::agents::AgentArtifact)
Q_DECLARE_METATYPE(qtcode::agents::AgentRequestStatus)
Q_DECLARE_METATYPE(qtcode::agents::AgentSessionStatus)

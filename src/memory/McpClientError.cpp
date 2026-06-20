#include "memory/McpClientError.h"

namespace qtcode::memory {

McpClientError McpClientError::success()
{
    return {};
}

McpClientError McpClientError::failure(
    McpClientErrorCode code,
    const QString &message,
    const QString &detail)
{
    McpClientError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    return error;
}

QString mcpClientErrorCodeLabel(McpClientErrorCode code)
{
    switch (code) {
    case McpClientErrorCode::None:
        return QStringLiteral("None");
    case McpClientErrorCode::ServerDisabled:
        return QStringLiteral("ServerDisabled");
    case McpClientErrorCode::UnsupportedTransport:
        return QStringLiteral("UnsupportedTransport");
    case McpClientErrorCode::ProcessStartFailed:
        return QStringLiteral("ProcessStartFailed");
    case McpClientErrorCode::Timeout:
        return QStringLiteral("Timeout");
    case McpClientErrorCode::InvalidResponse:
        return QStringLiteral("InvalidResponse");
    case McpClientErrorCode::ToolCallFailed:
        return QStringLiteral("ToolCallFailed");
    case McpClientErrorCode::ToolReturnedError:
        return QStringLiteral("ToolReturnedError");
    }

    return QStringLiteral("Unknown");
}

} // namespace qtcode::memory

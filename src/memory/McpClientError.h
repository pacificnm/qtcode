#pragma once

#include <QString>

namespace qtcode::memory {

enum class McpClientErrorCode
{
    None,
    ServerDisabled,
    UnsupportedTransport,
    ProcessStartFailed,
    Timeout,
    InvalidResponse,
    ToolCallFailed,
    ToolReturnedError,
};

struct McpClientError
{
    McpClientErrorCode code = McpClientErrorCode::None;
    QString message;
    QString detail;

    [[nodiscard]] bool isSuccess() const
    {
        return code == McpClientErrorCode::None;
    }

    [[nodiscard]] static McpClientError success();
    [[nodiscard]] static McpClientError failure(
        McpClientErrorCode code,
        const QString &message,
        const QString &detail = {});
};

[[nodiscard]] QString mcpClientErrorCodeLabel(McpClientErrorCode code);

} // namespace qtcode::memory

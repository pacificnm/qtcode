#pragma once

#include <QString>

namespace qtcode::memory {

enum class McpHealthState
{
    Unknown,
    Checking,
    Healthy,
    Unhealthy,
};

struct McpHealthResult
{
    McpHealthState state = McpHealthState::Unknown;
    QString message;
    QString checkedAt;
    QString serverName;
    QString serverInfo;
};

[[nodiscard]] QString mcpHealthStateLabel(McpHealthState state);

} // namespace qtcode::memory

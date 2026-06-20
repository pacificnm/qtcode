#include "memory/McpHealthResult.h"

namespace qtcode::memory {

QString mcpHealthStateLabel(McpHealthState state)
{
    switch (state) {
    case McpHealthState::Unknown:
        return QStringLiteral("Unknown");
    case McpHealthState::Checking:
        return QStringLiteral("Checking");
    case McpHealthState::Healthy:
        return QStringLiteral("Healthy");
    case McpHealthState::Unhealthy:
        return QStringLiteral("Unavailable");
    }

    return QStringLiteral("Unknown");
}

} // namespace qtcode::memory

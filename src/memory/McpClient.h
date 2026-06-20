#pragma once

#include "memory/McpHealthResult.h"
#include "settings/McpServerModels.h"

namespace qtcode::memory {

class McpClient
{
public:
    [[nodiscard]] static McpHealthResult checkServerHealth(
        const settings::McpServerRecord &server,
        const QString &workingDirectory,
        int timeoutMs = 8000);
};

} // namespace qtcode::memory

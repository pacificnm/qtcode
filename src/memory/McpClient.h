#pragma once

#include "memory/McpClientError.h"
#include "memory/McpHealthResult.h"
#include "memory/MemorySearchModels.h"
#include "settings/McpServerModels.h"

namespace qtcode::memory {

class McpClient
{
public:
    [[nodiscard]] static McpHealthResult checkServerHealth(
        const settings::McpServerRecord &server,
        const QString &workingDirectory,
        int timeoutMs = 8000);

    [[nodiscard]] static MemorySearchOutcome searchProjectMemory(
        const settings::McpServerRecord &server,
        const QString &workingDirectory,
        const QString &query,
        const MemorySearchOptions &options = {},
        int timeoutMs = 15000);
};

} // namespace qtcode::memory

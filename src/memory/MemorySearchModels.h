#pragma once

#include "memory/ContextResult.h"
#include "memory/McpClientError.h"

namespace qtcode::memory {

struct MemorySearchOptions
{
    int limit = 8;
};

struct MemorySearchOutcome
{
    QList<ContextResult> results;
    McpClientError error = McpClientError::success();

    [[nodiscard]] bool isSuccess() const
    {
        return error.isSuccess();
    }
};

} // namespace qtcode::memory

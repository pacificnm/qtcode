#pragma once

#include "memory/ContextResult.h"
#include "settings/McpServerModels.h"
#include "settings/ProjectModels.h"

#include <QString>
#include <QStringList>

namespace qtcode::memory {
class MemoryService;
} // namespace qtcode::memory

namespace qtcode::core {

class McpServerService;

struct ContextRetrievalOutcome
{
    QString retrievalQuery;
    QList<memory::ContextResult> results;
    QStringList contextExcerpts;
    bool memorySearchAttempted = false;
    bool memoryUnavailable = false;
    QString statusMessage;
};

class ContextManager
{
public:
    ContextManager(
        memory::MemoryService *memoryService,
        McpServerService *mcpServerService);

    [[nodiscard]] ContextRetrievalOutcome retrieveForAgentRequest(
        const QString &prompt,
        const settings::ProjectRecord &project) const;

    [[nodiscard]] static QString buildRetrievalQuery(
        const QString &prompt,
        const settings::ProjectRecord &project);
    [[nodiscard]] static QStringList contextExcerptsFromResults(
        const QList<memory::ContextResult> &results);

private:
    [[nodiscard]] settings::McpServerRecord selectMemoryServer() const;

    memory::MemoryService *m_memoryService = nullptr;
    McpServerService *m_mcpServerService = nullptr;
};

} // namespace qtcode::core

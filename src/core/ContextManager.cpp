#include "core/ContextManager.h"

#include "core/McpServerService.h"
#include "memory/MemoryService.h"
#include "shared/Logging.h"

#include <QFileInfo>

namespace qtcode::core {

ContextManager::ContextManager(
    memory::MemoryService *memoryService,
    McpServerService *mcpServerService)
    : m_memoryService(memoryService)
    , m_mcpServerService(mcpServerService)
{
}

QString ContextManager::buildRetrievalQuery(
    const QString &prompt,
    const settings::ProjectRecord &project)
{
    QStringList queryParts;
    const QString trimmedPrompt = prompt.trimmed();
    if (!project.name.trimmed().isEmpty()) {
        queryParts.append(project.name.trimmed());
    }

    const QString rootPath = project.rootPath.trimmed();
    if (!rootPath.isEmpty()) {
        queryParts.append(QFileInfo(rootPath).fileName());
    }

    if (!trimmedPrompt.isEmpty()) {
        queryParts.append(trimmedPrompt);
    }

    return queryParts.join(QStringLiteral(" "));
}

QStringList ContextManager::contextExcerptsFromResults(
    const QList<memory::ContextResult> &results)
{
    QStringList excerpts;
    excerpts.reserve(results.size());

    for (const memory::ContextResult &result : results) {
        const QString title = result.title.trimmed().isEmpty() ? result.sourceUri : result.title;
        const QString excerpt = result.excerpt.trimmed();
        if (title.isEmpty() && excerpt.isEmpty()) {
            continue;
        }

        if (title.isEmpty()) {
            excerpts.append(excerpt);
            continue;
        }

        excerpts.append(
            excerpt.isEmpty() ? QStringLiteral("--- %1 ---").arg(title)
                              : QStringLiteral("--- %1 ---\n%2").arg(title, excerpt));
    }

    return excerpts;
}

settings::McpServerRecord ContextManager::selectMemoryServer() const
{
    if (m_mcpServerService == nullptr) {
        return {};
    }

    const QList<settings::McpServerRecord> servers = m_mcpServerService->servers();
    for (const settings::McpServerRecord &server : servers) {
        if (server.enabled && server.name == QStringLiteral("qtcode-memory")) {
            return server;
        }
    }

    for (const settings::McpServerRecord &server : servers) {
        if (server.enabled) {
            return server;
        }
    }

    return {};
}

ContextRetrievalOutcome ContextManager::retrieveForAgentRequest(
    const QString &prompt,
    const settings::ProjectRecord &project) const
{
    ContextRetrievalOutcome outcome;
    outcome.retrievalQuery = buildRetrievalQuery(prompt, project);

    if (m_memoryService == nullptr || m_mcpServerService == nullptr) {
        outcome.statusMessage = QStringLiteral("Project memory retrieval is unavailable.");
        return outcome;
    }

    const settings::McpServerRecord memoryServer = selectMemoryServer();
    if (memoryServer.id.isEmpty()) {
        outcome.statusMessage = QStringLiteral("No enabled MCP memory server is configured.");
        return outcome;
    }

    outcome.memorySearchAttempted = true;
    const memory::MemorySearchOutcome searchOutcome = m_memoryService->searchProjectMemory(
        memoryServer,
        project.rootPath,
        outcome.retrievalQuery);

    if (!searchOutcome.isSuccess()) {
        outcome.memoryUnavailable = true;
        outcome.statusMessage = searchOutcome.error.detail.isEmpty()
            ? searchOutcome.error.message
            : QStringLiteral("%1 %2").arg(searchOutcome.error.message, searchOutcome.error.detail);
        qCWarning(qtcodeCore) << "Context retrieval failed for project" << project.name
                              << outcome.statusMessage;
        return outcome;
    }

    outcome.results = searchOutcome.results;
    outcome.contextExcerpts = contextExcerptsFromResults(outcome.results);
    if (outcome.results.isEmpty()) {
        outcome.statusMessage = QStringLiteral("No matching project memory found.");
    } else {
        outcome.statusMessage =
            QStringLiteral("Retrieved %1 project memory result(s).").arg(outcome.results.size());
    }

    qCInfo(qtcodeCore) << "Context retrieval for" << project.name << "returned"
                       << outcome.results.size() << "result(s)";
    return outcome;
}

} // namespace qtcode::core

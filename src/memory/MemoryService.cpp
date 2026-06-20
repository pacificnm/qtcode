#include "memory/MemoryService.h"

#include "memory/McpClient.h"
#include "shared/Logging.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace qtcode::memory {
MemoryService::MemoryService(QObject *parent)
    : QObject(parent)
{
}

MemoryService::~MemoryService() = default;

McpHealthResult MemoryService::healthForServer(const QString &serverId) const
{
    return m_healthByServerId.value(serverId);
}

void MemoryService::checkServerHealth(
    const settings::McpServerRecord &server,
    const QString &workingDirectory)
{
    startHealthCheck(server, workingDirectory);
}

void MemoryService::checkEnabledServers(
    const QList<settings::McpServerRecord> &servers,
    const QString &workingDirectory)
{
    for (const settings::McpServerRecord &server : servers) {
        if (!server.enabled) {
            continue;
        }
        startHealthCheck(server, workingDirectory);
    }
}

void MemoryService::startHealthCheck(
    const settings::McpServerRecord &server,
    const QString &workingDirectory)
{
    if (server.id.isEmpty()) {
        return;
    }

    McpHealthResult checking;
    checking.state = McpHealthState::Checking;
    checking.message = QStringLiteral("Checking MCP server availability...");
    checking.serverName = server.name;
    checking.checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_healthByServerId.insert(server.id, checking);
    emit serverHealthUpdated(server.id, checking);

    auto *watcher = new QFutureWatcher<McpHealthResult>(this);
    connect(watcher, &QFutureWatcher<McpHealthResult>::finished, this, [this, watcher, server]() {
        const McpHealthResult result = watcher->result();
        m_healthByServerId.insert(server.id, result);
        emit serverHealthUpdated(server.id, result);
        qCInfo(qtcodeMemory) << "MCP health check for" << server.name << mcpHealthStateLabel(result.state)
                             << result.message;
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([server, workingDirectory]() {
        return McpClient::checkServerHealth(server, workingDirectory);
    }));
}

MemorySearchOutcome MemoryService::searchProjectMemory(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    const QString &query,
    const MemorySearchOptions &options) const
{
    const MemorySearchOutcome outcome =
        McpClient::searchProjectMemory(server, workingDirectory, query, options);
    if (!outcome.isSuccess()) {
        qCWarning(qtcodeMemory) << "Project memory search failed for" << server.name
                                << mcpClientErrorCodeLabel(outcome.error.code) << outcome.error.message
                                << outcome.error.detail;
    } else {
        qCInfo(qtcodeMemory) << "Project memory search returned" << outcome.results.size()
                             << "result(s) for" << server.name;
    }
    return outcome;
}

} // namespace qtcode::memory

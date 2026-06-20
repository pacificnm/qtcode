#include "core/McpServerService.h"

#include "shared/Logging.h"
#include "storage/repositories/McpServerRepository.h"
#include "storage/StorageService.h"

#include <QDateTime>
#include <QUuid>

namespace qtcode::core {

McpServerService::McpServerService(storage::StorageService &storageService, QObject *parent)
    : QObject(parent)
    , m_storageService(storageService)
{
}

bool McpServerService::ensureDefaultConfiguration(QString *errorMessage)
{
    if (!reloadServers(errorMessage)) {
        return false;
    }

    if (!m_servers.isEmpty()) {
        return true;
    }

    settings::McpServerRecord defaultServer = settings::McpServerRecord::defaultQtcodeMemoryServer();
    defaultServer.id = createId();
    defaultServer.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    defaultServer.updatedAt = defaultServer.createdAt;

    storage::McpServerRepository repository(m_storageService);
    if (!repository.insertServer(defaultServer, errorMessage)) {
        return false;
    }

    m_servers.append(defaultServer);
    emit serversChanged();
    qCInfo(qtcodeMemory) << "Seeded default MCP server" << defaultServer.name;
    return true;
}

QList<settings::McpServerRecord> McpServerService::servers() const
{
    return m_servers;
}

bool McpServerService::saveServer(settings::McpServerRecord *server, QString *errorMessage)
{
    if (server == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server record must not be null.");
        }
        return false;
    }

    if (server->name.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server name must not be empty.");
        }
        return false;
    }

    if (server->endpoint.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server endpoint must not be empty.");
        }
        return false;
    }

    if (server->transport.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server transport must not be empty.");
        }
        return false;
    }

    server->name = server->name.trimmed();
    server->endpoint = server->endpoint.trimmed();
    server->transport = server->transport.trimmed();
    server->updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    storage::McpServerRepository repository(m_storageService);
    const bool isNew = server->id.isEmpty();
    if (isNew) {
        server->id = createId();
        server->createdAt = server->updatedAt;
        if (!repository.insertServer(*server, errorMessage)) {
            return false;
        }
    } else if (!repository.updateServer(*server, errorMessage)) {
        return false;
    }

    if (!reloadServers(errorMessage)) {
        return false;
    }

    emit serversChanged();
    return true;
}

bool McpServerService::deleteServer(const QString &serverId, QString *errorMessage)
{
    storage::McpServerRepository repository(m_storageService);
    if (!repository.deleteServer(serverId, errorMessage)) {
        return false;
    }

    if (!reloadServers(errorMessage)) {
        return false;
    }

    emit serversChanged();
    return true;
}

QString McpServerService::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool McpServerService::reloadServers(QString *errorMessage)
{
    storage::McpServerRepository repository(m_storageService);
    QList<settings::McpServerRecord> servers;
    if (!repository.listServers(&servers, errorMessage)) {
        return false;
    }

    m_servers = servers;
    return true;
}

} // namespace qtcode::core

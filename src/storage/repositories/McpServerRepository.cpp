#include "storage/repositories/McpServerRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

settings::McpServerRecord serverFromQuery(const QSqlQuery &query)
{
    settings::McpServerRecord server;
    server.id = query.value(QStringLiteral("id")).toString();
    server.name = query.value(QStringLiteral("name")).toString();
    server.endpoint = query.value(QStringLiteral("endpoint")).toString();
    server.transport = query.value(QStringLiteral("transport")).toString();
    server.enabled = query.value(QStringLiteral("enabled")).toInt() != 0;
    server.createdAt = query.value(QStringLiteral("created_at")).toString();
    server.updatedAt = query.value(QStringLiteral("updated_at")).toString();

    const QByteArray configBytes = query.value(QStringLiteral("config_json")).toByteArray();
    if (!configBytes.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(configBytes);
        if (document.isObject()) {
            server.configJson = document.object();
        }
    }

    return server;
}

} // namespace

McpServerRepository::McpServerRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool McpServerRepository::listServers(
    QList<settings::McpServerRecord> *servers,
    QString *errorMessage) const
{
    if (servers == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server list output must not be null.");
        }
        return false;
    }

    servers->clear();

    QSqlQuery query(m_storageService.database());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, endpoint, transport, config_json, enabled, created_at, updated_at "
            "FROM mcp_servers "
            "ORDER BY name ASC"))) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list MCP servers: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list MCP servers" << message;
        return false;
    }

    while (query.next()) {
        servers->append(serverFromQuery(query));
    }

    return true;
}

bool McpServerRepository::insertServer(
    const settings::McpServerRecord &server,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO mcp_servers ("
        "id, name, endpoint, transport, config_json, enabled, created_at, updated_at) "
        "VALUES ("
        ":id, :name, :endpoint, :transport, :config_json, :enabled, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":id"), server.id);
    query.bindValue(QStringLiteral(":name"), server.name);
    query.bindValue(QStringLiteral(":endpoint"), server.endpoint);
    query.bindValue(QStringLiteral(":transport"), server.transport);
    query.bindValue(
        QStringLiteral(":config_json"),
        QString::fromUtf8(QJsonDocument(server.configJson).toJson(QJsonDocument::Compact)));
    query.bindValue(QStringLiteral(":enabled"), server.enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":created_at"), server.createdAt);
    query.bindValue(QStringLiteral(":updated_at"), server.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert MCP server: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert MCP server" << message;
        return false;
    }

    return true;
}

bool McpServerRepository::updateServer(
    const settings::McpServerRecord &server,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE mcp_servers SET "
        "name = :name, "
        "endpoint = :endpoint, "
        "transport = :transport, "
        "config_json = :config_json, "
        "enabled = :enabled, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), server.id);
    query.bindValue(QStringLiteral(":name"), server.name);
    query.bindValue(QStringLiteral(":endpoint"), server.endpoint);
    query.bindValue(QStringLiteral(":transport"), server.transport);
    query.bindValue(
        QStringLiteral(":config_json"),
        QString::fromUtf8(QJsonDocument(server.configJson).toJson(QJsonDocument::Compact)));
    query.bindValue(QStringLiteral(":enabled"), server.enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":updated_at"), server.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update MCP server: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to update MCP server" << message;
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool McpServerRepository::deleteServer(const QString &serverId, QString *errorMessage)
{
    if (serverId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("MCP server id must not be empty.");
        }
        return false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral("DELETE FROM mcp_servers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), serverId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to delete MCP server: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to delete MCP server" << message;
        return false;
    }

    return query.numRowsAffected() > 0;
}

} // namespace qtcode::storage

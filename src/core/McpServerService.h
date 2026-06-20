#pragma once

#include "settings/McpServerModels.h"

#include <QList>
#include <QString>

#include <QObject>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class McpServerService final : public QObject
{
    Q_OBJECT

public:
    explicit McpServerService(storage::StorageService &storageService, QObject *parent = nullptr);

    [[nodiscard]] bool ensureDefaultConfiguration(QString *errorMessage = nullptr);
    [[nodiscard]] QList<settings::McpServerRecord> servers() const;
    [[nodiscard]] bool saveServer(
        settings::McpServerRecord *server,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool deleteServer(
        const QString &serverId,
        QString *errorMessage = nullptr);

signals:
    void serversChanged();

private:
    [[nodiscard]] static QString createId();
    [[nodiscard]] bool reloadServers(QString *errorMessage = nullptr);

    storage::StorageService &m_storageService;
    QList<settings::McpServerRecord> m_servers;
};

} // namespace qtcode::core

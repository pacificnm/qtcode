#pragma once

#include "settings/McpServerModels.h"

#include <QList>
#include <QString>

namespace qtcode::storage {

class StorageService;

class McpServerRepository
{
public:
    explicit McpServerRepository(StorageService &storageService);

    [[nodiscard]] bool listServers(
        QList<settings::McpServerRecord> *servers,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool insertServer(
        const settings::McpServerRecord &server,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool updateServer(
        const settings::McpServerRecord &server,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool deleteServer(
        const QString &serverId,
        QString *errorMessage = nullptr);

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

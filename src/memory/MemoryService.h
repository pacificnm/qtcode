#pragma once

#include "memory/McpHealthResult.h"
#include "memory/MemorySearchModels.h"
#include "settings/McpServerModels.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace qtcode::memory {

class MemoryService final : public QObject
{
    Q_OBJECT

public:
    explicit MemoryService(QObject *parent = nullptr);
    ~MemoryService() override;

    [[nodiscard]] McpHealthResult healthForServer(const QString &serverId) const;
    void checkServerHealth(
        const settings::McpServerRecord &server,
        const QString &workingDirectory);
    void checkEnabledServers(
        const QList<settings::McpServerRecord> &servers,
        const QString &workingDirectory);

    [[nodiscard]] MemorySearchOutcome searchProjectMemory(
        const settings::McpServerRecord &server,
        const QString &workingDirectory,
        const QString &query,
        const MemorySearchOptions &options = {}) const;

signals:
    void serverHealthUpdated(const QString &serverId, const qtcode::memory::McpHealthResult &result);

private:
    void startHealthCheck(
        const settings::McpServerRecord &server,
        const QString &workingDirectory);

    QHash<QString, McpHealthResult> m_healthByServerId;
};

} // namespace qtcode::memory

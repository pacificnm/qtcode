#pragma once

#include <QString>
#include <QStringList>

namespace qtcode::storage {

class StorageService;

struct SchemaMigration
{
    int version = 0;
    QString name;
    QStringList statements;
};

class MigrationRunner
{
public:
    explicit MigrationRunner(StorageService &storageService);

    [[nodiscard]] bool runPendingMigrations(QString *errorMessage = nullptr);

private:
    [[nodiscard]] bool ensureMigrationTable(QString *errorMessage) const;
    [[nodiscard]] bool isMigrationApplied(int version, bool *isApplied, QString *errorMessage) const;
    [[nodiscard]] bool applyMigration(const SchemaMigration &migration, QString *errorMessage);
    [[nodiscard]] static QList<SchemaMigration> registeredMigrations();

    StorageService &m_storageService;
};

} // namespace qtcode::storage

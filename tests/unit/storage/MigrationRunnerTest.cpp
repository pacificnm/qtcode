#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace {

[[nodiscard]] bool tableExists(qtcode::storage::StorageService &storageService, const QString &tableName)
{
    QSqlQuery query(storageService.database());
    query.prepare(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name = :table_name"));
    query.bindValue(QStringLiteral(":table_name"), tableName);
    if (!query.exec()) {
        return false;
    }

    return query.next();
}

[[nodiscard]] int migrationCount(qtcode::storage::StorageService &storageService)
{
    QSqlQuery query(storageService.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM schema_migrations"))) {
        return -1;
    }

    if (!query.next()) {
        return -1;
    }

    return query.value(0).toInt();
}

} // namespace

class MigrationRunnerTest final : public QObject
{
    Q_OBJECT

private slots:
    void migrationsApplyToEmptyDatabase();
    void rerunIsSafe();
    void schemaMigrationsTableIsRecorded();
};

void MigrationRunnerTest::migrationsApplyToEmptyDatabase()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("migration_test.db")));
    QString errorMessage;
    QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

    qtcode::storage::MigrationRunner migrationRunner(storageService);
    QVERIFY2(migrationRunner.runPendingMigrations(&errorMessage), qPrintable(errorMessage));

    static const QStringList expectedTables = {
        QStringLiteral("schema_migrations"),
        QStringLiteral("settings"),
        QStringLiteral("projects"),
        QStringLiteral("repositories"),
        QStringLiteral("agent_configs"),
        QStringLiteral("project_agent_preferences"),
        QStringLiteral("agent_sessions"),
        QStringLiteral("agent_messages"),
        QStringLiteral("context_retrievals"),
        QStringLiteral("context_results"),
        QStringLiteral("terminal_sessions"),
        QStringLiteral("mcp_servers"),
        QStringLiteral("github_cache"),
    };

    for (const QString &tableName : expectedTables) {
        QVERIFY2(tableExists(storageService, tableName), qPrintable(tableName));
    }
}

void MigrationRunnerTest::rerunIsSafe()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("migration_test.db")));
    QString errorMessage;
    QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

    qtcode::storage::MigrationRunner migrationRunner(storageService);
    QVERIFY2(migrationRunner.runPendingMigrations(&errorMessage), qPrintable(errorMessage));
    QVERIFY2(migrationRunner.runPendingMigrations(&errorMessage), qPrintable(errorMessage));

    QCOMPARE(migrationCount(storageService), 1);
}

void MigrationRunnerTest::schemaMigrationsTableIsRecorded()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("migration_test.db")));
    QString errorMessage;
    QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

    qtcode::storage::MigrationRunner migrationRunner(storageService);
    QVERIFY2(migrationRunner.runPendingMigrations(&errorMessage), qPrintable(errorMessage));

    QSqlQuery query(storageService.database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT version, name, applied_at FROM schema_migrations ORDER BY version")));

    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QCOMPARE(query.value(1).toString(), QStringLiteral("initial_schema"));
    QVERIFY(!query.value(2).toString().isEmpty());
    QVERIFY(!query.next());
}

QTEST_MAIN(MigrationRunnerTest)

#include "MigrationRunnerTest.moc"

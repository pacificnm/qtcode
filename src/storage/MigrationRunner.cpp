#include "storage/MigrationRunner.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QDateTime>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

QString formatMigrationFailure(const SchemaMigration &migration, int statementIndex, const QString &details)
{
    return QStringLiteral("Migration %1 (%2) failed at statement %3: %4")
        .arg(migration.version)
        .arg(migration.name)
        .arg(statementIndex + 1)
        .arg(details);
}

SchemaMigration initialSchemaMigration()
{
    SchemaMigration migration;
    migration.version = 1;
    migration.name = QStringLiteral("initial_schema");
    migration.statements = {
        QStringLiteral(
            "CREATE TABLE settings ("
            "key TEXT PRIMARY KEY,"
            "value_json TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE projects ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "root_path TEXT NOT NULL UNIQUE,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "last_opened_at TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE repositories ("
            "id TEXT PRIMARY KEY,"
            "project_id TEXT NOT NULL,"
            "local_path TEXT NOT NULL,"
            "remote_url TEXT,"
            "default_branch TEXT,"
            "github_owner TEXT,"
            "github_name TEXT,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "FOREIGN KEY (project_id) REFERENCES projects(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE agent_configs ("
            "id TEXT PRIMARY KEY,"
            "agent_key TEXT NOT NULL,"
            "display_name TEXT NOT NULL,"
            "config_json TEXT NOT NULL,"
            "enabled INTEGER NOT NULL DEFAULT 1,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE project_agent_preferences ("
            "project_id TEXT PRIMARY KEY,"
            "agent_key TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "FOREIGN KEY (project_id) REFERENCES projects(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE agent_sessions ("
            "id TEXT PRIMARY KEY,"
            "project_id TEXT NOT NULL,"
            "agent_key TEXT NOT NULL,"
            "title TEXT NOT NULL,"
            "status TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "FOREIGN KEY (project_id) REFERENCES projects(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE agent_messages ("
            "id TEXT PRIMARY KEY,"
            "session_id TEXT NOT NULL,"
            "role TEXT NOT NULL,"
            "content TEXT NOT NULL,"
            "metadata_json TEXT,"
            "created_at TEXT NOT NULL,"
            "FOREIGN KEY (session_id) REFERENCES agent_sessions(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE context_retrievals ("
            "id TEXT PRIMARY KEY,"
            "session_id TEXT,"
            "project_id TEXT NOT NULL,"
            "query TEXT NOT NULL,"
            "provider_key TEXT NOT NULL,"
            "result_count INTEGER NOT NULL,"
            "metadata_json TEXT,"
            "created_at TEXT NOT NULL,"
            "FOREIGN KEY (session_id) REFERENCES agent_sessions(id),"
            "FOREIGN KEY (project_id) REFERENCES projects(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE context_results ("
            "id TEXT PRIMARY KEY,"
            "retrieval_id TEXT NOT NULL,"
            "source_type TEXT NOT NULL,"
            "source_uri TEXT,"
            "title TEXT,"
            "excerpt TEXT NOT NULL,"
            "score REAL,"
            "metadata_json TEXT,"
            "FOREIGN KEY (retrieval_id) REFERENCES context_retrievals(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE terminal_sessions ("
            "id TEXT PRIMARY KEY,"
            "project_id TEXT,"
            "title TEXT NOT NULL,"
            "shell_path TEXT NOT NULL,"
            "working_directory TEXT NOT NULL,"
            "profile_json TEXT,"
            "last_command TEXT,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL,"
            "FOREIGN KEY (project_id) REFERENCES projects(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE mcp_servers ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "endpoint TEXT NOT NULL,"
            "transport TEXT NOT NULL,"
            "config_json TEXT NOT NULL,"
            "enabled INTEGER NOT NULL DEFAULT 1,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE github_cache ("
            "id TEXT PRIMARY KEY,"
            "repository_id TEXT NOT NULL,"
            "object_type TEXT NOT NULL,"
            "object_key TEXT NOT NULL,"
            "payload_json TEXT NOT NULL,"
            "fetched_at TEXT NOT NULL,"
            "FOREIGN KEY (repository_id) REFERENCES repositories(id),"
            "UNIQUE(repository_id, object_type, object_key)"
            ")"),
    };
    return migration;
}

} // namespace

MigrationRunner::MigrationRunner(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool MigrationRunner::runPendingMigrations(QString *errorMessage)
{
    if (!m_storageService.isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot run migrations before the database is open.");
        }
        return false;
    }

    if (!ensureMigrationTable(errorMessage)) {
        return false;
    }

    for (const SchemaMigration &migration : registeredMigrations()) {
        bool alreadyApplied = false;
        if (!isMigrationApplied(migration.version, &alreadyApplied, errorMessage)) {
            return false;
        }

        if (alreadyApplied) {
            qCDebug(qtcodeStorage) << "Skipping already applied migration" << migration.version
                                   << migration.name;
            continue;
        }

        qCInfo(qtcodeStorage) << "Applying migration" << migration.version << migration.name;
        if (!applyMigration(migration, errorMessage)) {
            qCWarning(qtcodeStorage) << "Migration failed:" << (errorMessage != nullptr ? *errorMessage : QString());
            return false;
        }
    }

    return true;
}

bool MigrationRunner::ensureMigrationTable(QString *errorMessage) const
{
    QSqlQuery query(m_storageService.database());
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "version INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "applied_at TEXT NOT NULL"
            ")"))) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to ensure schema_migrations table: %1").arg(message);
        }
        return false;
    }

    return true;
}

bool MigrationRunner::isMigrationApplied(int version, bool *isApplied, QString *errorMessage) const
{
    if (isApplied != nullptr) {
        *isApplied = false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral("SELECT version FROM schema_migrations WHERE version = :version"));
    query.bindValue(QStringLiteral(":version"), version);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query schema_migrations: %1").arg(message);
        }
        return false;
    }

    if (isApplied != nullptr) {
        *isApplied = query.next();
    }

    return true;
}

bool MigrationRunner::applyMigration(const SchemaMigration &migration, QString *errorMessage)
{
    if (!m_storageService.beginTransaction(errorMessage)) {
        if (errorMessage != nullptr) {
            *errorMessage = formatMigrationFailure(migration, 0, *errorMessage);
        }
        return false;
    }

    int statementIndex = 0;
    for (const QString &statement : migration.statements) {
        QSqlQuery query(m_storageService.database());
        if (!query.exec(statement)) {
            const QString details = query.lastError().text();
            if (errorMessage != nullptr) {
                *errorMessage = formatMigrationFailure(migration, statementIndex, details);
            }
            if (!m_storageService.rollbackTransaction()) {
                // Best-effort rollback after a failed migration statement.
            }
            return false;
        }
        ++statementIndex;
    }

    QSqlQuery insertQuery(m_storageService.database());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO schema_migrations (version, name, applied_at) "
        "VALUES (:version, :name, :applied_at)"));
    insertQuery.bindValue(QStringLiteral(":version"), migration.version);
    insertQuery.bindValue(QStringLiteral(":name"), migration.name);
    insertQuery.bindValue(
        QStringLiteral(":applied_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!insertQuery.exec()) {
        const QString details = insertQuery.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = formatMigrationFailure(migration, statementIndex, details);
        }
        if (!m_storageService.rollbackTransaction()) {
            // Best-effort rollback after a failed migration record insert.
        }
        return false;
    }

    if (!m_storageService.commitTransaction(errorMessage)) {
        if (errorMessage != nullptr) {
            *errorMessage = formatMigrationFailure(migration, statementIndex, *errorMessage);
        }
        return false;
    }

    qCInfo(qtcodeStorage) << "Applied migration" << migration.version << migration.name;
    return true;
}

QList<SchemaMigration> MigrationRunner::registeredMigrations()
{
    return {initialSchemaMigration()};
}

} // namespace qtcode::storage

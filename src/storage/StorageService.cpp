#include "storage/StorageService.h"

#include "shared/Logging.h"
#include "storage/StorageTransaction.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace qtcode::storage {

namespace {

constexpr auto kConnectionPrefix = "qtcode-storage-";

bool ensureParentDirectoryExists(const QString &databasePath, QString *errorMessage)
{
    const QFileInfo fileInfo(databasePath);
    const QString parentPath = fileInfo.absolutePath();

    if (parentPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Database path has no parent directory.");
        }
        return false;
    }

    QDir parentDirectory(parentPath);
    if (parentDirectory.exists()) {
        return true;
    }

    if (!parentDirectory.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to create database directory at %1").arg(parentPath);
        }
        return false;
    }

    return true;
}

} // namespace

QString StorageService::defaultDatabasePath()
{
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QDir directory(dataDirectory);
    return directory.filePath(QStringLiteral("qtcode.db"));
}

StorageService::StorageService(QString databasePath)
    : m_databasePath(databasePath.isEmpty() ? defaultDatabasePath() : std::move(databasePath))
    , m_connectionName(kConnectionPrefix + QString::number(reinterpret_cast<quintptr>(this), 16))
{
}

StorageService::~StorageService()
{
    close();
}

bool StorageService::open(QString *errorMessage)
{
    if (isOpen()) {
        return true;
    }

    if (!ensureParentDirectoryExists(m_databasePath, errorMessage)) {
        qCWarning(qtcodeStorage) << "Unable to prepare database directory for" << m_databasePath;
        return false;
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_database.setDatabaseName(m_databasePath);
    }

    if (!m_database.open()) {
        const QString message = m_database.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        qCWarning(qtcodeStorage) << "Failed to open SQLite database at" << m_databasePath << message;
        return false;
    }

    QSqlQuery foreignKeysQuery(m_database);
    if (!foreignKeysQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        const QString message = foreignKeysQuery.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        qCWarning(qtcodeStorage) << "Failed to enable SQLite foreign keys:" << message;
        close();
        return false;
    }

    qCInfo(qtcodeStorage) << "Opened SQLite database at" << m_databasePath;
    return true;
}

void StorageService::close()
{
    if (m_transactionActive) {
        if (!rollbackTransaction()) {
            // Best-effort rollback while closing the database.
        }
    }

    if (!m_database.isValid()) {
        return;
    }

    const QString connectionName = m_database.connectionName();
    if (m_database.isOpen()) {
        m_database.close();
        qCInfo(qtcodeStorage) << "Closed SQLite database at" << m_databasePath;
    }

    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool StorageService::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

QString StorageService::databasePath() const
{
    return m_databasePath;
}

QSqlDatabase StorageService::database() const
{
    return m_database;
}

bool StorageService::beginTransaction(QString *errorMessage)
{
    if (!isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot begin a transaction before the database is open.");
        }
        return false;
    }

    if (m_transactionActive) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("A storage transaction is already active.");
        }
        return false;
    }

    if (!m_database.transaction()) {
        const QString message = m_database.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        qCWarning(qtcodeStorage) << "Failed to begin transaction:" << message;
        return false;
    }

    m_transactionActive = true;
    qCDebug(qtcodeStorage) << "Started storage transaction";
    return true;
}

bool StorageService::commitTransaction(QString *errorMessage)
{
    if (!m_transactionActive) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No active storage transaction to commit.");
        }
        return false;
    }

    if (!m_database.commit()) {
        const QString message = m_database.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        qCWarning(qtcodeStorage) << "Failed to commit transaction:" << message;
        return false;
    }

    m_transactionActive = false;
    qCDebug(qtcodeStorage) << "Committed storage transaction";
    return true;
}

bool StorageService::rollbackTransaction(QString *errorMessage)
{
    if (!m_transactionActive) {
        return true;
    }

    if (!m_database.rollback()) {
        const QString message = m_database.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        qCWarning(qtcodeStorage) << "Failed to roll back transaction:" << message;
        m_transactionActive = false;
        return false;
    }

    m_transactionActive = false;
    qCDebug(qtcodeStorage) << "Rolled back storage transaction";
    return true;
}

bool StorageService::hasActiveTransaction() const
{
    return m_transactionActive;
}

StorageTransaction StorageService::createTransaction()
{
    return StorageTransaction(*this);
}

} // namespace qtcode::storage

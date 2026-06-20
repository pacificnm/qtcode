#pragma once

#include <QSqlDatabase>
#include <QString>

namespace qtcode::storage {

class StorageTransaction;

class StorageService
{
public:
    [[nodiscard]] static QString defaultDatabasePath();

    explicit StorageService(QString databasePath = {});
    ~StorageService();

    StorageService(const StorageService &) = delete;
    StorageService &operator=(const StorageService &) = delete;

    [[nodiscard]] bool open(QString *errorMessage = nullptr);
    void close();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString databasePath() const;
    [[nodiscard]] QSqlDatabase database() const;

    [[nodiscard]] bool beginTransaction(QString *errorMessage = nullptr);
    [[nodiscard]] bool commitTransaction(QString *errorMessage = nullptr);
    [[nodiscard]] bool rollbackTransaction(QString *errorMessage = nullptr);
    [[nodiscard]] bool hasActiveTransaction() const;

    [[nodiscard]] StorageTransaction createTransaction();

private:
    friend class StorageTransaction;

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
    bool m_transactionActive = false;
};

} // namespace qtcode::storage

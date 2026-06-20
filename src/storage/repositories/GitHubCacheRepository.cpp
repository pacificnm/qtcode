#include "storage/repositories/GitHubCacheRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace qtcode::storage {

GitHubCacheRepository::GitHubCacheRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool GitHubCacheRepository::upsertEntry(
    const QString &repositoryId,
    const QString &objectType,
    const QString &objectKey,
    const QJsonObject &payload,
    const QString &fetchedAt,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO github_cache ("
        "id, repository_id, object_type, object_key, payload_json, fetched_at"
        ") VALUES ("
        ":id, :repository_id, :object_type, :object_key, :payload_json, :fetched_at"
        ") ON CONFLICT(repository_id, object_type, object_key) DO UPDATE SET "
        "payload_json = excluded.payload_json, "
        "fetched_at = excluded.fetched_at"));
    query.bindValue(QStringLiteral(":id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":repository_id"), repositoryId);
    query.bindValue(QStringLiteral(":object_type"), objectType);
    query.bindValue(QStringLiteral(":object_key"), objectKey);
    query.bindValue(
        QStringLiteral(":payload_json"),
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    query.bindValue(QStringLiteral(":fetched_at"), fetchedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to upsert GitHub cache entry: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to upsert GitHub cache entry" << message;
        return false;
    }

    return true;
}

bool GitHubCacheRepository::loadEntry(
    const QString &repositoryId,
    const QString &objectType,
    const QString &objectKey,
    QJsonObject *payload,
    QString *fetchedAt,
    bool *found,
    QString *errorMessage) const
{
    if (found != nullptr) {
        *found = false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT payload_json, fetched_at "
        "FROM github_cache "
        "WHERE repository_id = :repository_id "
        "AND object_type = :object_type "
        "AND object_key = :object_key "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":repository_id"), repositoryId);
    query.bindValue(QStringLiteral(":object_type"), objectType);
    query.bindValue(QStringLiteral(":object_key"), objectKey);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to load GitHub cache entry: %1").arg(message);
        }
        return false;
    }

    if (!query.next()) {
        return true;
    }

    if (payload != nullptr) {
        const QJsonDocument document =
            QJsonDocument::fromJson(query.value(QStringLiteral("payload_json")).toByteArray());
        if (document.isObject()) {
            *payload = document.object();
        } else {
            *payload = QJsonObject {};
        }
    }

    if (fetchedAt != nullptr) {
        *fetchedAt = query.value(QStringLiteral("fetched_at")).toString();
    }

    if (found != nullptr) {
        *found = true;
    }

    return true;
}

} // namespace qtcode::storage

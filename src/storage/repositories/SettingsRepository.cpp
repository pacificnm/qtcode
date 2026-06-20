#include "storage/repositories/SettingsRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

SettingsRepository::SettingsRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool SettingsRepository::upsertJson(
    const QString &key,
    const QJsonObject &value,
    QString *errorMessage)
{
    if (!m_storageService.isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot save settings before the database is open.");
        }
        return false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO settings (key, value_json, updated_at) "
        "VALUES (:key, :value_json, :updated_at) "
        "ON CONFLICT(key) DO UPDATE SET "
        "value_json = excluded.value_json, "
        "updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(
        QStringLiteral(":value_json"),
        QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
    query.bindValue(
        QStringLiteral(":updated_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to save setting '%1': %2").arg(key, message);
        }
        qCWarning(qtcodeStorage) << "Failed to save setting" << key << message;
        return false;
    }

    qCDebug(qtcodeStorage) << "Saved setting" << key;
    return true;
}

bool SettingsRepository::loadJson(
    const QString &key,
    QJsonObject *value,
    bool *found,
    QString *errorMessage) const
{
    if (found != nullptr) {
        *found = false;
    }

    if (!m_storageService.isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot load settings before the database is open.");
        }
        return false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral("SELECT value_json FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to load setting '%1': %2").arg(key, message);
        }
        return false;
    }

    if (!query.next()) {
        return true;
    }

    const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toByteArray());
    if (!document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Setting '%1' does not contain a JSON object.").arg(key);
        }
        return false;
    }

    if (value != nullptr) {
        *value = document.object();
    }
    if (found != nullptr) {
        *found = true;
    }

    return true;
}

} // namespace qtcode::storage

#pragma once

#include <QJsonObject>
#include <QString>

namespace qtcode::storage {

class StorageService;

class SettingsRepository
{
public:
    explicit SettingsRepository(StorageService &storageService);

    [[nodiscard]] bool upsertJson(
        const QString &key,
        const QJsonObject &value,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool loadJson(
        const QString &key,
        QJsonObject *value,
        bool *found,
        QString *errorMessage = nullptr) const;

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

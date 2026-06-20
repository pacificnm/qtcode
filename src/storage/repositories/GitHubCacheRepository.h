#pragma once

#include "github/GitHubModels.h"

#include <QJsonObject>
#include <QString>

namespace qtcode::storage {

class StorageService;

class GitHubCacheRepository
{
public:
    explicit GitHubCacheRepository(StorageService &storageService);

    [[nodiscard]] bool upsertEntry(
        const QString &repositoryId,
        const QString &objectType,
        const QString &objectKey,
        const QJsonObject &payload,
        const QString &fetchedAt,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool loadEntry(
        const QString &repositoryId,
        const QString &objectType,
        const QString &objectKey,
        QJsonObject *payload,
        QString *fetchedAt,
        bool *found,
        QString *errorMessage = nullptr) const;

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

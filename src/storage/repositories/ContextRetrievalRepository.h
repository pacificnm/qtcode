#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace qtcode::storage {

class StorageService;

struct PersistedContextRetrieval
{
    QString id;
    QString sessionId;
    QString projectId;
    QString query;
    QString providerKey;
    int resultCount = 0;
    QJsonObject metadataJson;
    QString createdAt;
};

struct PersistedContextResult
{
    QString id;
    QString retrievalId;
    QString sourceType;
    QString sourceUri;
    QString title;
    QString excerpt;
    double score = 0.0;
    QJsonObject metadataJson;
};

class ContextRetrievalRepository
{
public:
    explicit ContextRetrievalRepository(StorageService &storageService);

    [[nodiscard]] bool insertRetrieval(
        const PersistedContextRetrieval &retrieval,
        const QList<PersistedContextResult> &results,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool listRetrievalsForSession(
        const QString &sessionId,
        QList<PersistedContextRetrieval> *retrievals,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool listResultsForRetrieval(
        const QString &retrievalId,
        QList<PersistedContextResult> *results,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool latestRetrievalForSession(
        const QString &sessionId,
        PersistedContextRetrieval *retrieval,
        QList<PersistedContextResult> *results,
        QString *errorMessage = nullptr) const;

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

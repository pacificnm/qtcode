#include "storage/repositories/ContextRetrievalRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

PersistedContextRetrieval retrievalFromQuery(const QSqlQuery &query)
{
    PersistedContextRetrieval retrieval;
    retrieval.id = query.value(QStringLiteral("id")).toString();
    retrieval.sessionId = query.value(QStringLiteral("session_id")).toString();
    retrieval.projectId = query.value(QStringLiteral("project_id")).toString();
    retrieval.query = query.value(QStringLiteral("query")).toString();
    retrieval.providerKey = query.value(QStringLiteral("provider_key")).toString();
    retrieval.resultCount = query.value(QStringLiteral("result_count")).toInt();
    retrieval.createdAt = query.value(QStringLiteral("created_at")).toString();

    const QByteArray metadataBytes = query.value(QStringLiteral("metadata_json")).toByteArray();
    if (!metadataBytes.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(metadataBytes);
        if (document.isObject()) {
            retrieval.metadataJson = document.object();
        }
    }

    return retrieval;
}

PersistedContextResult resultFromQuery(const QSqlQuery &query)
{
    PersistedContextResult result;
    result.id = query.value(QStringLiteral("id")).toString();
    result.retrievalId = query.value(QStringLiteral("retrieval_id")).toString();
    result.sourceType = query.value(QStringLiteral("source_type")).toString();
    result.sourceUri = query.value(QStringLiteral("source_uri")).toString();
    result.title = query.value(QStringLiteral("title")).toString();
    result.excerpt = query.value(QStringLiteral("excerpt")).toString();
    result.score = query.value(QStringLiteral("score")).toDouble();
    return result;
}

} // namespace

ContextRetrievalRepository::ContextRetrievalRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool ContextRetrievalRepository::insertRetrieval(
    const PersistedContextRetrieval &retrieval,
    const QList<PersistedContextResult> &results,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO context_retrievals ("
        "id, session_id, project_id, query, provider_key, result_count, metadata_json, created_at) "
        "VALUES ("
        ":id, :session_id, :project_id, :query, :provider_key, :result_count, :metadata_json, :created_at)"));
    query.bindValue(QStringLiteral(":id"), retrieval.id);
    query.bindValue(QStringLiteral(":session_id"), retrieval.sessionId);
    query.bindValue(QStringLiteral(":project_id"), retrieval.projectId);
    query.bindValue(QStringLiteral(":query"), retrieval.query);
    query.bindValue(QStringLiteral(":provider_key"), retrieval.providerKey);
    query.bindValue(QStringLiteral(":result_count"), results.size());
    query.bindValue(
        QStringLiteral(":metadata_json"),
        QString::fromUtf8(QJsonDocument(retrieval.metadataJson).toJson(QJsonDocument::Compact)));
    query.bindValue(QStringLiteral(":created_at"), retrieval.createdAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert context retrieval: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert context retrieval" << message;
        return false;
    }

    for (const PersistedContextResult &result : results) {
        QSqlQuery resultQuery(m_storageService.database());
        resultQuery.prepare(QStringLiteral(
            "INSERT INTO context_results ("
            "id, retrieval_id, source_type, source_uri, title, excerpt, score, metadata_json) "
            "VALUES ("
            ":id, :retrieval_id, :source_type, :source_uri, :title, :excerpt, :score, :metadata_json)"));
        resultQuery.bindValue(QStringLiteral(":id"), result.id);
        resultQuery.bindValue(QStringLiteral(":retrieval_id"), result.retrievalId);
        resultQuery.bindValue(QStringLiteral(":source_type"), result.sourceType);
        resultQuery.bindValue(QStringLiteral(":source_uri"), result.sourceUri);
        resultQuery.bindValue(QStringLiteral(":title"), result.title);
        resultQuery.bindValue(QStringLiteral(":excerpt"), result.excerpt);
        resultQuery.bindValue(QStringLiteral(":score"), result.score);
        resultQuery.bindValue(
            QStringLiteral(":metadata_json"),
            result.metadataJson.isEmpty()
                ? QString {}
                : QString::fromUtf8(QJsonDocument(result.metadataJson).toJson(QJsonDocument::Compact)));

        if (!resultQuery.exec()) {
            const QString message = resultQuery.lastError().text();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to insert context result: %1").arg(message);
            }
            qCWarning(qtcodeStorage) << "Failed to insert context result" << message;
            return false;
        }
    }

    return true;
}

bool ContextRetrievalRepository::listRetrievalsForSession(
    const QString &sessionId,
    QList<PersistedContextRetrieval> *retrievals,
    QString *errorMessage) const
{
    if (retrievals == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Context retrieval list output must not be null.");
        }
        return false;
    }

    retrievals->clear();
    if (sessionId.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, session_id, project_id, query, provider_key, result_count, metadata_json, created_at "
        "FROM context_retrievals "
        "WHERE session_id = :session_id "
        "ORDER BY created_at ASC"));
    query.bindValue(QStringLiteral(":session_id"), sessionId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list context retrievals: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list context retrievals" << message;
        return false;
    }

    while (query.next()) {
        retrievals->append(retrievalFromQuery(query));
    }

    return true;
}

bool ContextRetrievalRepository::listResultsForRetrieval(
    const QString &retrievalId,
    QList<PersistedContextResult> *results,
    QString *errorMessage) const
{
    if (results == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Context result list output must not be null.");
        }
        return false;
    }

    results->clear();
    if (retrievalId.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "SELECT id, retrieval_id, source_type, source_uri, title, excerpt, score, metadata_json "
        "FROM context_results "
        "WHERE retrieval_id = :retrieval_id "
        "ORDER BY score DESC, title ASC"));
    query.bindValue(QStringLiteral(":retrieval_id"), retrievalId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list context results: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list context results" << message;
        return false;
    }

    while (query.next()) {
        results->append(resultFromQuery(query));
    }

    return true;
}

bool ContextRetrievalRepository::latestRetrievalForSession(
    const QString &sessionId,
    PersistedContextRetrieval *retrieval,
    QList<PersistedContextResult> *results,
    QString *errorMessage) const
{
    QList<PersistedContextRetrieval> retrievals;
    if (!listRetrievalsForSession(sessionId, &retrievals, errorMessage)) {
        return false;
    }

    if (retrievals.isEmpty()) {
        if (retrieval != nullptr) {
            *retrieval = {};
        }
        if (results != nullptr) {
            results->clear();
        }
        return true;
    }

    const PersistedContextRetrieval latest = retrievals.last();
    if (retrieval != nullptr) {
        *retrieval = latest;
    }

    if (results == nullptr) {
        return true;
    }

    return listResultsForRetrieval(latest.id, results, errorMessage);
}

} // namespace qtcode::storage

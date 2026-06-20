#include "github/GitHubCachePolicy.h"

#include <QDateTime>

namespace qtcode::github {

QString GitHubCacheMetadata::statusLabel() const
{
    return GitHubCachePolicy::formatStatusLabel(fromCache, fetchedAt, isStale);
}

bool GitHubCachePolicy::isStale(const QString &fetchedAtIso, int maxAgeMs)
{
    if (fetchedAtIso.isEmpty() || maxAgeMs <= 0) {
        return true;
    }

    QDateTime fetched = QDateTime::fromString(fetchedAtIso, Qt::ISODateWithMs);
    if (!fetched.isValid()) {
        fetched = QDateTime::fromString(fetchedAtIso, Qt::ISODate);
    }

    if (!fetched.isValid()) {
        return true;
    }

    if (fetched.timeSpec() == Qt::LocalTime) {
        fetched = fetched.toUTC();
    }

    return fetched.msecsTo(QDateTime::currentDateTimeUtc()) > maxAgeMs;
}

QString GitHubCachePolicy::formatStatusLabel(
    bool fromCache,
    const QString &fetchedAt,
    bool isStale)
{
    if (!fromCache) {
        return {};
    }

    if (fetchedAt.isEmpty()) {
        return isStale ? QStringLiteral("Stale cached GitHub metadata")
                       : QStringLiteral("Cached GitHub metadata");
    }

    if (isStale) {
        return QStringLiteral("Stale cached GitHub metadata from %1").arg(fetchedAt);
    }

    return QStringLiteral("Cached GitHub metadata from %1").arg(fetchedAt);
}

} // namespace qtcode::github

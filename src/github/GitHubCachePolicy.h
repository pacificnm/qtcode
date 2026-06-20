#pragma once

#include <QString>

namespace qtcode::github {

struct GitHubCacheMetadata
{
    bool fromCache = false;
    bool isStale = false;
    QString fetchedAt;

    [[nodiscard]] QString statusLabel() const;
};

struct GitHubCachePolicy
{
    static constexpr int kListCacheTtlMs = 15 * 60 * 1000;
    static constexpr int kDetailCacheTtlMs = 60 * 60 * 1000;

    [[nodiscard]] static bool isStale(const QString &fetchedAtIso, int maxAgeMs);
    [[nodiscard]] static QString formatStatusLabel(
        bool fromCache,
        const QString &fetchedAt,
        bool isStale);
};

} // namespace qtcode::github

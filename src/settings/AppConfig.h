#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace qtcode::settings {

inline constexpr auto kAppConfigGroupGeneral = "General";
inline constexpr auto kAppConfigKeyRestoreLastProject = "restoreLastProjectOnStartup";
inline constexpr auto kAppConfigKeyStartMaximized = "startMaximized";
inline constexpr auto kAppConfigKeyRepoHelpPath = "repoHelpPath";
inline constexpr auto kAppConfigDefaultRepoHelpPath = "doc/index.md";

[[nodiscard]] inline QString normalizedRepoHelpPath(const QString &path)
{
    QString normalized = path.trimmed();
    if (normalized.isEmpty()) {
        normalized = QString::fromLatin1(kAppConfigDefaultRepoHelpPath);
    }

    if (QFileInfo(normalized).suffix().isEmpty()) {
        normalized = QDir(normalized).filePath(QStringLiteral("index.md"));
    }

    return normalized;
}

struct AppConfig
{
    bool restoreLastProjectOnStartup = true;
    bool startMaximized = false;
    QString repoHelpPath = QString::fromLatin1(kAppConfigDefaultRepoHelpPath);

    [[nodiscard]] static AppConfig defaults();
};

} // namespace qtcode::settings

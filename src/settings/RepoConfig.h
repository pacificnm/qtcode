#pragma once

#include "settings/AppConfig.h"
#include "settings/ProjectModels.h"

#include <QDir>
#include <QFileInfo>

#include <QString>

namespace qtcode::settings {

inline constexpr auto kRepoConfigRelativePath = ".qtcode/config.yaml";
inline constexpr auto kRepoConfigKeyRepoHelpPath = "repoHelpPath";
inline constexpr auto kRepoConfigKeyHelpEntryPath = "entryPath";
inline constexpr auto kRepoConfigKeyDefaultAgentKey = "defaultAgentKey";
inline constexpr auto kRepoConfigKeyProjectDisplayName = "displayName";
inline constexpr auto kRepoConfigKeyProjectPath = "path";

struct RepoConfig
{
    QString repoHelpPath;
    QString defaultAgentKey;
    QString projectDisplayName;
    QString projectPath;

    [[nodiscard]] bool hasRepoHelpPath() const
    {
        return !repoHelpPath.trimmed().isEmpty();
    }

    [[nodiscard]] bool hasDefaultAgentKey() const
    {
        return !defaultAgentKey.trimmed().isEmpty();
    }

    [[nodiscard]] bool hasProjectDisplayName() const
    {
        return !projectDisplayName.trimmed().isEmpty();
    }

    [[nodiscard]] bool hasProjectPath() const
    {
        return !projectPath.trimmed().isEmpty();
    }

    [[nodiscard]] static RepoConfig empty()
    {
        return RepoConfig {};
    }
};

[[nodiscard]] inline QString effectiveRepoHelpPath(const AppConfig &appConfig, const RepoConfig &repoConfig)
{
    if (repoConfig.hasRepoHelpPath()) {
        return normalizedRepoHelpPath(repoConfig.repoHelpPath);
    }

    return normalizedRepoHelpPath(appConfig.repoHelpPath);
}

[[nodiscard]] inline QString effectiveProjectDisplayName(
    const ProjectRecord &project,
    const RepoConfig &repoConfig)
{
    if (repoConfig.hasProjectDisplayName()) {
        return repoConfig.projectDisplayName.trimmed();
    }

    return project.name.trimmed();
}

[[nodiscard]] inline QString effectiveProjectPath(
    const ProjectRecord &project,
    const RepoConfig &repoConfig)
{
    QString path = repoConfig.hasProjectPath() ? repoConfig.projectPath.trimmed()
                                               : project.rootPath.trimmed();
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo pathInfo(path);
    if (pathInfo.isRelative()) {
        path = QDir(project.rootPath).absoluteFilePath(path);
    }

    const QFileInfo resolvedInfo(path);
    if (resolvedInfo.exists()) {
        return QDir::cleanPath(resolvedInfo.canonicalFilePath());
    }

    return QDir::cleanPath(path);
}

} // namespace qtcode::settings

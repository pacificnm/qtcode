#pragma once

#include "settings/AppConfig.h"

#include <QString>

namespace qtcode::settings {

inline constexpr auto kRepoConfigRelativePath = ".qtcode/config.yaml";
inline constexpr auto kRepoConfigKeyRepoHelpPath = "repoHelpPath";
inline constexpr auto kRepoConfigKeyHelpEntryPath = "entryPath";
inline constexpr auto kRepoConfigKeyDefaultAgentKey = "defaultAgentKey";

struct RepoConfig
{
    QString repoHelpPath;
    QString defaultAgentKey;

    [[nodiscard]] bool hasRepoHelpPath() const
    {
        return !repoHelpPath.trimmed().isEmpty();
    }

    [[nodiscard]] bool hasDefaultAgentKey() const
    {
        return !defaultAgentKey.trimmed().isEmpty();
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

} // namespace qtcode::settings

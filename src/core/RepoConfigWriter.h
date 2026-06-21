#pragma once

#include "settings/RepoConfig.h"

#include <QString>

namespace qtcode::core {

class RepoConfigWriter
{
public:
    [[nodiscard]] static bool save(
        const QString &projectRootPath,
        const qtcode::settings::RepoConfig &config,
        QString *errorMessage = nullptr);
    [[nodiscard]] static bool saveRepoHelpPath(
        const QString &projectRootPath,
        const QString &repoHelpPath,
        QString *errorMessage = nullptr);
};

} // namespace qtcode::core

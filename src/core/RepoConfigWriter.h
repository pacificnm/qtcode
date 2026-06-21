#pragma once

#include <QString>

namespace qtcode::core {

class RepoConfigWriter
{
public:
    [[nodiscard]] static bool saveRepoHelpPath(
        const QString &projectRootPath,
        const QString &repoHelpPath,
        QString *errorMessage = nullptr);
};

} // namespace qtcode::core

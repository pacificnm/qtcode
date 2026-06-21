#pragma once

#include "settings/RepoConfig.h"

#include <QString>

namespace qtcode::core {

class RepoConfigLoader
{
public:
    [[nodiscard]] static settings::RepoConfig loadFromProjectRoot(const QString &projectRootPath);
    [[nodiscard]] static settings::RepoConfig loadFromYamlContent(const QString &yamlContent);
};

} // namespace qtcode::core

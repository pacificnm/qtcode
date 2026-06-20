#pragma once

#include <QString>

namespace qtcode::github {

struct GitHubRepositoryIdentity
{
    QString owner;
    QString name;
    bool isGitHub = false;
};

[[nodiscard]] GitHubRepositoryIdentity parseGitHubRemoteUrl(const QString &remoteUrl);

} // namespace qtcode::github

#include "github/GitHubService.h"

namespace qtcode::github {

void github_module_anchor()
{
}

GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl)
{
    return parseGitHubRemoteUrl(remoteUrl);
}

} // namespace qtcode::github

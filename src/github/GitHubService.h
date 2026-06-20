#pragma once

#include "github/GitHubRemoteResolver.h"

namespace qtcode::github {

void github_module_anchor();

[[nodiscard]] GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl);

} // namespace qtcode::github

#pragma once

#include "github/GitHubModels.h"

#include <QString>

namespace qtcode::github {

[[nodiscard]] QString formatIssueContextExcerpt(
    const GitHubIssueDetail &detail,
    int maxBodyLength = 4000);
[[nodiscard]] QString formatPullRequestContextExcerpt(
    const GitHubPullRequestDetail &detail,
    int maxBodyLength = 4000);

} // namespace qtcode::github

#pragma once

#include <QString>

namespace qtcode::github {

[[nodiscard]] QString formatPullRequestContextExcerpt(
    const struct GitHubPullRequestDetail &detail,
    int maxBodyLength = 4000);

} // namespace qtcode::github

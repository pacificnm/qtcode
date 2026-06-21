#pragma once

#include <QString>
#include <QStringList>

namespace qtcode::github {

[[nodiscard]] QString suggestedIssueBranchName(int issueNumber, const QString &title);
[[nodiscard]] QStringList matchingIssueBranches(
    int issueNumber,
    const QStringList &repositoryBranchNames);
[[nodiscard]] QString resolveIssueBranchName(
    int issueNumber,
    const QStringList &linkedBranches,
    const QStringList &repositoryBranchNames);

} // namespace qtcode::github

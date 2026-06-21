#pragma once

#include "github/GitHubModels.h"

namespace qtcode::github {

class GhCliClient
{
public:
    void setExecutablePath(const QString &executablePath);
    [[nodiscard]] QString executablePath() const;
    [[nodiscard]] bool isConfigured() const;

    [[nodiscard]] GitHubIssueListResult listIssues(
        const QString &owner,
        const QString &name,
        int limit = 25) const;
    [[nodiscard]] GitHubIssueDetailResult viewIssue(
        const QString &owner,
        const QString &name,
        int issueNumber) const;
    [[nodiscard]] GitHubPullRequestListResult listPullRequests(
        const QString &owner,
        const QString &name,
        int limit = 25) const;
    [[nodiscard]] GitHubPullRequestDetailResult viewPullRequest(
        const QString &owner,
        const QString &name,
        int pullRequestNumber) const;
    [[nodiscard]] GitHubRepositoryInfo viewRepository(
        const QString &owner,
        const QString &name) const;
    [[nodiscard]] GitHubIssueBranchListResult listIssueLinkedBranches(
        const QString &owner,
        const QString &name,
        int issueNumber) const;
    [[nodiscard]] GitHubIssueBranchDevelopResult developIssueBranch(
        const QString &owner,
        const QString &name,
        int issueNumber,
        const QString &baseBranch,
        const QString &branchName,
        bool checkout = false) const;

private:
    QString m_executablePath;
};

} // namespace qtcode::github

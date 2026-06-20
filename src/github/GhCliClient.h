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

private:
    QString m_executablePath;
};

} // namespace qtcode::github

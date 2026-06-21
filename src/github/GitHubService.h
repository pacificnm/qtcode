#pragma once

#include "github/GitHubModels.h"
#include "github/GitHubRemoteResolver.h"
#include "github/GhCliClient.h"

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::github {

class GitHubService
{
public:
    explicit GitHubService(qtcode::storage::StorageService &storageService);

    void setGhExecutablePath(const QString &executablePath);
    [[nodiscard]] GitHubIssueListResult listIssuesForProject(const QString &projectId) const;
    [[nodiscard]] GitHubIssueDetailResult viewIssueForProject(
        const QString &projectId,
        int issueNumber) const;
    [[nodiscard]] GitHubPullRequestListResult listPullRequestsForProject(const QString &projectId) const;
    [[nodiscard]] GitHubPullRequestDetailResult viewPullRequestForProject(
        const QString &projectId,
        int pullRequestNumber) const;
    [[nodiscard]] GitHubIssueBranchListResult listIssueLinkedBranchesForProject(
        const QString &projectId,
        int issueNumber) const;
    [[nodiscard]] GitHubIssueBranchDevelopResult developIssueBranchForProject(
        const QString &projectId,
        int issueNumber,
        const QString &baseBranch,
        const QString &branchName,
        bool checkout = false) const;

private:
    struct ResolvedGitHubRepository
    {
        QString repositoryId;
        QString owner;
        QString name;
        QString errorMessage;
        bool success = false;
    };

    [[nodiscard]] ResolvedGitHubRepository resolvePrimaryGitHubRepository(
        const QString &projectId) const;
    [[nodiscard]] GitHubIssueListResult listIssues(
        const QString &repositoryId,
        const QString &owner,
        const QString &name,
        int limit) const;
    [[nodiscard]] GitHubIssueListResult loadIssuesFromCache(const QString &repositoryId) const;
    [[nodiscard]] bool persistIssuesToCache(
        const QString &repositoryId,
        const QList<GitHubIssue> &issues) const;
    [[nodiscard]] GitHubIssueDetailResult loadIssueFromCache(
        const QString &repositoryId,
        int issueNumber) const;
    [[nodiscard]] bool persistIssueToCache(
        const QString &repositoryId,
        const GitHubIssueDetail &detail) const;
    [[nodiscard]] GitHubPullRequestListResult listPullRequests(
        const QString &repositoryId,
        const QString &owner,
        const QString &name,
        int limit) const;
    [[nodiscard]] GitHubPullRequestListResult loadPullRequestsFromCache(
        const QString &repositoryId) const;
    [[nodiscard]] bool persistPullRequestsToCache(
        const QString &repositoryId,
        const QList<GitHubPullRequest> &pullRequests) const;
    [[nodiscard]] GitHubPullRequestDetailResult loadPullRequestFromCache(
        const QString &repositoryId,
        int pullRequestNumber) const;
    [[nodiscard]] bool persistPullRequestToCache(
        const QString &repositoryId,
        const GitHubPullRequestDetail &detail) const;

    qtcode::storage::StorageService &m_storageService;
    GhCliClient m_ghClient;
};

[[nodiscard]] GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl);

} // namespace qtcode::github

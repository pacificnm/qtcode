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

private:
    [[nodiscard]] GitHubIssueListResult listIssues(
        const QString &repositoryId,
        const QString &owner,
        const QString &name,
        int limit) const;
    [[nodiscard]] GitHubIssueListResult loadIssuesFromCache(const QString &repositoryId) const;
    [[nodiscard]] bool persistIssuesToCache(
        const QString &repositoryId,
        const QList<GitHubIssue> &issues) const;

    qtcode::storage::StorageService &m_storageService;
    GhCliClient m_ghClient;
};

[[nodiscard]] GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl);

} // namespace qtcode::github

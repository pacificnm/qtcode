#pragma once

#include <QString>
#include <QList>

namespace qtcode::github {

struct GitHubRepositoryInfo
{
    QString owner;
    QString name;
    QString url;
    QString description;
};

struct GitHubIssue
{
    int number = 0;
    QString title;
    QString state;
    QString author;
    QString url;
    QString updatedAt;
};

struct GitHubIssueDetail
{
    int number = 0;
    QString title;
    QString state;
    QString author;
    QString url;
    QString body;
    QString updatedAt;
};

struct GitHubIssueDetailResult
{
    bool success = false;
    bool fromCache = false;
    bool isStale = false;
    QString fetchedAt;
    QString errorMessage;
    GitHubIssueDetail detail;
};

struct GitHubIssueListResult
{
    bool success = false;
    bool fromCache = false;
    bool isStale = false;
    QString fetchedAt;
    QString errorMessage;
    QList<GitHubIssue> issues;
};

struct GitHubPullRequest
{
    int number = 0;
    QString title;
    QString state;
    QString author;
    QString url;
    QString updatedAt;
};

struct GitHubPullRequestListResult
{
    bool success = false;
    bool fromCache = false;
    bool isStale = false;
    QString fetchedAt;
    QString errorMessage;
    QList<GitHubPullRequest> pullRequests;
};

struct GitHubPullRequestDetail
{
    int number = 0;
    QString title;
    QString state;
    QString author;
    QString url;
    QString body;
    QString baseRef;
    QString headRef;
    QString updatedAt;
};

struct GitHubPullRequestDetailResult
{
    bool success = false;
    bool fromCache = false;
    bool isStale = false;
    QString fetchedAt;
    QString errorMessage;
    GitHubPullRequestDetail detail;
};

struct GitHubIssueBranchListResult
{
    bool success = false;
    QString errorMessage;
    QStringList branchNames;
};

struct GitHubIssueBranchDevelopResult
{
    bool success = false;
    QString errorMessage;
    QString branchName;
    QString standardOutput;
};

} // namespace qtcode::github

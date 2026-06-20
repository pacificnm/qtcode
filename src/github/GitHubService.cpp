#include "github/GitHubService.h"

#include "github/GhCliClient.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/repositories/GitHubCacheRepository.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/StorageService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace qtcode::github {

namespace {

constexpr int kDefaultIssueLimit = 25;
constexpr int kDefaultPullRequestLimit = 25;
constexpr auto kIssueListObjectType = "issue_list";
constexpr auto kIssueListObjectKey = "default";
constexpr auto kIssueDetailObjectType = "issue";
constexpr auto kPullRequestListObjectType = "pull_request_list";
constexpr auto kPullRequestListObjectKey = "default";
constexpr auto kPullRequestDetailObjectType = "pull_request";

QJsonObject issuesToCachePayload(const QList<GitHubIssue> &issues)
{
    QJsonArray issueArray;

    for (const GitHubIssue &issue : issues) {
        QJsonObject object;
        object.insert(QStringLiteral("number"), issue.number);
        object.insert(QStringLiteral("title"), issue.title);
        object.insert(QStringLiteral("state"), issue.state);
        object.insert(QStringLiteral("author"), issue.author);
        object.insert(QStringLiteral("url"), issue.url);
        object.insert(QStringLiteral("updatedAt"), issue.updatedAt);
        issueArray.append(object);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("issues"), issueArray);
    return payload;
}

GitHubIssueListResult issuesFromCachePayload(const QJsonObject &payload)
{
    GitHubIssueListResult result;
    result.fromCache = true;
    result.success = true;

    const QJsonArray issueArray = payload.value(QStringLiteral("issues")).toArray();
    result.issues.reserve(issueArray.size());

    for (const QJsonValue &value : issueArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        GitHubIssue issue;
        issue.number = object.value(QStringLiteral("number")).toInt();
        issue.title = object.value(QStringLiteral("title")).toString();
        issue.state = object.value(QStringLiteral("state")).toString();
        issue.author = object.value(QStringLiteral("author")).toString();
        issue.url = object.value(QStringLiteral("url")).toString();
        issue.updatedAt = object.value(QStringLiteral("updatedAt")).toString();

        if (issue.number <= 0 || issue.title.isEmpty()) {
            continue;
        }

        result.issues.append(issue);
    }

    return result;
}

QJsonObject pullRequestsToCachePayload(const QList<GitHubPullRequest> &pullRequests)
{
    QJsonArray pullRequestArray;

    for (const GitHubPullRequest &pullRequest : pullRequests) {
        QJsonObject object;
        object.insert(QStringLiteral("number"), pullRequest.number);
        object.insert(QStringLiteral("title"), pullRequest.title);
        object.insert(QStringLiteral("state"), pullRequest.state);
        object.insert(QStringLiteral("author"), pullRequest.author);
        object.insert(QStringLiteral("url"), pullRequest.url);
        object.insert(QStringLiteral("updatedAt"), pullRequest.updatedAt);
        pullRequestArray.append(object);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("pullRequests"), pullRequestArray);
    return payload;
}

GitHubPullRequestListResult pullRequestsFromCachePayload(const QJsonObject &payload)
{
    GitHubPullRequestListResult result;
    result.fromCache = true;
    result.success = true;

    const QJsonArray pullRequestArray = payload.value(QStringLiteral("pullRequests")).toArray();
    result.pullRequests.reserve(pullRequestArray.size());

    for (const QJsonValue &value : pullRequestArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        GitHubPullRequest pullRequest;
        pullRequest.number = object.value(QStringLiteral("number")).toInt();
        pullRequest.title = object.value(QStringLiteral("title")).toString();
        pullRequest.state = object.value(QStringLiteral("state")).toString();
        pullRequest.author = object.value(QStringLiteral("author")).toString();
        pullRequest.url = object.value(QStringLiteral("url")).toString();
        pullRequest.updatedAt = object.value(QStringLiteral("updatedAt")).toString();

        if (pullRequest.number <= 0 || pullRequest.title.isEmpty()) {
            continue;
        }

        result.pullRequests.append(pullRequest);
    }

    return result;
}

QJsonObject pullRequestDetailToCachePayload(const GitHubPullRequestDetail &detail)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("number"), detail.number);
    payload.insert(QStringLiteral("title"), detail.title);
    payload.insert(QStringLiteral("state"), detail.state);
    payload.insert(QStringLiteral("author"), detail.author);
    payload.insert(QStringLiteral("url"), detail.url);
    payload.insert(QStringLiteral("body"), detail.body);
    payload.insert(QStringLiteral("baseRef"), detail.baseRef);
    payload.insert(QStringLiteral("headRef"), detail.headRef);
    payload.insert(QStringLiteral("updatedAt"), detail.updatedAt);
    return payload;
}

GitHubPullRequestDetailResult pullRequestDetailFromCachePayload(const QJsonObject &payload)
{
    GitHubPullRequestDetailResult result;
    result.fromCache = true;
    result.success = true;
    result.detail.number = payload.value(QStringLiteral("number")).toInt();
    result.detail.title = payload.value(QStringLiteral("title")).toString();
    result.detail.state = payload.value(QStringLiteral("state")).toString();
    result.detail.author = payload.value(QStringLiteral("author")).toString();
    result.detail.url = payload.value(QStringLiteral("url")).toString();
    result.detail.body = payload.value(QStringLiteral("body")).toString();
    result.detail.baseRef = payload.value(QStringLiteral("baseRef")).toString();
    result.detail.headRef = payload.value(QStringLiteral("headRef")).toString();
    result.detail.updatedAt = payload.value(QStringLiteral("updatedAt")).toString();
    return result;
}

QString pullRequestDetailCacheKey(int pullRequestNumber)
{
    return QStringLiteral("pr-%1").arg(pullRequestNumber);
}

QJsonObject issueDetailToCachePayload(const GitHubIssueDetail &detail)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("number"), detail.number);
    payload.insert(QStringLiteral("title"), detail.title);
    payload.insert(QStringLiteral("state"), detail.state);
    payload.insert(QStringLiteral("author"), detail.author);
    payload.insert(QStringLiteral("url"), detail.url);
    payload.insert(QStringLiteral("body"), detail.body);
    payload.insert(QStringLiteral("updatedAt"), detail.updatedAt);
    return payload;
}

GitHubIssueDetailResult issueDetailFromCachePayload(const QJsonObject &payload)
{
    GitHubIssueDetailResult result;
    result.fromCache = true;
    result.success = true;
    result.detail.number = payload.value(QStringLiteral("number")).toInt();
    result.detail.title = payload.value(QStringLiteral("title")).toString();
    result.detail.state = payload.value(QStringLiteral("state")).toString();
    result.detail.author = payload.value(QStringLiteral("author")).toString();
    result.detail.url = payload.value(QStringLiteral("url")).toString();
    result.detail.body = payload.value(QStringLiteral("body")).toString();
    result.detail.updatedAt = payload.value(QStringLiteral("updatedAt")).toString();
    return result;
}

QString issueDetailCacheKey(int issueNumber)
{
    return QStringLiteral("issue-%1").arg(issueNumber);
}

QString currentTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

GitHubService::GitHubService(storage::StorageService &storageService)
    : m_storageService(storageService)
{
}

void GitHubService::setGhExecutablePath(const QString &executablePath)
{
    m_ghClient.setExecutablePath(executablePath);
}

GitHubService::ResolvedGitHubRepository GitHubService::resolvePrimaryGitHubRepository(
    const QString &projectId) const
{
    ResolvedGitHubRepository resolved;

    if (projectId.isEmpty()) {
        resolved.errorMessage = QStringLiteral("Project id must not be empty.");
        return resolved;
    }

    storage::ProjectRepository projectRepository(m_storageService);
    settings::RepositoryRecord repositoryRecord;
    bool found = false;
    if (!projectRepository.findPrimaryRepository(projectId, &repositoryRecord, &found, &resolved.errorMessage)) {
        return resolved;
    }

    if (!found) {
        resolved.errorMessage = QStringLiteral("No repository metadata found for the active project.");
        return resolved;
    }

    if (repositoryRecord.githubOwner.isEmpty() || repositoryRecord.githubName.isEmpty()) {
        resolved.errorMessage = QStringLiteral("The active repository does not have a GitHub remote.");
        return resolved;
    }

    resolved.repositoryId = repositoryRecord.id;
    resolved.owner = repositoryRecord.githubOwner;
    resolved.name = repositoryRecord.githubName;
    resolved.success = true;
    return resolved;
}

GitHubIssueListResult GitHubService::listIssuesForProject(const QString &projectId) const
{
    GitHubIssueListResult result;

    const ResolvedGitHubRepository resolved = resolvePrimaryGitHubRepository(projectId);
    if (!resolved.success) {
        result.errorMessage = resolved.errorMessage;
        return result;
    }

    return listIssues(resolved.repositoryId, resolved.owner, resolved.name, kDefaultIssueLimit);
}

GitHubIssueDetailResult GitHubService::viewIssueForProject(
    const QString &projectId,
    int issueNumber) const
{
    GitHubIssueDetailResult result;

    const ResolvedGitHubRepository resolved = resolvePrimaryGitHubRepository(projectId);
    if (!resolved.success) {
        result.errorMessage = resolved.errorMessage;
        return result;
    }

    if (!m_ghClient.isConfigured()) {
        GitHubIssueDetailResult cachedResult = loadIssueFromCache(resolved.repositoryId, issueNumber);
        if (cachedResult.success) {
            return cachedResult;
        }

        result.errorMessage = QStringLiteral("GitHub CLI is not available.");
        return result;
    }

    result = m_ghClient.viewIssue(resolved.owner, resolved.name, issueNumber);
    if (result.success) {
        if (!persistIssueToCache(resolved.repositoryId, result.detail)) {
            qCWarning(qtcodeGithub) << "Failed to cache GitHub issue" << issueNumber;
        }
        return result;
    }

    GitHubIssueDetailResult cachedResult = loadIssueFromCache(resolved.repositoryId, issueNumber);
    if (cachedResult.success) {
        return cachedResult;
    }

    return result;
}

GitHubIssueListResult GitHubService::listIssues(
    const QString &repositoryId,
    const QString &owner,
    const QString &name,
    int limit) const
{
    if (!m_ghClient.isConfigured()) {
        GitHubIssueListResult cachedResult = loadIssuesFromCache(repositoryId);
        if (cachedResult.success) {
            qCInfo(qtcodeGithub) << "Loaded" << cachedResult.issues.size()
                                 << "cached GitHub issue(s) for" << owner << name;
            return cachedResult;
        }

        GitHubIssueListResult result;
        result.errorMessage = QStringLiteral("GitHub CLI is not available.");
        return result;
    }

    GitHubIssueListResult result = m_ghClient.listIssues(owner, name, limit);
    if (result.success) {
        if (!persistIssuesToCache(repositoryId, result.issues)) {
            qCWarning(qtcodeGithub) << "Failed to cache GitHub issues for" << owner << name;
        }

        qCInfo(qtcodeGithub) << "Loaded" << result.issues.size() << "GitHub issue(s) for" << owner << name;
        return result;
    }

    GitHubIssueListResult cachedResult = loadIssuesFromCache(repositoryId);
    if (cachedResult.success) {
        qCInfo(qtcodeGithub) << "Using cached GitHub issues after CLI failure for" << owner << name;
        return cachedResult;
    }

    return result;
}

GitHubIssueListResult GitHubService::loadIssuesFromCache(const QString &repositoryId) const
{
    GitHubIssueListResult result;

    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QJsonObject payload;
    bool found = false;
    QString cacheError;
    if (!cacheRepository.loadEntry(
            repositoryId,
            QString::fromUtf8(kIssueListObjectType),
            QString::fromUtf8(kIssueListObjectKey),
            &payload,
            nullptr,
            &found,
            &cacheError)) {
        result.errorMessage = cacheError;
        return result;
    }

    if (!found) {
        result.errorMessage = QStringLiteral("No cached GitHub issues are available.");
        return result;
    }

    return issuesFromCachePayload(payload);
}

bool GitHubService::persistIssuesToCache(
    const QString &repositoryId,
    const QList<GitHubIssue> &issues) const
{
    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QString cacheError;
    return cacheRepository.upsertEntry(
        repositoryId,
        QString::fromUtf8(kIssueListObjectType),
        QString::fromUtf8(kIssueListObjectKey),
        issuesToCachePayload(issues),
        currentTimestamp(),
        &cacheError);
}

GitHubIssueDetailResult GitHubService::loadIssueFromCache(
    const QString &repositoryId,
    int issueNumber) const
{
    GitHubIssueDetailResult result;

    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QJsonObject payload;
    bool found = false;
    QString cacheError;
    if (!cacheRepository.loadEntry(
            repositoryId,
            QString::fromUtf8(kIssueDetailObjectType),
            issueDetailCacheKey(issueNumber),
            &payload,
            nullptr,
            &found,
            &cacheError)) {
        result.errorMessage = cacheError;
        return result;
    }

    if (!found) {
        result.errorMessage = QStringLiteral("No cached GitHub issue detail is available.");
        return result;
    }

    return issueDetailFromCachePayload(payload);
}

bool GitHubService::persistIssueToCache(
    const QString &repositoryId,
    const GitHubIssueDetail &detail) const
{
    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QString cacheError;
    return cacheRepository.upsertEntry(
        repositoryId,
        QString::fromUtf8(kIssueDetailObjectType),
        issueDetailCacheKey(detail.number),
        issueDetailToCachePayload(detail),
        currentTimestamp(),
        &cacheError);
}

GitHubPullRequestListResult GitHubService::listPullRequestsForProject(const QString &projectId) const
{
    GitHubPullRequestListResult result;

    const ResolvedGitHubRepository resolved = resolvePrimaryGitHubRepository(projectId);
    if (!resolved.success) {
        result.errorMessage = resolved.errorMessage;
        return result;
    }

    return listPullRequests(
        resolved.repositoryId,
        resolved.owner,
        resolved.name,
        kDefaultPullRequestLimit);
}

GitHubPullRequestDetailResult GitHubService::viewPullRequestForProject(
    const QString &projectId,
    int pullRequestNumber) const
{
    GitHubPullRequestDetailResult result;

    const ResolvedGitHubRepository resolved = resolvePrimaryGitHubRepository(projectId);
    if (!resolved.success) {
        result.errorMessage = resolved.errorMessage;
        return result;
    }

    if (!m_ghClient.isConfigured()) {
        GitHubPullRequestDetailResult cachedResult =
            loadPullRequestFromCache(resolved.repositoryId, pullRequestNumber);
        if (cachedResult.success) {
            return cachedResult;
        }

        result.errorMessage = QStringLiteral("GitHub CLI is not available.");
        return result;
    }

    result = m_ghClient.viewPullRequest(resolved.owner, resolved.name, pullRequestNumber);
    if (result.success) {
        if (!persistPullRequestToCache(resolved.repositoryId, result.detail)) {
            qCWarning(qtcodeGithub) << "Failed to cache GitHub pull request" << pullRequestNumber;
        }
        return result;
    }

    GitHubPullRequestDetailResult cachedResult =
        loadPullRequestFromCache(resolved.repositoryId, pullRequestNumber);
    if (cachedResult.success) {
        return cachedResult;
    }

    return result;
}

GitHubPullRequestListResult GitHubService::listPullRequests(
    const QString &repositoryId,
    const QString &owner,
    const QString &name,
    int limit) const
{
    if (!m_ghClient.isConfigured()) {
        GitHubPullRequestListResult cachedResult = loadPullRequestsFromCache(repositoryId);
        if (cachedResult.success) {
            qCInfo(qtcodeGithub) << "Loaded" << cachedResult.pullRequests.size()
                                 << "cached GitHub pull request(s) for" << owner << name;
            return cachedResult;
        }

        GitHubPullRequestListResult result;
        result.errorMessage = QStringLiteral("GitHub CLI is not available.");
        return result;
    }

    GitHubPullRequestListResult result = m_ghClient.listPullRequests(owner, name, limit);
    if (result.success) {
        if (!persistPullRequestsToCache(repositoryId, result.pullRequests)) {
            qCWarning(qtcodeGithub) << "Failed to cache GitHub pull requests for" << owner << name;
        }

        qCInfo(qtcodeGithub) << "Loaded" << result.pullRequests.size()
                             << "GitHub pull request(s) for" << owner << name;
        return result;
    }

    GitHubPullRequestListResult cachedResult = loadPullRequestsFromCache(repositoryId);
    if (cachedResult.success) {
        qCInfo(qtcodeGithub) << "Using cached GitHub pull requests after CLI failure for" << owner << name;
        return cachedResult;
    }

    return result;
}

GitHubPullRequestListResult GitHubService::loadPullRequestsFromCache(const QString &repositoryId) const
{
    GitHubPullRequestListResult result;

    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QJsonObject payload;
    bool found = false;
    QString cacheError;
    if (!cacheRepository.loadEntry(
            repositoryId,
            QString::fromUtf8(kPullRequestListObjectType),
            QString::fromUtf8(kPullRequestListObjectKey),
            &payload,
            nullptr,
            &found,
            &cacheError)) {
        result.errorMessage = cacheError;
        return result;
    }

    if (!found) {
        result.errorMessage = QStringLiteral("No cached GitHub pull requests are available.");
        return result;
    }

    return pullRequestsFromCachePayload(payload);
}

bool GitHubService::persistPullRequestsToCache(
    const QString &repositoryId,
    const QList<GitHubPullRequest> &pullRequests) const
{
    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QString cacheError;
    return cacheRepository.upsertEntry(
        repositoryId,
        QString::fromUtf8(kPullRequestListObjectType),
        QString::fromUtf8(kPullRequestListObjectKey),
        pullRequestsToCachePayload(pullRequests),
        currentTimestamp(),
        &cacheError);
}

GitHubPullRequestDetailResult GitHubService::loadPullRequestFromCache(
    const QString &repositoryId,
    int pullRequestNumber) const
{
    GitHubPullRequestDetailResult result;

    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QJsonObject payload;
    bool found = false;
    QString cacheError;
    if (!cacheRepository.loadEntry(
            repositoryId,
            QString::fromUtf8(kPullRequestDetailObjectType),
            pullRequestDetailCacheKey(pullRequestNumber),
            &payload,
            nullptr,
            &found,
            &cacheError)) {
        result.errorMessage = cacheError;
        return result;
    }

    if (!found) {
        result.errorMessage = QStringLiteral("No cached GitHub pull request detail is available.");
        return result;
    }

    return pullRequestDetailFromCachePayload(payload);
}

bool GitHubService::persistPullRequestToCache(
    const QString &repositoryId,
    const GitHubPullRequestDetail &detail) const
{
    storage::GitHubCacheRepository cacheRepository(m_storageService);
    QString cacheError;
    return cacheRepository.upsertEntry(
        repositoryId,
        QString::fromUtf8(kPullRequestDetailObjectType),
        pullRequestDetailCacheKey(detail.number),
        pullRequestDetailToCachePayload(detail),
        currentTimestamp(),
        &cacheError);
}

GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl)
{
    return parseGitHubRemoteUrl(remoteUrl);
}

} // namespace qtcode::github

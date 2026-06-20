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
constexpr auto kIssueListObjectType = "issue_list";
constexpr auto kIssueListObjectKey = "default";

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

GitHubIssueListResult GitHubService::listIssuesForProject(const QString &projectId) const
{
    GitHubIssueListResult result;

    if (projectId.isEmpty()) {
        result.errorMessage = QStringLiteral("Project id must not be empty.");
        return result;
    }

    storage::ProjectRepository projectRepository(m_storageService);
    settings::RepositoryRecord repositoryRecord;
    bool found = false;
    QString lookupError;
    if (!projectRepository.findPrimaryRepository(projectId, &repositoryRecord, &found, &lookupError)) {
        result.errorMessage = lookupError;
        return result;
    }

    if (!found) {
        result.errorMessage = QStringLiteral("No repository metadata found for the active project.");
        return result;
    }

    if (repositoryRecord.githubOwner.isEmpty() || repositoryRecord.githubName.isEmpty()) {
        result.errorMessage = QStringLiteral("The active repository does not have a GitHub remote.");
        return result;
    }

    return listIssues(
        repositoryRecord.id,
        repositoryRecord.githubOwner,
        repositoryRecord.githubName,
        kDefaultIssueLimit);
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

GitHubRepositoryIdentity resolveRemoteUrl(const QString &remoteUrl)
{
    return parseGitHubRemoteUrl(remoteUrl);
}

} // namespace qtcode::github

#include "github/GhCliClient.h"

#include "shared/Logging.h"
#include "shared/ProcessRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace qtcode::github {

namespace {

constexpr int kGhCommandTimeoutMs = 15000;

QString repositorySlug(const QString &owner, const QString &name)
{
    return QStringLiteral("%1/%2").arg(owner, name);
}

GitHubIssue issueFromJsonObject(const QJsonObject &object)
{
    GitHubIssue issue;
    issue.number = object.value(QStringLiteral("number")).toInt();
    issue.title = object.value(QStringLiteral("title")).toString().trimmed();
    issue.state = object.value(QStringLiteral("state")).toString().trimmed();
    issue.url = object.value(QStringLiteral("url")).toString().trimmed();
    issue.updatedAt = object.value(QStringLiteral("updatedAt")).toString().trimmed();

    const QJsonValue authorValue = object.value(QStringLiteral("author"));
    if (authorValue.isObject()) {
        issue.author = authorValue.toObject().value(QStringLiteral("login")).toString().trimmed();
    }

    return issue;
}

GitHubIssueListResult parseIssueListJson(const QByteArray &jsonBytes, QString *errorMessage)
{
    GitHubIssueListResult result;

    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI issue list returned invalid JSON.");
        }
        return result;
    }

    const QJsonArray issuesArray = document.array();
    result.issues.reserve(issuesArray.size());

    for (const QJsonValue &value : issuesArray) {
        if (!value.isObject()) {
            continue;
        }

        const GitHubIssue issue = issueFromJsonObject(value.toObject());
        if (issue.number <= 0 || issue.title.isEmpty()) {
            continue;
        }

        result.issues.append(issue);
    }

    result.success = true;
    return result;
}

GitHubIssueDetailResult parseIssueDetailJson(const QByteArray &jsonBytes, QString *errorMessage)
{
    GitHubIssueDetailResult result;

    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI issue view returned invalid JSON.");
        }
        return result;
    }

    const QJsonObject object = document.object();
    result.detail.number = object.value(QStringLiteral("number")).toInt();
    result.detail.title = object.value(QStringLiteral("title")).toString().trimmed();
    result.detail.state = object.value(QStringLiteral("state")).toString().trimmed();
    result.detail.url = object.value(QStringLiteral("url")).toString().trimmed();
    result.detail.body = object.value(QStringLiteral("body")).toString().trimmed();
    result.detail.updatedAt = object.value(QStringLiteral("updatedAt")).toString().trimmed();

    const QJsonValue authorValue = object.value(QStringLiteral("author"));
    if (authorValue.isObject()) {
        result.detail.author = authorValue.toObject().value(QStringLiteral("login")).toString().trimmed();
    }

    if (result.detail.number <= 0 || result.detail.title.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI issue view returned incomplete data.");
        }
        return result;
    }

    result.success = true;
    return result;
}

GitHubPullRequest pullRequestFromJsonObject(const QJsonObject &object)
{
    GitHubPullRequest pullRequest;
    pullRequest.number = object.value(QStringLiteral("number")).toInt();
    pullRequest.title = object.value(QStringLiteral("title")).toString().trimmed();
    pullRequest.state = object.value(QStringLiteral("state")).toString().trimmed();
    pullRequest.url = object.value(QStringLiteral("url")).toString().trimmed();
    pullRequest.updatedAt = object.value(QStringLiteral("updatedAt")).toString().trimmed();

    const QJsonValue authorValue = object.value(QStringLiteral("author"));
    if (authorValue.isObject()) {
        pullRequest.author = authorValue.toObject().value(QStringLiteral("login")).toString().trimmed();
    }

    return pullRequest;
}

GitHubPullRequestListResult parsePullRequestListJson(const QByteArray &jsonBytes, QString *errorMessage)
{
    GitHubPullRequestListResult result;

    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI pull request list returned invalid JSON.");
        }
        return result;
    }

    const QJsonArray pullRequestArray = document.array();
    result.pullRequests.reserve(pullRequestArray.size());

    for (const QJsonValue &value : pullRequestArray) {
        if (!value.isObject()) {
            continue;
        }

        const GitHubPullRequest pullRequest = pullRequestFromJsonObject(value.toObject());
        if (pullRequest.number <= 0 || pullRequest.title.isEmpty()) {
            continue;
        }

        result.pullRequests.append(pullRequest);
    }

    result.success = true;
    return result;
}

GitHubPullRequestDetailResult parsePullRequestDetailJson(const QByteArray &jsonBytes, QString *errorMessage)
{
    GitHubPullRequestDetailResult result;

    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI pull request view returned invalid JSON.");
        }
        return result;
    }

    const QJsonObject object = document.object();
    result.detail.number = object.value(QStringLiteral("number")).toInt();
    result.detail.title = object.value(QStringLiteral("title")).toString().trimmed();
    result.detail.state = object.value(QStringLiteral("state")).toString().trimmed();
    result.detail.url = object.value(QStringLiteral("url")).toString().trimmed();
    result.detail.body = object.value(QStringLiteral("body")).toString().trimmed();
    result.detail.baseRef = object.value(QStringLiteral("baseRefName")).toString().trimmed();
    result.detail.headRef = object.value(QStringLiteral("headRefName")).toString().trimmed();
    result.detail.updatedAt = object.value(QStringLiteral("updatedAt")).toString().trimmed();

    const QJsonValue authorValue = object.value(QStringLiteral("author"));
    if (authorValue.isObject()) {
        result.detail.author = authorValue.toObject().value(QStringLiteral("login")).toString().trimmed();
    }

    if (result.detail.number <= 0 || result.detail.title.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("GitHub CLI pull request view returned incomplete data.");
        }
        return result;
    }

    result.success = true;
    return result;
}

} // namespace

void GhCliClient::setExecutablePath(const QString &executablePath)
{
    m_executablePath = executablePath.trimmed();
}

QString GhCliClient::executablePath() const
{
    return m_executablePath;
}

bool GhCliClient::isConfigured() const
{
    return !m_executablePath.isEmpty();
}

GitHubIssueListResult GhCliClient::listIssues(
    const QString &owner,
    const QString &name,
    int limit) const
{
    GitHubIssueListResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (limit <= 0) {
        limit = 25;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("issue"),
         QStringLiteral("list"),
         QStringLiteral("--repo"),
         repositorySlug(owner, name),
         QStringLiteral("--json"),
         QStringLiteral("number,title,state,url,updatedAt,author"),
         QStringLiteral("--limit"),
         QString::number(limit)},
        kGhCommandTimeoutMs);

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        const QString details =
            QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
        result.errorMessage = details.isEmpty()
            ? QStringLiteral("GitHub CLI issue list failed.")
            : details;
        qCWarning(qtcodeGithub) << "gh issue list failed for" << repositorySlug(owner, name) << details;
        return result;
    }

    return parseIssueListJson(processResult.standardOutput.toUtf8(), &result.errorMessage);
}

GitHubIssueDetailResult GhCliClient::viewIssue(
    const QString &owner,
    const QString &name,
    int issueNumber) const
{
    GitHubIssueDetailResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (issueNumber <= 0) {
        result.errorMessage = QStringLiteral("Issue number must be positive.");
        return result;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("issue"),
         QStringLiteral("view"),
         QString::number(issueNumber),
         QStringLiteral("--repo"),
         repositorySlug(owner, name),
         QStringLiteral("--json"),
         QStringLiteral("number,title,state,url,body,author,updatedAt")},
        kGhCommandTimeoutMs);

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        const QString details =
            QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
        result.errorMessage = details.isEmpty()
            ? QStringLiteral("GitHub CLI issue view failed.")
            : details;
        qCWarning(qtcodeGithub) << "gh issue view failed for" << repositorySlug(owner, name)
                                << issueNumber << details;
        return result;
    }

    return parseIssueDetailJson(processResult.standardOutput.toUtf8(), &result.errorMessage);
}

GitHubPullRequestListResult GhCliClient::listPullRequests(
    const QString &owner,
    const QString &name,
    int limit) const
{
    GitHubPullRequestListResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (limit <= 0) {
        limit = 25;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("pr"),
         QStringLiteral("list"),
         QStringLiteral("--repo"),
         repositorySlug(owner, name),
         QStringLiteral("--json"),
         QStringLiteral("number,title,state,url,updatedAt,author"),
         QStringLiteral("--limit"),
         QString::number(limit)},
        kGhCommandTimeoutMs);

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        const QString details =
            QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
        result.errorMessage = details.isEmpty()
            ? QStringLiteral("GitHub CLI pull request list failed.")
            : details;
        qCWarning(qtcodeGithub) << "gh pr list failed for" << repositorySlug(owner, name) << details;
        return result;
    }

    return parsePullRequestListJson(processResult.standardOutput.toUtf8(), &result.errorMessage);
}

GitHubPullRequestDetailResult GhCliClient::viewPullRequest(
    const QString &owner,
    const QString &name,
    int pullRequestNumber) const
{
    GitHubPullRequestDetailResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (pullRequestNumber <= 0) {
        result.errorMessage = QStringLiteral("Pull request number must be positive.");
        return result;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("pr"),
         QStringLiteral("view"),
         QString::number(pullRequestNumber),
         QStringLiteral("--repo"),
         repositorySlug(owner, name),
         QStringLiteral("--json"),
         QStringLiteral("number,title,state,url,body,author,baseRefName,headRefName,updatedAt")},
        kGhCommandTimeoutMs);

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        const QString details =
            QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
        result.errorMessage = details.isEmpty()
            ? QStringLiteral("GitHub CLI pull request view failed.")
            : details;
        qCWarning(qtcodeGithub) << "gh pr view failed for" << repositorySlug(owner, name)
                                << pullRequestNumber << details;
        return result;
    }

    return parsePullRequestDetailJson(processResult.standardOutput.toUtf8(), &result.errorMessage);
}

GitHubRepositoryInfo GhCliClient::viewRepository(
    const QString &owner,
    const QString &name) const
{
    GitHubRepositoryInfo repositoryInfo;
    repositoryInfo.owner = owner;
    repositoryInfo.name = name;

    if (!isConfigured() || owner.isEmpty() || name.isEmpty()) {
        return repositoryInfo;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("repo"),
         QStringLiteral("view"),
         repositorySlug(owner, name),
         QStringLiteral("--json"),
         QStringLiteral("name,owner,url,description")},
        kGhCommandTimeoutMs);

    if (!processResult.started || processResult.exitCode != 0) {
        qCWarning(qtcodeGithub) << "gh repo view failed for" << repositorySlug(owner, name)
                                << processResult.standardError;
        return repositoryInfo;
    }

    const QJsonDocument document = QJsonDocument::fromJson(processResult.standardOutput.toUtf8());
    if (!document.isObject()) {
        return repositoryInfo;
    }

    const QJsonObject object = document.object();
    repositoryInfo.name = object.value(QStringLiteral("name")).toString().trimmed();
    repositoryInfo.url = object.value(QStringLiteral("url")).toString().trimmed();
    repositoryInfo.description = object.value(QStringLiteral("description")).toString().trimmed();

    const QJsonValue ownerValue = object.value(QStringLiteral("owner"));
    if (ownerValue.isObject()) {
        const QString resolvedOwner =
            ownerValue.toObject().value(QStringLiteral("login")).toString().trimmed();
        if (!resolvedOwner.isEmpty()) {
            repositoryInfo.owner = resolvedOwner;
        }
    }

    return repositoryInfo;
}

QStringList parseIssueDevelopListOutput(const QString &output)
{
    QStringList branchNames;

    const QStringList lines = output.split(QChar::fromLatin1('\n'), Qt::SkipEmptyParts);
    branchNames.reserve(lines.size());

    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        QString branchName = trimmedLine.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).trimmed();
        if (branchName.startsWith(QLatin1Char('"')) && branchName.endsWith(QLatin1Char('"'))) {
            branchName = branchName.mid(1, branchName.size() - 2);
        }

        if (!branchName.isEmpty()) {
            branchNames.append(branchName);
        }
    }

    branchNames.removeDuplicates();
    return branchNames;
}

GitHubIssueBranchListResult GhCliClient::listIssueLinkedBranches(
    const QString &owner,
    const QString &name,
    int issueNumber) const
{
    GitHubIssueBranchListResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (issueNumber <= 0) {
        result.errorMessage = QStringLiteral("Issue number must be positive.");
        return result;
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        {QStringLiteral("issue"),
         QStringLiteral("develop"),
         QStringLiteral("--list"),
         QString::number(issueNumber),
         QStringLiteral("--repo"),
         repositorySlug(owner, name)},
        kGhCommandTimeoutMs);

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        const QString details =
            QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
        result.errorMessage = details.isEmpty()
            ? QStringLiteral("GitHub CLI issue develop list failed.")
            : details;
        qCWarning(qtcodeGithub) << "gh issue develop --list failed for" << repositorySlug(owner, name)
                                << issueNumber << details;
        return result;
    }

    result.branchNames = parseIssueDevelopListOutput(processResult.standardOutput);
    result.success = true;
    return result;
}

GitHubIssueBranchDevelopResult GhCliClient::developIssueBranch(
    const QString &owner,
    const QString &name,
    int issueNumber,
    const QString &baseBranch,
    const QString &branchName,
    bool checkout) const
{
    GitHubIssueBranchDevelopResult result;

    if (!isConfigured()) {
        result.errorMessage = QStringLiteral("GitHub CLI executable is not configured.");
        return result;
    }

    if (owner.isEmpty() || name.isEmpty()) {
        result.errorMessage = QStringLiteral("GitHub owner and repository name are required.");
        return result;
    }

    if (issueNumber <= 0) {
        result.errorMessage = QStringLiteral("Issue number must be positive.");
        return result;
    }

    const QString trimmedBaseBranch = baseBranch.trimmed();
    if (trimmedBaseBranch.isEmpty()) {
        result.errorMessage = QStringLiteral("Base branch is required.");
        return result;
    }

    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedBranchName.isEmpty()) {
        result.errorMessage = QStringLiteral("Branch name is required.");
        return result;
    }

    QStringList arguments {
        QStringLiteral("issue"),
        QStringLiteral("develop"),
        QString::number(issueNumber),
        QStringLiteral("--repo"),
        repositorySlug(owner, name),
        QStringLiteral("--base"),
        trimmedBaseBranch,
        QStringLiteral("--name"),
        trimmedBranchName};

    if (checkout) {
        arguments.append(QStringLiteral("--checkout"));
    }

    const shared::ProcessResult processResult = shared::ProcessRunner::run(
        m_executablePath,
        arguments,
        kGhCommandTimeoutMs);

    result.standardOutput =
        QStringLiteral("%1\n%2").arg(processResult.standardOutput, processResult.standardError).trimmed();
    result.branchName = trimmedBranchName;

    if (!processResult.started) {
        result.errorMessage = QStringLiteral("Failed to start GitHub CLI: %1")
                                  .arg(processResult.standardError);
        return result;
    }

    if (processResult.exitCode != 0) {
        result.errorMessage = result.standardOutput.isEmpty()
            ? QStringLiteral("GitHub CLI issue develop failed.")
            : result.standardOutput;
        qCWarning(qtcodeGithub) << "gh issue develop failed for" << repositorySlug(owner, name)
                                << issueNumber << result.errorMessage;
        return result;
    }

    result.success = true;
    return result;
}

} // namespace qtcode::github

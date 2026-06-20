#include "github/GhCliClient.h"

#include "shared/Logging.h"
#include "shared/ProcessRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

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

} // namespace qtcode::github

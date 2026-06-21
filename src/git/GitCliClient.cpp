#include "git/GitCliClient.h"

#include "shared/Logging.h"
#include "shared/ProcessRunner.h"

namespace qtcode::git {

namespace {

constexpr int kGitCommandTimeoutMs = 30000;
constexpr int kGitPushTimeoutMs = 120000;

QString combinedProcessOutput(const qtcode::shared::ProcessResult &result)
{
    return QStringLiteral("%1\n%2").arg(result.standardOutput, result.standardError).trimmed();
}

} // namespace

void GitCliClient::setExecutablePath(const QString &executablePath)
{
    m_executablePath = executablePath.trimmed();
}

void GitCliClient::setWorkingDirectory(const QString &workingDirectory)
{
    m_workingDirectory = workingDirectory.trimmed();
}

QString GitCliClient::executablePath() const
{
    return m_executablePath;
}

QString GitCliClient::workingDirectory() const
{
    return m_workingDirectory;
}

bool GitCliClient::isConfigured() const
{
    return !m_executablePath.isEmpty() && !m_workingDirectory.isEmpty();
}

GitOperationResult GitCliClient::stagePaths(const QStringList &relativePaths) const
{
    if (relativePaths.isEmpty()) {
        GitOperationResult result;
        result.success = true;
        return result;
    }

    QStringList arguments {QStringLiteral("add"), QStringLiteral("--")};
    arguments.append(relativePaths);
    return runGit(arguments, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::stageAll() const
{
    return runGit({QStringLiteral("add"), QStringLiteral("-A")}, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::unstagePaths(const QStringList &relativePaths) const
{
    if (relativePaths.isEmpty()) {
        GitOperationResult result;
        result.success = true;
        return result;
    }

    QStringList arguments {QStringLiteral("reset"), QStringLiteral("HEAD"), QStringLiteral("--")};
    arguments.append(relativePaths);
    return runGit(arguments, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::unstageAll() const
{
    return runGit({QStringLiteral("reset"), QStringLiteral("HEAD")}, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::commit(const QString &message) const
{
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.isEmpty()) {
        GitOperationResult result;
        result.errorMessage = QStringLiteral("Commit message is required.");
        return result;
    }

    return runGit({QStringLiteral("commit"), QStringLiteral("-m"), trimmedMessage}, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::push() const
{
    return runGit({QStringLiteral("push")}, kGitPushTimeoutMs);
}

GitOperationResult GitCliClient::checkoutBranch(const QString &branchName) const
{
    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedBranchName.isEmpty()) {
        GitOperationResult result;
        result.errorMessage = QStringLiteral("Branch name is required.");
        return result;
    }

    return runGit({QStringLiteral("checkout"), trimmedBranchName}, kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::createBranch(const QString &branchName) const
{
    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedBranchName.isEmpty()) {
        GitOperationResult result;
        result.errorMessage = QStringLiteral("Branch name is required.");
        return result;
    }

    return runGit(
        {QStringLiteral("checkout"), QStringLiteral("-b"), trimmedBranchName},
        kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::fetchRemoteBranch(
    const QString &remote,
    const QString &branchName) const
{
    const QString trimmedRemote = remote.trimmed();
    const QString trimmedBranchName = branchName.trimmed();
    if (trimmedRemote.isEmpty() || trimmedBranchName.isEmpty()) {
        GitOperationResult result;
        result.errorMessage = QStringLiteral("Remote and branch name are required.");
        return result;
    }

    return runGit(
        {QStringLiteral("fetch"), trimmedRemote, trimmedBranchName},
        kGitPushTimeoutMs);
}

GitOperationResult GitCliClient::checkoutRemoteBranch(
    const QString &branchName,
    const QString &remote) const
{
    const QString trimmedBranchName = branchName.trimmed();
    const QString trimmedRemote = remote.trimmed();
    if (trimmedBranchName.isEmpty() || trimmedRemote.isEmpty()) {
        GitOperationResult result;
        result.errorMessage = QStringLiteral("Branch and remote names are required.");
        return result;
    }

    GitOperationResult checkoutResult = runGit(
        {QStringLiteral("checkout"), trimmedBranchName},
        kGitCommandTimeoutMs);
    if (checkoutResult.success) {
        return checkoutResult;
    }

    const GitOperationResult fetchResult = fetchRemoteBranch(trimmedRemote, trimmedBranchName);
    if (!fetchResult.success) {
        return fetchResult;
    }

    checkoutResult = runGit(
        {QStringLiteral("checkout"), trimmedBranchName},
        kGitCommandTimeoutMs);
    if (checkoutResult.success) {
        return checkoutResult;
    }

    return runGit(
        {QStringLiteral("checkout"),
         QStringLiteral("-B"),
         trimmedBranchName,
         QStringLiteral("%1/%2").arg(trimmedRemote, trimmedBranchName)},
        kGitCommandTimeoutMs);
}

GitOperationResult GitCliClient::listBranchReferences(
    QStringList *branchReferences,
    bool includeRemote) const
{
    GitOperationResult result;

    if (branchReferences == nullptr) {
        result.errorMessage = QStringLiteral("Branch reference output pointer is null.");
        return result;
    }

    branchReferences->clear();

    const GitOperationResult localResult = runGit(
        {QStringLiteral("for-each-ref"),
         QStringLiteral("--format=%(refname:short)"),
         QStringLiteral("refs/heads/")},
        kGitCommandTimeoutMs);
    if (!localResult.success) {
        return localResult;
    }

    for (const QString &line : localResult.standardOutput.split(QChar::fromLatin1('\n'), Qt::SkipEmptyParts)) {
        const QString branchName = line.trimmed();
        if (!branchName.isEmpty()) {
            branchReferences->append(branchName);
        }
    }

    if (includeRemote) {
        const GitOperationResult remoteResult = runGit(
            {QStringLiteral("for-each-ref"),
             QStringLiteral("--format=%(refname:short)"),
             QStringLiteral("refs/remotes/")},
            kGitCommandTimeoutMs);
        if (!remoteResult.success) {
            return remoteResult;
        }

        for (const QString &line :
             remoteResult.standardOutput.split(QChar::fromLatin1('\n'), Qt::SkipEmptyParts)) {
            const QString branchName = line.trimmed();
            if (branchName.isEmpty() || branchName.endsWith(QStringLiteral("/HEAD"))) {
                continue;
            }
            branchReferences->append(branchName);
        }
    }

    branchReferences->removeDuplicates();
    result.success = true;
    return result;
}

GitOperationResult GitCliClient::runGit(const QStringList &arguments, int timeoutMs) const
{
    GitOperationResult result;

    if (m_executablePath.isEmpty()) {
        result.errorMessage = QStringLiteral("Git executable is not configured.");
        return result;
    }

    if (m_workingDirectory.isEmpty()) {
        result.errorMessage = QStringLiteral("Git working directory is not configured.");
        return result;
    }

    const qtcode::shared::ProcessResult processResult =
        qtcode::shared::ProcessRunner::run(m_executablePath, arguments, timeoutMs, m_workingDirectory);

    result.standardOutput = combinedProcessOutput(processResult);

    if (!processResult.started) {
        result.errorMessage = processResult.standardError.isEmpty()
            ? QStringLiteral("Failed to start Git.")
            : processResult.standardError;
        qCWarning(qtcodeGit) << "Git command failed to start:" << arguments.join(QLatin1Char(' '))
                             << result.errorMessage;
        return result;
    }

    if (processResult.exitCode != 0) {
        result.errorMessage = result.standardOutput.isEmpty()
            ? QStringLiteral("Git command failed with exit code %1.").arg(processResult.exitCode)
            : result.standardOutput;
        qCWarning(qtcodeGit) << "Git command failed:" << arguments.join(QLatin1Char(' '))
                             << result.errorMessage;
        return result;
    }

    result.success = true;
    qCInfo(qtcodeGit) << "Git command succeeded:" << arguments.join(QLatin1Char(' '));
    return result;
}

} // namespace qtcode::git

#pragma once

#include "git/GitRepository.h"
#include "git/GitCommitSummary.h"
#include "git/GitOperationResult.h"
#include "git/GitStatus.h"

#include <QString>
#include <QStringList>
#include <QList>

namespace qtcode::git {

class GitCliClient;

class GitService
{
public:
    GitService();
    ~GitService();

    GitService(const GitService &) = delete;
    GitService &operator=(const GitService &) = delete;

    [[nodiscard]] bool validateRepository(
        const QString &path,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool inspectRepository(
        const QString &path,
        GitRepositoryInfo *info,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool loadWorkingTreeStatus(
        const QString &path,
        GitWorkingTreeStatus *status,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool loadRecentCommits(
        const QString &path,
        int limit,
        QList<GitCommitSummary> *commits,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool loadPrimaryRemoteUrl(
        const QString &path,
        QString *remoteUrl,
        QString *errorMessage = nullptr) const;

    [[nodiscard]] GitOperationResult stageFiles(
        const QString &path,
        const QString &gitExecutable,
        const QStringList &relativePaths) const;
    [[nodiscard]] GitOperationResult stageAll(
        const QString &path,
        const QString &gitExecutable) const;
    [[nodiscard]] GitOperationResult unstageFiles(
        const QString &path,
        const QString &gitExecutable,
        const QStringList &relativePaths) const;
    [[nodiscard]] GitOperationResult unstageAll(
        const QString &path,
        const QString &gitExecutable) const;
    [[nodiscard]] GitOperationResult commit(
        const QString &path,
        const QString &gitExecutable,
        const QString &message) const;
    [[nodiscard]] GitOperationResult push(
        const QString &path,
        const QString &gitExecutable) const;
};

} // namespace qtcode::git

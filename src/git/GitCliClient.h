#pragma once

#include "git/GitOperationResult.h"

#include <QString>
#include <QStringList>

namespace qtcode::git {

class GitCliClient
{
public:
    void setExecutablePath(const QString &executablePath);
    void setWorkingDirectory(const QString &workingDirectory);

    [[nodiscard]] QString executablePath() const;
    [[nodiscard]] QString workingDirectory() const;
    [[nodiscard]] bool isConfigured() const;

    [[nodiscard]] GitOperationResult stagePaths(const QStringList &relativePaths) const;
    [[nodiscard]] GitOperationResult stageAll() const;
    [[nodiscard]] GitOperationResult unstagePaths(const QStringList &relativePaths) const;
    [[nodiscard]] GitOperationResult unstageAll() const;
    [[nodiscard]] GitOperationResult commit(const QString &message) const;
    [[nodiscard]] GitOperationResult push() const;
    [[nodiscard]] GitOperationResult checkoutBranch(const QString &branchName) const;
    [[nodiscard]] GitOperationResult createBranch(const QString &branchName) const;
    [[nodiscard]] GitOperationResult fetchRemoteBranch(
        const QString &remote,
        const QString &branchName) const;
    [[nodiscard]] GitOperationResult checkoutRemoteBranch(
        const QString &branchName,
        const QString &remote = QStringLiteral("origin")) const;
    [[nodiscard]] GitOperationResult listBranchReferences(
        QStringList *branchReferences,
        bool includeRemote = true) const;

private:
    [[nodiscard]] GitOperationResult runGit(
        const QStringList &arguments,
        int timeoutMs) const;

    QString m_executablePath;
    QString m_workingDirectory;
};

} // namespace qtcode::git

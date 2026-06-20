#pragma once

#include "git/GitStatus.h"

#include <QList>
#include <QString>

namespace qtcode::git {

struct GitCommitSummary
{
    QString shortSha;
    QString subject;
    QString author;
    QString committedAt;
};

struct RepositoryGitSnapshot
{
    bool success = false;
    QString errorMessage;
    GitWorkingTreeStatus status;
    QList<GitCommitSummary> commits;
};

[[nodiscard]] RepositoryGitSnapshot loadRepositoryGitSnapshot(
    const QString &path,
    int commitLimit = 10);

} // namespace qtcode::git

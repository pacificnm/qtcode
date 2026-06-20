#pragma once

#include "git/GitRepository.h"
#include "git/GitStatus.h"

#include <QString>

namespace qtcode::git {

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
};

} // namespace qtcode::git

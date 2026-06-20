#include "git/GitService.h"

#include "shared/Logging.h"

#include <git2.h>

#include <QFileInfo>

namespace qtcode::git {

namespace {

QString libgit2ErrorMessage(const char *fallback)
{
    const git_error *error = git_error_last();
    if (error == nullptr || error->message == nullptr) {
        return QString::fromUtf8(fallback);
    }

    return QString::fromUtf8(error->message);
}

QString oidToHex(const git_oid *oid)
{
    char buffer[GIT_OID_HEXSZ + 1] = {};
    git_oid_tostr(buffer, GIT_OID_HEXSZ + 1, oid);
    return QString::fromUtf8(buffer);
}

QString shortOid(const git_oid *oid, int length = 8)
{
    char buffer[GIT_OID_HEXSZ + 1] = {};
    git_oid_tostr(buffer, length + 1, oid);
    return QString::fromUtf8(buffer);
}

bool populateHeadInfo(
    git_repository *repository,
    GitRepositoryInfo *info,
    QString *errorMessage)
{
    git_reference *head = nullptr;
    const int headResult = git_repository_head(&head, repository);
    if (headResult == GIT_ENOTFOUND) {
        info->branchName = QStringLiteral("(no commits yet)");
        info->hasHead = false;
        return true;
    }

    if (headResult < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to read repository HEAD: %1")
                                .arg(libgit2ErrorMessage("unable to read HEAD"));
        }
        return false;
    }

    info->hasHead = true;
    info->isDetachedHead = git_repository_head_detached(repository) == 1;

    if (info->isDetachedHead) {
        git_object *peeled = nullptr;
        if (git_reference_peel(&peeled, head, GIT_OBJ_COMMIT) < 0) {
            git_reference_free(head);
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to resolve detached HEAD: %1")
                                    .arg(libgit2ErrorMessage("unable to peel HEAD"));
            }
            return false;
        }

        const git_commit *commit = reinterpret_cast<git_commit *>(peeled);
        const git_oid *oid = git_commit_id(commit);
        info->commitId = oidToHex(oid);
        info->branchName = QStringLiteral("HEAD detached at %1").arg(shortOid(oid));
        git_object_free(peeled);
    } else {
        const char *branchName = nullptr;
        if (git_branch_name(&branchName, head) == 0 && branchName != nullptr) {
            info->branchName = QString::fromUtf8(branchName);
        } else {
            const char *shorthand = git_reference_shorthand(head);
            info->branchName = shorthand != nullptr ? QString::fromUtf8(shorthand) : QStringLiteral("HEAD");
        }

        git_object *peeled = nullptr;
        if (git_reference_peel(&peeled, head, GIT_OBJ_COMMIT) == 0) {
            const git_commit *commit = reinterpret_cast<git_commit *>(peeled);
            info->commitId = oidToHex(git_commit_id(commit));
            git_object_free(peeled);
        }
    }

    git_reference_free(head);
    return true;
}

} // namespace

GitService::GitService()
{
    git_libgit2_init();
}

GitService::~GitService()
{
    git_libgit2_shutdown();
}

bool GitService::validateRepository(const QString &path, QString *errorMessage) const
{
    GitRepositoryInfo info;
    return inspectRepository(path, &info, errorMessage);
}

bool GitService::inspectRepository(
    const QString &path,
    GitRepositoryInfo *info,
    QString *errorMessage) const
{
    if (info == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git repository output pointer is null.");
        }
        return false;
    }

    *info = GitRepositoryInfo {};
    info->localPath = path;

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Repository path is not a directory: %1").arg(path);
        }
        return false;
    }

    const QByteArray nativePath = fileInfo.canonicalFilePath().toUtf8();
    git_repository *repository = nullptr;
    const int openResult = git_repository_open(&repository, nativePath.constData());
    if (openResult < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Path is not a Git repository: %1 (%2)")
                                .arg(path, libgit2ErrorMessage("unable to open repository"));
        }
        qCDebug(qtcodeGit) << "Repository validation failed for" << path;
        return false;
    }

    info->localPath = QString::fromUtf8(nativePath);

    if (!populateHeadInfo(repository, info, errorMessage)) {
        git_repository_free(repository);
        return false;
    }

    git_repository_free(repository);

    qCInfo(qtcodeGit) << "Validated repository at" << info->localPath
                      << "branch" << info->branchName
                      << "detached" << info->isDetachedHead;
    return true;
}

} // namespace qtcode::git

#include "git/GitService.h"

#include "shared/Logging.h"

#include <git2.h>

#include <QFileInfo>

#include <algorithm>

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

QString formatStatusFlags(unsigned int status)
{
    QStringList labels;

    if ((status & GIT_STATUS_INDEX_NEW) != 0U) {
        labels.append(QStringLiteral("Staged new"));
    }
    if ((status & GIT_STATUS_INDEX_MODIFIED) != 0U) {
        labels.append(QStringLiteral("Staged modified"));
    }
    if ((status & GIT_STATUS_INDEX_DELETED) != 0U) {
        labels.append(QStringLiteral("Staged deleted"));
    }
    if ((status & GIT_STATUS_INDEX_RENAMED) != 0U) {
        labels.append(QStringLiteral("Staged renamed"));
    }
    if ((status & GIT_STATUS_WT_NEW) != 0U) {
        labels.append(QStringLiteral("Untracked"));
    }
    if ((status & GIT_STATUS_WT_MODIFIED) != 0U) {
        labels.append(QStringLiteral("Modified"));
    }
    if ((status & GIT_STATUS_WT_DELETED) != 0U) {
        labels.append(QStringLiteral("Deleted"));
    }
    if ((status & GIT_STATUS_WT_RENAMED) != 0U) {
        labels.append(QStringLiteral("Renamed"));
    }
    if ((status & GIT_STATUS_WT_TYPECHANGE) != 0U) {
        labels.append(QStringLiteral("Type changed"));
    }

    if (labels.isEmpty()) {
        return QStringLiteral("Changed");
    }

    return labels.join(QStringLiteral(", "));
}

QString changedFilePathFromEntry(const git_status_entry *entry)
{
    if (entry == nullptr) {
        return {};
    }

    if (entry->index_to_workdir != nullptr && entry->index_to_workdir->new_file.path != nullptr) {
        return QString::fromUtf8(entry->index_to_workdir->new_file.path);
    }

    if (entry->head_to_index != nullptr && entry->head_to_index->old_file.path != nullptr) {
        return QString::fromUtf8(entry->head_to_index->old_file.path);
    }

    if (entry->head_to_index != nullptr && entry->head_to_index->new_file.path != nullptr) {
        return QString::fromUtf8(entry->head_to_index->new_file.path);
    }

    return {};
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

bool GitService::loadWorkingTreeStatus(
    const QString &path,
    GitWorkingTreeStatus *status,
    QString *errorMessage) const
{
    if (status == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git working tree status output pointer is null.");
        }
        return false;
    }

    *status = GitWorkingTreeStatus {};

    GitRepositoryInfo repositoryInfo;
    if (!inspectRepository(path, &repositoryInfo, errorMessage)) {
        return false;
    }

    status->branchName = repositoryInfo.branchName;
    status->isDetachedHead = repositoryInfo.isDetachedHead;

    const QByteArray nativePath = repositoryInfo.localPath.toUtf8();
    git_repository *repository = nullptr;
    if (git_repository_open(&repository, nativePath.constData()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to reopen repository for status: %1")
                                .arg(libgit2ErrorMessage("unable to open repository"));
        }
        return false;
    }

    git_status_list *statusList = nullptr;
    git_status_options options = GIT_STATUS_OPTIONS_INIT;
    options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED
        | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
        | GIT_STATUS_OPT_DISABLE_PATHSPEC_MATCH;

    if (git_status_list_new(&statusList, repository, &options) < 0) {
        git_repository_free(repository);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to read repository status: %1")
                                .arg(libgit2ErrorMessage("unable to load status"));
        }
        return false;
    }

    const size_t entryCount = git_status_list_entrycount(statusList);
    status->changedFiles.reserve(static_cast<int>(entryCount));

    for (size_t index = 0; index < entryCount; ++index) {
        const git_status_entry *entry = git_status_byindex(statusList, index);
        if (entry == nullptr) {
            continue;
        }

        ChangedFile changedFile;
        changedFile.path = changedFilePathFromEntry(entry);
        changedFile.statusLabel = formatStatusFlags(entry->status);

        if (changedFile.path.isEmpty()) {
            continue;
        }

        status->changedFiles.append(changedFile);
    }

    std::sort(status->changedFiles.begin(), status->changedFiles.end(), [](const ChangedFile &left, const ChangedFile &right) {
        return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
    });

    git_status_list_free(statusList);
    git_repository_free(repository);

    qCInfo(qtcodeGit) << "Loaded" << status->changedFiles.size() << "changed file(s) for"
                      << repositoryInfo.localPath;
    return true;
}

} // namespace qtcode::git

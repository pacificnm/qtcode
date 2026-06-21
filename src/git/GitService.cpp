#include "git/GitService.h"

#include "git/GitCliClient.h"
#include "git/GitCommitSummary.h"
#include "shared/Logging.h"

#include <git2.h>

#include <QDateTime>
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

QString formatIndexStatus(unsigned int status)
{
    if ((status & GIT_STATUS_INDEX_NEW) != 0U) {
        return QStringLiteral("A");
    }
    if ((status & GIT_STATUS_INDEX_MODIFIED) != 0U) {
        return QStringLiteral("M");
    }
    if ((status & GIT_STATUS_INDEX_DELETED) != 0U) {
        return QStringLiteral("D");
    }
    if ((status & GIT_STATUS_INDEX_RENAMED) != 0U) {
        return QStringLiteral("R");
    }
    if ((status & GIT_STATUS_INDEX_TYPECHANGE) != 0U) {
        return QStringLiteral("T");
    }

    return QStringLiteral("M");
}

QString formatWorktreeStatus(unsigned int status)
{
    if ((status & GIT_STATUS_WT_NEW) != 0U) {
        return QStringLiteral("U");
    }
    if ((status & GIT_STATUS_WT_MODIFIED) != 0U) {
        return QStringLiteral("M");
    }
    if ((status & GIT_STATUS_WT_DELETED) != 0U) {
        return QStringLiteral("D");
    }
    if ((status & GIT_STATUS_WT_RENAMED) != 0U) {
        return QStringLiteral("R");
    }
    if ((status & GIT_STATUS_WT_TYPECHANGE) != 0U) {
        return QStringLiteral("T");
    }

    return QStringLiteral("M");
}

bool hasIndexChanges(unsigned int status)
{
    return (status
            & (GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_INDEX_DELETED
                | GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_TYPECHANGE))
        != 0U;
}

bool hasWorktreeChanges(unsigned int status)
{
    return (status
            & (GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_DELETED
                | GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_TYPECHANGE))
        != 0U;
}

bool populateBranchTracking(git_repository *repository, GitWorkingTreeStatus *status)
{
    if (status == nullptr || status->isDetachedHead) {
        return true;
    }

    if (status->branchName.isEmpty() || status->branchName.startsWith(QStringLiteral("HEAD"))
        || status->branchName.startsWith(QStringLiteral("("))) {
        return true;
    }

    const QByteArray branchNameBytes = status->branchName.toUtf8();
    const QByteArray localRefBytes =
        QStringLiteral("refs/heads/%1").arg(status->branchName).toUtf8();

    git_oid localOid {};
    if (git_reference_name_to_id(&localOid, repository, localRefBytes.constData()) != 0) {
        return true;
    }

    auto applyAheadBehind = [&](const char *upstreamRef) -> bool {
        git_oid upstreamOid {};
        if (git_reference_name_to_id(&upstreamOid, repository, upstreamRef) != 0) {
            return false;
        }

        size_t ahead = 0;
        size_t behind = 0;
        if (git_graph_ahead_behind(&ahead, &behind, repository, &localOid, &upstreamOid) != 0) {
            return false;
        }

        status->hasUpstream = true;
        status->commitsAhead = static_cast<int>(ahead);
        status->commitsBehind = static_cast<int>(behind);
        return true;
    };

    git_buf upstreamName = {};
    if (git_branch_upstream_name(&upstreamName, repository, branchNameBytes.constData()) == 0) {
        if (applyAheadBehind(upstreamName.ptr)) {
            git_buf_dispose(&upstreamName);
            return true;
        }
    }
    git_buf_dispose(&upstreamName);

    const QByteArray originRefBytes =
        QStringLiteral("refs/remotes/origin/%1").arg(status->branchName).toUtf8();
    applyAheadBehind(originRefBytes.constData());
    return true;
}

GitCliClient configuredGitCliClient(const QString &path, const QString &gitExecutable)
{
    GitCliClient client;
    client.setExecutablePath(gitExecutable);
    client.setWorkingDirectory(path);
    return client;
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
    status->stagedFiles.reserve(static_cast<int>(entryCount));
    status->unstagedFiles.reserve(static_cast<int>(entryCount));

    for (size_t index = 0; index < entryCount; ++index) {
        const git_status_entry *entry = git_status_byindex(statusList, index);
        if (entry == nullptr) {
            continue;
        }

        const QString path = changedFilePathFromEntry(entry);
        if (path.isEmpty()) {
            continue;
        }

        if (hasIndexChanges(entry->status)) {
            ChangedFile stagedFile;
            stagedFile.path = path;
            stagedFile.statusLabel = formatIndexStatus(entry->status);
            status->stagedFiles.append(stagedFile);
        }

        if (hasWorktreeChanges(entry->status)) {
            ChangedFile unstagedFile;
            unstagedFile.path = path;
            unstagedFile.statusLabel = formatWorktreeStatus(entry->status);
            status->unstagedFiles.append(unstagedFile);
        }
    }

    auto sortChangedFiles = [](QList<ChangedFile> *files) {
        std::sort(files->begin(), files->end(), [](const ChangedFile &left, const ChangedFile &right) {
            return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
        });
    };
    sortChangedFiles(&status->stagedFiles);
    sortChangedFiles(&status->unstagedFiles);

    populateBranchTracking(repository, status);

    git_status_list_free(statusList);
    git_repository_free(repository);

    qCInfo(qtcodeGit) << "Loaded" << status->stagedFiles.size() << "staged and"
                      << status->unstagedFiles.size() << "unstaged file(s) for"
                      << repositoryInfo.localPath
                      << "ahead" << status->commitsAhead << "behind" << status->commitsBehind;
    return true;
}

GitOperationResult GitService::stageFiles(
    const QString &path,
    const QString &gitExecutable,
    const QStringList &relativePaths) const
{
    return configuredGitCliClient(path, gitExecutable).stagePaths(relativePaths);
}

GitOperationResult GitService::stageAll(const QString &path, const QString &gitExecutable) const
{
    return configuredGitCliClient(path, gitExecutable).stageAll();
}

GitOperationResult GitService::unstageFiles(
    const QString &path,
    const QString &gitExecutable,
    const QStringList &relativePaths) const
{
    return configuredGitCliClient(path, gitExecutable).unstagePaths(relativePaths);
}

GitOperationResult GitService::unstageAll(const QString &path, const QString &gitExecutable) const
{
    return configuredGitCliClient(path, gitExecutable).unstageAll();
}

GitOperationResult GitService::commit(
    const QString &path,
    const QString &gitExecutable,
    const QString &message) const
{
    return configuredGitCliClient(path, gitExecutable).commit(message);
}

GitOperationResult GitService::push(const QString &path, const QString &gitExecutable) const
{
    return configuredGitCliClient(path, gitExecutable).push();
}

bool GitService::listLocalBranches(
    const QString &path,
    QStringList *branchNames,
    QString *currentBranch,
    QString *errorMessage) const
{
    if (branchNames == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git branch list output pointer is null.");
        }
        return false;
    }

    branchNames->clear();
    if (currentBranch != nullptr) {
        currentBranch->clear();
    }

    GitRepositoryInfo repositoryInfo;
    if (!inspectRepository(path, &repositoryInfo, errorMessage)) {
        return false;
    }

    if (currentBranch != nullptr && !repositoryInfo.isDetachedHead) {
        *currentBranch = repositoryInfo.branchName;
    }

    const QByteArray nativePath = repositoryInfo.localPath.toUtf8();
    git_repository *repository = nullptr;
    if (git_repository_open(&repository, nativePath.constData()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to reopen repository for branch list: %1")
                                .arg(libgit2ErrorMessage("unable to open repository"));
        }
        return false;
    }

    git_branch_iterator *iterator = nullptr;
    if (git_branch_iterator_new(&iterator, repository, GIT_BRANCH_LOCAL) < 0) {
        git_repository_free(repository);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to enumerate local branches: %1")
                                .arg(libgit2ErrorMessage("unable to create branch iterator"));
        }
        return false;
    }

    git_reference *reference = nullptr;
    git_branch_t branchType = GIT_BRANCH_LOCAL;
    while (git_branch_next(&reference, &branchType, iterator) == 0) {
        const char *branchName = nullptr;
        if (git_branch_name(&branchName, reference) == 0 && branchName != nullptr) {
            branchNames->append(QString::fromUtf8(branchName));
        }
        git_reference_free(reference);
        reference = nullptr;
    }

    git_branch_iterator_free(iterator);
    git_repository_free(repository);

    branchNames->removeDuplicates();
    std::sort(branchNames->begin(), branchNames->end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    qCInfo(qtcodeGit) << "Loaded" << branchNames->size() << "local branch(es) for"
                      << repositoryInfo.localPath;
    return true;
}

GitOperationResult GitService::checkoutBranch(
    const QString &path,
    const QString &gitExecutable,
    const QString &branchName) const
{
    return configuredGitCliClient(path, gitExecutable).checkoutBranch(branchName);
}

GitOperationResult GitService::createBranch(
    const QString &path,
    const QString &gitExecutable,
    const QString &branchName) const
{
    return configuredGitCliClient(path, gitExecutable).createBranch(branchName);
}

GitOperationResult GitService::fetchRemoteBranch(
    const QString &path,
    const QString &gitExecutable,
    const QString &remote,
    const QString &branchName) const
{
    return configuredGitCliClient(path, gitExecutable).fetchRemoteBranch(remote, branchName);
}

GitOperationResult GitService::checkoutRemoteBranch(
    const QString &path,
    const QString &gitExecutable,
    const QString &branchName,
    const QString &remote) const
{
    return configuredGitCliClient(path, gitExecutable).checkoutRemoteBranch(branchName, remote);
}

bool GitService::listRepositoryBranchReferences(
    const QString &path,
    const QString &gitExecutable,
    QStringList *branchReferences,
    bool includeRemote,
    QString *errorMessage) const
{
    if (branchReferences == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git branch reference output pointer is null.");
        }
        return false;
    }

    GitRepositoryInfo repositoryInfo;
    if (!inspectRepository(path, &repositoryInfo, errorMessage)) {
        return false;
    }

    if (gitExecutable.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git executable is not configured.");
        }
        return false;
    }

    const GitCliClient client = configuredGitCliClient(path, gitExecutable);
    const GitOperationResult result = client.listBranchReferences(branchReferences, includeRemote);
    if (!result.success && errorMessage != nullptr) {
        *errorMessage = result.errorMessage;
    }

    return result.success;
}

QString commitSubject(const git_commit *commit)
{
    const char *message = git_commit_message(commit);
    if (message == nullptr) {
        return {};
    }

    return QString::fromUtf8(message).split(QLatin1Char('\n')).constFirst().trimmed();
}

bool GitService::loadRecentCommits(
    const QString &path,
    int limit,
    QList<GitCommitSummary> *commits,
    QString *errorMessage) const
{
    if (commits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Git commit list output pointer is null.");
        }
        return false;
    }

    commits->clear();

    if (limit <= 0) {
        limit = 10;
    }

    GitRepositoryInfo repositoryInfo;
    if (!inspectRepository(path, &repositoryInfo, errorMessage)) {
        return false;
    }

    const QByteArray nativePath = repositoryInfo.localPath.toUtf8();
    git_repository *repository = nullptr;
    if (git_repository_open(&repository, nativePath.constData()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to reopen repository for commit history: %1")
                                .arg(libgit2ErrorMessage("unable to open repository"));
        }
        return false;
    }

    git_revwalk *walker = nullptr;
    if (git_revwalk_new(&walker, repository) < 0) {
        git_repository_free(repository);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to initialize commit walker: %1")
                                .arg(libgit2ErrorMessage("unable to create revwalk"));
        }
        return false;
    }

    git_revwalk_sorting(walker, GIT_SORT_TIME);
    if (git_revwalk_push_head(walker) < 0) {
        git_revwalk_free(walker);
        git_repository_free(repository);
        commits->clear();
        qCInfo(qtcodeGit) << "No commits available for" << repositoryInfo.localPath;
        return true;
    }

    git_oid oid {};
    while (commits->size() < limit && git_revwalk_next(&oid, walker) == 0) {
        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, repository, &oid) < 0) {
            continue;
        }

        GitCommitSummary summary;
        summary.shortSha = shortOid(git_commit_id(commit));
        summary.subject = commitSubject(commit);

        const git_signature *author = git_commit_author(commit);
        if (author != nullptr && author->name != nullptr) {
            summary.author = QString::fromUtf8(author->name);
        }

        summary.committedAt = QDateTime::fromSecsSinceEpoch(git_commit_time(commit), Qt::UTC)
                                .toString(Qt::ISODate);

        commits->append(summary);
        git_commit_free(commit);
    }

    git_revwalk_free(walker);
    git_repository_free(repository);

    qCInfo(qtcodeGit) << "Loaded" << commits->size() << "recent commit(s) for"
                      << repositoryInfo.localPath;
    return true;
}

bool GitService::loadPrimaryRemoteUrl(
    const QString &path,
    QString *remoteUrl,
    QString *errorMessage) const
{
    if (remoteUrl != nullptr) {
        remoteUrl->clear();
    }

    GitRepositoryInfo repositoryInfo;
    if (!inspectRepository(path, &repositoryInfo, errorMessage)) {
        return false;
    }

    const QByteArray nativePath = repositoryInfo.localPath.toUtf8();
    git_repository *repository = nullptr;
    if (git_repository_open(&repository, nativePath.constData()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to reopen repository for remote lookup: %1")
                                .arg(libgit2ErrorMessage("unable to open repository"));
        }
        return false;
    }

    git_remote *originRemote = nullptr;
    const int originResult = git_remote_lookup(&originRemote, repository, "origin");
    if (originResult == 0 && originRemote != nullptr) {
        const char *url = git_remote_url(originRemote);
        if (url != nullptr && remoteUrl != nullptr) {
            *remoteUrl = QString::fromUtf8(url);
        }
        git_remote_free(originRemote);
        git_repository_free(repository);
        qCInfo(qtcodeGit) << "Loaded primary remote for" << repositoryInfo.localPath
                          << (remoteUrl != nullptr ? *remoteUrl : QString {});
        return true;
    }

    if (originResult != GIT_ENOTFOUND && originResult < 0) {
        git_repository_free(repository);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to read origin remote: %1")
                                .arg(libgit2ErrorMessage("unable to lookup origin"));
        }
        return false;
    }

    git_strarray remoteNames = {};
    if (git_remote_list(&remoteNames, repository) < 0) {
        git_repository_free(repository);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list repository remotes: %1")
                                .arg(libgit2ErrorMessage("unable to list remotes"));
        }
        return false;
    }

    if (remoteNames.count > 0 && remoteUrl != nullptr) {
        git_remote *fallbackRemote = nullptr;
        if (git_remote_lookup(&fallbackRemote, repository, remoteNames.strings[0]) == 0
            && fallbackRemote != nullptr) {
            const char *url = git_remote_url(fallbackRemote);
            if (url != nullptr) {
                *remoteUrl = QString::fromUtf8(url);
            }
            git_remote_free(fallbackRemote);
        }
    }

    git_strarray_free(&remoteNames);
    git_repository_free(repository);

    qCInfo(qtcodeGit) << "Loaded fallback primary remote for" << repositoryInfo.localPath
                      << (remoteUrl != nullptr ? *remoteUrl : QString {});
    return true;
}

RepositoryGitSnapshot loadRepositoryGitSnapshot(const QString &path, int commitLimit)
{
    RepositoryGitSnapshot snapshot;
    GitService gitService;

    if (!gitService.loadWorkingTreeStatus(path, &snapshot.status, &snapshot.errorMessage)) {
        return snapshot;
    }

    if (commitLimit > 0
        && !gitService.loadRecentCommits(path, commitLimit, &snapshot.commits, &snapshot.errorMessage)) {
        return snapshot;
    }

    snapshot.success = true;
    return snapshot;
}

} // namespace qtcode::git

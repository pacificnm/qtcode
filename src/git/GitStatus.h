#pragma once

#include <QString>
#include <QList>

namespace qtcode::git {

struct ChangedFile
{
    QString path;
    QString statusLabel;
};

struct GitWorkingTreeStatus
{
    QString branchName;
    bool isDetachedHead = false;
    bool hasUpstream = false;
    int commitsAhead = 0;
    int commitsBehind = 0;
    QList<ChangedFile> stagedFiles;
    QList<ChangedFile> unstagedFiles;
};

} // namespace qtcode::git

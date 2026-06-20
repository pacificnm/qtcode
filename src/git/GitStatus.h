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
    QList<ChangedFile> changedFiles;
};

} // namespace qtcode::git

#pragma once

#include <QString>

namespace qtcode::git {

struct GitRepositoryInfo
{
    QString localPath;
    QString branchName;
    QString commitId;
    bool isDetachedHead = false;
    bool hasHead = false;
};

} // namespace qtcode::git

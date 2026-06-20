#pragma once

#include <QString>
#include <QList>

namespace qtcode::github {

struct GitHubRepositoryInfo
{
    QString owner;
    QString name;
    QString url;
    QString description;
};

struct GitHubIssue
{
    int number = 0;
    QString title;
    QString state;
    QString author;
    QString url;
    QString updatedAt;
};

struct GitHubIssueListResult
{
    bool success = false;
    bool fromCache = false;
    QString errorMessage;
    QList<GitHubIssue> issues;
};

} // namespace qtcode::github

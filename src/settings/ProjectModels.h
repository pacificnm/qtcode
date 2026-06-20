#pragma once

#include <QString>

namespace qtcode::settings {

inline constexpr auto kActiveProjectSettingKey = "app.active_project_id";

struct ProjectRecord
{
    QString id;
    QString name;
    QString rootPath;
    QString createdAt;
    QString updatedAt;
    QString lastOpenedAt;
};

struct RepositoryRecord
{
    QString id;
    QString projectId;
    QString localPath;
    QString remoteUrl;
    QString defaultBranch;
    QString githubOwner;
    QString githubName;
    QString createdAt;
    QString updatedAt;
};

} // namespace qtcode::settings

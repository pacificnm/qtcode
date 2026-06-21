#pragma once

#include <QString>

namespace qtcode::settings {

inline constexpr auto kActiveProjectSettingKey = "app.active_project_id";
inline constexpr auto kActiveAgentSessionByProjectSettingKey = "app.active_agent_session_by_project";
inline constexpr auto kAgentSessionRequestOptionsSettingKey = "app.agent_session_request_options";

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

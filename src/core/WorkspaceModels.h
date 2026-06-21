#pragma once

#include <QString>
#include <QStringList>

namespace qtcode::core {

enum class WorkspaceHealthState
{
    Ok,
    Warning,
    Error,
    Unknown,
};

struct WorkspaceHealthCheck
{
    QString id;
    QString label;
    WorkspaceHealthState state = WorkspaceHealthState::Unknown;
    QString message;
    QString fixHint;
};

struct WorkspaceInstallContext
{
    QString projectName;
    QString rootPath;
    QString scopeKey;
    QString databaseUrl = QStringLiteral("postgresql:///qtcode_memory");
};

struct WorkspaceInstallResult
{
    bool success = false;
    QStringList createdPaths;
    QStringList skippedPaths;
    QString message;
};

} // namespace qtcode::core

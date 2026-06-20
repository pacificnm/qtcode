#pragma once

#include "agents/AgentModels.h"

#include <QString>

namespace qtcode::agents {

class DiffApplier
{
public:
    [[nodiscard]] static bool applyArtifact(
        const QString &projectRoot,
        const AgentArtifact &artifact,
        QString *errorMessage = nullptr);

private:
    [[nodiscard]] static bool resolveTargetPath(
        const QString &projectRoot,
        const QString &filePath,
        QString *resolvedPath,
        QString *errorMessage);
    [[nodiscard]] static bool applyFileWrite(
        const QString &targetPath,
        const QString &content,
        QString *errorMessage);
    [[nodiscard]] static bool applyUnifiedDiff(
        const QString &projectRoot,
        const QString &patchContent,
        QString *errorMessage);
};

} // namespace qtcode::agents

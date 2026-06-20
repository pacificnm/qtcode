#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace qtcode::memory {

enum class ContextSourceType
{
    ProjectMemory,
    AgentContext,
    Documentation,
    ArchitectureDecision,
    CodingStandard,
};

struct ContextResult
{
    ContextSourceType sourceType = ContextSourceType::ProjectMemory;
    QString sourceUri;
    QString title;
    QString excerpt;
    double score = 0.0;
    QJsonObject metadata;
    QString retrievedAt;
};

[[nodiscard]] QString contextSourceTypeLabel(ContextSourceType sourceType);
[[nodiscard]] QList<ContextResult> parseProjectMemoryToolOutput(
    const QString &toolOutput,
    const QString &retrievedAt);

} // namespace qtcode::memory

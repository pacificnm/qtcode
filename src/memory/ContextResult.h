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
    SourceFile,
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
[[nodiscard]] QString contextResultSourceKey(const ContextResult &result);
[[nodiscard]] QList<ContextResult> dedupeContextResultsBySource(const QList<ContextResult> &results);
[[nodiscard]] QList<ContextResult> parseProjectMemoryToolOutput(
    const QString &toolOutput,
    const QString &retrievedAt);
[[nodiscard]] ContextResult contextResultFromFilePath(
    const QString &absolutePath,
    const QString &projectRoot = {},
    const QString &contentOverride = {},
    int maxContentLength = 65536);

} // namespace qtcode::memory

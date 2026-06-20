#include "memory/ContextResult.h"

namespace qtcode::memory {

QString contextSourceTypeLabel(ContextSourceType sourceType)
{
    switch (sourceType) {
    case ContextSourceType::ProjectMemory:
        return QStringLiteral("project_memory");
    case ContextSourceType::AgentContext:
        return QStringLiteral("agent_context");
    case ContextSourceType::Documentation:
        return QStringLiteral("documentation");
    case ContextSourceType::ArchitectureDecision:
        return QStringLiteral("architecture_decision");
    case ContextSourceType::CodingStandard:
        return QStringLiteral("coding_standard");
    }

    return QStringLiteral("unknown");
}

QList<ContextResult> parseProjectMemoryToolOutput(
    const QString &toolOutput,
    const QString &retrievedAt)
{
    QList<ContextResult> results;
    const QString trimmedOutput = toolOutput.trimmed();
    if (trimmedOutput.isEmpty()) {
        return results;
    }

    if (trimmedOutput.startsWith(QStringLiteral("No matching project memory"), Qt::CaseInsensitive)) {
        return results;
    }

    QStringList sections = trimmedOutput.split(QStringLiteral("\n\n--- "), Qt::SkipEmptyParts);
    for (int index = 0; index < sections.size(); ++index) {
        QString section = sections.at(index).trimmed();
        if (section.isEmpty()) {
            continue;
        }

        if (!section.startsWith(QStringLiteral("--- "))) {
            section.prepend(QStringLiteral("--- "));
        }

        const int headerEnd = section.indexOf(QStringLiteral(" ---\n"));
        if (headerEnd < 0) {
            continue;
        }

        ContextResult result;
        result.sourceType = ContextSourceType::ProjectMemory;
        result.sourceUri = section.mid(4, headerEnd - 4).trimmed();
        result.title = result.sourceUri;
        result.excerpt = section.mid(headerEnd + 5).trimmed();
        result.retrievedAt = retrievedAt;
        results.append(result);
    }

    return results;
}

} // namespace qtcode::memory

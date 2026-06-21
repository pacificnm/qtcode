#include "memory/ContextResult.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace qtcode::memory {

namespace {

constexpr int kBinarySampleSize = 8192;

[[nodiscard]] bool looksBinaryFile(const QString &absolutePath)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return file.read(kBinarySampleSize).contains('\0');
}

[[nodiscard]] QString relativePathFromProjectRoot(
    const QString &absolutePath,
    const QString &projectRoot)
{
    const QString trimmedRoot = projectRoot.trimmed();
    if (trimmedRoot.isEmpty()) {
        return QFileInfo(absolutePath).absoluteFilePath();
    }

    const QString relativePath = QDir(trimmedRoot).relativeFilePath(absolutePath);
    if (relativePath.startsWith(QStringLiteral(".."))) {
        return QFileInfo(absolutePath).absoluteFilePath();
    }

    return relativePath;
}

} // namespace

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
    case ContextSourceType::SourceFile:
        return QStringLiteral("source_file");
    }

    return QStringLiteral("unknown");
}

QString contextResultSourceKey(const ContextResult &result)
{
    const QString sourceUri = result.sourceUri.trimmed();
    if (!sourceUri.isEmpty()) {
        return sourceUri;
    }

    return result.title.trimmed();
}

QList<ContextResult> dedupeContextResultsBySource(const QList<ContextResult> &results)
{
    QList<ContextResult> deduped;
    deduped.reserve(results.size());

    QHash<QString, int> indexByKey;
    for (const ContextResult &result : results) {
        const QString key = contextResultSourceKey(result);
        if (key.isEmpty()) {
            deduped.append(result);
            continue;
        }

        const int existingIndex = indexByKey.value(key, -1);
        if (existingIndex < 0) {
            indexByKey.insert(key, deduped.size());
            deduped.append(result);
            continue;
        }

        ContextResult &existing = deduped[existingIndex];
        existing.score = qMax(existing.score, result.score);

        const QString excerpt = result.excerpt.trimmed();
        if (excerpt.isEmpty() || existing.excerpt.contains(excerpt)) {
            continue;
        }

        if (!existing.excerpt.trimmed().isEmpty()) {
            existing.excerpt.append(QStringLiteral("\n\n"));
        }
        existing.excerpt.append(excerpt);
    }

    return deduped;
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

ContextResult contextResultFromFilePath(
    const QString &absolutePath,
    const QString &projectRoot,
    const QString &contentOverride,
    int maxContentLength)
{
    ContextResult result;
    const QFileInfo fileInfo(absolutePath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) {
        return result;
    }

    if (contentOverride.isEmpty() && looksBinaryFile(fileInfo.absoluteFilePath())) {
        return result;
    }

    QString content = contentOverride;
    if (content.isEmpty()) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return result;
        }

        content = QString::fromUtf8(file.readAll());
    }

    if (maxContentLength > 0 && content.size() > maxContentLength) {
        content = content.left(maxContentLength).trimmed() + QStringLiteral("\n…");
    }

    result.sourceType = ContextSourceType::SourceFile;
    result.sourceUri = relativePathFromProjectRoot(fileInfo.absoluteFilePath(), projectRoot);
    result.title = result.sourceUri;
    result.excerpt = content;
    result.score = 1.0;
    result.retrievedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return result;
}

} // namespace qtcode::memory

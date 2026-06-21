#include "commands/CommandLibraryService.h"

#include "commands/QtCommandYamlParser.h"
#include "shared/Logging.h"

#include <algorithm>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

namespace qtcode::commands {

CommandLibraryService::CommandLibraryService(QObject *parent)
    : QObject(parent)
{
}

void CommandLibraryService::setProjectRoot(const QString &projectRoot)
{
    const QString normalizedRoot = QDir(projectRoot).absolutePath();
    if (m_projectRoot == normalizedRoot) {
        return;
    }

    m_projectRoot = normalizedRoot;
    (void)refresh();
}

QString CommandLibraryService::projectRoot() const
{
    return m_projectRoot;
}

bool CommandLibraryService::refresh()
{
    m_commands.clear();
    m_diagnostics.clear();
    m_lookupKeys.clear();

    if (m_projectRoot.trimmed().isEmpty()) {
        emit commandsChanged();
        return true;
    }

    const QString commandsDirectory = commandsDirectoryForRoot(m_projectRoot);
    const QDir commandsDir(commandsDirectory);
    if (!commandsDir.exists()) {
        qCInfo(qtcodeCommands) << "No command library directory at" << commandsDirectory;
        emit commandsChanged();
        return true;
    }

    QDirIterator iterator(
        commandsDirectory,
        QStringList{QStringLiteral("*.yaml"), QStringLiteral("*.yml")},
        QDir::Files,
        QDirIterator::NoIteratorFlags);

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        const QtCommandYamlParseResult parseResult =
            QtCommandYamlParser::parseFile(filePath, QtCommandYamlParseMode::IndexOnly);

        QtCommandIndexEntry entry = parseResult.command.index;
        const QFileInfo fileInfo(filePath);
        entry.sourcePath = filePath;
        entry.sourceRelativePath =
            QDir(m_projectRoot).relativeFilePath(filePath);
        entry.lastModified = fileInfo.lastModified();
        entry.parseable = parseResult.success;
        entry.diagnostics = parseResult.diagnostics;
        m_commands.append(entry);
        m_diagnostics.append(parseResult.diagnostics);

        const auto registerLookupKey = [this, &entry](const QString &value) {
            const QString key = normalizedLookupKey(value);
            if (key.isEmpty()) {
                return;
            }
            m_lookupKeys.insert(key, entry.sourcePath);
        };

        registerLookupKey(entry.name);
        for (const QString &alias : entry.aliases) {
            registerLookupKey(alias);
        }
    }

    std::sort(m_commands.begin(), m_commands.end(), [](const QtCommandIndexEntry &left, const QtCommandIndexEntry &right) {
        const QString leftLabel = left.title.isEmpty() ? left.name : left.title;
        const QString rightLabel = right.title.isEmpty() ? right.name : right.title;
        return QString::compare(leftLabel, rightLabel, Qt::CaseInsensitive) < 0;
    });

    qCInfo(qtcodeCommands) << "Loaded" << m_commands.size() << "commands from" << commandsDirectory;
    emit commandsChanged();
    return true;
}

bool CommandLibraryService::hasCommandsDirectory() const
{
    if (m_projectRoot.trimmed().isEmpty()) {
        return false;
    }

    return QDir(commandsDirectoryForRoot(m_projectRoot)).exists();
}

QList<QtCommandIndexEntry> CommandLibraryService::commands() const
{
    return m_commands;
}

QList<QtCommandIndexEntry> CommandLibraryService::search(const QString &query) const
{
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        return m_commands;
    }

    const QStringList tokens =
        trimmedQuery.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    QList<QtCommandIndexEntry> matches;
    matches.reserve(m_commands.size());
    for (const QtCommandIndexEntry &entry : m_commands) {
        if (entryMatchesQuery(entry, tokens)) {
            matches.append(entry);
        }
    }

    std::sort(matches.begin(), matches.end(), [&trimmedQuery](const QtCommandIndexEntry &left, const QtCommandIndexEntry &right) {
        const int leftRank = entrySearchRank(left, trimmedQuery);
        const int rightRank = entrySearchRank(right, trimmedQuery);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }

        const QString leftLabel = left.title.isEmpty() ? left.name : left.title;
        const QString rightLabel = right.title.isEmpty() ? right.name : right.title;
        return QString::compare(leftLabel, rightLabel, Qt::CaseInsensitive) < 0;
    });

    return matches;
}

std::optional<QtCommandDefinition> CommandLibraryService::loadCommand(const QString &nameOrAlias) const
{
    const QString lookupKey = normalizedLookupKey(nameOrAlias);
    if (lookupKey.isEmpty()) {
        return std::nullopt;
    }

    const auto iterator = m_lookupKeys.constFind(lookupKey);
    if (iterator != m_lookupKeys.cend()) {
        return loadCommandFromPath(iterator.value());
    }

    for (const QtCommandIndexEntry &entry : m_commands) {
        if (normalizedLookupKey(entry.name) == lookupKey) {
            return loadCommandFromPath(entry.sourcePath);
        }
    }

    return std::nullopt;
}

std::optional<QtCommandDefinition> CommandLibraryService::loadCommandFromPath(
    const QString &sourcePath) const
{
    if (sourcePath.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const QtCommandYamlParseResult parseResult =
        QtCommandYamlParser::parseFile(sourcePath, QtCommandYamlParseMode::Full);

    QtCommandDefinition definition = parseResult.command;
    const QFileInfo fileInfo(sourcePath);
    definition.index.sourcePath = sourcePath;
    definition.index.sourceRelativePath =
        m_projectRoot.trimmed().isEmpty()
            ? fileInfo.fileName()
            : QDir(m_projectRoot).relativeFilePath(sourcePath);
    definition.index.lastModified = fileInfo.lastModified();
    definition.index.parseable = parseResult.success;
    definition.index.diagnostics = parseResult.diagnostics;
    definition.fullyLoaded = true;
    return definition;
}

QList<QtCommandDiagnostic> CommandLibraryService::diagnostics() const
{
    return m_diagnostics;
}

QString CommandLibraryService::commandsDirectoryForRoot(const QString &projectRoot)
{
    return QDir(projectRoot).absoluteFilePath(QString::fromLatin1(kCommandsRelativeDirectory));
}

QString CommandLibraryService::normalizedLookupKey(const QString &value)
{
    return value.trimmed().toLower();
}

bool CommandLibraryService::entryMatchesQuery(
    const QtCommandIndexEntry &entry,
    const QStringList &tokens)
{
    auto haystackForToken = [&entry](const QString &token) -> bool {
        const QString needle = token.trimmed();
        if (needle.isEmpty()) {
            return true;
        }

        const auto containsNeedle = [&needle](const QString &value) {
            return value.contains(needle, Qt::CaseInsensitive);
        };

        if (containsNeedle(entry.name) || containsNeedle(entry.title)
            || containsNeedle(entry.description) || containsNeedle(entry.category)) {
            return true;
        }

        for (const QString &tag : entry.tags) {
            if (containsNeedle(tag)) {
                return true;
            }
        }
        for (const QString &alias : entry.aliases) {
            if (containsNeedle(alias)) {
                return true;
            }
        }
        for (const QString &hint : entry.memoryHints) {
            if (containsNeedle(hint)) {
                return true;
            }
        }
        for (const QString &rulePath : entry.rulesPaths) {
            if (containsNeedle(rulePath)) {
                return true;
            }
        }
        for (const QString &examplePath : entry.examplesPaths) {
            if (containsNeedle(examplePath)) {
                return true;
            }
        }

        return false;
    };

    for (const QString &token : tokens) {
        if (!haystackForToken(token)) {
            return false;
        }
    }

    return true;
}

int CommandLibraryService::entrySearchRank(
    const QtCommandIndexEntry &entry,
    const QString &query)
{
    const QString normalizedQuery = normalizedLookupKey(query);
    if (normalizedQuery.isEmpty()) {
        return 100;
    }

    if (normalizedLookupKey(entry.name) == normalizedQuery) {
        return 0;
    }

    for (const QString &alias : entry.aliases) {
        if (normalizedLookupKey(alias) == normalizedQuery) {
            return 1;
        }
    }

    if (entry.name.startsWith(query, Qt::CaseInsensitive)
        || entry.title.startsWith(query, Qt::CaseInsensitive)) {
        return 2;
    }

    return 10;
}

} // namespace qtcode::commands

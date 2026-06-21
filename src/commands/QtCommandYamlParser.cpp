#include "commands/QtCommandYamlParser.h"

#include <QFile>
#include <QFileInfo>

namespace qtcode::commands {

namespace {

enum class YamlSection
{
    None,
    Tags,
    Aliases,
    MemoryHints,
    Rules,
    Examples,
    Outputs,
    Inputs,
    InputItem,
    Validation,
};

[[nodiscard]] int lineIndent(const QString &line)
{
    int indent = 0;
    for (const QChar ch : line) {
        if (ch == QLatin1Char(' ')) {
            ++indent;
            continue;
        }
        if (ch == QLatin1Char('\t')) {
            indent += 4;
            continue;
        }
        break;
    }
    return indent;
}

[[nodiscard]] bool isTopLevelLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    return !trimmed.isEmpty() && lineIndent(line) == 0;
}

[[nodiscard]] QString stripQuotes(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"'))
            || (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.mid(1, value.size() - 2);
        }
    }
    return value.trimmed();
}

[[nodiscard]] QString parseKeyedScalar(const QString &line, const QString &key)
{
    const QString trimmed = line.trimmed();
    const QString prefix = key + QStringLiteral(":");
    if (!trimmed.startsWith(prefix)) {
        return {};
    }

    QString value = trimmed.mid(prefix.size()).trimmed();
    if (value == QStringLiteral("|") || value == QStringLiteral(">")) {
        return {};
    }

    return stripQuotes(value);
}

[[nodiscard]] bool parseBoolScalar(const QString &value, bool *parsed)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("true") || normalized == QStringLiteral("yes")) {
        *parsed = true;
        return true;
    }
    if (normalized == QStringLiteral("false") || normalized == QStringLiteral("no")) {
        *parsed = false;
        return true;
    }
    return false;
}

[[nodiscard]] QString parseListItemValue(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char('-'))) {
        return {};
    }

    const QString value = trimmed.mid(1).trimmed();
    if (value.contains(QLatin1Char(':'))) {
        return {};
    }

    return stripQuotes(value);
}

[[nodiscard]] bool looksLikeMappingListItem(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char('-'))) {
        return false;
    }

    return trimmed.mid(1).trimmed().contains(QLatin1Char(':'));
}

[[nodiscard]] bool isSectionHeaderLine(const QString &line, const QString &sectionName)
{
    const QString trimmed = line.trimmed();
    return trimmed == sectionName + QStringLiteral(":") || trimmed.startsWith(sectionName + QStringLiteral(": "));
}

[[nodiscard]] YamlSection sectionForHeader(const QString &line)
{
    if (isSectionHeaderLine(line, QStringLiteral("tags"))) {
        return YamlSection::Tags;
    }
    if (isSectionHeaderLine(line, QStringLiteral("aliases"))) {
        return YamlSection::Aliases;
    }
    if (isSectionHeaderLine(line, QStringLiteral("memoryHints"))) {
        return YamlSection::MemoryHints;
    }
    if (isSectionHeaderLine(line, QStringLiteral("rules"))) {
        return YamlSection::Rules;
    }
    if (isSectionHeaderLine(line, QStringLiteral("examples"))) {
        return YamlSection::Examples;
    }
    if (isSectionHeaderLine(line, QStringLiteral("outputs"))) {
        return YamlSection::Outputs;
    }
    if (isSectionHeaderLine(line, QStringLiteral("inputs"))) {
        return YamlSection::Inputs;
    }
    if (isSectionHeaderLine(line, QStringLiteral("validation"))) {
        return YamlSection::Validation;
    }
    return YamlSection::None;
}

[[nodiscard]] bool shouldSkipSectionInIndexMode(const YamlSection section)
{
    return section == YamlSection::Inputs || section == YamlSection::Validation;
}

void appendMissingFieldDiagnostic(
    QList<QtCommandDiagnostic> *diagnostics,
    const QString &fieldName,
    const QString &filePath)
{
    diagnostics->append(makeDiagnostic(
        QtCommandDiagnosticSeverity::Warning,
        QStringLiteral("missing-required-field"),
        QStringLiteral("Missing required field '%1'.").arg(fieldName),
        filePath));
}

void validateRequiredFields(
    const QtCommandDefinition &command,
    const QString &filePath,
    QList<QtCommandDiagnostic> *diagnostics)
{
    const auto &index = command.index;
    if (index.name.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("name"), filePath);
    }
    if (index.title.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("title"), filePath);
    }
    if (index.description.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("description"), filePath);
    }
    if (index.category.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("category"), filePath);
    }
    if (command.version.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("version"), filePath);
    }
    if (command.templatePath.trimmed().isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("template"), filePath);
    }
    if (command.rulesPaths.isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("rules"), filePath);
    }
    if (command.examplesPaths.isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("examples"), filePath);
    }
    if (command.validation.settings.isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("validation"), filePath);
    }
    if (command.inputs.isEmpty()) {
        appendMissingFieldDiagnostic(diagnostics, QStringLiteral("inputs"), filePath);
    }
}

[[nodiscard]] bool hasErrorDiagnostic(const QList<QtCommandDiagnostic> &diagnostics)
{
    for (const QtCommandDiagnostic &diagnostic : diagnostics) {
        if (diagnostic.isError()) {
            return true;
        }
    }
    return false;
}

} // namespace

QtCommandYamlParseResult QtCommandYamlParser::parseFile(
    const QString &filePath,
    const QtCommandYamlParseMode mode)
{
    QFile commandFile(filePath);
    if (!commandFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QtCommandYamlParseResult result;
        result.diagnostics.append(makeDiagnostic(
            QtCommandDiagnosticSeverity::Error,
            QStringLiteral("invalid-yaml"),
            QStringLiteral("Unable to read command file."),
            filePath));
        return result;
    }

    return parseContent(QString::fromUtf8(commandFile.readAll()), filePath, mode);
}

QtCommandYamlParseResult QtCommandYamlParser::parseContent(
    const QString &yamlContent,
    const QString &filePathForDiagnostics,
    const QtCommandYamlParseMode mode)
{
    QtCommandYamlParseResult result;
    result.command.index.sourcePath = filePathForDiagnostics;
    result.command.index.parseable = true;

    YamlSection section = YamlSection::None;
    int skipSectionUntilIndent = -1;
    QtCommandInput currentInput;

    const QStringList lines = yamlContent.split(QLatin1Char('\n'));
    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        const QString &line = lines.at(lineNumber);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (skipSectionUntilIndent >= 0) {
            if (isTopLevelLine(line)) {
                skipSectionUntilIndent = -1;
            } else {
                continue;
            }
        }

        if (isTopLevelLine(line)) {
            if (section == YamlSection::InputItem && !currentInput.name.isEmpty()) {
                result.command.inputs.append(currentInput);
                currentInput = QtCommandInput{};
            }
            section = YamlSection::None;

            const YamlSection headerSection = sectionForHeader(line);
            if (headerSection != YamlSection::None) {
                if (mode == QtCommandYamlParseMode::IndexOnly
                    && shouldSkipSectionInIndexMode(headerSection)) {
                    skipSectionUntilIndent = 0;
                    continue;
                }

                section = headerSection;
                if (headerSection == YamlSection::Inputs) {
                    currentInput = QtCommandInput{};
                }
                continue;
            }

            const QString legacyId = parseKeyedScalar(line, QStringLiteral("id"));
            if (!legacyId.isEmpty() && result.command.index.name.isEmpty()) {
                result.command.index.name = legacyId;
            }

            const QString name = parseKeyedScalar(line, QStringLiteral("name"));
            if (!name.isEmpty()) {
                result.command.index.name = name;
            }

            const QString title = parseKeyedScalar(line, QStringLiteral("title"));
            if (!title.isEmpty()) {
                result.command.index.title = title;
            }

            const QString description = parseKeyedScalar(line, QStringLiteral("description"));
            if (!description.isEmpty()) {
                result.command.index.description = description;
            }

            const QString category = parseKeyedScalar(line, QStringLiteral("category"));
            if (!category.isEmpty()) {
                result.command.index.category = category;
            }

            const QString status = parseKeyedScalar(line, QStringLiteral("status"));
            if (!status.isEmpty()) {
                result.command.index.status = commandStatusFromString(status);
            }

            const QString version = parseKeyedScalar(line, QStringLiteral("version"));
            if (!version.isEmpty()) {
                result.command.version = version;
            }

            const QString templatePath = parseKeyedScalar(line, QStringLiteral("template"));
            if (!templatePath.isEmpty()) {
                result.command.templatePath = templatePath;
            }

            const QString defaultOutput = parseKeyedScalar(line, QStringLiteral("defaultOutput"));
            if (!defaultOutput.isEmpty()) {
                result.command.defaultOutput = defaultOutput;
            }

            if (trimmed.contains(QLatin1Char(':')) && !trimmed.endsWith(QLatin1Char(':'))) {
                const int colonIndex = trimmed.indexOf(QLatin1Char(':'));
                const QString key = trimmed.left(colonIndex).trimmed();
                const QString value = stripQuotes(trimmed.mid(colonIndex + 1));
                if (key.isEmpty()) {
                    result.diagnostics.append(makeDiagnostic(
                        QtCommandDiagnosticSeverity::Error,
                        QStringLiteral("invalid-yaml"),
                        QStringLiteral("Invalid top-level mapping at line %1.").arg(lineNumber + 1),
                        filePathForDiagnostics));
                    result.command.index.parseable = false;
                } else if (value.isEmpty()) {
                    result.diagnostics.append(makeDiagnostic(
                        QtCommandDiagnosticSeverity::Warning,
                        QStringLiteral("invalid-yaml"),
                        QStringLiteral("Empty scalar value for '%1' at line %2.")
                            .arg(key, QString::number(lineNumber + 1)),
                        filePathForDiagnostics));
                }
            }

            continue;
        }

        if (section == YamlSection::Tags) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.index.tags.append(item);
            }
            continue;
        }

        if (section == YamlSection::Aliases) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.index.aliases.append(item);
            }
            continue;
        }

        if (section == YamlSection::MemoryHints) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.index.memoryHints.append(item);
            }
            continue;
        }

        if (section == YamlSection::Rules) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.rulesPaths.append(item);
                result.command.index.rulesPaths.append(item);
            }
            continue;
        }

        if (section == YamlSection::Examples) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.examplesPaths.append(item);
                result.command.index.examplesPaths.append(item);
            }
            continue;
        }

        if (section == YamlSection::Outputs) {
            const QString item = parseListItemValue(line);
            if (!item.isEmpty()) {
                result.command.outputs.append(item);
            }
            continue;
        }

        if (section == YamlSection::Validation) {
            const QString key = trimmed.section(QLatin1Char(':'), 0, 0).trimmed();
            const QString rawValue = trimmed.section(QLatin1Char(':'), 1).trimmed();
            bool parsedValue = false;
            if (!key.isEmpty() && parseBoolScalar(rawValue, &parsedValue)) {
                result.command.validation.settings.insert(key, parsedValue);
            } else if (!key.isEmpty()) {
                result.diagnostics.append(makeDiagnostic(
                    QtCommandDiagnosticSeverity::Warning,
                    QStringLiteral("invalid-yaml"),
                    QStringLiteral("Validation setting '%1' must be a boolean.").arg(key),
                    filePathForDiagnostics));
            }
            continue;
        }

        if (section == YamlSection::Inputs || section == YamlSection::InputItem) {
            if (looksLikeMappingListItem(line)) {
                if (section == YamlSection::InputItem && !currentInput.name.isEmpty()) {
                    result.command.inputs.append(currentInput);
                }
                currentInput = QtCommandInput{};
                section = YamlSection::InputItem;

                const int dashIndex = line.indexOf(QLatin1Char('-'));
                const QString inlineName = parseKeyedScalar(line.mid(dashIndex + 1), QStringLiteral("name"));
                if (!inlineName.isEmpty()) {
                    currentInput.name = inlineName;
                }
            }

            const QString inputName = parseKeyedScalar(line, QStringLiteral("name"));
            if (!inputName.isEmpty()) {
                currentInput.name = inputName;
            }

            const QString label = parseKeyedScalar(line, QStringLiteral("label"));
            if (!label.isEmpty()) {
                currentInput.label = label;
            }

            const QString type = parseKeyedScalar(line, QStringLiteral("type"));
            if (!type.isEmpty()) {
                currentInput.type = type;
            }

            const QString example = parseKeyedScalar(line, QStringLiteral("example"));
            if (!example.isEmpty()) {
                currentInput.example = example;
            }

            const QString defaultValue = parseKeyedScalar(line, QStringLiteral("defaultValue"));
            if (!defaultValue.isEmpty()) {
                currentInput.defaultValue = defaultValue;
            }

            const QString requiredValue = parseKeyedScalar(line, QStringLiteral("required"));
            if (!requiredValue.isEmpty()) {
                bool parsedRequired = false;
                if (parseBoolScalar(requiredValue, &parsedRequired)) {
                    currentInput.required = parsedRequired;
                } else {
                    result.diagnostics.append(makeDiagnostic(
                        QtCommandDiagnosticSeverity::Warning,
                        QStringLiteral("invalid-yaml"),
                        QStringLiteral("Input '%1' has a non-boolean required value.")
                            .arg(currentInput.name),
                        filePathForDiagnostics));
                }
            }
        }
    }

    if (section == YamlSection::InputItem && !currentInput.name.isEmpty()) {
        result.command.inputs.append(currentInput);
    }

    if (result.command.index.name.isEmpty()) {
        const QFileInfo fileInfo(filePathForDiagnostics);
        result.command.index.name = fileInfo.baseName();
    }

    result.command.fullyLoaded = mode == QtCommandYamlParseMode::Full;

    if (mode == QtCommandYamlParseMode::Full) {
        validateRequiredFields(result.command, filePathForDiagnostics, &result.diagnostics);
    }

    result.command.index.diagnostics = result.diagnostics;
    result.success = !hasErrorDiagnostic(result.diagnostics);

    return result;
}

} // namespace qtcode::commands

#pragma once

#include "commands/QtCommandModels.h"

namespace qtcode::commands {

enum class QtCommandYamlParseMode
{
    IndexOnly,
    Full,
};

struct QtCommandYamlParseResult
{
    QtCommandDefinition command;
    QList<QtCommandDiagnostic> diagnostics;
    bool success = false;
};

class QtCommandYamlParser
{
public:
    [[nodiscard]] static QtCommandYamlParseResult parseFile(
        const QString &filePath,
        QtCommandYamlParseMode mode = QtCommandYamlParseMode::Full);
    [[nodiscard]] static QtCommandYamlParseResult parseContent(
        const QString &yamlContent,
        const QString &filePathForDiagnostics,
        QtCommandYamlParseMode mode = QtCommandYamlParseMode::Full);
};

} // namespace qtcode::commands

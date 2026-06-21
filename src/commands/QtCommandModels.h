#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace qtcode::commands {

inline constexpr auto kCommandsRelativeDirectory = ".qtcode/commands";

enum class QtCommandDiagnosticSeverity
{
    Info,
    Warning,
    Error,
};

enum class QtCommandStatus
{
    Unknown,
    Draft,
    Stable,
    Deprecated,
};

struct QtCommandDiagnostic
{
    QtCommandDiagnosticSeverity severity = QtCommandDiagnosticSeverity::Error;
    QString code;
    QString message;
    QString filePath;

    [[nodiscard]] bool isError() const
    {
        return severity == QtCommandDiagnosticSeverity::Error;
    }
};

struct QtCommandInput
{
    QString name;
    QString label;
    QString type;
    bool required = false;
    QString example;
    QString defaultValue;
};

struct QtCommandValidation
{
    QHash<QString, bool> settings;
};

struct QtCommandIndexEntry
{
    QString name;
    QString title;
    QString description;
    QString category;
    QStringList tags;
    QtCommandStatus status = QtCommandStatus::Unknown;
    QStringList aliases;
    QStringList memoryHints;
    QStringList rulesPaths;
    QStringList examplesPaths;
    QString sourcePath;
    QString sourceRelativePath;
    QDateTime lastModified;
    bool parseable = true;
    QList<QtCommandDiagnostic> diagnostics;
};

struct QtCommandDefinition
{
    QtCommandIndexEntry index;
    QString version;
    QString templatePath;
    QStringList rulesPaths;
    QStringList examplesPaths;
    QStringList outputs;
    QString defaultOutput;
    QList<QtCommandInput> inputs;
    QtCommandValidation validation;
    bool fullyLoaded = false;
};

[[nodiscard]] QString diagnosticSeverityToString(QtCommandDiagnosticSeverity severity);
[[nodiscard]] QtCommandDiagnosticSeverity diagnosticSeverityFromString(const QString &value);
[[nodiscard]] QString commandStatusToString(QtCommandStatus status);
[[nodiscard]] QtCommandStatus commandStatusFromString(const QString &value);

[[nodiscard]] QtCommandDiagnostic makeDiagnostic(
    QtCommandDiagnosticSeverity severity,
    const QString &code,
    const QString &message,
    const QString &filePath = {});

} // namespace qtcode::commands

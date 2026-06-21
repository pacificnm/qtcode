#include "commands/QtCommandModels.h"

namespace qtcode::commands {

QString diagnosticSeverityToString(const QtCommandDiagnosticSeverity severity)
{
    switch (severity) {
    case QtCommandDiagnosticSeverity::Info:
        return QStringLiteral("info");
    case QtCommandDiagnosticSeverity::Warning:
        return QStringLiteral("warning");
    case QtCommandDiagnosticSeverity::Error:
        return QStringLiteral("error");
    }

    return QStringLiteral("error");
}

QtCommandDiagnosticSeverity diagnosticSeverityFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("info")) {
        return QtCommandDiagnosticSeverity::Info;
    }
    if (normalized == QStringLiteral("warning")) {
        return QtCommandDiagnosticSeverity::Warning;
    }
    return QtCommandDiagnosticSeverity::Error;
}

QString commandStatusToString(const QtCommandStatus status)
{
    switch (status) {
    case QtCommandStatus::Draft:
        return QStringLiteral("draft");
    case QtCommandStatus::Stable:
        return QStringLiteral("stable");
    case QtCommandStatus::Deprecated:
        return QStringLiteral("deprecated");
    case QtCommandStatus::Unknown:
        break;
    }

    return {};
}

QtCommandStatus commandStatusFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("draft")) {
        return QtCommandStatus::Draft;
    }
    if (normalized == QStringLiteral("stable")) {
        return QtCommandStatus::Stable;
    }
    if (normalized == QStringLiteral("deprecated")) {
        return QtCommandStatus::Deprecated;
    }
    return QtCommandStatus::Unknown;
}

QtCommandDiagnostic makeDiagnostic(
    const QtCommandDiagnosticSeverity severity,
    const QString &code,
    const QString &message,
    const QString &filePath)
{
    QtCommandDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.filePath = filePath;
    return diagnostic;
}

} // namespace qtcode::commands

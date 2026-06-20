#include "terminal/TerminalProfile.h"

#include <QFileInfo>

namespace qtcode::terminal {

namespace {

constexpr auto kShellPathKey = "shellPath";
constexpr auto kWorkingDirectoryModeKey = "workingDirectoryMode";
constexpr auto kCustomWorkingDirectoryKey = "customWorkingDirectory";

} // namespace

QJsonObject TerminalProfile::toJson() const
{
    QJsonObject json;
    json.insert(QString::fromUtf8(kShellPathKey), shellPath);
    json.insert(
        QString::fromUtf8(kWorkingDirectoryModeKey),
        workingDirectoryModeToString(workingDirectoryMode));
    json.insert(QString::fromUtf8(kCustomWorkingDirectoryKey), customWorkingDirectory);
    return json;
}

TerminalProfile TerminalProfile::fromJson(const QJsonObject &json)
{
    TerminalProfile profile = TerminalProfile::defaults();
    profile.shellPath = json.value(QString::fromUtf8(kShellPathKey)).toString();

    bool modeOk = false;
    profile.workingDirectoryMode = workingDirectoryModeFromString(
        json.value(QString::fromUtf8(kWorkingDirectoryModeKey)).toString(),
        &modeOk);
    if (!modeOk) {
        profile.workingDirectoryMode = WorkingDirectoryMode::ProjectRoot;
    }

    profile.customWorkingDirectory =
        json.value(QString::fromUtf8(kCustomWorkingDirectoryKey)).toString();
    return profile;
}

TerminalProfile TerminalProfile::defaults()
{
    return TerminalProfile {};
}

bool TerminalProfile::hasShellOverride() const
{
    return !shellPath.trimmed().isEmpty();
}

bool TerminalProfile::hasWorkingDirectoryOverride() const
{
    return workingDirectoryMode != WorkingDirectoryMode::ProjectRoot
        || !customWorkingDirectory.trimmed().isEmpty();
}

QString defaultShellFromEnvironment()
{
    const QByteArray shell = qgetenv("SHELL");
    if (!shell.isEmpty()) {
        return QString::fromLocal8Bit(shell);
    }

    return QStringLiteral("/bin/bash");
}

QString resolveShellPath(const QString &configuredShellPath)
{
    const QString trimmedPath = configuredShellPath.trimmed();
    if (!trimmedPath.isEmpty()) {
        const QFileInfo shellInfo(trimmedPath);
        if (shellInfo.exists() && shellInfo.isExecutable()) {
            return shellInfo.canonicalFilePath();
        }
    }

    return defaultShellFromEnvironment();
}

QString workingDirectoryModeToString(WorkingDirectoryMode mode)
{
    switch (mode) {
    case WorkingDirectoryMode::Home:
        return QStringLiteral("home");
    case WorkingDirectoryMode::CustomPath:
        return QStringLiteral("custom");
    case WorkingDirectoryMode::ProjectRoot:
    default:
        return QStringLiteral("project_root");
    }
}

WorkingDirectoryMode workingDirectoryModeFromString(const QString &value, bool *ok)
{
    const QString normalizedValue = value.trimmed().toLower();
    if (normalizedValue.isEmpty() || normalizedValue == QStringLiteral("project_root")) {
        if (ok != nullptr) {
            *ok = true;
        }
        return WorkingDirectoryMode::ProjectRoot;
    }

    if (normalizedValue == QStringLiteral("home")) {
        if (ok != nullptr) {
            *ok = true;
        }
        return WorkingDirectoryMode::Home;
    }

    if (normalizedValue == QStringLiteral("custom")) {
        if (ok != nullptr) {
            *ok = true;
        }
        return WorkingDirectoryMode::CustomPath;
    }

    if (ok != nullptr) {
        *ok = false;
    }
    return WorkingDirectoryMode::ProjectRoot;
}

TerminalProfile mergeProfiles(
    const TerminalProfile &globalProfile,
    const TerminalProfile &projectProfile)
{
    TerminalProfile merged = globalProfile;

    if (projectProfile.hasShellOverride()) {
        merged.shellPath = projectProfile.shellPath;
    }

    if (projectProfile.hasWorkingDirectoryOverride()) {
        merged.workingDirectoryMode = projectProfile.workingDirectoryMode;
        merged.customWorkingDirectory = projectProfile.customWorkingDirectory;
    }

    return merged;
}

} // namespace qtcode::terminal

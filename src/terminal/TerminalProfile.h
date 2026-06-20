#pragma once

#include <QJsonObject>
#include <QString>

namespace qtcode::terminal {

enum class WorkingDirectoryMode {
    ProjectRoot,
    Home,
    CustomPath,
};

struct TerminalProfile
{
    QString shellPath;
    WorkingDirectoryMode workingDirectoryMode = WorkingDirectoryMode::ProjectRoot;
    QString customWorkingDirectory;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static TerminalProfile fromJson(const QJsonObject &json);
    [[nodiscard]] static TerminalProfile defaults();

    [[nodiscard]] bool hasShellOverride() const;
    [[nodiscard]] bool hasWorkingDirectoryOverride() const;
};

[[nodiscard]] QString defaultShellFromEnvironment();
[[nodiscard]] QString resolveShellPath(const QString &configuredShellPath);
[[nodiscard]] QString workingDirectoryModeToString(WorkingDirectoryMode mode);
[[nodiscard]] WorkingDirectoryMode workingDirectoryModeFromString(
    const QString &value,
    bool *ok = nullptr);
[[nodiscard]] TerminalProfile mergeProfiles(
    const TerminalProfile &globalProfile,
    const TerminalProfile &projectProfile);

inline constexpr auto kDefaultShellSettingKey = "app.default_shell";
inline constexpr auto kGlobalTerminalProfileSettingKey = "app.terminal_profile";
inline constexpr auto kProjectTerminalProfileSettingKeyPrefix = "terminal.project_profile.";

} // namespace qtcode::terminal

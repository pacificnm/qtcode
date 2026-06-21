#pragma once

#include <QString>

namespace qtcode::settings {

inline constexpr auto kAppConfigGroupGeneral = "General";
inline constexpr auto kAppConfigKeyRestoreLastProject = "restoreLastProjectOnStartup";
inline constexpr auto kAppConfigKeyStartMaximized = "startMaximized";

struct AppConfig
{
    bool restoreLastProjectOnStartup = true;
    bool startMaximized = false;

    [[nodiscard]] static AppConfig defaults();
};

} // namespace qtcode::settings

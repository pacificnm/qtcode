#pragma once

#include <QString>

namespace qtcode::terminal {

[[nodiscard]] QString defaultShellFromEnvironment();
[[nodiscard]] QString resolveShellPath(const QString &configuredShellPath);

} // namespace qtcode::terminal

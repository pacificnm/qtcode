#include "terminal/TerminalProfile.h"

#include <QFileInfo>

namespace qtcode::terminal {

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

} // namespace qtcode::terminal

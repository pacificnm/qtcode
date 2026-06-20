#pragma once

#include <QString>

namespace qtcode::terminal {

struct TerminalSession
{
    QString id;
    QString projectId;
    QString title;
    QString shellPath;
    QString workingDirectory;
    QString profileJson;
    QString lastCommand;
    QString createdAt;
    QString updatedAt;
};

} // namespace qtcode::terminal

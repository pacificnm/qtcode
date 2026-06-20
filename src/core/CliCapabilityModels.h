#pragma once

#include <QString>

namespace qtcode::core {

struct CliToolCapability
{
    QString toolId;
    QString displayName;
    bool available = false;
    QString executablePath;
    QString version;
    QString unavailableMessage;
};

struct CliCapabilitiesSnapshot
{
    CliToolCapability git;
    CliToolCapability gh;
    CliToolCapability agentCli;
};

} // namespace qtcode::core

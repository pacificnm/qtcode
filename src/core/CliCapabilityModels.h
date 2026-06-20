#pragma once

#include <QString>

namespace qtcode::core {

struct CliToolCapability
{
    QString toolId;
    QString displayName;
    bool available = false;
    bool authenticated = false;
    QString executablePath;
    QString version;
    QString unavailableMessage;
    QString authUnavailableMessage;
    QString authAccount;
};

struct CliCapabilitiesSnapshot
{
    CliToolCapability git;
    CliToolCapability gh;
    CliToolCapability agentCli;
};

} // namespace qtcode::core

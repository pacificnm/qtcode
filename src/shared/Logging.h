#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(qtcodeApp)
Q_DECLARE_LOGGING_CATEGORY(qtcodeUi)
Q_DECLARE_LOGGING_CATEGORY(qtcodeCore)
Q_DECLARE_LOGGING_CATEGORY(qtcodeStorage)
Q_DECLARE_LOGGING_CATEGORY(qtcodeAgents)
Q_DECLARE_LOGGING_CATEGORY(qtcodeTerminal)
Q_DECLARE_LOGGING_CATEGORY(qtcodeGit)
Q_DECLARE_LOGGING_CATEGORY(qtcodeGithub)
Q_DECLARE_LOGGING_CATEGORY(qtcodeMemory)
Q_DECLARE_LOGGING_CATEGORY(qtcodeCommands)

namespace qtcode::shared {

void configureLogging();

} // namespace qtcode::shared

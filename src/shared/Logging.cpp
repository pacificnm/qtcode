#include "shared/Logging.h"

#include <QLoggingCategory>
#include <QSysInfo>

Q_LOGGING_CATEGORY(qtcodeApp, "qtcode.app")
Q_LOGGING_CATEGORY(qtcodeUi, "qtcode.ui")
Q_LOGGING_CATEGORY(qtcodeCore, "qtcode.core")
Q_LOGGING_CATEGORY(qtcodeStorage, "qtcode.storage")
Q_LOGGING_CATEGORY(qtcodeAgents, "qtcode.agents")
Q_LOGGING_CATEGORY(qtcodeTerminal, "qtcode.terminal")
Q_LOGGING_CATEGORY(qtcodeGit, "qtcode.git")
Q_LOGGING_CATEGORY(qtcodeGithub, "qtcode.github")
Q_LOGGING_CATEGORY(qtcodeMemory, "qtcode.memory")
Q_LOGGING_CATEGORY(qtcodeCommands, "qtcode.commands")

namespace qtcode::shared {

void configureLogging()
{
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
#ifndef NDEBUG
        QLoggingCategory::setFilterRules(QStringLiteral("qtcode.*.debug=true"));
#endif
    }

    qCInfo(qtcodeApp) << "Logging configured for QTCode subsystems";
    qCInfo(qtcodeApp) << "Host:" << QSysInfo::prettyProductName()
                      << "kernel:" << QSysInfo::kernelVersion();
}

} // namespace qtcode::shared

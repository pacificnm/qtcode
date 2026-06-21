#include "shared/ProcessRunner.h"

#include <QProcess>

namespace qtcode::shared {

ProcessResult ProcessRunner::run(
    const QString &program,
    const QStringList &arguments,
    int timeoutMs,
    const QString &workingDirectory)
{
    ProcessResult result;

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }
    process.start(QProcess::ReadOnly);

    if (!process.waitForStarted(timeoutMs)) {
        result.exitCode = process.exitCode();
        result.standardError = process.errorString();
        return result;
    }

    result.started = true;

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(timeoutMs);
        result.exitCode = process.exitCode();
        result.standardError = QStringLiteral("Process timed out.");
        result.standardOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        return result;
    }

    result.exitCode = process.exitCode();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    result.standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return result;
}

} // namespace qtcode::shared

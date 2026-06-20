#pragma once

#include <QString>
#include <QStringList>

namespace qtcode::shared {

struct ProcessResult
{
    bool started = false;
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
};

class ProcessRunner
{
public:
    [[nodiscard]] static ProcessResult run(
        const QString &program,
        const QStringList &arguments = {},
        int timeoutMs = 5000);
};

} // namespace qtcode::shared

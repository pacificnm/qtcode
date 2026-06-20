#include "memory/McpClient.h"

#include "shared/Logging.h"
#include "shared/SecretStore.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace qtcode::memory {

namespace {

constexpr auto kInitializeRequestId = 1;

McpHealthResult makeResult(
    McpHealthState state,
    const settings::McpServerRecord &server,
    const QString &message,
    const QString &serverInfo = {})
{
    McpHealthResult result;
    result.state = state;
    result.message = message;
    result.serverName = server.name;
    result.serverInfo = serverInfo;
    result.checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return result;
}

QByteArray buildMessageLine(const QJsonObject &payload)
{
    return QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n';
}

bool readJsonLine(QProcess &process, QJsonObject *payload, int timeoutMs)
{
    if (payload == nullptr) {
        return false;
    }

    while (true) {
        if (!process.canReadLine()) {
            if (!process.waitForReadyRead(timeoutMs)) {
                return false;
            }
        }

        const QByteArray line = process.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            return false;
        }

        *payload = document.object();
        return true;
    }
}

QProcessEnvironment buildProcessEnvironment(const settings::McpServerRecord &server)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString &secretKey : server.secretEnvKeys()) {
        if (environment.contains(secretKey)) {
            continue;
        }

        QString secretValue;
        QString secretError;
        if (shared::SecretStore::loadServerSecret(server.id, secretKey, &secretValue, &secretError)) {
            environment.insert(secretKey, secretValue);
        } else {
            qCInfo(qtcodeMemory) << "Secret env key not loaded for health check:" << secretKey
                                 << secretError;
        }
    }
    return environment;
}

McpHealthResult checkStdioServerHealth(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    int timeoutMs)
{
    if (server.endpoint.trimmed().isEmpty()) {
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("MCP server endpoint must not be empty."));
    }

    QProcess process;
    process.setProgram(server.endpoint);
    process.setArguments(server.args());
    process.setProcessEnvironment(buildProcessEnvironment(server));
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }

    process.start(QProcess::ReadWrite);
    if (!process.waitForStarted(timeoutMs)) {
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("Failed to start MCP server process: %1").arg(process.errorString()));
    }

    QJsonObject initializeRequest;
    initializeRequest.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    initializeRequest.insert(QStringLiteral("id"), kInitializeRequestId);
    initializeRequest.insert(QStringLiteral("method"), QStringLiteral("initialize"));
    QJsonObject params;
    params.insert(QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05"));
    params.insert(QStringLiteral("capabilities"), QJsonObject {});
    QJsonObject clientInfo;
    clientInfo.insert(QStringLiteral("name"), QStringLiteral("qtcode"));
    clientInfo.insert(QStringLiteral("version"), QStringLiteral("0.1.0"));
    params.insert(QStringLiteral("clientInfo"), clientInfo);
    initializeRequest.insert(QStringLiteral("params"), params);

    process.write(buildMessageLine(initializeRequest));
    if (!process.waitForBytesWritten(timeoutMs)) {
        process.kill();
        process.waitForFinished(timeoutMs);
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("Timed out while sending MCP initialize request."));
    }

    QJsonObject response;
    if (!readJsonLine(process, &response, timeoutMs)) {
        const QString stderrOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
        process.kill();
        process.waitForFinished(timeoutMs);
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            stderrOutput.isEmpty()
                ? QStringLiteral("Timed out waiting for MCP initialize response.")
                : QStringLiteral("MCP initialize failed: %1").arg(stderrOutput));
    }

    if (response.contains(QStringLiteral("error"))) {
        const QJsonObject errorObject = response.value(QStringLiteral("error")).toObject();
        process.kill();
        process.waitForFinished(timeoutMs);
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("MCP server returned error: %1")
                .arg(errorObject.value(QStringLiteral("message")).toString()));
    }

    const QJsonObject resultObject = response.value(QStringLiteral("result")).toObject();
    const QJsonObject serverInfoObject = resultObject.value(QStringLiteral("serverInfo")).toObject();
    const QString serverInfoName = serverInfoObject.value(QStringLiteral("name")).toString();
    const QString serverInfoVersion = serverInfoObject.value(QStringLiteral("version")).toString();
    const QString serverInfo = serverInfoName.isEmpty()
        ? QString {}
        : serverInfoVersion.isEmpty() ? serverInfoName
                                      : QStringLiteral("%1 %2").arg(serverInfoName, serverInfoVersion);

    process.kill();
    process.waitForFinished(timeoutMs);

    return makeResult(
        McpHealthState::Healthy,
        server,
        serverInfo.isEmpty() ? QStringLiteral("MCP server responded to initialize.")
                             : QStringLiteral("MCP server %1 responded to initialize.").arg(serverInfo),
        serverInfo);
}

} // namespace

McpHealthResult McpClient::checkServerHealth(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    int timeoutMs)
{
    if (!server.enabled) {
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("MCP server is disabled."));
    }

    const QString transport = server.transport.trimmed().toLower();
    if (transport == QStringLiteral("stdio")) {
        return checkStdioServerHealth(server, workingDirectory, timeoutMs);
    }

    if (transport == QStringLiteral("sse") || transport == QStringLiteral("http")) {
        return makeResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("Health checks for transport '%1' are not implemented yet.").arg(transport));
    }

    return makeResult(
        McpHealthState::Unhealthy,
        server,
        QStringLiteral("Unsupported MCP transport '%1'.").arg(server.transport));
}

} // namespace qtcode::memory

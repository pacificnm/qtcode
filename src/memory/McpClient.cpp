#include "memory/McpClient.h"

#include "memory/ContextResult.h"
#include "shared/Logging.h"
#include "shared/SecretStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace qtcode::memory {

namespace {

constexpr auto kInitializeRequestId = 1;

McpHealthResult makeHealthResult(
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
            qCInfo(qtcodeMemory) << "Secret env key not loaded for MCP client:" << secretKey << secretError;
        }
    }
    return environment;
}

QString extractToolTextContent(const QJsonObject &response, McpClientError *error)
{
    if (error == nullptr) {
        return {};
    }

    if (!response.contains(QStringLiteral("result"))) {
        *error = McpClientError::failure(
            McpClientErrorCode::InvalidResponse,
            QStringLiteral("MCP tool response is missing a result object."));
        return {};
    }

    const QJsonObject resultObject = response.value(QStringLiteral("result")).toObject();
    if (resultObject.value(QStringLiteral("isError")).toBool()) {
        QStringList errorParts;
        const QJsonArray content = resultObject.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &entry : content) {
            const QJsonObject item = entry.toObject();
            if (item.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                errorParts.append(item.value(QStringLiteral("text")).toString());
            }
        }

        *error = McpClientError::failure(
            McpClientErrorCode::ToolReturnedError,
            QStringLiteral("MCP tool returned an error response."),
            errorParts.join(QStringLiteral("\n")));
        return {};
    }

    QStringList textParts;
    const QJsonArray content = resultObject.value(QStringLiteral("content")).toArray();
    for (const QJsonValue &entry : content) {
        const QJsonObject item = entry.toObject();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
            textParts.append(item.value(QStringLiteral("text")).toString());
        }
    }

    return textParts.join(QStringLiteral("\n\n"));
}

class McpStdioSession
{
public:
    McpStdioSession(
        const settings::McpServerRecord &server,
        const QString &workingDirectory,
        int timeoutMs)
        : m_server(server)
        , m_workingDirectory(workingDirectory)
        , m_timeoutMs(timeoutMs)
    {
    }

    ~McpStdioSession()
    {
        shutdown();
    }

    [[nodiscard]] bool initialize(McpClientError *error)
    {
        if (m_initialized) {
            return true;
        }

        if (m_server.endpoint.trimmed().isEmpty()) {
            if (error != nullptr) {
                *error = McpClientError::failure(
                    McpClientErrorCode::InvalidResponse,
                    QStringLiteral("MCP server endpoint must not be empty."));
            }
            return false;
        }

        m_process.setProgram(m_server.endpoint);
        m_process.setArguments(m_server.args());
        m_process.setProcessEnvironment(buildProcessEnvironment(m_server));
        if (!m_workingDirectory.isEmpty()) {
            m_process.setWorkingDirectory(m_workingDirectory);
        }

        m_process.start(QProcess::ReadWrite);
        if (!m_process.waitForStarted(m_timeoutMs)) {
            if (error != nullptr) {
                *error = McpClientError::failure(
                    McpClientErrorCode::ProcessStartFailed,
                    QStringLiteral("Failed to start MCP server process."),
                    m_process.errorString());
            }
            return false;
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

        QJsonObject response;
        if (!sendRequest(initializeRequest, kInitializeRequestId, &response, error)) {
            shutdown();
            return false;
        }

        if (response.contains(QStringLiteral("error"))) {
            if (error != nullptr) {
                *error = McpClientError::failure(
                    McpClientErrorCode::InvalidResponse,
                    QStringLiteral("MCP initialize failed."),
                    response.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString());
            }
            shutdown();
            return false;
        }

        m_initialized = true;
        return true;
    }

    [[nodiscard]] QString callTool(
        const QString &toolName,
        const QJsonObject &arguments,
        McpClientError *error)
    {
        if (!m_initialized && !initialize(error)) {
            return {};
        }

        const int requestId = m_nextRequestId++;
        QJsonObject request;
        request.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
        request.insert(QStringLiteral("id"), requestId);
        request.insert(QStringLiteral("method"), QStringLiteral("tools/call"));
        QJsonObject params;
        params.insert(QStringLiteral("name"), toolName);
        params.insert(QStringLiteral("arguments"), arguments);
        request.insert(QStringLiteral("params"), params);

        QJsonObject response;
        if (!sendRequest(request, requestId, &response, error)) {
            return {};
        }

        return extractToolTextContent(response, error);
    }

    void shutdown()
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
            m_process.waitForFinished(m_timeoutMs);
        }
        m_initialized = false;
    }

private:
    [[nodiscard]] bool sendRequest(
        const QJsonObject &request,
        int expectedId,
        QJsonObject *response,
        McpClientError *error)
    {
        if (response == nullptr) {
            return false;
        }

        m_process.write(buildMessageLine(request));
        if (!m_process.waitForBytesWritten(m_timeoutMs)) {
            if (error != nullptr) {
                *error = McpClientError::failure(
                    McpClientErrorCode::Timeout,
                    QStringLiteral("Timed out while sending MCP request."),
                    request.value(QStringLiteral("method")).toString());
            }
            return false;
        }

        while (true) {
            QJsonObject payload;
            if (!readJsonLine(m_process, &payload, m_timeoutMs)) {
                const QString stderrOutput = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
                if (error != nullptr) {
                    *error = McpClientError::failure(
                        McpClientErrorCode::Timeout,
                        QStringLiteral("Timed out waiting for MCP response."),
                        stderrOutput);
                }
                return false;
            }

            if (!payload.contains(QStringLiteral("id"))) {
                continue;
            }

            if (payload.value(QStringLiteral("id")).toInt() != expectedId) {
                continue;
            }

            *response = payload;
            return true;
        }
    }

    settings::McpServerRecord m_server;
    QString m_workingDirectory;
    int m_timeoutMs = 8000;
    QProcess m_process;
    int m_nextRequestId = 2;
    bool m_initialized = false;
};

McpHealthResult checkStdioServerHealth(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    int timeoutMs)
{
    McpStdioSession session(server, workingDirectory, timeoutMs);
    McpClientError error;
    if (!session.initialize(&error)) {
        return makeHealthResult(
            McpHealthState::Unhealthy,
            server,
            error.message,
            error.detail);
    }

    return makeHealthResult(
        McpHealthState::Healthy,
        server,
        QStringLiteral("MCP server responded to initialize."));
}

} // namespace

McpHealthResult McpClient::checkServerHealth(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    int timeoutMs)
{
    if (!server.enabled) {
        return makeHealthResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("MCP server is disabled."));
    }

    const QString transport = server.transport.trimmed().toLower();
    if (transport == QStringLiteral("stdio")) {
        return checkStdioServerHealth(server, workingDirectory, timeoutMs);
    }

    if (transport == QStringLiteral("sse") || transport == QStringLiteral("http")) {
        return makeHealthResult(
            McpHealthState::Unhealthy,
            server,
            QStringLiteral("Health checks for transport '%1' are not implemented yet.").arg(transport));
    }

    return makeHealthResult(
        McpHealthState::Unhealthy,
        server,
        QStringLiteral("Unsupported MCP transport '%1'.").arg(server.transport));
}

MemorySearchOutcome McpClient::searchProjectMemory(
    const settings::McpServerRecord &server,
    const QString &workingDirectory,
    const QString &query,
    const MemorySearchOptions &options,
    int timeoutMs)
{
    MemorySearchOutcome outcome;
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        outcome.error = McpClientError::failure(
            McpClientErrorCode::InvalidResponse,
            QStringLiteral("Memory search query must not be empty."));
        return outcome;
    }

    if (!server.enabled) {
        outcome.error = McpClientError::failure(
            McpClientErrorCode::ServerDisabled,
            QStringLiteral("MCP server '%1' is disabled.").arg(server.name));
        return outcome;
    }

    const QString transport = server.transport.trimmed().toLower();
    if (transport != QStringLiteral("stdio")) {
        outcome.error = McpClientError::failure(
            McpClientErrorCode::UnsupportedTransport,
            QStringLiteral("Memory search for transport '%1' is not implemented yet.").arg(transport));
        return outcome;
    }

    McpStdioSession session(server, workingDirectory, timeoutMs);
    QJsonObject arguments;
    arguments.insert(QStringLiteral("query"), trimmedQuery);
    arguments.insert(QStringLiteral("limit"), qMax(1, options.limit));

    const QString toolOutput = session.callTool(QStringLiteral("search_project_memory"), arguments, &outcome.error);
    if (!outcome.error.isSuccess()) {
        return outcome;
    }

    const QString retrievedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    outcome.results = parseProjectMemoryToolOutput(toolOutput, retrievedAt);
    return outcome;
}

} // namespace qtcode::memory

#include "agents/adapters/CursorAgentAdapter.h"

#include "shared/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace qtcode::agents {

namespace {

QString composePromptWithContext(const AgentRequest &request)
{
    if (request.contextExcerpts.isEmpty()) {
        return request.prompt;
    }

    QStringList parts;
    parts.append(QStringLiteral("Retrieved project context:"));
    parts.append(request.contextExcerpts);
    parts.append(QStringLiteral("User request:"));
    parts.append(request.prompt);
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace

CursorAgentAdapter::CursorAgentAdapter(QObject *parent)
    : AgentAdapter(parent)
    , m_process(new QProcess(this))
{
    m_executablePath = QStandardPaths::findExecutable(QStringLiteral("cursor"));
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &CursorAgentAdapter::onProcessReadyRead);
    connect(m_process, &QProcess::finished, this, &CursorAgentAdapter::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CursorAgentAdapter::onProcessErrorOccurred);
}

CursorAgentAdapter::~CursorAgentAdapter()
{
    if (m_requestInFlight && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

QString CursorAgentAdapter::agentKey() const
{
    return QStringLiteral("cursor");
}

QString CursorAgentAdapter::displayName() const
{
    return QStringLiteral("Cursor CLI");
}

AgentCapabilities CursorAgentAdapter::capabilities() const
{
    return AgentCapability::StreamingText | AgentCapability::DiffOutput
        | AgentCapability::SupportsNonInteractiveMode | AgentCapability::SupportsProjectConfig;
}

bool CursorAgentAdapter::isAvailable() const
{
    return !m_executablePath.isEmpty();
}

QString CursorAgentAdapter::executablePath() const
{
    return m_executablePath;
}

bool CursorAgentAdapter::validateConfiguration(QString *errorMessage) const
{
    if (isAvailable()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Cursor CLI was not found on PATH.");
    }
    return false;
}

bool CursorAgentAdapter::startRequest(const AgentRequest &request, QString *errorMessage)
{
    if (m_requestInFlight) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cursor adapter already has a request in flight.");
        }
        return false;
    }

    if (!validateConfiguration(errorMessage)) {
        AgentEvent event;
        event.type = AgentEventType::Error;
        event.error.kind = AgentErrorKind::MissingExecutable;
        event.error.message = QStringLiteral("Cursor CLI was not found on PATH.");
        emit eventEmitted(event);
        emit requestFinished(AgentRequestStatus::Failed, event.error.message);
        return false;
    }

    if (request.prompt.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent prompt must not be empty.");
        }
        return false;
    }

    const QString workingDirectory = request.workingDirectory.trimmed().isEmpty()
        ? QDir::currentPath()
        : request.workingDirectory;
    const QFileInfo workingDirectoryInfo(workingDirectory);
    if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
        const QString message =
            QStringLiteral("Working directory does not exist: %1").arg(workingDirectory);
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    m_pendingOutput.clear();
    m_receivedAssistantOutput = false;
    m_accumulatedAssistantText.clear();
    m_requestInFlight = true;

    QStringList arguments;
    arguments << QStringLiteral("agent")
              << QStringLiteral("-f")
              << QStringLiteral("--print")
              << QStringLiteral("--output-format")
              << QStringLiteral("stream-json")
              << QStringLiteral("--stream-partial-output")
              << composePromptWithContext(request);

    qCInfo(qtcodeAgents) << "Starting Cursor agent in" << workingDirectory;

    AgentEvent statusEvent;
    statusEvent.type = AgentEventType::StatusUpdate;
    statusEvent.text = QStringLiteral("Starting Cursor request…");
    emit eventEmitted(statusEvent);

    m_process->setProgram(m_executablePath);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(QDir::cleanPath(workingDirectoryInfo.canonicalFilePath()));
    m_process->start();

    if (!m_process->waitForStarted(5000)) {
        const QString message = QStringLiteral("Failed to start Cursor CLI: %1")
                                    .arg(m_process->errorString());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }

        AgentEvent event;
        event.type = AgentEventType::Error;
        event.error = errorFromProcess(m_process->error(), message);
        emit eventEmitted(event);
        finishRequest(AgentRequestStatus::Failed, message);
        return false;
    }

    return true;
}

void CursorAgentAdapter::cancelRequest()
{
    if (!m_requestInFlight) {
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }

    finishRequest(AgentRequestStatus::Canceled, QStringLiteral("Cursor request canceled."));
}

void CursorAgentAdapter::setExecutablePath(const QString &executablePath)
{
    m_executablePath = executablePath;
}

void CursorAgentAdapter::onProcessReadyRead()
{
    m_pendingOutput.append(QString::fromUtf8(m_process->readAllStandardOutput()));

    int newlineIndex = m_pendingOutput.indexOf(QLatin1Char('\n'));
    while (newlineIndex >= 0) {
        const QString line = m_pendingOutput.left(newlineIndex).trimmed();
        m_pendingOutput.remove(0, newlineIndex + 1);

        if (!line.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                emitNormalizedEvent(document.object());
            } else {
                qCDebug(qtcodeAgents) << "Skipping non-JSON Cursor output line:" << line;
            }
        }

        newlineIndex = m_pendingOutput.indexOf(QLatin1Char('\n'));
    }
}

void CursorAgentAdapter::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_pendingOutput.trimmed().isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(m_pendingOutput.trimmed().toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            emitNormalizedEvent(document.object());
        }
        m_pendingOutput.clear();
    }

    if (!m_requestInFlight) {
        return;
    }

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        const QString message = QStringLiteral("Cursor CLI exited with code %1.").arg(exitCode);

        qCWarning(qtcodeAgents) << "Cursor agent failed with exit code" << exitCode;

        AgentEvent event;
        event.type = AgentEventType::Error;
        event.error.kind = AgentErrorKind::ProcessFailed;
        event.error.message = message;
        emit eventEmitted(event);
        finishRequest(AgentRequestStatus::Failed, message);
        return;
    }

    qCInfo(qtcodeAgents) << "Cursor agent completed successfully";
    finishRequest(AgentRequestStatus::Succeeded, {});
}

void CursorAgentAdapter::onProcessErrorOccurred(QProcess::ProcessError processError)
{
    if (!m_requestInFlight) {
        return;
    }

    const QString message = QStringLiteral("Cursor CLI process error: %1")
                                .arg(m_process->errorString());

    AgentEvent event;
    event.type = AgentEventType::Error;
    event.error = errorFromProcess(processError, message);
    emit eventEmitted(event);
    finishRequest(AgentRequestStatus::Failed, message);
}

void CursorAgentAdapter::emitOutputText(const QString &text, bool startNewMessage)
{
    if (text.isEmpty()) {
        return;
    }

    m_receivedAssistantOutput = true;

    AgentEvent event;
    event.type = AgentEventType::OutputText;
    event.text = text;
    event.startNewMessage = startNewMessage;
    emit eventEmitted(event);
}

void CursorAgentAdapter::emitAssistantStreamText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (text == m_accumulatedAssistantText) {
        return;
    }

    QString delta;
    if (text.startsWith(m_accumulatedAssistantText)) {
        delta = text.mid(m_accumulatedAssistantText.size());
        m_accumulatedAssistantText = text;
    } else {
        delta = text;
        m_accumulatedAssistantText += text;
    }

    emitOutputText(delta);
}

void CursorAgentAdapter::emitActivityText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    AgentEvent event;
    event.type = AgentEventType::OutputText;
    event.messageRole = QStringLiteral("activity");
    event.startNewMessage = true;
    event.text = text;
    emit eventEmitted(event);
}

void CursorAgentAdapter::emitStatusText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    AgentEvent event;
    event.type = AgentEventType::StatusUpdate;
    event.text = text;
    emit eventEmitted(event);
}

void CursorAgentAdapter::emitAssistantMessageObject(const QJsonObject &messageObject)
{
    const QJsonValue contentValue = messageObject.value(QStringLiteral("content"));
    if (contentValue.isArray()) {
        QStringList parts;
        for (const QJsonValue &value : contentValue.toArray()) {
            if (!value.isObject()) {
                continue;
            }

            const QString part = value.toObject().value(QStringLiteral("text")).toString();
            if (!part.isEmpty()) {
                parts.append(part);
            }
        }

        emitAssistantStreamText(parts.join(QString()));
        return;
    }

    if (contentValue.isString()) {
        emitAssistantStreamText(contentValue.toString());
        return;
    }

    emitAssistantStreamText(messageObject.value(QStringLiteral("text")).toString());
}

QString CursorAgentAdapter::extractShellCommand(const QJsonObject &eventObject)
{
    const QJsonObject toolCall = eventObject.value(QStringLiteral("tool_call")).toObject();
    const QJsonObject shellToolCall = toolCall.value(QStringLiteral("shellToolCall")).toObject();
    const QJsonObject args = shellToolCall.value(QStringLiteral("args")).toObject();
    QString command = args.value(QStringLiteral("command")).toString().trimmed();
    if (command.isEmpty()) {
        command = shellToolCall.value(QStringLiteral("description")).toString().trimmed();
    }
    if (command.length() > 120) {
        command = command.left(117) + QStringLiteral("…");
    }
    return command;
}

void CursorAgentAdapter::emitToolCallEvent(const QJsonObject &eventObject)
{
    const QString subtype = eventObject.value(QStringLiteral("subtype")).toString();
    const QString command = extractShellCommand(eventObject);
    if (command.isEmpty()) {
        return;
    }

    if (subtype == QStringLiteral("started")) {
        const QString activityText = QStringLiteral("Running: %1").arg(command);
        emitActivityText(activityText);
        emitStatusText(activityText);
        return;
    }

    if (subtype == QStringLiteral("completed")) {
        emitStatusText(QStringLiteral("Working…"));
    }
}

void CursorAgentAdapter::emitNormalizedEvent(const QJsonObject &eventObject)
{
    const QString type = eventObject.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("assistant")) {
        emitAssistantMessageObject(eventObject.value(QStringLiteral("message")).toObject());
        return;
    }

    if (type == QStringLiteral("tool_call")) {
        emitToolCallEvent(eventObject);
        return;
    }

    if (type == QStringLiteral("result") && !m_receivedAssistantOutput) {
        emitOutputText(eventObject.value(QStringLiteral("result")).toString());
        return;
    }

    if (type == QStringLiteral("system")) {
        emitStatusText(QStringLiteral("Cursor agent connected"));
    }
}

void CursorAgentAdapter::finishRequest(AgentRequestStatus status, const QString &errorMessage)
{
    m_requestInFlight = false;
    emit requestFinished(status, errorMessage);
}

AgentError CursorAgentAdapter::errorFromProcess(
    QProcess::ProcessError processError,
    const QString &details)
{
    AgentError error;
    error.message = details;

    switch (processError) {
    case QProcess::FailedToStart:
        error.kind = AgentErrorKind::MissingExecutable;
        break;
    case QProcess::Crashed:
        error.kind = AgentErrorKind::ProcessFailed;
        break;
    default:
        error.kind = AgentErrorKind::ProcessFailed;
        break;
    }

    return error;
}

} // namespace qtcode::agents

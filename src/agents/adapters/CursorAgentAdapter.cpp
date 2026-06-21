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

QList<AgentOption> CursorAgentAdapter::supportedModels() const
{
    if (!m_supportedModels.isEmpty()) {
        return m_supportedModels;
    }

    return {
        {QStringLiteral("auto"), QStringLiteral("Auto")},
    };
}

bool CursorAgentAdapter::refreshSupportedModels(QString *errorMessage)
{
    if (!validateConfiguration(errorMessage)) {
        return false;
    }

    QProcess process;
    process.setProgram(m_executablePath);
    process.setArguments({QStringLiteral("agent"), QStringLiteral("models")});
    process.start();
    if (!process.waitForFinished(15000)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Timed out while listing Cursor models.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list Cursor models: %1")
                                .arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
        }
        return false;
    }

    const QList<AgentOption> parsedModels =
        parseModelsOutput(QString::fromUtf8(process.readAllStandardOutput()));
    if (parsedModels.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cursor models output was empty.");
        }
        return false;
    }

    m_supportedModels = parsedModels;
    return true;
}

QList<AgentOption> CursorAgentAdapter::parseModelsOutput(const QString &output)
{
    QList<AgentOption> models;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(QStringLiteral("Available models"))) {
            continue;
        }

        const int separatorIndex = trimmedLine.indexOf(QStringLiteral(" - "));
        if (separatorIndex <= 0) {
            continue;
        }

        AgentOption option;
        option.key = trimmedLine.left(separatorIndex).trimmed();
        option.label = trimmedLine.mid(separatorIndex + 3).trimmed();
        if (!option.key.isEmpty()) {
            models.append(option);
        }
    }

    return models;
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
              << QStringLiteral("--stream-partial-output");

    if (!request.modelKey.isEmpty()) {
        arguments << QStringLiteral("--model") << request.modelKey;
    }

    const QString executionMode = request.executionModeKey.trimmed();
    if (executionMode == QStringLiteral("plan")) {
        arguments << QStringLiteral("--mode") << QStringLiteral("plan");
    } else if (executionMode == QStringLiteral("ask")) {
        arguments << QStringLiteral("--mode") << QStringLiteral("ask");
    }

    arguments << composePromptWithContext(request);

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

    if (!startNewMessage) {
        if (text.startsWith(m_accumulatedAssistantText)) {
            m_accumulatedAssistantText = text;
        } else {
            m_accumulatedAssistantText.append(text);
        }
    }

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

QString CursorAgentAdapter::truncateForActivity(const QString &text, int maxLength)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() <= maxLength) {
        return trimmed;
    }
    return trimmed.left(maxLength - 1) + QStringLiteral("…");
}

QString CursorAgentAdapter::extractToolCallActivityText(const QJsonObject &eventObject)
{
    const QJsonObject toolCall = eventObject.value(QStringLiteral("tool_call")).toObject();
    for (auto iterator = toolCall.begin(); iterator != toolCall.end(); ++iterator) {
        const QString toolKey = iterator.key();
        const QJsonObject payload = iterator.value().toObject();
        const QJsonObject args = payload.value(QStringLiteral("args")).toObject();

        if (toolKey == QStringLiteral("shellToolCall")) {
            QString command = args.value(QStringLiteral("command")).toString().trimmed();
            if (command.isEmpty()) {
                command = payload.value(QStringLiteral("description")).toString().trimmed();
            }
            if (command.isEmpty()) {
                continue;
            }
            return QStringLiteral("Running: %1").arg(truncateForActivity(command));
        }

        if (toolKey == QStringLiteral("readToolCall")) {
            const QString path = args.value(QStringLiteral("path")).toString().trimmed();
            if (!path.isEmpty()) {
                return QStringLiteral("Read: %1").arg(path);
            }
        }

        if (toolKey == QStringLiteral("writeToolCall") || toolKey == QStringLiteral("editToolCall")) {
            const QString path = args.value(QStringLiteral("path")).toString().trimmed();
            if (!path.isEmpty()) {
                const QString verb = toolKey == QStringLiteral("writeToolCall")
                    ? QStringLiteral("Write")
                    : QStringLiteral("Edit");
                return QStringLiteral("%1: %2").arg(verb, path);
            }
        }

        if (toolKey == QStringLiteral("deleteToolCall")) {
            const QString path = args.value(QStringLiteral("path")).toString().trimmed();
            if (!path.isEmpty()) {
                return QStringLiteral("Delete: %1").arg(path);
            }
        }

        if (toolKey == QStringLiteral("grepToolCall")) {
            const QString pattern = args.value(QStringLiteral("pattern")).toString().trimmed();
            if (!pattern.isEmpty()) {
                return QStringLiteral("Grep: %1").arg(truncateForActivity(pattern, 80));
            }
        }

        if (toolKey == QStringLiteral("globToolCall")) {
            QString pattern = args.value(QStringLiteral("globPattern")).toString().trimmed();
            if (pattern.isEmpty()) {
                pattern = args.value(QStringLiteral("pattern")).toString().trimmed();
            }
            if (!pattern.isEmpty()) {
                return QStringLiteral("Glob: %1").arg(truncateForActivity(pattern, 80));
            }
        }

        if (toolKey == QStringLiteral("lsToolCall")) {
            const QString path = args.value(QStringLiteral("path")).toString().trimmed();
            if (!path.isEmpty()) {
                return QStringLiteral("List: %1").arg(path);
            }
        }

        if (toolKey == QStringLiteral("todoToolCall")) {
            const QString content = args.value(QStringLiteral("content")).toString().trimmed();
            if (!content.isEmpty()) {
                return QStringLiteral("Todo: %1").arg(truncateForActivity(content, 80));
            }
        }

        if (toolKey == QStringLiteral("function")) {
            const QString name = payload.value(QStringLiteral("name")).toString().trimmed();
            if (!name.isEmpty()) {
                return QStringLiteral("Tool: %1").arg(name);
            }
        }
    }

    return {};
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
    const QString activityText = extractToolCallActivityText(eventObject);
    if (activityText.isEmpty()) {
        return;
    }

    if (subtype == QStringLiteral("started")) {
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
        emitAssistantStreamText(eventObject.value(QStringLiteral("result")).toString());
        return;
    }

    if (type == QStringLiteral("system")) {
        const QString subtype = eventObject.value(QStringLiteral("subtype")).toString();
        if (subtype == QStringLiteral("init")) {
            const QString model = eventObject.value(QStringLiteral("model")).toString().trimmed();
            if (!model.isEmpty()) {
                emitStatusText(QStringLiteral("Connected · %1").arg(model));
                return;
            }
        }

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

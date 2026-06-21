#include "agents/adapters/CodexAgentAdapter.h"

#include "shared/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace qtcode::agents {

CodexAgentAdapter::CodexAgentAdapter(QObject *parent)
    : AgentAdapter(parent)
    , m_process(new QProcess(this))
{
    m_executablePath = QStandardPaths::findExecutable(QStringLiteral("codex"));
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &CodexAgentAdapter::onProcessReadyRead);
    connect(m_process, &QProcess::finished, this, &CodexAgentAdapter::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CodexAgentAdapter::onProcessErrorOccurred);
}

CodexAgentAdapter::~CodexAgentAdapter()
{
    if (m_requestInFlight && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

QString CodexAgentAdapter::agentKey() const
{
    return QStringLiteral("codex");
}

QString CodexAgentAdapter::displayName() const
{
    return QStringLiteral("Codex CLI");
}

AgentCapabilities CodexAgentAdapter::capabilities() const
{
    return AgentCapability::StreamingText | AgentCapability::DiffOutput
        | AgentCapability::SupportsNonInteractiveMode | AgentCapability::SupportsProjectConfig;
}

bool CodexAgentAdapter::isAvailable() const
{
    return !m_executablePath.isEmpty();
}

QString CodexAgentAdapter::executablePath() const
{
    return m_executablePath;
}

bool CodexAgentAdapter::validateConfiguration(QString *errorMessage) const
{
    if (isAvailable()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Codex CLI was not found on PATH.");
    }
    return false;
}

#include "agents/AgentModels.h"

namespace {

QString composePromptWithContext(const qtcode::agents::AgentRequest &request)
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

bool CodexAgentAdapter::startRequest(const AgentRequest &request, QString *errorMessage)
{
    if (m_requestInFlight) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Codex adapter already has a request in flight.");
        }
        return false;
    }

    if (!validateConfiguration(errorMessage)) {
        AgentEvent event;
        event.type = AgentEventType::Error;
        event.error.kind = AgentErrorKind::MissingExecutable;
        event.error.message = QStringLiteral("Codex CLI was not found on PATH.");
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
    m_itemEmittedLengths.clear();
    m_requestInFlight = true;

    QStringList arguments;
    arguments << QStringLiteral("exec")
              << QStringLiteral("--skip-git-repo-check")
              << QStringLiteral("--cd")
              << QDir::cleanPath(workingDirectoryInfo.canonicalFilePath())
              << QStringLiteral("--json")
              << composePromptWithContext(request);

    qCInfo(qtcodeAgents) << "Starting Codex exec in" << workingDirectory;

    AgentEvent statusEvent;
    statusEvent.type = AgentEventType::StatusUpdate;
    statusEvent.text = QStringLiteral("Starting Codex request…");
    emit eventEmitted(statusEvent);

    m_process->setProgram(m_executablePath);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(QDir::cleanPath(workingDirectoryInfo.canonicalFilePath()));
    m_process->start();

    if (!m_process->waitForStarted(5000)) {
        const QString message = QStringLiteral("Failed to start Codex CLI: %1")
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

void CodexAgentAdapter::cancelRequest()
{
    if (!m_requestInFlight) {
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }

    finishRequest(AgentRequestStatus::Canceled, QStringLiteral("Codex request canceled."));
}

void CodexAgentAdapter::setExecutablePath(const QString &executablePath)
{
    m_executablePath = executablePath;
}

void CodexAgentAdapter::onProcessReadyRead()
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
                qCDebug(qtcodeAgents) << "Skipping non-JSON Codex output line:" << line;
            }
        }

        newlineIndex = m_pendingOutput.indexOf(QLatin1Char('\n'));
    }
}

void CodexAgentAdapter::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
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
        const QString stderrOutput = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        const QString message = stderrOutput.isEmpty()
            ? QStringLiteral("Codex CLI exited with code %1.").arg(exitCode)
            : stderrOutput;

        qCWarning(qtcodeAgents) << "Codex exec failed with exit code" << exitCode << message;

        AgentEvent event;
        event.type = AgentEventType::Error;
        event.error.kind = AgentErrorKind::ProcessFailed;
        event.error.message = message;
        emit eventEmitted(event);
        finishRequest(AgentRequestStatus::Failed, message);
        return;
    }

    qCInfo(qtcodeAgents) << "Codex exec completed successfully";
    finishRequest(AgentRequestStatus::Succeeded, {});
}

void CodexAgentAdapter::onProcessErrorOccurred(QProcess::ProcessError processError)
{
    if (!m_requestInFlight) {
        return;
    }

    const QString message = QStringLiteral("Codex CLI process error: %1")
                                .arg(m_process->errorString());

    AgentEvent event;
    event.type = AgentEventType::Error;
    event.error = errorFromProcess(processError, message);
    emit eventEmitted(event);
    finishRequest(AgentRequestStatus::Failed, message);
}

void CodexAgentAdapter::emitNormalizedEvent(const QJsonObject &eventObject)
{
    const QString type = eventObject.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("item.completed") || type == QStringLiteral("item.updated")) {
        const QJsonObject item = eventObject.value(QStringLiteral("item")).toObject();
        const QString itemType = item.value(QStringLiteral("type")).toString();
        if (itemType == QStringLiteral("agent_message")) {
            emitAgentMessageText(
                item.value(QStringLiteral("id")).toString(),
                item.value(QStringLiteral("text")).toString());
            return;
        }

        if (itemType == QStringLiteral("command_execution") && type == QStringLiteral("item.completed")) {
            emitCommandExecutionItem(item);
            return;
        }
    }

    if (type == QStringLiteral("thread.started")) {
        emitStatusText(QStringLiteral("Agent session started…"));
        return;
    }

    if (type == QStringLiteral("turn.started")) {
        emitStatusText(QStringLiteral("Thinking…"));
        return;
    }

    if (type == QStringLiteral("turn.completed")) {
        emitStatusText(QStringLiteral("Finishing response…"));
    }
}

void CodexAgentAdapter::emitAgentMessageText(const QString &itemId, const QString &fullText)
{
    if (fullText.isEmpty()) {
        return;
    }

    const int alreadyEmitted = m_itemEmittedLengths.value(itemId);
    if (fullText.size() <= alreadyEmitted) {
        return;
    }

    const QString delta = fullText.mid(alreadyEmitted);
    m_itemEmittedLengths.insert(itemId, fullText.size());

    AgentEvent event;
    event.type = AgentEventType::OutputText;
    event.text = delta;
    event.startNewMessage = alreadyEmitted == 0;
    emit eventEmitted(event);
}

void CodexAgentAdapter::emitCommandExecutionItem(const QJsonObject &item)
{
    QString command = item.value(QStringLiteral("command")).toString().trimmed();
    if (command.isEmpty()) {
        return;
    }

    if (command.length() > 120) {
        command = command.left(117) + QStringLiteral("…");
    }

    const QString activityText = QStringLiteral("Ran: %1").arg(command);

    AgentEvent activityEvent;
    activityEvent.type = AgentEventType::OutputText;
    activityEvent.messageRole = QStringLiteral("activity");
    activityEvent.startNewMessage = true;
    activityEvent.text = activityText;
    emit eventEmitted(activityEvent);

    emitStatusText(QStringLiteral("Running: %1").arg(command));
}

void CodexAgentAdapter::emitStatusText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    AgentEvent event;
    event.type = AgentEventType::StatusUpdate;
    event.text = text;
    emit eventEmitted(event);
}

void CodexAgentAdapter::finishRequest(AgentRequestStatus status, const QString &errorMessage)
{
    m_requestInFlight = false;
    emit requestFinished(status, errorMessage);
}

AgentError CodexAgentAdapter::errorFromProcess(
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

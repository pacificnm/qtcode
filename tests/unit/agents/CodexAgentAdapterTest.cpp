#include "agents/adapters/CodexAgentAdapter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

namespace {

[[nodiscard]] bool hasCapability(
    qtcode::agents::AgentCapabilities capabilities,
    qtcode::agents::AgentCapability capability)
{
    return capabilities.testFlag(capability);
}

[[nodiscard]] QString createFakeCodexScript(const QString &directoryPath, int exitCode)
{
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake_codex"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&script);
    stream << "#!/bin/sh\n";
    stream << "exit " << exitCode << "\n";
    script.close();

    if (!QFile::setPermissions(
            scriptPath,
            QFile::ExeUser | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther)) {
        return {};
    }

    return scriptPath;
}

[[nodiscard]] QString createFakeCodexJsonScript(const QString &directoryPath)
{
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake_codex_json"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&script);
    stream << "#!/bin/sh\n";
    stream << "printf '%s\\n' "
              << "'{\"type\":\"thread.started\",\"thread_id\":\"test-thread\"}' "
              << "'{\"type\":\"turn.started\"}' "
              << "'{\"type\":\"item.completed\",\"item\":{\"id\":\"item_0\",\"type\":\"agent_message\",\"text\":\"Codex stub reply.\"}}' "
              << "'{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}'\n";
    stream << "exit 0\n";
    script.close();

    if (!QFile::setPermissions(
            scriptPath,
            QFile::ExeUser | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther)) {
        return {};
    }

    return scriptPath;
}

[[nodiscard]] QString createFakeCodexMultiMessageScript(const QString &directoryPath)
{
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake_codex_multi"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&script);
    stream << "#!/bin/sh\n";
    stream << "printf '%s\\n' "
              << "'{\"type\":\"turn.started\"}' "
              << "'{\"type\":\"item.completed\",\"item\":{\"id\":\"item_0\",\"type\":\"agent_message\",\"text\":\"First reply.\"}}' "
              << "'{\"type\":\"item.completed\",\"item\":{\"id\":\"item_1\",\"type\":\"command_execution\",\"command\":\"git status\",\"status\":\"completed\"}}' "
              << "'{\"type\":\"item.completed\",\"item\":{\"id\":\"item_2\",\"type\":\"agent_message\",\"text\":\"Second reply.\"}}' "
              << "'{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}'\n";
    stream << "exit 0\n";
    script.close();

    if (!QFile::setPermissions(
            scriptPath,
            QFile::ExeUser | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther)) {
        return {};
    }

    return scriptPath;
}

[[nodiscard]] QString createFakeCodexUpdatedScript(const QString &directoryPath)
{
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake_codex_updated"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&script);
    stream << "#!/bin/sh\n";
    stream << "printf '%s\\n' "
              << "'{\"type\":\"item.updated\",\"item\":{\"id\":\"item_0\",\"type\":\"agent_message\",\"text\":\"Hel\"}}' "
              << "'{\"type\":\"item.completed\",\"item\":{\"id\":\"item_0\",\"type\":\"agent_message\",\"text\":\"Hello\"}}'\n";
    stream << "exit 0\n";
    script.close();

    if (!QFile::setPermissions(
            scriptPath,
            QFile::ExeUser | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther)) {
        return {};
    }

    return scriptPath;
}

[[nodiscard]] bool findErrorEvent(
    const QSignalSpy &eventSpy,
    qtcode::agents::AgentErrorKind expectedKind,
    qtcode::agents::AgentEvent *matchedEvent = nullptr)
{
    for (int index = 0; index < eventSpy.count(); ++index) {
        const auto event = eventSpy.at(index).at(0).value<qtcode::agents::AgentEvent>();
        if (event.type != qtcode::agents::AgentEventType::Error) {
            continue;
        }

        if (event.error.kind != expectedKind) {
            continue;
        }

        if (matchedEvent != nullptr) {
            *matchedEvent = event;
        }

        return true;
    }

    return false;
}

} // namespace

class CodexAgentAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reportsExpectedCapabilities();
    void missingExecutableFailsValidation();
    void missingExecutableStartRequestMapsError();
    void failedToStartExecutableMapsMissingExecutableError();
    void processFailureMapsToProcessFailedError();
    void jsonOutputEmitsAssistantReply();
    void multipleAgentMessagesStartNewMessages();
    void updatedAndCompletedEventsDoNotDuplicateText();
};

void CodexAgentAdapterTest::initTestCase()
{
    qRegisterMetaType<qtcode::agents::AgentEvent>("qtcode::agents::AgentEvent");
    qRegisterMetaType<qtcode::agents::AgentRequestStatus>("qtcode::agents::AgentRequestStatus");
}

void CodexAgentAdapterTest::reportsExpectedCapabilities()
{
    qtcode::agents::CodexAgentAdapter adapter;

    QCOMPARE(adapter.agentKey(), QStringLiteral("codex"));
    QCOMPARE(adapter.displayName(), QStringLiteral("Codex CLI"));

    const qtcode::agents::AgentCapabilities capabilities = adapter.capabilities();
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::StreamingText));
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::DiffOutput));
    QVERIFY(
        hasCapability(capabilities, qtcode::agents::AgentCapability::SupportsNonInteractiveMode));
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::SupportsProjectConfig));
    QVERIFY(!hasCapability(capabilities, qtcode::agents::AgentCapability::RequiresTerminal));
    QVERIFY(!hasCapability(capabilities, qtcode::agents::AgentCapability::McpAware));
}

void CodexAgentAdapterTest::missingExecutableFailsValidation()
{
    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath({});

    QString errorMessage;
    QVERIFY(!adapter.validateConfiguration(&errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("Codex CLI was not found on PATH."));
    QVERIFY(!adapter.isAvailable());
}

void CodexAgentAdapterTest::missingExecutableStartRequestMapsError()
{
    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath({});

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = QDir::tempPath();

    QString errorMessage;
    QVERIFY(!adapter.startRequest(request, &errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("Codex CLI was not found on PATH."));
    QCOMPARE(eventSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const auto event = eventSpy.at(0).at(0).value<qtcode::agents::AgentEvent>();
    QCOMPARE(event.type, qtcode::agents::AgentEventType::Error);
    QCOMPARE(event.error.kind, qtcode::agents::AgentErrorKind::MissingExecutable);
    QCOMPARE(
        finishedSpy.at(0).at(0).value<qtcode::agents::AgentRequestStatus>(),
        qtcode::agents::AgentRequestStatus::Failed);
}

void CodexAgentAdapterTest::failedToStartExecutableMapsMissingExecutableError()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString nonExecutablePath = tempDir.filePath(QStringLiteral("codex-not-executable"));
    {
        QFile nonExecutable(nonExecutablePath);
        QVERIFY(nonExecutable.open(QIODevice::WriteOnly));
        nonExecutable.write("not an executable\n");
    }

    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath(nonExecutablePath);

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = tempDir.path();

    QString errorMessage;
    QVERIFY(!adapter.startRequest(request, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(findErrorEvent(eventSpy, qtcode::agents::AgentErrorKind::MissingExecutable));
    QVERIFY(finishedSpy.count() >= 1);
    QCOMPARE(
        finishedSpy.at(finishedSpy.count() - 1)
            .at(0)
            .value<qtcode::agents::AgentRequestStatus>(),
        qtcode::agents::AgentRequestStatus::Failed);
}

void CodexAgentAdapterTest::processFailureMapsToProcessFailedError()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createFakeCodexScript(tempDir.path(), 42);
    QVERIFY(!scriptPath.isEmpty());

    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath(scriptPath);

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = tempDir.path();

    QString errorMessage;
    QVERIFY(adapter.startRequest(request, &errorMessage));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(
        finishedSpy.at(0).at(0).value<qtcode::agents::AgentRequestStatus>(),
        qtcode::agents::AgentRequestStatus::Failed);

    bool sawProcessFailed = findErrorEvent(eventSpy, qtcode::agents::AgentErrorKind::ProcessFailed);
    QVERIFY(sawProcessFailed);
}

void CodexAgentAdapterTest::jsonOutputEmitsAssistantReply()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createFakeCodexJsonScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath(scriptPath);

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = tempDir.path();

    QString errorMessage;
    QVERIFY(adapter.startRequest(request, &errorMessage));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(
        finishedSpy.at(0).at(0).value<qtcode::agents::AgentRequestStatus>(),
        qtcode::agents::AgentRequestStatus::Succeeded);

    bool sawOutputText = false;
    for (int index = 0; index < eventSpy.count(); ++index) {
        const auto event = eventSpy.at(index).at(0).value<qtcode::agents::AgentEvent>();
        if (event.type == qtcode::agents::AgentEventType::OutputText
            && event.text == QStringLiteral("Codex stub reply.")) {
            sawOutputText = true;
            break;
        }
    }

    QVERIFY(sawOutputText);
}

void CodexAgentAdapterTest::multipleAgentMessagesStartNewMessages()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createFakeCodexMultiMessageScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath(scriptPath);

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = tempDir.path();

    QString errorMessage;
    QVERIFY(adapter.startRequest(request, &errorMessage));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

    QStringList outputTexts;
    bool sawActivity = false;
    for (int index = 0; index < eventSpy.count(); ++index) {
        const auto event = eventSpy.at(index).at(0).value<qtcode::agents::AgentEvent>();
        if (event.type == qtcode::agents::AgentEventType::OutputText
            && event.messageRole != QStringLiteral("activity")) {
            outputTexts.append(event.text);
        }
        if (event.messageRole == QStringLiteral("activity")) {
            sawActivity = true;
        }
    }

    QCOMPARE(outputTexts.size(), 2);
    QCOMPARE(outputTexts.at(0), QStringLiteral("First reply."));
    QCOMPARE(outputTexts.at(1), QStringLiteral("Second reply."));
    QVERIFY(sawActivity);
}

void CodexAgentAdapterTest::updatedAndCompletedEventsDoNotDuplicateText()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createFakeCodexUpdatedScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    qtcode::agents::CodexAgentAdapter adapter;
    adapter.setExecutablePath(scriptPath);

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Say hello.");
    request.workingDirectory = tempDir.path();

    QString errorMessage;
    QVERIFY(adapter.startRequest(request, &errorMessage));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

    QString combinedOutput;
    for (int index = 0; index < eventSpy.count(); ++index) {
        const auto event = eventSpy.at(index).at(0).value<qtcode::agents::AgentEvent>();
        if (event.type == qtcode::agents::AgentEventType::OutputText
            && event.messageRole != QStringLiteral("activity")) {
            combinedOutput.append(event.text);
        }
    }

    QCOMPARE(combinedOutput, QStringLiteral("Hello"));
}

QTEST_MAIN(CodexAgentAdapterTest)

#include "CodexAgentAdapterTest.moc"

#include "agents/adapters/CursorAgentAdapter.h"

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

[[nodiscard]] bool findErrorEvent(
    const QSignalSpy &eventSpy,
    qtcode::agents::AgentErrorKind expectedKind)
{
    for (int index = 0; index < eventSpy.count(); ++index) {
        const auto event = eventSpy.at(index).at(0).value<qtcode::agents::AgentEvent>();
        if (event.type != qtcode::agents::AgentEventType::Error) {
            continue;
        }

        if (event.error.kind == expectedKind) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] QString createFakeCursorStreamScript(const QString &directoryPath)
{
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake_cursor_stream"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&script);
    stream << "#!/bin/sh\n";
    stream << "printf '%s\\n' "
              << "'{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}}' "
              << "'{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}}' "
              << "'{\"type\":\"result\",\"subtype\":\"success\",\"result\":\"hello\"}'\n";
    stream << "exit 0\n";
    script.close();

    if (!QFile::setPermissions(
            scriptPath,
            QFile::ExeUser | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther)) {
        return {};
    }

    return scriptPath;
}

} // namespace

class CursorAgentAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reportsExpectedCapabilities();
    void missingExecutableFailsValidation();
    void missingExecutableStartRequestMapsError();
    void duplicateAssistantEventsDoNotDuplicateText();
};

void CursorAgentAdapterTest::initTestCase()
{
    qRegisterMetaType<qtcode::agents::AgentEvent>("qtcode::agents::AgentEvent");
    qRegisterMetaType<qtcode::agents::AgentRequestStatus>("qtcode::agents::AgentRequestStatus");
}

void CursorAgentAdapterTest::reportsExpectedCapabilities()
{
    qtcode::agents::CursorAgentAdapter adapter;

    QCOMPARE(adapter.agentKey(), QStringLiteral("cursor"));
    QCOMPARE(adapter.displayName(), QStringLiteral("Cursor CLI"));

    const qtcode::agents::AgentCapabilities capabilities = adapter.capabilities();
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::StreamingText));
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::DiffOutput));
    QVERIFY(
        hasCapability(capabilities, qtcode::agents::AgentCapability::SupportsNonInteractiveMode));
    QVERIFY(hasCapability(capabilities, qtcode::agents::AgentCapability::SupportsProjectConfig));
}

void CursorAgentAdapterTest::missingExecutableFailsValidation()
{
    qtcode::agents::CursorAgentAdapter adapter;
    adapter.setExecutablePath({});

    QString errorMessage;
    QVERIFY(!adapter.validateConfiguration(&errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("Cursor CLI was not found on PATH."));
    QVERIFY(!adapter.isAvailable());
}

void CursorAgentAdapterTest::missingExecutableStartRequestMapsError()
{
    qtcode::agents::CursorAgentAdapter adapter;
    adapter.setExecutablePath({});

    QSignalSpy eventSpy(&adapter, &qtcode::agents::AgentAdapter::eventEmitted);
    QSignalSpy finishedSpy(&adapter, &qtcode::agents::AgentAdapter::requestFinished);

    qtcode::agents::AgentRequest request;
    request.prompt = QStringLiteral("Summarize repository status.");
    request.workingDirectory = QDir::tempPath();

    QString errorMessage;
    QVERIFY(!adapter.startRequest(request, &errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("Cursor CLI was not found on PATH."));
    QCOMPARE(eventSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(findErrorEvent(eventSpy, qtcode::agents::AgentErrorKind::MissingExecutable));
    QCOMPARE(
        finishedSpy.at(0).at(0).value<qtcode::agents::AgentRequestStatus>(),
        qtcode::agents::AgentRequestStatus::Failed);
}

void CursorAgentAdapterTest::duplicateAssistantEventsDoNotDuplicateText()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createFakeCursorStreamScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    qtcode::agents::CursorAgentAdapter adapter;
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
        if (event.type == qtcode::agents::AgentEventType::OutputText) {
            combinedOutput.append(event.text);
        }
    }

    QCOMPARE(combinedOutput, QStringLiteral("hello"));
}

QTEST_MAIN(CursorAgentAdapterTest)

#include "CursorAgentAdapterTest.moc"

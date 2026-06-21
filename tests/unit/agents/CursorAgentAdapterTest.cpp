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

} // namespace

class CursorAgentAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsExpectedCapabilities();
    void missingExecutableFailsValidation();
    void missingExecutableStartRequestMapsError();
};

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

QTEST_MAIN(CursorAgentAdapterTest)

#include "CursorAgentAdapterTest.moc"

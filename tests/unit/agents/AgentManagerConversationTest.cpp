#include "agents/AgentAdapter.h"
#include "agents/AgentManager.h"
#include "agents/AgentModels.h"
#include "agents/AgentSession.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "storage/repositories/ProjectRepository.h"

#include <QDateTime>
#include <QTimer>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace {

class StubAgentAdapter final : public qtcode::agents::AgentAdapter
{
    Q_OBJECT

public:
    explicit StubAgentAdapter(QObject *parent = nullptr)
        : AgentAdapter(parent)
    {
    }

    [[nodiscard]] QString agentKey() const override
    {
        return QStringLiteral("stub");
    }

    [[nodiscard]] QString displayName() const override
    {
        return QStringLiteral("Stub Agent");
    }

    [[nodiscard]] qtcode::agents::AgentCapabilities capabilities() const override
    {
        return qtcode::agents::AgentCapability::StreamingText;
    }

    [[nodiscard]] bool isAvailable() const override
    {
        return true;
    }

    [[nodiscard]] bool validateConfiguration(QString *errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        return true;
    }

    [[nodiscard]] bool startRequest(
        const qtcode::agents::AgentRequest &request,
        QString *errorMessage = nullptr) override
    {
        Q_UNUSED(errorMessage);

        if (request.prompt.trimmed().isEmpty()) {
            return false;
        }

        m_pendingPrompt = request.prompt;
        QTimer::singleShot(0, this, &StubAgentAdapter::emitDeferredReply);
        return true;
    }

    void cancelRequest() override
    {
        emit requestFinished(
            qtcode::agents::AgentRequestStatus::Canceled,
            QStringLiteral("Canceled"));
    }

private slots:
    void emitDeferredReply()
    {
        qtcode::agents::AgentEvent outputEvent;
        outputEvent.type = qtcode::agents::AgentEventType::OutputText;
        outputEvent.text = QStringLiteral("Stub agent reply for %1").arg(m_pendingPrompt);
        emit eventEmitted(outputEvent);

        emit requestFinished(qtcode::agents::AgentRequestStatus::Succeeded, {});
    }

private:
    QString m_pendingPrompt;
};

[[nodiscard]] bool openMigratedDatabase(
    qtcode::storage::StorageService *storageService,
    QString *errorMessage = nullptr)
{
    if (!storageService->open(errorMessage)) {
        return false;
    }

    qtcode::storage::MigrationRunner migrationRunner(*storageService);
    return migrationRunner.runPendingMigrations(errorMessage);
}

[[nodiscard]] QString createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

class AgentManagerConversationTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void deferredDispatchPersistsAssistantReply();
};

void AgentManagerConversationTest::initTestCase()
{
    qRegisterMetaType<qtcode::agents::AgentEvent>("qtcode::agents::AgentEvent");
    qRegisterMetaType<qtcode::agents::AgentRequestStatus>("qtcode::agents::AgentRequestStatus");
}

void AgentManagerConversationTest::deferredDispatchPersistsAssistantReply()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("qtcode.db")));
    QString errorMessage;
    QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

    qtcode::settings::ProjectRecord project;
    project.id = createId();
    project.name = QStringLiteral("Test Project");
    project.rootPath = tempDir.path();
    project.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    project.updatedAt = project.createdAt;
    project.lastOpenedAt = project.createdAt;

    qtcode::storage::ProjectRepository projectRepository(storageService);
    QVERIFY(projectRepository.insertProject(project, &errorMessage));

    qtcode::agents::AgentManager manager(storageService);
    auto stubAdapter = std::make_unique<StubAgentAdapter>();
    StubAgentAdapter *stubAdapterRaw = stubAdapter.get();
    QVERIFY(manager.registerAdapter(std::move(stubAdapter), &errorMessage));

    qtcode::agents::AgentSession *session = manager.createSession(
        project.id,
        QStringLiteral("stub"),
        QStringLiteral("Stub session"),
        &errorMessage);
    QVERIFY2(session != nullptr, qPrintable(errorMessage));

    qtcode::agents::AgentRequest request;
    request.sessionId = session->id();
    request.projectId = project.id;
    request.prompt = QStringLiteral("Hello stub agent");
    request.workingDirectory = project.rootPath;
    request.nonInteractive = true;

    QVERIFY(manager.dispatchRequest(session->id(), request, &errorMessage));
    QCOMPARE(session->status(), qtcode::agents::AgentSessionStatus::Running);

    QSignalSpy finishedSpy(stubAdapterRaw, &qtcode::agents::AgentAdapter::requestFinished);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

    const QList<qtcode::agents::AgentMessage> messages = session->messages();
    QCOMPARE(messages.size(), 2);
    QCOMPARE(messages.at(0).role, QStringLiteral("user"));
    QCOMPARE(messages.at(0).content, request.prompt);
    QCOMPARE(messages.at(1).role, QStringLiteral("assistant"));
    QVERIFY(messages.at(1).content.contains(QStringLiteral("Stub agent reply")));
    QCOMPARE(session->status(), qtcode::agents::AgentSessionStatus::Completed);
}

QTEST_MAIN(AgentManagerConversationTest)

#include "AgentManagerConversationTest.moc"

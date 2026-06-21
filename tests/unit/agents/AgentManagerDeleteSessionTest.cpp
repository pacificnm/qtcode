#include "agents/AgentManager.h"
#include "agents/AgentSession.h"
#include "settings/ProjectModels.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "storage/repositories/AgentSessionRepository.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace {

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

class AgentManagerDeleteSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void deleteSessionRemovesPersistenceAndActiveSelection();
};

void AgentManagerDeleteSessionTest::deleteSessionRemovesPersistenceAndActiveSelection()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString databasePath = tempDir.filePath(QStringLiteral("qtcode.db"));
    const QString projectId = createId();
    const QString sessionToKeep = createId();
    const QString sessionToDelete = createId();

    qtcode::settings::ProjectRecord project;
    project.id = projectId;
    project.name = QStringLiteral("Delete Session Project");
    project.rootPath = tempDir.path();
    project.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    project.updatedAt = project.createdAt;
    project.lastOpenedAt = project.createdAt;

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

        qtcode::storage::ProjectRepository projectRepository(storageService);
        QVERIFY(projectRepository.insertProject(project, &errorMessage));

        qtcode::storage::AgentSessionRepository sessionRepository(storageService);
        const auto insertSession = [&](const QString &sessionId, const QString &title) {
            qtcode::storage::PersistedAgentSession record;
            record.id = sessionId;
            record.projectId = projectId;
            record.agentKey = QStringLiteral("codex");
            record.title = title;
            record.status = QStringLiteral("idle");
            record.createdAt = project.createdAt;
            record.updatedAt = project.updatedAt;
            return sessionRepository.insertSession(record, &errorMessage);
        };

        QVERIFY(insertSession(sessionToKeep, QStringLiteral("Keep me")));
        QVERIFY(insertSession(sessionToDelete, QStringLiteral("Delete me")));

        qtcode::agents::AgentManager agentManager(storageService);
        QVERIFY(agentManager.restoreState(&errorMessage));
        QCOMPARE(agentManager.sessionsForProject(projectId).size(), 2);
        QVERIFY(agentManager.persistActiveSessionForProject(projectId, sessionToDelete, &errorMessage));
        QVERIFY(agentManager.persistRequestOptionsForSession(
            sessionToDelete,
            qtcode::agents::AgentSessionRequestOptions {
                QStringLiteral("gpt-test"),
                QStringLiteral("agent")},
            &errorMessage));

        QVERIFY(agentManager.deleteSession(sessionToDelete, &errorMessage));
        QCOMPARE(agentManager.session(sessionToDelete), nullptr);
        QCOMPARE(agentManager.sessionsForProject(projectId).size(), 1);
        QCOMPARE(agentManager.sessionsForProject(projectId).first()->id(), sessionToKeep);
        QCOMPARE(agentManager.activeSessionIdForProject(projectId), QString());
    }

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

        qtcode::storage::AgentSessionRepository sessionRepository(storageService);
        QList<qtcode::storage::PersistedAgentSession> persistedSessions;
        QVERIFY(sessionRepository.listSessions(&persistedSessions, &errorMessage));
        QCOMPARE(persistedSessions.size(), 1);
        QCOMPARE(persistedSessions.first().id, sessionToKeep);

        qtcode::storage::SettingsRepository settingsRepository(storageService);
        QJsonObject activeSessionJson;
        bool found = false;
        QVERIFY(settingsRepository.loadJson(
            qtcode::settings::kActiveAgentSessionByProjectSettingKey,
            &activeSessionJson,
            &found,
            &errorMessage));
        QVERIFY(!activeSessionJson.contains(projectId));

        QJsonObject requestOptionsJson;
        found = false;
        QVERIFY(settingsRepository.loadJson(
            qtcode::settings::kAgentSessionRequestOptionsSettingKey,
            &requestOptionsJson,
            &found,
            &errorMessage));
        QVERIFY(!requestOptionsJson.contains(sessionToDelete));
    }
}

QTEST_MAIN(AgentManagerDeleteSessionTest)

#include "AgentManagerDeleteSessionTest.moc"

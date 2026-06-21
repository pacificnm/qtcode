#include "agents/AgentManager.h"
#include "settings/ProjectModels.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "storage/repositories/SettingsRepository.h"

#include <QTemporaryDir>
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

} // namespace

class AgentManagerActiveSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void activeSessionPersistsPerProject();
};

void AgentManagerActiveSessionTest::activeSessionPersistsPerProject()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString databasePath = tempDir.filePath(QStringLiteral("qtcode.db"));
    const QString projectA = QStringLiteral("project-a");
    const QString projectB = QStringLiteral("project-b");
    const QString sessionA = QStringLiteral("session-a");
    const QString sessionB = QStringLiteral("session-b");

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

        qtcode::agents::AgentManager agentManager(storageService);
        QVERIFY(agentManager.persistActiveSessionForProject(projectA, sessionA, &errorMessage));
        QVERIFY(agentManager.persistActiveSessionForProject(projectB, sessionB, &errorMessage));
        QCOMPARE(agentManager.activeSessionIdForProject(projectA), sessionA);
        QCOMPARE(agentManager.activeSessionIdForProject(projectB), sessionB);
    }

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

        qtcode::agents::AgentManager agentManager(storageService);
        QCOMPARE(agentManager.activeSessionIdForProject(projectA), sessionA);
        QCOMPARE(agentManager.activeSessionIdForProject(projectB), sessionB);
        QCOMPARE(agentManager.activeSessionIdForProject(QStringLiteral("missing-project")), QString());
    }
}

QTEST_MAIN(AgentManagerActiveSessionTest)

#include "AgentManagerActiveSessionTest.moc"

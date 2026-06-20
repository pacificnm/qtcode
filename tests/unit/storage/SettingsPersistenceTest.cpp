#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "storage/repositories/SettingsRepository.h"

#include <QFile>
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

class SettingsPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void settingsPersistAcrossDatabaseReopen();
    void backupCopyPreservesSettings();
};

void SettingsPersistenceTest::settingsPersistAcrossDatabaseReopen()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString databasePath = tempDir.filePath(QStringLiteral("qtcode.db"));

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

        qtcode::storage::SettingsRepository settingsRepository(storageService);
        QJsonObject layout;
        layout.insert(QStringLiteral("windowWidth"), 1440);
        layout.insert(QStringLiteral("windowHeight"), 900);
        QVERIFY(settingsRepository.upsertJson(QStringLiteral("app.panel_layout"), layout, &errorMessage));
    }

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(storageService.open(&errorMessage), qPrintable(errorMessage));

        qtcode::storage::SettingsRepository settingsRepository(storageService);
        QJsonObject loadedLayout;
        bool found = false;
        QVERIFY(settingsRepository.loadJson(QStringLiteral("app.panel_layout"), &loadedLayout, &found, &errorMessage));
        QVERIFY(found);
        QCOMPARE(loadedLayout.value(QStringLiteral("windowWidth")).toInt(), 1440);
        QCOMPARE(loadedLayout.value(QStringLiteral("windowHeight")).toInt(), 900);
    }
}

void SettingsPersistenceTest::backupCopyPreservesSettings()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString databasePath = tempDir.filePath(QStringLiteral("qtcode.db"));
    const QString backupPath = tempDir.filePath(QStringLiteral("qtcode.backup.db"));

    {
        qtcode::storage::StorageService storageService(databasePath);
        QString errorMessage;
        QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

        qtcode::storage::SettingsRepository settingsRepository(storageService);
        QJsonObject activeProject;
        activeProject.insert(QStringLiteral("projectId"), QStringLiteral("project-123"));
        QVERIFY(settingsRepository.upsertJson(QStringLiteral("app.active_project_id"), activeProject, &errorMessage));
    }

    QVERIFY(QFile::exists(databasePath));
    if (QFile::exists(backupPath)) {
        QVERIFY(QFile::remove(backupPath));
    }
    QVERIFY(QFile::copy(databasePath, backupPath));

    {
        qtcode::storage::StorageService restoredStorage(backupPath);
        QString errorMessage;
        QVERIFY2(openMigratedDatabase(&restoredStorage, &errorMessage), qPrintable(errorMessage));

        qtcode::storage::SettingsRepository settingsRepository(restoredStorage);
        QJsonObject loadedProject;
        bool found = false;
        QVERIFY(settingsRepository.loadJson(
            QStringLiteral("app.active_project_id"),
            &loadedProject,
            &found,
            &errorMessage));
        QVERIFY(found);
        QCOMPARE(loadedProject.value(QStringLiteral("projectId")).toString(), QStringLiteral("project-123"));
    }
}

QTEST_MAIN(SettingsPersistenceTest)

#include "SettingsPersistenceTest.moc"

#include "core/RepoConfigLoader.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/TerminalSessionRepository.h"
#include "terminal/TerminalManager.h"
#include "terminal/TerminalProfile.h"

#include <QDir>
#include <QJsonDocument>
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

[[nodiscard]] qtcode::settings::ProjectRecord insertProject(
    qtcode::storage::StorageService &storageService,
    const QString &rootPath,
    const QString &name)
{
    qtcode::settings::ProjectRecord project;
    project.id = QStringLiteral("project-1");
    project.name = name;
    project.rootPath = rootPath;
    project.createdAt = QStringLiteral("2026-06-20T00:00:00.000Z");
    project.updatedAt = project.createdAt;
    project.lastOpenedAt = project.createdAt;

    qtcode::storage::ProjectRepository repository(storageService);
    QString errorMessage;
    if (!repository.insertProject(project, &errorMessage)) {
        return {};
    }

    return project;
}

} // namespace

class TerminalManagerWorkingDirectoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolveProjectWorkingDirectoryUsesCanonicalProjectRoot();
    void resolveSessionWorkingDirectoryIgnoresStalePersistedHomePath();
    void syncSessionsToActiveProjectUpdatesPersistedWorkingDirectory();
    void resolveProjectWorkingDirectoryUsesRepoConfigPath();
};

void TerminalManagerWorkingDirectoryTest::resolveProjectWorkingDirectoryUsesCanonicalProjectRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString nestedPath = tempDir.filePath(QStringLiteral("repo/nested"));
    QVERIFY(QDir().mkpath(nestedPath));

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("qtcode.db")));
    QString errorMessage;
    QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::ProjectRecord project =
        insertProject(storageService, nestedPath, QStringLiteral("repo"));
    QVERIFY(!project.id.isEmpty());

    qtcode::terminal::TerminalManager terminalManager(storageService);
    QVERIFY(terminalManager.restoreState(&errorMessage));

    QString workingDirectory;
    QVERIFY(terminalManager.resolveProjectWorkingDirectory(
        project.id,
        &workingDirectory,
        nullptr,
        &errorMessage));
    QCOMPARE(workingDirectory, QDir(nestedPath).canonicalPath());
}

void TerminalManagerWorkingDirectoryTest::resolveSessionWorkingDirectoryIgnoresStalePersistedHomePath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString repoPath = tempDir.filePath(QStringLiteral("repo"));
    QVERIFY(QDir().mkpath(repoPath));

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("qtcode.db")));
    QString errorMessage;
    QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::ProjectRecord project =
        insertProject(storageService, repoPath, QStringLiteral("repo"));
    QVERIFY(!project.id.isEmpty());

    qtcode::terminal::TerminalManager terminalManager(storageService);
    QVERIFY(terminalManager.restoreState(&errorMessage));

    qtcode::terminal::TerminalSession session;
    session.id = QStringLiteral("session-1");
    session.projectId = project.id;
    session.workingDirectory = QDir::homePath();
    session.profileJson = QString::fromUtf8(
        QJsonDocument(qtcode::terminal::TerminalProfile::defaults().toJson())
            .toJson(QJsonDocument::Compact));

    QVERIFY(terminalManager.resolveSessionWorkingDirectory(&session, &errorMessage));
    QCOMPARE(session.workingDirectory, QDir(repoPath).canonicalPath());
}

void TerminalManagerWorkingDirectoryTest::syncSessionsToActiveProjectUpdatesPersistedWorkingDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString repoPath = tempDir.filePath(QStringLiteral("repo"));
    QVERIFY(QDir().mkpath(repoPath));

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("qtcode.db")));
    QString errorMessage;
    QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::ProjectRecord project =
        insertProject(storageService, repoPath, QStringLiteral("repo"));
    QVERIFY(!project.id.isEmpty());

    qtcode::terminal::TerminalSession session;
    session.id = QStringLiteral("session-1");
    session.projectId = QString();
    session.title = QStringLiteral("Terminal");
    session.shellPath = QStringLiteral("/bin/bash");
    session.workingDirectory = QDir::homePath();
    session.profileJson = QString::fromUtf8(
        QJsonDocument(qtcode::terminal::TerminalProfile::defaults().toJson())
            .toJson(QJsonDocument::Compact));
    session.createdAt = QStringLiteral("2026-06-20T00:00:00.000Z");
    session.updatedAt = session.createdAt;

    qtcode::storage::TerminalSessionRepository repository(storageService);
    QVERIFY(repository.insertSession(session, &errorMessage));

    qtcode::terminal::TerminalManager terminalManager(storageService);
    QVERIFY(terminalManager.restoreState(&errorMessage));

    const QList<qtcode::terminal::TerminalSession> sessionsBeforeSync = terminalManager.sessions();
    QCOMPARE(sessionsBeforeSync.size(), 1);
    QCOMPARE(sessionsBeforeSync.first().workingDirectory, QDir::homePath());

    QVERIFY(terminalManager.syncSessionsToActiveProject(project.id, &errorMessage));

    const QList<qtcode::terminal::TerminalSession> sessionsAfterSync = terminalManager.sessions();
    QCOMPARE(sessionsAfterSync.size(), 1);
    QCOMPARE(sessionsAfterSync.first().projectId, project.id);
    QCOMPARE(sessionsAfterSync.first().workingDirectory, QDir(repoPath).canonicalPath());
}

void TerminalManagerWorkingDirectoryTest::resolveProjectWorkingDirectoryUsesRepoConfigPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString repoPath = tempDir.filePath(QStringLiteral("repo"));
    const QString terminalPath = tempDir.filePath(QStringLiteral("terminal-root"));
    QVERIFY(QDir().mkpath(repoPath));
    QVERIFY(QDir().mkpath(terminalPath));

    QDir(repoPath).mkpath(QStringLiteral(".qtcode"));
    QFile configFile(QDir(repoPath).filePath(QStringLiteral(".qtcode/config.yaml")));
    QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Text));
    configFile.write(
        "project:\n"
        "  displayName: Terminal Project\n"
        "  path: ");
    configFile.write(terminalPath.toUtf8());
    configFile.write("\n");
    configFile.close();

    qtcode::storage::StorageService storageService(tempDir.filePath(QStringLiteral("qtcode.db")));
    QString errorMessage;
    QVERIFY2(openMigratedDatabase(&storageService, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::ProjectRecord project =
        insertProject(storageService, repoPath, QStringLiteral("repo"));
    QVERIFY(!project.id.isEmpty());

    qtcode::terminal::TerminalManager terminalManager(storageService);
    QVERIFY(terminalManager.restoreState(&errorMessage));

    QString workingDirectory;
    QString projectName;
    QVERIFY(terminalManager.resolveProjectWorkingDirectory(
        project.id,
        &workingDirectory,
        &projectName,
        &errorMessage));
    QCOMPARE(workingDirectory, QDir(terminalPath).canonicalPath());
    QCOMPARE(projectName, QStringLiteral("Terminal Project"));
}

QTEST_MAIN(TerminalManagerWorkingDirectoryTest)

#include "TerminalManagerWorkingDirectoryTest.moc"

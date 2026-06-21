#include "core/WorkspaceInstaller.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class WorkspaceInstallerTest final : public QObject
{
    Q_OBJECT

private slots:
    void installsWorkspaceFilesIntoEmptyRepository();
    void skipsExistingFilesOnSecondInstall();
    void reportsHealthForInstalledWorkspace();
};

void WorkspaceInstallerTest::installsWorkspaceFilesIntoEmptyRepository()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::core::WorkspaceInstaller installer;
    qtcode::core::WorkspaceInstallContext context;
    context.projectName = QStringLiteral("demo");
    context.rootPath = tempDir.path();
    context.scopeKey = tempDir.path();

    qtcode::core::WorkspaceInstallResult result;
    QString errorMessage;
    QVERIFY2(installer.install(context, &result, &errorMessage), qPrintable(errorMessage));
    QVERIFY(result.success);
    QVERIFY(result.createdPaths.size() > 0);

    QVERIFY(QFileInfo::exists(qtcode::core::WorkspaceInstaller::workspaceManifestPath(tempDir.path())));
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("scripts/index-memory"))));
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("tools/mcp_memory_server.py"))));
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral(".cursor/mcp.json"))));

    QFile manifestFile(qtcode::core::WorkspaceInstaller::workspaceManifestPath(tempDir.path()));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString manifestText = QString::fromUtf8(manifestFile.readAll());
    QVERIFY(manifestText.contains(context.scopeKey));
    QVERIFY(manifestText.contains(context.projectName));
}

void WorkspaceInstallerTest::skipsExistingFilesOnSecondInstall()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::core::WorkspaceInstaller installer;
    qtcode::core::WorkspaceInstallContext context;
    context.projectName = QStringLiteral("demo");
    context.rootPath = tempDir.path();
    context.scopeKey = tempDir.path();

    qtcode::core::WorkspaceInstallResult firstResult;
    QString errorMessage;
    QVERIFY(installer.install(context, &firstResult, &errorMessage));

    qtcode::core::WorkspaceInstallResult secondResult;
    QVERIFY(installer.install(context, &secondResult, &errorMessage));
    QVERIFY(secondResult.createdPaths.isEmpty());
    QVERIFY(secondResult.skippedPaths.size() >= firstResult.createdPaths.size());
}

void WorkspaceInstallerTest::reportsHealthForInstalledWorkspace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::core::WorkspaceInstaller installer;
    qtcode::core::WorkspaceInstallContext context;
    context.projectName = QStringLiteral("demo");
    context.rootPath = tempDir.path();
    context.scopeKey = tempDir.path();

    qtcode::core::WorkspaceInstallResult result;
    QString errorMessage;
    QVERIFY(installer.install(context, &result, &errorMessage));

    const QList<qtcode::core::WorkspaceHealthCheck> checks = installer.evaluateHealth(tempDir.path());
    QVERIFY(checks.size() >= 4);

    const auto manifestCheck = std::find_if(
        checks.cbegin(),
        checks.cend(),
        [](const qtcode::core::WorkspaceHealthCheck &check) {
            return check.id == QStringLiteral("manifest");
        });
    QVERIFY(manifestCheck != checks.cend());
    QCOMPARE(manifestCheck->state, qtcode::core::WorkspaceHealthState::Ok);
}

QTEST_MAIN(WorkspaceInstallerTest)
#include "WorkspaceInstallerTest.moc"

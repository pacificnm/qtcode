#include "git/GitCliClient.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class GitCliClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void stageCommitAndPushInTemporaryRepository();
};

void GitCliClientTest::stageCommitAndPushInTemporaryRepository()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString repositoryPath = tempDir.path();
    const QString gitExecutable = QStringLiteral("git");

    auto runGit = [&](const QStringList &arguments) {
        QProcess process;
        process.setWorkingDirectory(repositoryPath);
        process.setProgram(gitExecutable);
        process.setArguments(arguments);
        process.start(QProcess::ReadOnly);
        QVERIFY(process.waitForStarted(5000));
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    };

    runGit({QStringLiteral("init")});
    runGit({QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@example.com")});
    runGit({QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("QTCode Test")});

    const QString filePath = repositoryPath + QStringLiteral("/README.md");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("hello");
    file.close();

    qtcode::git::GitCliClient client;
    client.setExecutablePath(gitExecutable);
    client.setWorkingDirectory(repositoryPath);

    const qtcode::git::GitOperationResult stageResult = client.stageAll();
    QVERIFY(stageResult.success);

    const qtcode::git::GitOperationResult commitResult = client.commit(QStringLiteral("Initial commit"));
    QVERIFY(commitResult.success);

    const qtcode::git::GitOperationResult unstageResult = client.unstageAll();
    QVERIFY(unstageResult.success);

    QVERIFY(file.open(QIODevice::Append | QIODevice::Text));
    file.write("\nworld");
    file.close();

    const qtcode::git::GitOperationResult stageFileResult =
        client.stagePaths({QStringLiteral("README.md")});
    QVERIFY(stageFileResult.success);

    const qtcode::git::GitOperationResult secondCommitResult =
        client.commit(QStringLiteral("Second commit"));
    QVERIFY(secondCommitResult.success);
}

QTEST_MAIN(GitCliClientTest)
#include "GitCliClientTest.moc"

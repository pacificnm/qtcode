#include "git/GitService.h"

#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class GitServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadWorkingTreeStatusReportsCommitsAheadWithUpstream();
    void loadWorkingTreeStatusReportsCommitsAheadWithoutConfiguredUpstream();
};

void GitServiceTest::loadWorkingTreeStatusReportsCommitsAheadWithUpstream()
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

    runGit({QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")});
    runGit({QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@example.com")});
    runGit({QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("QTCode Test")});

    QFile file(repositoryPath + QStringLiteral("/README.md"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("hello");
    file.close();

    runGit({QStringLiteral("add"), QStringLiteral("README.md")});
    runGit({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Initial commit")});

    runGit({QStringLiteral("branch"), QStringLiteral("-m"), QStringLiteral("main")});
    runGit({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), repositoryPath});
    runGit({QStringLiteral("fetch"), QStringLiteral("origin")});
    runGit({QStringLiteral("branch"), QStringLiteral("--set-upstream-to=origin/main"), QStringLiteral("main")});

    QVERIFY(file.open(QIODevice::Append | QIODevice::Text));
    file.write("\nworld");
    file.close();

    runGit({QStringLiteral("add"), QStringLiteral("README.md")});
    runGit({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Second commit")});

    qtcode::git::GitService gitService;
    qtcode::git::GitWorkingTreeStatus status;
    QVERIFY(gitService.loadWorkingTreeStatus(repositoryPath, &status));

    QCOMPARE(status.commitsAhead, 1);
    QVERIFY(status.hasUpstream);
}

void GitServiceTest::loadWorkingTreeStatusReportsCommitsAheadWithoutConfiguredUpstream()
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

    runGit({QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")});
    runGit({QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@example.com")});
    runGit({QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("QTCode Test")});

    QFile file(repositoryPath + QStringLiteral("/README.md"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("hello");
    file.close();

    runGit({QStringLiteral("add"), QStringLiteral("README.md")});
    runGit({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Initial commit")});

    runGit({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), repositoryPath});
    runGit({QStringLiteral("fetch"), QStringLiteral("origin")});

    QVERIFY(file.open(QIODevice::Append | QIODevice::Text));
    file.write("\nworld");
    file.close();

    runGit({QStringLiteral("add"), QStringLiteral("README.md")});
    runGit({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Second commit")});

    qtcode::git::GitService gitService;
    qtcode::git::GitWorkingTreeStatus status;
    QVERIFY(gitService.loadWorkingTreeStatus(repositoryPath, &status));

    QCOMPARE(status.commitsAhead, 1);
}

QTEST_MAIN(GitServiceTest)
#include "GitServiceTest.moc"

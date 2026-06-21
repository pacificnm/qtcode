#include "core/RepoConfigLoader.h"
#include "core/RepoConfigWriter.h"

#include "settings/RepoConfig.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class RepoConfigWriterTest final : public QObject
{
    Q_OBJECT

private slots:
    void saveRepoHelpPathCreatesConfigFile();
    void saveEmptyRepoHelpPathClearsOverride();
    void saveRepoHelpPathCreatesQtcodeDirectory();
};

void RepoConfigWriterTest::saveRepoHelpPathCreatesConfigFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::saveRepoHelpPath(
        tempDir.path(),
        QStringLiteral("docs/README.md"),
        &errorMessage));

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QCOMPARE(config.repoHelpPath, QStringLiteral("docs/README.md"));
}

void RepoConfigWriterTest::saveEmptyRepoHelpPathClearsOverride()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::saveRepoHelpPath(
        tempDir.path(),
        QStringLiteral("docs/README.md"),
        &errorMessage));
    QVERIFY(qtcode::core::RepoConfigWriter::saveRepoHelpPath(
        tempDir.path(),
        QString(),
        &errorMessage));

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QVERIFY(!config.hasRepoHelpPath());
}

void RepoConfigWriterTest::saveRepoHelpPathCreatesQtcodeDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::saveRepoHelpPath(
        tempDir.path(),
        QStringLiteral("doc/index.md"),
        &errorMessage));

    QVERIFY(QDir(tempDir.path()).exists(QStringLiteral(".qtcode")));
}

QTEST_MAIN(RepoConfigWriterTest)

#include "RepoConfigWriterTest.moc"

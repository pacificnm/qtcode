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
    void saveConfigPreservesDefaultAgentWhenClearingHelpPath();
    void saveConfigWritesDefaultAgentAndHelpPath();
    void saveConfigWritesProjectSettings();
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

void RepoConfigWriterTest::saveConfigPreservesDefaultAgentWhenClearingHelpPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::settings::RepoConfig config;
    config.defaultAgentKey = QStringLiteral("cursor");
    config.repoHelpPath = QStringLiteral("docs/README.md");

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::save(tempDir.path(), config, &errorMessage));
    QVERIFY(qtcode::core::RepoConfigWriter::saveRepoHelpPath(tempDir.path(), QString(), &errorMessage));

    const qtcode::settings::RepoConfig loaded =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QVERIFY(!loaded.hasRepoHelpPath());
    QCOMPARE(loaded.defaultAgentKey, QStringLiteral("cursor"));
}

void RepoConfigWriterTest::saveConfigWritesDefaultAgentAndHelpPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::settings::RepoConfig config;
    config.defaultAgentKey = QStringLiteral("codex");
    config.repoHelpPath = QStringLiteral("docs/README.md");

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::save(tempDir.path(), config, &errorMessage));

    const qtcode::settings::RepoConfig loaded =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QCOMPARE(loaded.defaultAgentKey, QStringLiteral("codex"));
    QCOMPARE(loaded.repoHelpPath, QStringLiteral("docs/README.md"));
}

void RepoConfigWriterTest::saveConfigWritesProjectSettings()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtcode::settings::RepoConfig config;
    config.projectDisplayName = QStringLiteral("demo");
    config.projectPath = tempDir.path();

    QString errorMessage;
    QVERIFY(qtcode::core::RepoConfigWriter::save(tempDir.path(), config, &errorMessage));

    const qtcode::settings::RepoConfig loaded =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QCOMPARE(loaded.projectDisplayName, QStringLiteral("demo"));
    QCOMPARE(loaded.projectPath, tempDir.path());
}

QTEST_MAIN(RepoConfigWriterTest)

#include "RepoConfigWriterTest.moc"

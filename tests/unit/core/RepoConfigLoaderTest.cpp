#include "core/RepoConfigLoader.h"

#include "settings/RepoConfig.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class RepoConfigLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadFromMissingFileReturnsEmptyConfig();
    void loadTopLevelRepoHelpPath();
    void loadNestedHelpEntryPath();
    void loadNestedDefaultAgentKey();
    void loadTopLevelDefaultAgentKey();
    void loadNestedProjectSettings();
    void repoHelpPathOverridesSystemDefault();
};

void RepoConfigLoaderTest::loadFromMissingFileReturnsEmptyConfig()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QVERIFY(!config.hasRepoHelpPath());
}

void RepoConfigLoaderTest::loadTopLevelRepoHelpPath()
{
    const QString yaml = QStringLiteral(
        "# repo settings\n"
        "repoHelpPath: docs/README.md\n");

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromYamlContent(yaml);
    QCOMPARE(config.repoHelpPath, QStringLiteral("docs/README.md"));
}

void RepoConfigLoaderTest::loadNestedHelpEntryPath()
{
    const QString yaml = QStringLiteral(
        "help:\n"
        "  entryPath: docs/index.md\n");

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromYamlContent(yaml);
    QCOMPARE(config.repoHelpPath, QStringLiteral("docs/index.md"));
}

void RepoConfigLoaderTest::loadNestedDefaultAgentKey()
{
    const QString yaml = QStringLiteral(
        "agent:\n"
        "  defaultAgentKey: cursor\n");

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromYamlContent(yaml);
    QCOMPARE(config.defaultAgentKey, QStringLiteral("cursor"));
}

void RepoConfigLoaderTest::loadTopLevelDefaultAgentKey()
{
    const QString yaml = QStringLiteral(
        "defaultAgentKey: codex\n");

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromYamlContent(yaml);
    QCOMPARE(config.defaultAgentKey, QStringLiteral("codex"));
}

void RepoConfigLoaderTest::loadNestedProjectSettings()
{
    const QString yaml = QStringLiteral(
        "project:\n"
        "  displayName: My Project\n"
        "  path: /tmp/my-project\n");

    const qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromYamlContent(yaml);
    QCOMPARE(config.projectDisplayName, QStringLiteral("My Project"));
    QCOMPARE(config.projectPath, QStringLiteral("/tmp/my-project"));
}

void RepoConfigLoaderTest::repoHelpPathOverridesSystemDefault()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir repoRoot(tempDir.path());
    QVERIFY(repoRoot.mkpath(QStringLiteral(".qtcode")));

    QFile configFile(repoRoot.filePath(QString::fromLatin1(qtcode::settings::kRepoConfigRelativePath)));
    QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Text));
    configFile.write("help:\n  entryPath: docs/README.md\n");
    configFile.close();

    qtcode::settings::AppConfig appConfig;
    appConfig.repoHelpPath = QStringLiteral("doc/index.md");

    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(tempDir.path());
    QCOMPARE(
        qtcode::settings::effectiveRepoHelpPath(appConfig, repoConfig),
        QStringLiteral("docs/README.md"));
}

QTEST_MAIN(RepoConfigLoaderTest)

#include "RepoConfigLoaderTest.moc"

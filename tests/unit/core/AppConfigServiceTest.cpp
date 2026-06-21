#include "core/AppConfigService.h"

#include <QStandardPaths>
#include <QtTest>

class AppConfigServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void saveAndLoadRoundTrip();
    void legacyDirectoryOnlyPathNormalizesToIndexFile();
};

void AppConfigServiceTest::saveAndLoadRoundTrip()
{
    QStandardPaths::setTestModeEnabled(true);

    qtcode::core::AppConfigService configService;

    qtcode::settings::AppConfig config;
    config.restoreLastProjectOnStartup = false;
    config.startMaximized = true;
    config.repoHelpPath = QStringLiteral("docs/README.md");

    QString errorMessage;
    QVERIFY2(configService.save(config, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::AppConfig defaults = configService.defaultConfig();
    QCOMPARE(defaults.restoreLastProjectOnStartup, true);
    QCOMPARE(defaults.startMaximized, false);
    QCOMPARE(defaults.repoHelpPath, QString::fromLatin1(qtcode::settings::kAppConfigDefaultRepoHelpPath));

    qtcode::core::AppConfigService loadedService;
    QVERIFY2(loadedService.load(&errorMessage), qPrintable(errorMessage));
    QCOMPARE(loadedService.config().restoreLastProjectOnStartup, false);
    QCOMPARE(loadedService.config().startMaximized, true);
    QCOMPARE(loadedService.config().repoHelpPath, QStringLiteral("docs/README.md"));
}

void AppConfigServiceTest::legacyDirectoryOnlyPathNormalizesToIndexFile()
{
    QStandardPaths::setTestModeEnabled(true);

    qtcode::core::AppConfigService configService;

    qtcode::settings::AppConfig config;
    config.repoHelpPath = QStringLiteral("doc");

    QString errorMessage;
    QVERIFY2(configService.save(config, &errorMessage), qPrintable(errorMessage));

    qtcode::core::AppConfigService loadedService;
    QVERIFY2(loadedService.load(&errorMessage), qPrintable(errorMessage));
    QCOMPARE(loadedService.config().repoHelpPath, QStringLiteral("doc/index.md"));
}

QTEST_MAIN(AppConfigServiceTest)

#include "AppConfigServiceTest.moc"

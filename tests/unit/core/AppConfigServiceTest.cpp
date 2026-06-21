#include "core/AppConfigService.h"

#include <QStandardPaths>
#include <QtTest>

class AppConfigServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void saveAndLoadRoundTrip();
};

void AppConfigServiceTest::saveAndLoadRoundTrip()
{
    QStandardPaths::setTestModeEnabled(true);

    qtcode::core::AppConfigService configService;

    qtcode::settings::AppConfig config;
    config.restoreLastProjectOnStartup = false;
    config.startMaximized = true;

    QString errorMessage;
    QVERIFY2(configService.save(config, &errorMessage), qPrintable(errorMessage));

    const qtcode::settings::AppConfig defaults = configService.defaultConfig();
    QCOMPARE(defaults.restoreLastProjectOnStartup, true);
    QCOMPARE(defaults.startMaximized, false);

    qtcode::core::AppConfigService loadedService;
    QVERIFY2(loadedService.load(&errorMessage), qPrintable(errorMessage));
    QCOMPARE(loadedService.config().restoreLastProjectOnStartup, false);
    QCOMPARE(loadedService.config().startMaximized, true);
}

QTEST_MAIN(AppConfigServiceTest)

#include "AppConfigServiceTest.moc"

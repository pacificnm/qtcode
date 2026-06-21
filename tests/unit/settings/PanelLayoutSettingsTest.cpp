#include "settings/SettingsModels.h"

#include <QtTest>

class PanelLayoutSettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void fromJsonClampsOversizedLeftColumn();
    void clampLeftColumnWidthMovesExcessToCenter();
    void clampRightPanelWidthUsesConfiguredRange();
    void defaultsUseAgentSessionsRightPanel();
    void toJsonPersistsCollapseStateOnly();
};

void PanelLayoutSettingsTest::clampRightPanelWidthUsesConfiguredRange()
{
    QCOMPARE(qtcode::settings::clampRightPanelWidth(120), qtcode::settings::kRightColumnMinWidth);
    QCOMPARE(qtcode::settings::clampRightPanelWidth(320), 320);
    QCOMPARE(qtcode::settings::clampRightPanelWidth(900), qtcode::settings::kRightColumnMaxWidth);
}

void PanelLayoutSettingsTest::fromJsonClampsOversizedLeftColumn()
{
    QJsonObject json;
    json.insert(QStringLiteral("columnSizes"), QJsonArray {720, 400, 320});

    const qtcode::settings::PanelLayoutSettings layout =
        qtcode::settings::PanelLayoutSettings::fromJson(json);

    QCOMPARE(layout.columnSizes.size(), 3);
    QCOMPARE(layout.columnSizes.at(0), qtcode::settings::kLeftColumnMaxWidth);
    QCOMPARE(layout.columnSizes.at(1), 800);
    QCOMPARE(layout.columnSizes.at(2), 320);
}

void PanelLayoutSettingsTest::clampLeftColumnWidthMovesExcessToCenter()
{
    QList<int> sizes {520, 500, 320};

    qtcode::settings::PanelLayoutSettings::clampLeftColumnWidth(&sizes);

    QCOMPARE(sizes.at(0), qtcode::settings::kLeftColumnMaxWidth);
    QCOMPARE(sizes.at(1), 700);
    QCOMPARE(sizes.at(2), 320);
}

void PanelLayoutSettingsTest::defaultsUseAgentSessionsRightPanel()
{
    const qtcode::settings::PanelLayoutSettings layout =
        qtcode::settings::PanelLayoutSettings::defaults();

    QCOMPARE(
        layout.activeRightPanel,
        QString::fromLatin1(qtcode::settings::kRightPanelSessions));
}

void PanelLayoutSettingsTest::toJsonPersistsCollapseStateOnly()
{
    qtcode::settings::PanelLayoutSettings layout;
    layout.activeRightPanel = QString::fromLatin1(qtcode::settings::kRightPanelContext);
    layout.rightColumnCollapsed = true;
    layout.terminalCollapsed = true;
    layout.storedTerminalHeight = 280;
    layout.verticalSizes = {640, 180};
    layout.windowWidth = 1600;
    layout.windowHeight = 900;

    const QJsonObject json = layout.toJson();

    QCOMPARE(
        json.value(QStringLiteral("layoutSchemaVersion")).toInt(),
        qtcode::settings::kPanelLayoutSchemaVersion);
    QVERIFY(json.contains(QStringLiteral("activeRightPanel")));
    QVERIFY(json.contains(QStringLiteral("rightColumnCollapsed")));
    QVERIFY(json.contains(QStringLiteral("terminalCollapsed")));
    QVERIFY(json.contains(QStringLiteral("storedTerminalHeight")));
    QVERIFY(!json.contains(QStringLiteral("columnSizes")));
    QVERIFY(!json.contains(QStringLiteral("verticalSizes")));
    QVERIFY(!json.contains(QStringLiteral("windowWidth")));
    QVERIFY(!json.contains(QStringLiteral("windowHeight")));
    QVERIFY(!json.contains(QStringLiteral("storedRightColumnWidth")));
}

QTEST_MAIN(PanelLayoutSettingsTest)

#include "PanelLayoutSettingsTest.moc"

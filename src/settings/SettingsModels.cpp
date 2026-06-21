#include "settings/SettingsModels.h"

#include <QJsonArray>
#include <QLatin1String>

namespace qtcode::settings {

namespace {

QList<int> jsonArrayToSizes(const QJsonArray &array, const QList<int> &fallback)
{
    QList<int> sizes;
    sizes.reserve(array.size());

    for (const QJsonValue &value : array) {
        if (!value.isDouble()) {
            return fallback;
        }
        sizes.append(value.toInt());
    }

    if (sizes.isEmpty()) {
        return fallback;
    }

    return sizes;
}

QJsonArray sizesToJsonArray(const QList<int> &sizes)
{
    QJsonArray array;
    for (int size : sizes) {
        array.append(size);
    }
    return array;
}

QString normalizeRightPanelId(const QString &panelId, const QString &fallback)
{
    if (panelId == QLatin1String(kRightPanelSessions)
        || panelId == QLatin1String(kRightPanelContext)
        || panelId == QLatin1String(kRightPanelMcp)
        || panelId == QLatin1String(kRightPanelNone)) {
        return panelId;
    }

    if (panelId == QLatin1String("changes")) {
        return QString::fromLatin1(kRightPanelNone);
    }

    return fallback;
}

} // namespace

QJsonObject PanelLayoutSettings::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("layoutSchemaVersion"), kPanelLayoutSchemaVersion);
    json.insert(QStringLiteral("columnSizes"), sizesToJsonArray(columnSizes));
    json.insert(QStringLiteral("verticalSizes"), sizesToJsonArray(verticalSizes));
    json.insert(QStringLiteral("activeRightPanel"), activeRightPanel);
    json.insert(QStringLiteral("rightColumnCollapsed"), rightColumnCollapsed);
    json.insert(QStringLiteral("storedRightColumnWidth"), storedRightColumnWidth);
    json.insert(QStringLiteral("windowWidth"), windowWidth);
    json.insert(QStringLiteral("windowHeight"), windowHeight);
    json.insert(QStringLiteral("terminalCollapsed"), terminalCollapsed);
    json.insert(QStringLiteral("storedTerminalHeight"), storedTerminalHeight);
    return json;
}

PanelLayoutSettings PanelLayoutSettings::fromJson(const QJsonObject &json)
{
    const PanelLayoutSettings defaults = PanelLayoutSettings::defaults();
    PanelLayoutSettings layout = defaults;

    QList<int> legacyHorizontalSizes;

    if (json.contains(QStringLiteral("columnSizes"))) {
        layout.columnSizes = jsonArrayToSizes(
            json.value(QStringLiteral("columnSizes")).toArray(),
            defaults.columnSizes);
    }

    if (json.contains(QStringLiteral("horizontalSizes"))) {
        legacyHorizontalSizes = jsonArrayToSizes(
            json.value(QStringLiteral("horizontalSizes")).toArray(),
            QList<int> {640, 320});
    }

    if (json.contains(QStringLiteral("verticalSizes"))) {
        layout.verticalSizes = jsonArrayToSizes(
            json.value(QStringLiteral("verticalSizes")).toArray(),
            defaults.verticalSizes);
    }

    if (layout.columnSizes.size() == 4) {
        const int schemaVersion = json.value(QStringLiteral("layoutSchemaVersion")).toInt(0);
        if (schemaVersion >= kPanelLayoutSchemaVersion - 1) {
            layout.columnSizes = {
                layout.columnSizes.at(0),
                layout.columnSizes.at(2),
                layout.columnSizes.at(3),
            };
        } else {
            const int rightWidth = layout.columnSizes.at(2) > 0 ? layout.columnSizes.at(2)
                                                                : layout.columnSizes.at(3);
            layout.columnSizes = {
                layout.columnSizes.at(0),
                layout.columnSizes.at(1),
                rightWidth,
            };
        }
    }

    if (layout.columnSizes.size() == 3) {
        // Preserve three-column layouts.
    } else if (layout.columnSizes.size() == 2 && legacyHorizontalSizes.size() >= 2) {
        layout.columnSizes = {
            layout.columnSizes.at(0),
            legacyHorizontalSizes.at(0),
            legacyHorizontalSizes.at(1),
        };
    } else if (layout.columnSizes.size() == 3 && legacyHorizontalSizes.empty()
               && json.contains(QStringLiteral("columnSizes"))) {
        // Preserve three-column layouts from the current shell topology.
    } else if (!json.contains(QStringLiteral("columnSizes")) && legacyHorizontalSizes.size() >= 3) {
        layout.columnSizes = {
            legacyHorizontalSizes.at(0),
            legacyHorizontalSizes.at(1),
            legacyHorizontalSizes.at(2),
        };
    }

    if (json.contains(QStringLiteral("activeRightPanel"))) {
        layout.activeRightPanel = normalizeRightPanelId(
            json.value(QStringLiteral("activeRightPanel")).toString(),
            defaults.activeRightPanel);
    } else if (json.contains(QStringLiteral("contextPanelVisible"))
               && json.value(QStringLiteral("contextPanelVisible")).toBool()) {
        layout.activeRightPanel = QString::fromLatin1(kRightPanelContext);
    } else if (json.contains(QStringLiteral("agentPanelVisible"))
               && !json.value(QStringLiteral("agentPanelVisible")).toBool()) {
        layout.activeRightPanel = QString::fromLatin1(kRightPanelNone);
    }

    if (layout.activeRightPanel == QString::fromLatin1(kRightPanelNone)) {
        layout.rightColumnCollapsed = true;
        layout.activeRightPanel = QString::fromLatin1(kRightPanelSessions);
    }

    if (json.contains(QStringLiteral("rightColumnCollapsed"))) {
        layout.rightColumnCollapsed =
            json.value(QStringLiteral("rightColumnCollapsed")).toBool(defaults.rightColumnCollapsed);
    } else if (layout.columnSizes.size() >= 3 && layout.columnSizes.at(2) == 0) {
        layout.rightColumnCollapsed = true;
    }

    if (json.contains(QStringLiteral("storedRightColumnWidth"))) {
        layout.storedRightColumnWidth = json.value(QStringLiteral("storedRightColumnWidth"))
                                            .toInt(defaults.storedRightColumnWidth);
    } else if (layout.columnSizes.size() >= 3 && layout.columnSizes.at(2) > 120) {
        layout.storedRightColumnWidth = layout.columnSizes.at(2);
    }

    if (json.contains(QStringLiteral("windowWidth"))) {
        layout.windowWidth = json.value(QStringLiteral("windowWidth")).toInt(defaults.windowWidth);
    }

    if (json.contains(QStringLiteral("windowHeight"))) {
        layout.windowHeight = json.value(QStringLiteral("windowHeight")).toInt(defaults.windowHeight);
    }

    if (json.contains(QStringLiteral("terminalCollapsed"))) {
        layout.terminalCollapsed = json.value(QStringLiteral("terminalCollapsed")).toBool(false);
    }

    if (json.contains(QStringLiteral("storedTerminalHeight"))) {
        layout.storedTerminalHeight =
            json.value(QStringLiteral("storedTerminalHeight")).toInt(defaults.storedTerminalHeight);
    }

    clampLeftColumnWidth(&layout.columnSizes);

    return layout;
}

void PanelLayoutSettings::clampLeftColumnWidth(QList<int> *columnSizes)
{
    if (columnSizes == nullptr || columnSizes->size() < 3) {
        return;
    }

    const int originalLeft = columnSizes->at(0);
    const int clampedLeft = qBound(kLeftColumnMinWidth, originalLeft, kLeftColumnMaxWidth);
    if (clampedLeft == originalLeft) {
        return;
    }

    (*columnSizes)[0] = clampedLeft;
    (*columnSizes)[1] += originalLeft - clampedLeft;
}

PanelLayoutSettings PanelLayoutSettings::defaults()
{
    return PanelLayoutSettings {};
}

} // namespace qtcode::settings

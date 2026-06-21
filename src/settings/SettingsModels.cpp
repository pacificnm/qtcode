#include "settings/SettingsModels.h"

#include <QJsonArray>

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

} // namespace

QJsonObject PanelLayoutSettings::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("columnSizes"), sizesToJsonArray(columnSizes));
    json.insert(QStringLiteral("horizontalSizes"), sizesToJsonArray(horizontalSizes));
    json.insert(QStringLiteral("verticalSizes"), sizesToJsonArray(verticalSizes));
    json.insert(QStringLiteral("windowWidth"), windowWidth);
    json.insert(QStringLiteral("windowHeight"), windowHeight);
    return json;
}

PanelLayoutSettings PanelLayoutSettings::fromJson(const QJsonObject &json)
{
    const PanelLayoutSettings defaults = PanelLayoutSettings::defaults();
    PanelLayoutSettings layout = defaults;

    if (json.contains(QStringLiteral("columnSizes"))) {
        layout.columnSizes = jsonArrayToSizes(
            json.value(QStringLiteral("columnSizes")).toArray(),
            defaults.columnSizes);
    }

    if (json.contains(QStringLiteral("horizontalSizes"))) {
        layout.horizontalSizes = jsonArrayToSizes(
            json.value(QStringLiteral("horizontalSizes")).toArray(),
            defaults.horizontalSizes);
    }

    if (json.contains(QStringLiteral("verticalSizes"))) {
        layout.verticalSizes = jsonArrayToSizes(
            json.value(QStringLiteral("verticalSizes")).toArray(),
            defaults.verticalSizes);
    }

    if (!json.contains(QStringLiteral("columnSizes")) && layout.horizontalSizes.size() >= 3) {
        layout.columnSizes = {layout.horizontalSizes.at(0), defaults.columnSizes.at(1)};
        layout.horizontalSizes = {layout.horizontalSizes.at(1), layout.horizontalSizes.at(2)};
    }

    if (json.contains(QStringLiteral("windowWidth"))) {
        layout.windowWidth = json.value(QStringLiteral("windowWidth")).toInt(defaults.windowWidth);
    }

    if (json.contains(QStringLiteral("windowHeight"))) {
        layout.windowHeight = json.value(QStringLiteral("windowHeight")).toInt(defaults.windowHeight);
    }

    return layout;
}

PanelLayoutSettings PanelLayoutSettings::defaults()
{
    return PanelLayoutSettings {};
}

} // namespace qtcode::settings

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace qtcode::settings {

inline constexpr auto kPanelLayoutSettingKey = "app.panel_layout";

struct PanelLayoutSettings
{
    QList<int> horizontalSizes {280, 640, 320};
    QList<int> verticalSizes {560, 240};
    int windowWidth = 1280;
    int windowHeight = 800;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static PanelLayoutSettings fromJson(const QJsonObject &json);
    [[nodiscard]] static PanelLayoutSettings defaults();
};

} // namespace qtcode::settings

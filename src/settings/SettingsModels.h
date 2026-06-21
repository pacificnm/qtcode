#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace qtcode::settings {

inline constexpr auto kPanelLayoutSettingKey = "app.panel_layout";

struct PanelLayoutSettings
{
    QList<int> columnSizes {280, 480, 0, 320};
    QList<int> verticalSizes {560, 240};
    bool agentPanelVisible = true;
    bool contextPanelVisible = false;
    bool changesPanelVisible = false;
    int windowWidth = 1280;
    int windowHeight = 800;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static PanelLayoutSettings fromJson(const QJsonObject &json);
    [[nodiscard]] static PanelLayoutSettings defaults();
};

} // namespace qtcode::settings

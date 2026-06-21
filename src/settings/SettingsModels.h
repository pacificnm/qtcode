#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace qtcode::settings {

inline constexpr auto kPanelLayoutSettingKey = "app.panel_layout";

inline constexpr auto kPanelLayoutSchemaVersion = 3;

inline constexpr auto kRightPanelNone = "none";
inline constexpr auto kRightPanelSessions = "sessions";
inline constexpr auto kRightPanelContext = "context";
inline constexpr auto kRightPanelMcp = "mcp";

struct PanelLayoutSettings
{
    QList<int> columnSizes {280, 640, 320};
    QList<int> verticalSizes {560, 240};
    QString activeRightPanel = QString::fromLatin1(kRightPanelSessions);
    int windowWidth = 1280;
    int windowHeight = 800;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static PanelLayoutSettings fromJson(const QJsonObject &json);
    [[nodiscard]] static PanelLayoutSettings defaults();
};

} // namespace qtcode::settings

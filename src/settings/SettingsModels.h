#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace qtcode::settings {

inline constexpr auto kPanelLayoutSettingKey = "app.panel_layout";

inline constexpr auto kPanelLayoutSchemaVersion = 5;

inline constexpr auto kRightPanelNone = "none";
inline constexpr auto kRightPanelSessions = "sessions";
inline constexpr auto kRightPanelContext = "context";
inline constexpr auto kRightPanelMcp = "mcp";

inline constexpr int kLeftColumnDefaultWidth = 240;
inline constexpr int kLeftColumnMinWidth = 200;
inline constexpr int kLeftColumnMaxWidth = 320;

struct PanelLayoutSettings
{
    QList<int> columnSizes {kLeftColumnDefaultWidth, 640, 320};
    QList<int> verticalSizes {560, 240};
    QString activeRightPanel = QString::fromLatin1(kRightPanelSessions);
    bool rightColumnCollapsed = false;
    int storedRightColumnWidth = 320;
    int windowWidth = 1280;
    int windowHeight = 800;
    bool terminalCollapsed = false;
    int storedTerminalHeight = 240;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static PanelLayoutSettings fromJson(const QJsonObject &json);
    [[nodiscard]] static PanelLayoutSettings defaults();
    static void clampLeftColumnWidth(QList<int> *columnSizes);
};

} // namespace qtcode::settings

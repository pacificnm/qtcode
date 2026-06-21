#include "settings/AppConfig.h"
#include "settings/SettingsModels.h"

namespace qtcode::settings {

AppConfig AppConfig::defaults()
{
    AppConfig config;
    config.leftPanelWidth = kLeftColumnDefaultWidth;
    config.rightPanelWidth = kRightColumnDefaultWidth;
    return config;
}

} // namespace qtcode::settings

#pragma once

#include "settings/AppConfig.h"

#include <QString>

namespace qtcode::core {

class AppConfigService
{
public:
    AppConfigService();

    [[nodiscard]] const settings::AppConfig &config() const;
    [[nodiscard]] settings::AppConfig defaultConfig() const;

    [[nodiscard]] bool load(QString *errorMessage = nullptr);
    [[nodiscard]] bool save(const settings::AppConfig &config, QString *errorMessage = nullptr);

private:
    settings::AppConfig m_config;
};

} // namespace qtcode::core

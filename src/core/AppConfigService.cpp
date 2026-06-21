#include "core/AppConfigService.h"

#include "shared/Logging.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace qtcode::core {

namespace {

constexpr auto kConfigFileName = "qtcoderc";

} // namespace

AppConfigService::AppConfigService()
    : m_config(settings::AppConfig::defaults())
{
}

const settings::AppConfig &AppConfigService::config() const
{
    return m_config;
}

settings::AppConfig AppConfigService::defaultConfig() const
{
    return settings::AppConfig::defaults();
}

bool AppConfigService::load(QString *errorMessage)
{
    m_config = defaultConfig();

    const KSharedConfig::Ptr config = KSharedConfig::openConfig(QString::fromLatin1(kConfigFileName));
    const KConfigGroup group(config, QString::fromLatin1(settings::kAppConfigGroupGeneral));

    m_config.restoreLastProjectOnStartup = group.readEntry(
        QString::fromLatin1(settings::kAppConfigKeyRestoreLastProject),
        m_config.restoreLastProjectOnStartup);
    m_config.startMaximized = group.readEntry(
        QString::fromLatin1(settings::kAppConfigKeyStartMaximized),
        m_config.startMaximized);

    qCInfo(qtcodeCore) << "Loaded application config from" << config->name();
    return true;
}

bool AppConfigService::save(const settings::AppConfig &config, QString *errorMessage)
{
    const KSharedConfig::Ptr sharedConfig = KSharedConfig::openConfig(QString::fromLatin1(kConfigFileName));
    KConfigGroup group(sharedConfig, QString::fromLatin1(settings::kAppConfigGroupGeneral));

    group.writeEntry(
        QString::fromLatin1(settings::kAppConfigKeyRestoreLastProject),
        config.restoreLastProjectOnStartup);
    group.writeEntry(QString::fromLatin1(settings::kAppConfigKeyStartMaximized), config.startMaximized);

    if (!sharedConfig->sync()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to save application config file.");
        }
        qCWarning(qtcodeCore) << "Failed to save application config file";
        return false;
    }

    m_config = config;
    qCInfo(qtcodeCore) << "Saved application config to" << sharedConfig->name();
    return true;
}

} // namespace qtcode::core

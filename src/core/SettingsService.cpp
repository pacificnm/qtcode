#include "core/SettingsService.h"

#include "shared/Logging.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/StorageService.h"

namespace qtcode::core {

SettingsService::SettingsService(storage::StorageService &storageService)
    : m_storageService(storageService)
{
}

settings::PanelLayoutSettings SettingsService::defaultPanelLayout() const
{
    return settings::PanelLayoutSettings::defaults();
}

bool SettingsService::loadPanelLayout(
    settings::PanelLayoutSettings *layout,
    QString *errorMessage) const
{
    if (layout == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Panel layout output pointer is null.");
        }
        return false;
    }

    storage::SettingsRepository repository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!repository.loadJson(settings::kPanelLayoutSettingKey, &json, &found, errorMessage)) {
        return false;
    }

    if (!found) {
        *layout = defaultPanelLayout();
        qCInfo(qtcodeCore) << "Using default panel layout settings";
        return true;
    }

    *layout = settings::PanelLayoutSettings::fromJson(json);
    qCInfo(qtcodeCore) << "Loaded panel layout settings";
    return true;
}

bool SettingsService::savePanelLayout(
    const settings::PanelLayoutSettings &layout,
    QString *errorMessage)
{
    storage::SettingsRepository repository(m_storageService);
    if (!repository.upsertJson(settings::kPanelLayoutSettingKey, layout.toJson(), errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to save panel layout settings";
        return false;
    }

    qCInfo(qtcodeCore) << "Saved panel layout settings";
    return true;
}

} // namespace qtcode::core

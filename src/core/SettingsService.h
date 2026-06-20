#pragma once

#include "settings/SettingsModels.h"

#include <QString>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class SettingsService
{
public:
    explicit SettingsService(storage::StorageService &storageService);

    [[nodiscard]] settings::PanelLayoutSettings defaultPanelLayout() const;
    [[nodiscard]] bool loadPanelLayout(
        settings::PanelLayoutSettings *layout,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool savePanelLayout(
        const settings::PanelLayoutSettings &layout,
        QString *errorMessage = nullptr);

private:
    storage::StorageService &m_storageService;
};

} // namespace qtcode::core

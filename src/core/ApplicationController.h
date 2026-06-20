#pragma once

#include <QString>
#include <memory>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class SettingsService;
class ProjectManager;

class ApplicationController
{
public:
    ApplicationController();
    ~ApplicationController();

    ApplicationController(const ApplicationController &) = delete;
    ApplicationController &operator=(const ApplicationController &) = delete;

    [[nodiscard]] bool initialize(QString *errorMessage = nullptr);
    void shutdown();

    [[nodiscard]] storage::StorageService *storageService() const;
    [[nodiscard]] SettingsService *settingsService() const;
    [[nodiscard]] ProjectManager *projectManager() const;

private:
    std::unique_ptr<storage::StorageService> m_storageService;
    std::unique_ptr<SettingsService> m_settingsService;
    std::unique_ptr<ProjectManager> m_projectManager;
};

} // namespace qtcode::core

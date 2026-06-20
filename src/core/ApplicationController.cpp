#include "core/ApplicationController.h"

#include "core/ProjectManager.h"
#include "core/SettingsService.h"
#include "git/GitService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"

#include <QByteArray>

namespace qtcode::core {

ApplicationController::ApplicationController() = default;

ApplicationController::~ApplicationController()
{
    shutdown();
}

bool ApplicationController::initialize(QString *errorMessage)
{
    if (m_storageService != nullptr && m_storageService->isOpen()) {
        return true;
    }

    m_storageService = std::make_unique<storage::StorageService>();
    if (!m_storageService->open(errorMessage)) {
        qCWarning(qtcodeCore) << "StorageService failed to open";
        m_storageService.reset();
        return false;
    }

    storage::MigrationRunner migrationRunner(*m_storageService);
    if (!migrationRunner.runPendingMigrations(errorMessage)) {
        qCWarning(qtcodeCore) << "Database migrations failed";
        m_storageService->close();
        m_storageService.reset();
        return false;
    }

    m_settingsService = std::make_unique<SettingsService>(*m_storageService);
    m_gitService = std::make_unique<git::GitService>();
    m_projectManager = std::make_unique<ProjectManager>(*m_storageService, *m_gitService);

    if (!m_projectManager->restoreState(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to restore project state:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    if (const QByteArray addRepositoryPath = qgetenv("QTCODE_ADD_REPOSITORY"); !addRepositoryPath.isEmpty()) {
        settings::ProjectRecord project;
        if (!m_projectManager->addLocalRepository(QString::fromUtf8(addRepositoryPath), &project, errorMessage)) {
            qCWarning(qtcodeCore) << "Failed to add repository from QTCODE_ADD_REPOSITORY:"
                                  << (errorMessage != nullptr ? *errorMessage : QString());
            return false;
        }
    }

    qCInfo(qtcodeCore) << "Application services initialized";
    return true;
}

void ApplicationController::shutdown()
{
    m_projectManager.reset();
    m_gitService.reset();
    m_settingsService.reset();

    if (m_storageService == nullptr) {
        return;
    }

    m_storageService->close();
    m_storageService.reset();
    qCInfo(qtcodeCore) << "Application services shut down";
}

storage::StorageService *ApplicationController::storageService() const
{
    return m_storageService.get();
}

SettingsService *ApplicationController::settingsService() const
{
    return m_settingsService.get();
}

ProjectManager *ApplicationController::projectManager() const
{
    return m_projectManager.get();
}

git::GitService *ApplicationController::gitService() const
{
    return m_gitService.get();
}

} // namespace qtcode::core

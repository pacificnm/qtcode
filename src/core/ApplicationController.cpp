#include "core/ApplicationController.h"

#include "shared/Logging.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"

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

    qCInfo(qtcodeCore) << "Application services initialized";
    return true;
}

void ApplicationController::shutdown()
{
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

} // namespace qtcode::core

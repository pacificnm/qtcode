#pragma once

#include <QString>
#include <memory>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

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

private:
    std::unique_ptr<storage::StorageService> m_storageService;
};

} // namespace qtcode::core

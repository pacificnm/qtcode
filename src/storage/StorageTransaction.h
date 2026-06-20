#pragma once

#include "storage/StorageService.h"

namespace qtcode::storage {

class StorageTransaction
{
public:
    explicit StorageTransaction(StorageService &service);
    ~StorageTransaction();

    StorageTransaction(const StorageTransaction &) = delete;
    StorageTransaction &operator=(const StorageTransaction &) = delete;
    StorageTransaction(StorageTransaction &&other) noexcept;
    StorageTransaction &operator=(StorageTransaction &&other) noexcept;

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool commit(QString *errorMessage = nullptr);
    void rollback() noexcept;

private:
    StorageService *m_service = nullptr;
    bool m_active = false;
};

} // namespace qtcode::storage

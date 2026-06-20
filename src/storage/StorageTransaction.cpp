#include "storage/StorageTransaction.h"

namespace qtcode::storage {

StorageTransaction::StorageTransaction(StorageService &service)
    : m_service(&service)
    , m_active(service.beginTransaction())
{
}

StorageTransaction::~StorageTransaction()
{
    if (m_active && m_service != nullptr) {
        if (!m_service->rollbackTransaction()) {
            // Best-effort rollback during transaction cleanup.
        }
    }
}

StorageTransaction::StorageTransaction(StorageTransaction &&other) noexcept
    : m_service(other.m_service)
    , m_active(other.m_active)
{
    other.m_service = nullptr;
    other.m_active = false;
}

StorageTransaction &StorageTransaction::operator=(StorageTransaction &&other) noexcept
{
    if (this != &other) {
        if (m_active && m_service != nullptr) {
            if (!m_service->rollbackTransaction()) {
                // Best-effort rollback during transaction move assignment.
            }
        }

        m_service = other.m_service;
        m_active = other.m_active;

        other.m_service = nullptr;
        other.m_active = false;
    }

    return *this;
}

bool StorageTransaction::isActive() const
{
    return m_active;
}

bool StorageTransaction::commit(QString *errorMessage)
{
    if (!m_active || m_service == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No active storage transaction to commit.");
        }
        return false;
    }

    if (!m_service->commitTransaction(errorMessage)) {
        m_active = false;
        m_service = nullptr;
        return false;
    }

    m_active = false;
    m_service = nullptr;
    return true;
}

void StorageTransaction::rollback() noexcept
{
    if (!m_active || m_service == nullptr) {
        return;
    }

    if (!m_service->rollbackTransaction()) {
        // Best-effort rollback during explicit transaction cleanup.
    }
    m_active = false;
    m_service = nullptr;
}

} // namespace qtcode::storage

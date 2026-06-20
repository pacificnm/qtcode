#pragma once

#include "terminal/TerminalProfile.h"

#include <QString>

namespace qtcode::storage {

class StorageService;

class TerminalProfileStore
{
public:
    explicit TerminalProfileStore(StorageService &storageService);

    [[nodiscard]] bool loadGlobalProfile(
        terminal::TerminalProfile *profile,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool saveGlobalProfile(
        const terminal::TerminalProfile &profile,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool loadProjectProfile(
        const QString &projectId,
        terminal::TerminalProfile *profile,
        bool *found,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool saveProjectProfile(
        const QString &projectId,
        const terminal::TerminalProfile &profile,
        QString *errorMessage = nullptr);

private:
    [[nodiscard]] static QString projectProfileSettingKey(const QString &projectId);

    StorageService &m_storageService;
};

} // namespace qtcode::storage

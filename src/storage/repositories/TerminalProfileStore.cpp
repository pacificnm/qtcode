#include "storage/repositories/TerminalProfileStore.h"

#include "shared/Logging.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/StorageService.h"
#include "terminal/TerminalProfile.h"

namespace qtcode::storage {

TerminalProfileStore::TerminalProfileStore(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool TerminalProfileStore::loadGlobalProfile(
    terminal::TerminalProfile *profile,
    QString *errorMessage) const
{
    if (profile == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal profile output must not be null.");
        }
        return false;
    }

    *profile = terminal::TerminalProfile::defaults();

    SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!settingsRepository.loadJson(
            terminal::kGlobalTerminalProfileSettingKey,
            &json,
            &found,
            errorMessage)) {
        return false;
    }

    if (found) {
        *profile = terminal::TerminalProfile::fromJson(json);
        return true;
    }

    if (!settingsRepository.loadJson(terminal::kDefaultShellSettingKey, &json, &found, errorMessage)) {
        return false;
    }

    if (found) {
        profile->shellPath = json.value(QStringLiteral("path")).toString();
    }

    return true;
}

bool TerminalProfileStore::saveGlobalProfile(
    const terminal::TerminalProfile &profile,
    QString *errorMessage)
{
    SettingsRepository settingsRepository(m_storageService);
    if (!settingsRepository.upsertJson(
            terminal::kGlobalTerminalProfileSettingKey,
            profile.toJson(),
            errorMessage)) {
        return false;
    }

    qCInfo(qtcodeStorage) << "Saved global terminal profile with shell"
                           << profile.shellPath << "and working directory mode"
                           << terminal::workingDirectoryModeToString(profile.workingDirectoryMode);
    return true;
}

bool TerminalProfileStore::loadProjectProfile(
    const QString &projectId,
    terminal::TerminalProfile *profile,
    bool *found,
    QString *errorMessage) const
{
    if (profile == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal profile output must not be null.");
        }
        return false;
    }

    if (found != nullptr) {
        *found = false;
    }

    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project id must not be empty.");
        }
        return false;
    }

    SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool profileFound = false;
    if (!settingsRepository.loadJson(
            projectProfileSettingKey(projectId),
            &json,
            &profileFound,
            errorMessage)) {
        return false;
    }

    if (profileFound) {
        *profile = terminal::TerminalProfile::fromJson(json);
    } else {
        *profile = terminal::TerminalProfile::defaults();
    }

    if (found != nullptr) {
        *found = profileFound;
    }

    return true;
}

bool TerminalProfileStore::saveProjectProfile(
    const QString &projectId,
    const terminal::TerminalProfile &profile,
    QString *errorMessage)
{
    if (projectId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project id must not be empty.");
        }
        return false;
    }

    SettingsRepository settingsRepository(m_storageService);
    if (!settingsRepository.upsertJson(
            projectProfileSettingKey(projectId),
            profile.toJson(),
            errorMessage)) {
        return false;
    }

    qCInfo(qtcodeStorage) << "Saved project terminal profile for" << projectId;
    return true;
}

QString TerminalProfileStore::projectProfileSettingKey(const QString &projectId)
{
    return QStringLiteral("%1%2").arg(terminal::kProjectTerminalProfileSettingKeyPrefix, projectId);
}

} // namespace qtcode::storage

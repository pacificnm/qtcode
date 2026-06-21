#pragma once

#include <QString>

namespace qtcode::shared {

class SecretStore
{
public:
    [[nodiscard]] static QString walletKeyForServer(
        const QString &serverId,
        const QString &secretName);
    [[nodiscard]] static bool isWalletIntegrationAvailable();
    [[nodiscard]] static bool hasServerSecret(
        const QString &serverId,
        const QString &secretName);

    [[nodiscard]] static bool storeServerSecret(
        const QString &serverId,
        const QString &secretName,
        const QString &secretValue,
        QString *errorMessage = nullptr);
    [[nodiscard]] static bool loadServerSecret(
        const QString &serverId,
        const QString &secretName,
        QString *secretValue,
        QString *errorMessage = nullptr);
    [[nodiscard]] static bool deleteServerSecret(
        const QString &serverId,
        const QString &secretName,
        QString *errorMessage = nullptr);
};

} // namespace qtcode::shared

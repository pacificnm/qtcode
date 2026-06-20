#include "shared/SecretStore.h"

#include "shared/Logging.h"

namespace qtcode::shared {

QString SecretStore::walletKeyForServer(const QString &serverId, const QString &secretName)
{
    return QStringLiteral("mcp/%1/%2").arg(serverId, secretName);
}

bool SecretStore::isWalletIntegrationAvailable()
{
    return false;
}

bool SecretStore::storeServerSecret(
    const QString &serverId,
    const QString &secretName,
    const QString &secretValue,
    QString *errorMessage)
{
    Q_UNUSED(secretValue);

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral(
            "KDE Wallet integration is not available yet. "
            "Store secrets for MCP server '%1' (%2) outside SQLite for now.")
                            .arg(serverId, secretName);
    }

    qCInfo(qtcodeMemory) << "SecretStore wallet write deferred for" << walletKeyForServer(serverId, secretName);
    return false;
}

bool SecretStore::loadServerSecret(
    const QString &serverId,
    const QString &secretName,
    QString *secretValue,
    QString *errorMessage)
{
    if (secretValue != nullptr) {
        secretValue->clear();
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral(
            "KDE Wallet integration is not available yet for MCP server '%1' (%2).")
                            .arg(serverId, secretName);
    }

    return false;
}

} // namespace qtcode::shared

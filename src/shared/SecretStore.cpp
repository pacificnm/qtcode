#include "shared/SecretStore.h"

#include "shared/Logging.h"

#include <KWallet>

#include <memory>

namespace qtcode::shared {

namespace {

constexpr auto kWalletFolder = "QTCode";

struct WalletDeleter
{
    void operator()(KWallet::Wallet *wallet) const
    {
        delete wallet;
    }
};

using WalletPointer = std::unique_ptr<KWallet::Wallet, WalletDeleter>;

WalletPointer openQtCodeWallet(QString *errorMessage)
{
    if (!KWallet::Wallet::isEnabled()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("KDE Wallet is disabled on this system.");
        }
        return WalletPointer(nullptr);
    }

    KWallet::Wallet *rawWallet = KWallet::Wallet::openWallet(
        KWallet::Wallet::NetworkWallet(),
        0,
        KWallet::Wallet::Synchronous);
    if (rawWallet == nullptr || !rawWallet->isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to open KDE Wallet.");
        }
        delete rawWallet;
        return WalletPointer(nullptr);
    }

    WalletPointer wallet(rawWallet);
    if (!wallet->hasFolder(QString::fromUtf8(kWalletFolder))) {
        if (!wallet->createFolder(QString::fromUtf8(kWalletFolder))) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to create KDE Wallet folder '%1'.").arg(
                    QString::fromUtf8(kWalletFolder));
            }
            return WalletPointer(nullptr);
        }
    }

    if (!wallet->setFolder(QString::fromUtf8(kWalletFolder))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to select KDE Wallet folder '%1'.").arg(
                QString::fromUtf8(kWalletFolder));
        }
        return WalletPointer(nullptr);
    }

    return wallet;
}

} // namespace

QString SecretStore::walletKeyForServer(const QString &serverId, const QString &secretName)
{
    return QStringLiteral("mcp/%1/%2").arg(serverId, secretName);
}

bool SecretStore::isWalletIntegrationAvailable()
{
    return KWallet::Wallet::isEnabled();
}

bool SecretStore::hasServerSecret(const QString &serverId, const QString &secretName)
{
    QString errorMessage;
    const WalletPointer wallet = openQtCodeWallet(&errorMessage);
    if (!wallet) {
        return false;
    }

    return wallet->hasEntry(walletKeyForServer(serverId, secretName));
}

bool SecretStore::storeServerSecret(
    const QString &serverId,
    const QString &secretName,
    const QString &secretValue,
    QString *errorMessage)
{
    QString walletError;
    const WalletPointer wallet = openQtCodeWallet(&walletError);
    if (!wallet) {
        if (errorMessage != nullptr) {
            *errorMessage = walletError;
        }
        return false;
    }

    const QString walletKey = walletKeyForServer(serverId, secretName);
    if (wallet->writePassword(walletKey, secretValue) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Failed to store secret '%1' in KDE Wallet for MCP server '%2'.")
                                .arg(secretName, serverId);
        }
        return false;
    }

    if (wallet->sync() != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Stored secret '%1' but failed to sync KDE Wallet for MCP server '%2'.")
                                .arg(secretName, serverId);
        }
        return false;
    }

    qCInfo(qtcodeMemory) << "Stored MCP secret in KDE Wallet:" << walletKey;
    return true;
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

    QString walletError;
    const WalletPointer wallet = openQtCodeWallet(&walletError);
    if (!wallet) {
        if (errorMessage != nullptr) {
            *errorMessage = walletError;
        }
        return false;
    }

    QString loadedValue;
    const QString walletKey = walletKeyForServer(serverId, secretName);
    if (wallet->readPassword(walletKey, loadedValue) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Secret '%1' is not stored in KDE Wallet for MCP server '%2'.")
                                .arg(secretName, serverId);
        }
        return false;
    }

    if (secretValue != nullptr) {
        *secretValue = loadedValue;
    }

    return true;
}

bool SecretStore::deleteServerSecret(
    const QString &serverId,
    const QString &secretName,
    QString *errorMessage)
{
    QString walletError;
    const WalletPointer wallet = openQtCodeWallet(&walletError);
    if (!wallet) {
        if (errorMessage != nullptr) {
            *errorMessage = walletError;
        }
        return false;
    }

    const QString walletKey = walletKeyForServer(serverId, secretName);
    if (!wallet->hasEntry(walletKey)) {
        return true;
    }

    if (wallet->removeEntry(walletKey) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Failed to remove secret '%1' from KDE Wallet for MCP server '%2'.")
                                .arg(secretName, serverId);
        }
        return false;
    }

    wallet->sync();
    qCInfo(qtcodeMemory) << "Removed MCP secret from KDE Wallet:" << walletKey;
    return true;
}

} // namespace qtcode::shared

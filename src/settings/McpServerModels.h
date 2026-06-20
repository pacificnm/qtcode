#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace qtcode::settings {

struct McpServerRecord
{
    QString id;
    QString name;
    QString endpoint;
    QString transport;
    QJsonObject configJson;
    bool enabled = true;
    QString createdAt;
    QString updatedAt;

    [[nodiscard]] QStringList args() const;
    [[nodiscard]] QStringList secretEnvKeys() const;
    void setArgs(const QStringList &args);
    void setSecretEnvKeys(const QStringList &secretEnvKeys);

    [[nodiscard]] static McpServerRecord defaultQtcodeMemoryServer();
};

} // namespace qtcode::settings

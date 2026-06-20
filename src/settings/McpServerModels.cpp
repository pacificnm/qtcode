#include "settings/McpServerModels.h"

#include <QJsonArray>

namespace qtcode::settings {

namespace {

constexpr auto kArgsKey = "args";
constexpr auto kSecretEnvKeysKey = "secret_env_keys";

QStringList readStringList(const QJsonObject &object, const char *key)
{
    QStringList values;
    const QJsonValue jsonValue = object.value(QString::fromUtf8(key));
    if (!jsonValue.isArray()) {
        return values;
    }

    for (const QJsonValue &entry : jsonValue.toArray()) {
        const QString text = entry.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }

    return values;
}

} // namespace

QStringList McpServerRecord::args() const
{
    return readStringList(configJson, kArgsKey);
}

QStringList McpServerRecord::secretEnvKeys() const
{
    return readStringList(configJson, kSecretEnvKeysKey);
}

void McpServerRecord::setArgs(const QStringList &args)
{
    QJsonArray array;
    for (const QString &arg : args) {
        const QString trimmed = arg.trimmed();
        if (!trimmed.isEmpty()) {
            array.append(trimmed);
        }
    }
    configJson.insert(QString::fromUtf8(kArgsKey), array);
}

void McpServerRecord::setSecretEnvKeys(const QStringList &secretEnvKeys)
{
    QJsonArray array;
    for (const QString &key : secretEnvKeys) {
        const QString trimmed = key.trimmed();
        if (!trimmed.isEmpty()) {
            array.append(trimmed);
        }
    }
    configJson.insert(QString::fromUtf8(kSecretEnvKeysKey), array);
}

McpServerRecord McpServerRecord::defaultQtcodeMemoryServer()
{
    McpServerRecord server;
    server.name = QStringLiteral("qtcode-memory");
    server.endpoint = QStringLiteral("bash");
    server.transport = QStringLiteral("stdio");
    server.enabled = true;
    server.setArgs({QStringLiteral("scripts/run-memory-mcp")});
    server.setSecretEnvKeys(
        {QStringLiteral("OPENAI_API_KEY"), QStringLiteral("DATABASE_URL")});
    return server;
}

} // namespace qtcode::settings

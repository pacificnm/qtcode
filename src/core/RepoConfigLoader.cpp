#include "core/RepoConfigLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace qtcode::core {

namespace {

enum class ParseSection
{
    None,
    Help,
    Agent,
    Project,
};

[[nodiscard]] QString parseYamlScalarValue(const QString &line, const QString &key)
{
    const QString prefix = key + QStringLiteral(":");
    if (!line.trimmed().startsWith(prefix)) {
        return {};
    }

    QString value = line.trimmed().mid(prefix.size()).trimmed();
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"'))
            || (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.mid(1, value.size() - 2);
        }
    }

    return value.trimmed();
}

[[nodiscard]] bool isTopLevelLine(const QString &line)
{
    return !line.isEmpty() && line.at(0) != QLatin1Char(' ') && line.at(0) != QLatin1Char('\t');
}

} // namespace

settings::RepoConfig RepoConfigLoader::loadFromProjectRoot(const QString &projectRootPath)
{
    if (projectRootPath.trimmed().isEmpty()) {
        return settings::RepoConfig::empty();
    }

    const QString configPath =
        QDir(projectRootPath).absoluteFilePath(QString::fromLatin1(settings::kRepoConfigRelativePath));
    if (!QFileInfo::exists(configPath)) {
        return settings::RepoConfig::empty();
    }

    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return settings::RepoConfig::empty();
    }

    return loadFromYamlContent(QString::fromUtf8(configFile.readAll()));
}

settings::RepoConfig RepoConfigLoader::loadFromYamlContent(const QString &yamlContent)
{
    settings::RepoConfig config;
    ParseSection section = ParseSection::None;

    const QStringList lines = yamlContent.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (isTopLevelLine(line)) {
            if (trimmed == QStringLiteral("help:")) {
                section = ParseSection::Help;
                continue;
            }
            if (trimmed == QStringLiteral("agent:")) {
                section = ParseSection::Agent;
                continue;
            }
            if (trimmed == QStringLiteral("project:")) {
                section = ParseSection::Project;
                continue;
            }

            section = ParseSection::None;
            const QString repoHelpPath = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyRepoHelpPath));
            if (!repoHelpPath.isEmpty()) {
                config.repoHelpPath = repoHelpPath;
            }

            const QString defaultAgentKey = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyDefaultAgentKey));
            if (!defaultAgentKey.isEmpty()) {
                config.defaultAgentKey = defaultAgentKey;
            }
            continue;
        }

        if (section == ParseSection::Help) {
            const QString entryPath = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyHelpEntryPath));
            if (!entryPath.isEmpty()) {
                config.repoHelpPath = entryPath;
            }
            continue;
        }

        if (section == ParseSection::Agent) {
            const QString defaultAgentKey = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyDefaultAgentKey));
            if (!defaultAgentKey.isEmpty()) {
                config.defaultAgentKey = defaultAgentKey;
            }
            continue;
        }

        if (section == ParseSection::Project) {
            const QString displayName = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyProjectDisplayName));
            if (!displayName.isEmpty()) {
                config.projectDisplayName = displayName;
            }

            const QString projectPath = parseYamlScalarValue(
                line,
                QString::fromLatin1(settings::kRepoConfigKeyProjectPath));
            if (!projectPath.isEmpty()) {
                config.projectPath = projectPath;
            }
        }
    }

    return config;
}

} // namespace qtcode::core

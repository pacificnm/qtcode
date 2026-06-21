#include "core/RepoConfigWriter.h"

#include "core/RepoConfigLoader.h"
#include "settings/RepoConfig.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace qtcode::core {

namespace {

[[nodiscard]] QString configFilePath(const QString &projectRootPath)
{
    return QDir(projectRootPath).absoluteFilePath(
        QString::fromLatin1(qtcode::settings::kRepoConfigRelativePath));
}

[[nodiscard]] QString yamlContentForEmptyConfig()
{
    return QStringLiteral(
        "# Per-repository QTCode settings.\n"
        "# Values here override system defaults from File > Settings.\n"
        "#\n"
        "# agent:\n"
        "#   defaultAgentKey: codex\n"
        "#\n"
        "# help:\n"
        "#   entryPath: docs/README.md\n"
        "#\n"
        "# Alternatively:\n"
        "# repoHelpPath: docs/README.md\n"
        "# defaultAgentKey: codex\n");
}

[[nodiscard]] QString yamlContentForRepoConfig(const qtcode::settings::RepoConfig &config)
{
    if (!config.hasRepoHelpPath() && !config.hasDefaultAgentKey()) {
        return yamlContentForEmptyConfig();
    }

    QStringList lines;
    lines << QStringLiteral("# Per-repository QTCode settings.");

    if (config.hasDefaultAgentKey()) {
        lines << QStringLiteral("agent:");
        lines << QStringLiteral("  defaultAgentKey: %1").arg(config.defaultAgentKey.trimmed());
    }

    if (config.hasRepoHelpPath()) {
        const QString normalized =
            qtcode::settings::normalizedRepoHelpPath(config.repoHelpPath);
        lines << QStringLiteral("help:");
        lines << QStringLiteral("  entryPath: %1").arg(normalized);
    }

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace

bool RepoConfigWriter::save(
    const QString &projectRootPath,
    const qtcode::settings::RepoConfig &config,
    QString *errorMessage)
{
    if (projectRootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Project root path is empty.");
        }
        return false;
    }

    const QFileInfo projectRootInfo(projectRootPath);
    if (!projectRootInfo.exists() || !projectRootInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Project root is not a directory: %1").arg(projectRootPath);
        }
        return false;
    }

    QDir projectRoot(projectRootPath);
    if (!projectRoot.exists(QStringLiteral(".qtcode")) && !projectRoot.mkpath(QStringLiteral(".qtcode"))) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not create .qtcode directory under %1.").arg(projectRootPath);
        }
        return false;
    }

    QSaveFile configFile(configFilePath(projectRootPath));
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not open repository config for writing: %1")
                                 .arg(configFile.errorString());
        }
        return false;
    }

    const QString yamlContent = yamlContentForRepoConfig(config);
    if (configFile.write(yamlContent.toUtf8()) != yamlContent.toUtf8().size()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not write repository config: %1").arg(configFile.errorString());
        }
        return false;
    }

    if (!configFile.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not save repository config: %1").arg(configFile.errorString());
        }
        return false;
    }

    return true;
}

bool RepoConfigWriter::saveRepoHelpPath(
    const QString &projectRootPath,
    const QString &repoHelpPath,
    QString *errorMessage)
{
    qtcode::settings::RepoConfig config =
        RepoConfigLoader::loadFromProjectRoot(projectRootPath);
    config.repoHelpPath = repoHelpPath.trimmed().isEmpty() ? QString() : repoHelpPath.trimmed();
    return save(projectRootPath, config, errorMessage);
}

} // namespace qtcode::core

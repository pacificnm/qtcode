#include "core/RepoConfigWriter.h"

#include "settings/RepoConfig.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace qtcode::core {

namespace {

[[nodiscard]] QString configFilePath(const QString &projectRootPath)
{
    return QDir(projectRootPath).absoluteFilePath(
        QString::fromLatin1(qtcode::settings::kRepoConfigRelativePath));
}

[[nodiscard]] QString yamlContentForRepoHelpPath(const QString &repoHelpPath)
{
    if (repoHelpPath.trimmed().isEmpty()) {
        return QStringLiteral(
            "# Per-repository QTCode settings.\n"
            "# Values here override system defaults from File > Settings.\n"
            "#\n"
            "# help:\n"
            "#   entryPath: docs/README.md\n"
            "#\n"
            "# Alternatively:\n"
            "# repoHelpPath: docs/README.md\n");
    }

    const QString normalized = qtcode::settings::normalizedRepoHelpPath(repoHelpPath);
    return QStringLiteral(
               "# Per-repository QTCode settings.\n"
               "help:\n"
               "  entryPath: %1\n")
        .arg(normalized);
}

} // namespace

bool RepoConfigWriter::saveRepoHelpPath(
    const QString &projectRootPath,
    const QString &repoHelpPath,
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

    const QString yamlContent = yamlContentForRepoHelpPath(repoHelpPath);
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

} // namespace qtcode::core

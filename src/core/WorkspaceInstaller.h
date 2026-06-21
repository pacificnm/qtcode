#pragma once

#include "core/WorkspaceModels.h"

#include <QHash>
#include <QObject>

namespace qtcode::core {

class WorkspaceInstaller final : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceInstaller(QObject *parent = nullptr);

    [[nodiscard]] static QString workspaceManifestPath(const QString &rootPath);
    [[nodiscard]] static QString bundleRootPath();
    [[nodiscard]] static QString resolveBundleSourcePath(const QString &relativePath);
    [[nodiscard]] bool isInstalled(const QString &rootPath) const;
    [[nodiscard]] QList<WorkspaceHealthCheck> evaluateHealth(const QString &rootPath) const;
    [[nodiscard]] bool install(
        const WorkspaceInstallContext &context,
        WorkspaceInstallResult *result,
        QString *errorMessage = nullptr);

signals:
    void workspaceInstalled(const QString &rootPath);

private:
    [[nodiscard]] static QString renderTemplate(
        const QString &content,
        const QHash<QString, QString> &variables);
    [[nodiscard]] bool writeRenderedTemplate(
        const QString &templateRelativePath,
        const QString &destinationPath,
        const QHash<QString, QString> &variables,
        bool executable,
        WorkspaceInstallResult *result,
        QString *errorMessage) const;
    [[nodiscard]] bool copyStaticFile(
        const QString &sourceRelativePath,
        const QString &destinationPath,
        bool executable,
        WorkspaceInstallResult *result,
        QString *errorMessage) const;
    [[nodiscard]] bool ensureParentDirectory(
        const QString &destinationPath,
        QString *errorMessage) const;
    [[nodiscard]] static QHash<QString, QString> variablesForContext(
        const WorkspaceInstallContext &context);
};

[[nodiscard]] QString workspaceHealthStateLabel(WorkspaceHealthState state);

} // namespace qtcode::core

#include "core/WorkspaceInstaller.h"

#include "shared/Logging.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace qtcode::core {

namespace {

constexpr auto kManifestRelativePath = ".qtcode/workspace.yaml";
constexpr auto kTemplateVersion = 1;

QString readTextFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not read template file: %1").arg(path);
        }
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

bool writeTextFile(
    const QString &path,
    const QString &content,
    bool executable,
    QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not write file: %1").arg(path);
        }
        return false;
    }

    if (file.write(content.toUtf8()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not write file: %1").arg(path);
        }
        return false;
    }

    file.close();

    if (executable) {
        QFile::setPermissions(
            path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
                | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    }

    return true;
}

WorkspaceHealthCheck makeCheck(
    const QString &id,
    const QString &label,
    WorkspaceHealthState state,
    const QString &message,
    const QString &fixHint = {})
{
    WorkspaceHealthCheck check;
    check.id = id;
    check.label = label;
    check.state = state;
    check.message = message;
    check.fixHint = fixHint;
    return check;
}

} // namespace

WorkspaceInstaller::WorkspaceInstaller(QObject *parent)
    : QObject(parent)
{
}

QString WorkspaceInstaller::workspaceManifestPath(const QString &rootPath)
{
    return QDir(rootPath).filePath(QString::fromUtf8(kManifestRelativePath));
}

QString WorkspaceInstaller::bundleRootPath()
{
    const QByteArray overridePath = qgetenv("QTCODE_WORKSPACE_BUNDLE");
    if (!overridePath.isEmpty()) {
        return QDir::cleanPath(QString::fromUtf8(overridePath));
    }

#ifdef QTcode_SOURCE_DIR
    const QString sourceRoot = QString::fromUtf8(QTcode_SOURCE_DIR);
    if (QDir(sourceRoot).exists()) {
        return QDir::cleanPath(sourceRoot);
    }
#endif

    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(applicationDir).filePath(QStringLiteral("../share/qtcode/workspace-bundle")),
        QDir(applicationDir).filePath(QStringLiteral("../resources/workspace-bundle")),
    };

    for (const QString &candidate : candidates) {
        if (QDir(candidate).exists()) {
            return QDir::cleanPath(candidate);
        }
    }

    return QDir::cleanPath(applicationDir);
}

QString WorkspaceInstaller::resolveBundleSourcePath(const QString &relativePath)
{
    const QString bundleRoot = bundleRootPath();
    const QString normalizedRelative = QDir::fromNativeSeparators(relativePath);

    const QStringList candidates = {
        QDir(bundleRoot).filePath(normalizedRelative),
        QDir(bundleRoot).filePath(QStringLiteral("resources/workspace-bundle/") + normalizedRelative),
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }

    return QDir(bundleRoot).filePath(normalizedRelative);
}

bool WorkspaceInstaller::isInstalled(const QString &rootPath) const
{
    return QFileInfo::exists(workspaceManifestPath(rootPath));
}

QList<WorkspaceHealthCheck> WorkspaceInstaller::evaluateHealth(const QString &rootPath) const
{
    QList<WorkspaceHealthCheck> checks;
    const QString cleanedRoot = QDir::cleanPath(rootPath);

    if (cleanedRoot.isEmpty() || !QDir(cleanedRoot).exists()) {
        checks.append(makeCheck(
            QStringLiteral("root"),
            i18n("Project root"),
            WorkspaceHealthState::Error,
            i18n("Project root is unavailable.")));
        return checks;
    }

    const bool manifestExists = isInstalled(cleanedRoot);
    checks.append(makeCheck(
        QStringLiteral("manifest"),
        i18n("Workspace manifest"),
        manifestExists ? WorkspaceHealthState::Ok : WorkspaceHealthState::Error,
        manifestExists ? i18n("Found .qtcode/workspace.yaml.")
                       : i18n("Missing .qtcode/workspace.yaml."),
        manifestExists ? QString() : i18n("Use Set up QTCode workspace in the Repository panel.")));

    const auto scriptExists = [&cleanedRoot](const QString &relativePath) {
        return QFileInfo::exists(QDir(cleanedRoot).filePath(relativePath));
    };

    const bool scriptsReady = scriptExists(QStringLiteral("scripts/index-memory"))
        && scriptExists(QStringLiteral("scripts/run-memory-mcp"))
        && scriptExists(QStringLiteral("scripts/search-memory"));
    checks.append(makeCheck(
        QStringLiteral("scripts"),
        i18n("Memory scripts"),
        scriptsReady ? WorkspaceHealthState::Ok : WorkspaceHealthState::Error,
        scriptsReady ? i18n("Memory wrapper scripts are installed.")
                     : i18n("Memory wrapper scripts are missing."),
        scriptsReady ? QString() : i18n("Run workspace setup to install scripts/.")));

    const bool toolsReady = scriptExists(QStringLiteral("tools/index_memory.py"))
        && scriptExists(QStringLiteral("tools/mcp_memory_server.py"));
    checks.append(makeCheck(
        QStringLiteral("tools"),
        i18n("Python memory tools"),
        toolsReady ? WorkspaceHealthState::Ok : WorkspaceHealthState::Error,
        toolsReady ? i18n("Python memory tools are installed.")
                   : i18n("Python memory tools are missing."),
        toolsReady ? QString() : i18n("Run workspace setup to copy tools/.")));

    const bool venvReady = scriptExists(QStringLiteral(".venv/bin/python"));
    checks.append(makeCheck(
        QStringLiteral("venv"),
        i18n("Python virtual environment"),
        venvReady ? WorkspaceHealthState::Ok : WorkspaceHealthState::Warning,
        venvReady ? i18n("Found .venv/bin/python.")
                  : i18n("No project virtual environment yet."),
        venvReady ? QString() : i18n("Run: python3 -m venv .venv && .venv/bin/pip install openai psycopg \"mcp[cli]\"")));

    const bool cursorConfigured = scriptExists(QStringLiteral(".cursor/mcp.json"));
    checks.append(makeCheck(
        QStringLiteral("cursor"),
        i18n("Cursor MCP config"),
        cursorConfigured ? WorkspaceHealthState::Ok : WorkspaceHealthState::Warning,
        cursorConfigured ? i18n("Found .cursor/mcp.json.")
                         : i18n("Cursor MCP config is missing."),
        cursorConfigured ? QString() : i18n("Run workspace setup to install .cursor/mcp.json.")));

    return checks;
}

bool WorkspaceInstaller::install(
    const WorkspaceInstallContext &context,
    WorkspaceInstallResult *result,
    QString *errorMessage)
{
    if (result == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Install result must not be null.");
        }
        return false;
    }

    result->createdPaths.clear();
    result->skippedPaths.clear();
    result->success = false;

    const QString rootPath = QDir::cleanPath(context.rootPath);
    if (rootPath.isEmpty() || !QDir(rootPath).exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Project root does not exist.");
        }
        return false;
    }

    const QString bundleRoot = bundleRootPath();
    if (!QDir(bundleRoot).exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Workspace template bundle is unavailable at %1.").arg(bundleRoot);
        }
        return false;
    }

    const QHash<QString, QString> variables = variablesForContext(context);

    struct InstallEntry
    {
        QString sourceRelativePath;
        QString destinationRelativePath;
        bool templated = false;
        bool executable = false;
    };

    const QList<InstallEntry> entries = {
        {QStringLiteral("templates/workspace.yaml.tpl"),
            QString::fromUtf8(kManifestRelativePath),
            true,
            false},
        {QStringLiteral("templates/config.yaml.tpl"),
            QStringLiteral(".qtcode/config.yaml"),
            true,
            false},
        {QStringLiteral("templates/env.example.tpl"),
            QStringLiteral(".env.example"),
            true,
            false},
        {QStringLiteral("static/.cursor/mcp.json"),
            QStringLiteral(".cursor/mcp.json"),
            false,
            false},
        {QStringLiteral("static/.cursor/hooks.json"),
            QStringLiteral(".cursor/hooks.json"),
            false,
            false},
        {QStringLiteral("static/.cursor/rules/memory-workflow.mdc"),
            QStringLiteral(".cursor/rules/memory-workflow.mdc"),
            false,
            false},
        {QStringLiteral("static/.cursor/hooks/memory-workflow.py"),
            QStringLiteral(".cursor/hooks/memory-workflow.py"),
            false,
            true},
        {QStringLiteral("scripts/index-memory"),
            QStringLiteral("scripts/index-memory"),
            false,
            true},
        {QStringLiteral("scripts/search-memory"),
            QStringLiteral("scripts/search-memory"),
            false,
            true},
        {QStringLiteral("scripts/search-agent-context"),
            QStringLiteral("scripts/search-agent-context"),
            false,
            true},
        {QStringLiteral("scripts/save-agent-context"),
            QStringLiteral("scripts/save-agent-context"),
            false,
            true},
        {QStringLiteral("scripts/run-memory-mcp"),
            QStringLiteral("scripts/run-memory-mcp"),
            false,
            true},
        {QStringLiteral("scripts/setup-memory-db"),
            QStringLiteral("scripts/setup-memory-db"),
            false,
            true},
        {QStringLiteral("tools/index_memory.py"),
            QStringLiteral("tools/index_memory.py"),
            false,
            true},
        {QStringLiteral("tools/search_memory.py"),
            QStringLiteral("tools/search_memory.py"),
            false,
            true},
        {QStringLiteral("tools/search_agent_context.py"),
            QStringLiteral("tools/search_agent_context.py"),
            false,
            true},
        {QStringLiteral("tools/save_agent_context.py"),
            QStringLiteral("tools/save_agent_context.py"),
            false,
            true},
        {QStringLiteral("tools/mcp_memory_server.py"),
            QStringLiteral("tools/mcp_memory_server.py"),
            false,
            true},
        {QStringLiteral("tools/memory_common.py"),
            QStringLiteral("tools/memory_common.py"),
            false,
            false},
        {QStringLiteral("tools/memory_retrieval.py"),
            QStringLiteral("tools/memory_retrieval.py"),
            false,
            false},
        {QStringLiteral("tools/agent_context.py"),
            QStringLiteral("tools/agent_context.py"),
            false,
            false},
    };

    for (const InstallEntry &entry : entries) {
        const QString destinationPath = QDir(rootPath).filePath(entry.destinationRelativePath);
        if (QFileInfo::exists(destinationPath)) {
            result->skippedPaths.append(destinationPath);
            continue;
        }

        const QString sourcePath = resolveBundleSourcePath(entry.sourceRelativePath);
        const bool written = entry.templated
            ? writeRenderedTemplate(
                  sourcePath,
                  destinationPath,
                  variables,
                  entry.executable,
                  result,
                  errorMessage)
            : copyStaticFile(
                  sourcePath,
                  destinationPath,
                  entry.executable,
                  result,
                  errorMessage);

        if (!written) {
            return false;
        }
    }

    result->success = true;
    result->message = result->createdPaths.isEmpty()
        ? i18n("Workspace files were already present.")
        : i18n(
              "Installed %1 workspace file(s). Next, create .venv and run scripts/setup-memory-db if needed.",
              result->createdPaths.size());

    qCInfo(qtcodeCore) << "Installed QTCode workspace files in" << rootPath
                       << "created" << result->createdPaths.size()
                       << "skipped" << result->skippedPaths.size();

    emit workspaceInstalled(rootPath);
    return true;
}

QString WorkspaceInstaller::renderTemplate(
    const QString &content,
    const QHash<QString, QString> &variables)
{
    QString rendered = content;
    for (auto it = variables.cbegin(); it != variables.cend(); ++it) {
        rendered.replace(
            QStringLiteral("{{%1}}").arg(it.key()),
            it.value(),
            Qt::CaseSensitive);
    }

    static const QRegularExpression leftoverTokenPattern(QStringLiteral("\\{\\{[A-Z0-9_]+\\}\\}"));
    rendered.replace(leftoverTokenPattern, QString());
    return rendered;
}

bool WorkspaceInstaller::writeRenderedTemplate(
    const QString &templatePath,
    const QString &destinationPath,
    const QHash<QString, QString> &variables,
    bool executable,
    WorkspaceInstallResult *result,
    QString *errorMessage) const
{
    QString readError;
    const QString templateContent = readTextFile(templatePath, &readError);
    if (templateContent.isEmpty() && !QFileInfo::exists(templatePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = readError;
        }
        return false;
    }

    if (!ensureParentDirectory(destinationPath, errorMessage)) {
        return false;
    }

    const QString rendered = renderTemplate(templateContent, variables);
    if (!writeTextFile(destinationPath, rendered, executable, errorMessage)) {
        return false;
    }

    result->createdPaths.append(destinationPath);
    return true;
}

bool WorkspaceInstaller::copyStaticFile(
    const QString &sourcePath,
    const QString &destinationPath,
    bool executable,
    WorkspaceInstallResult *result,
    QString *errorMessage) const
{
    if (!QFileInfo::exists(sourcePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Missing workspace bundle file: %1").arg(sourcePath);
        }
        return false;
    }

    if (!ensureParentDirectory(destinationPath, errorMessage)) {
        return false;
    }

    if (QFileInfo::exists(destinationPath)) {
        result->skippedPaths.append(destinationPath);
        return true;
    }

    if (QFile::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }

    if (!QFile::copy(sourcePath, destinationPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not copy %1 to %2.").arg(sourcePath, destinationPath);
        }
        return false;
    }

    if (executable) {
        QFile::setPermissions(
            destinationPath,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
                | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    }

    result->createdPaths.append(destinationPath);
    return true;
}

bool WorkspaceInstaller::ensureParentDirectory(const QString &destinationPath, QString *errorMessage) const
{
    const QFileInfo destinationInfo(destinationPath);
    QDir parentDir = destinationInfo.dir();
    if (parentDir.exists()) {
        return true;
    }

    if (!parentDir.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("Could not create directory: %1").arg(parentDir.path());
        }
        return false;
    }

    return true;
}

QHash<QString, QString> WorkspaceInstaller::variablesForContext(const WorkspaceInstallContext &context)
{
    QHash<QString, QString> variables;
    variables.insert(QStringLiteral("PROJECT_NAME"), context.projectName);
    variables.insert(QStringLiteral("ROOT_PATH"), context.rootPath);
    variables.insert(QStringLiteral("SCOPE_KEY"), context.scopeKey);
    variables.insert(QStringLiteral("DATABASE_URL"), context.databaseUrl);
    variables.insert(
        QStringLiteral("INSTALLED_AT"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    variables.insert(QStringLiteral("TEMPLATE_VERSION"), QString::number(kTemplateVersion));
    return variables;
}

QString workspaceHealthStateLabel(WorkspaceHealthState state)
{
    switch (state) {
    case WorkspaceHealthState::Ok:
        return i18n("OK");
    case WorkspaceHealthState::Warning:
        return i18n("Needs attention");
    case WorkspaceHealthState::Error:
        return i18n("Missing");
    case WorkspaceHealthState::Unknown:
        return i18n("Unknown");
    }

    return i18n("Unknown");
}

} // namespace qtcode::core

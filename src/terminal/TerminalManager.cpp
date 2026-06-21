#include "terminal/TerminalManager.h"

#include "core/RepoConfigLoader.h"
#include "settings/ProjectModels.h"
#include "settings/RepoConfig.h"
#include "shared/Logging.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/TerminalProfileStore.h"
#include "storage/repositories/TerminalSessionRepository.h"
#include "storage/StorageService.h"
#include "terminal/TerminalProfile.h"

#include <qtermwidget.h>

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QMenu>
#include <QTimer>
#include <QUuid>

#include <algorithm>

#include <signal.h>

namespace {

constexpr auto kTerminalWorkingDirectoryProperty = "qtcode_working_directory";

[[nodiscard]] QString normalizedWorkingDirectory(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    if (info.exists()) {
        return QDir::cleanPath(info.canonicalFilePath());
    }

    return QDir::cleanPath(path);
}

[[nodiscard]] bool restartShellInDirectory(
    QTermWidget *widget,
    const QString &workingDirectory,
    QString *errorMessage)
{
    if (widget == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal widget must not be null.");
        }
        return false;
    }

    const QString targetDirectory = normalizedWorkingDirectory(workingDirectory);
    if (targetDirectory.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Working directory must not be empty.");
        }
        return false;
    }

    widget->setWorkingDirectory(targetDirectory);

    const int shellPid = widget->getShellPID();
    if (shellPid <= 0) {
        widget->startShellProgram();
        if (widget->getShellPID() <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Terminal shell did not start.");
            }
            return false;
        }
        widget->setProperty(kTerminalWorkingDirectoryProperty, targetDirectory);
        return true;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(widget, &QTermWidget::finished, &loop, &QEventLoop::quit);
    timeout.start();
    ::kill(shellPid, SIGHUP);
    loop.exec();

    widget->startShellProgram();
    if (widget->getShellPID() <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal shell did not restart.");
        }
        return false;
    }

    widget->setProperty(kTerminalWorkingDirectoryProperty, targetDirectory);
    return true;
}

} // namespace

namespace {

void applyDefaultColorScheme(QTermWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    const QString preferredScheme = QString::fromLatin1(qtcode::terminal::kDefaultTerminalColorScheme);
    const QStringList availableSchemes = QTermWidget::availableColorSchemes();
    if (availableSchemes.contains(preferredScheme)) {
        widget->setColorScheme(preferredScheme);
        return;
    }

    static const QStringList fallbackSchemes = {
        QStringLiteral("Linux"),
        QStringLiteral("GreenOnBlack"),
    };
    for (const QString &fallbackScheme : fallbackSchemes) {
        if (availableSchemes.contains(fallbackScheme)) {
            qCWarning(qtcodeTerminal) << "Preferred terminal color scheme" << preferredScheme
                                      << "is unavailable; using" << fallbackScheme;
            widget->setColorScheme(fallbackScheme);
            return;
        }
    }

    qCWarning(qtcodeTerminal) << "No bundled terminal color scheme found; using QTermWidget default";
}

void installTerminalContextMenu(QTermWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *copyAction =
        new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Copy"), widget);
    copyAction->setEnabled(false);
    QObject::connect(copyAction, &QAction::triggered, widget, &QTermWidget::copyClipboard);
    QObject::connect(widget, &QTermWidget::copyAvailable, copyAction, &QAction::setEnabled);

    auto *pasteAction =
        new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), QStringLiteral("Paste"), widget);
    QObject::connect(pasteAction, &QAction::triggered, widget, &QTermWidget::pasteClipboard);

    auto *pasteSelectionAction = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-paste")), QStringLiteral("Paste Selection"), widget);
    QObject::connect(
        pasteSelectionAction, &QAction::triggered, widget, &QTermWidget::pasteSelection);

    auto *clearAction = new QAction(QStringLiteral("Clear"), widget);
    QObject::connect(clearAction, &QAction::triggered, widget, &QTermWidget::clear);

    QObject::connect(
        widget,
        &QWidget::customContextMenuRequested,
        widget,
        [widget, copyAction, pasteAction, pasteSelectionAction, clearAction](const QPoint &position) {
            // Refresh at show time: right-click can emit copyAvailable(false) before the menu opens.
            copyAction->setEnabled(!widget->selectedText().isEmpty());

            QMenu menu(widget);
            menu.addAction(copyAction);
            menu.addAction(pasteAction);
            menu.addAction(pasteSelectionAction);
            menu.addSeparator();
            menu.addAction(clearAction);

            menu.exec(widget->mapToGlobal(position));
        });
}

} // namespace

namespace qtcode::terminal {

TerminalManager::TerminalManager(storage::StorageService &storageService, QObject *parent)
    : QObject(parent)
    , m_storageService(storageService)
{
}

bool TerminalManager::restoreState(QString *errorMessage)
{
    if (!loadGlobalProfile(errorMessage)) {
        return false;
    }

    if (!loadSessions(errorMessage)) {
        return false;
    }

    qCInfo(qtcodeTerminal) << "Restored" << m_sessions.size() << "terminal session(s) with shell"
                           << resolveShellPath() << "and working directory mode"
                           << workingDirectoryModeToString(m_globalProfile.workingDirectoryMode);
    return true;
}

bool TerminalManager::setDefaultShellPath(const QString &shellPath, QString *errorMessage)
{
    TerminalProfile profile = m_globalProfile;
    profile.shellPath = shellPath.trimmed().isEmpty() ? QString() : shellPath.trimmed();

    if (!profile.shellPath.isEmpty()) {
        const QFileInfo shellInfo(profile.shellPath);
        if (!shellInfo.exists() || !shellInfo.isExecutable()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Shell path is not executable: %1").arg(profile.shellPath);
            }
            return false;
        }
        profile.shellPath = shellInfo.canonicalFilePath();
    }

    return setGlobalProfile(profile, errorMessage);
}

QString TerminalManager::defaultShellPath() const
{
    return m_globalProfile.shellPath;
}

QString TerminalManager::resolveShellPath() const
{
    return terminal::resolveShellPath(m_globalProfile.shellPath);
}

TerminalProfile TerminalManager::globalProfile() const
{
    return m_globalProfile;
}

bool TerminalManager::setGlobalProfile(const TerminalProfile &profile, QString *errorMessage)
{
    TerminalProfile validatedProfile = profile;

    if (!validatedProfile.shellPath.trimmed().isEmpty()) {
        const QFileInfo shellInfo(validatedProfile.shellPath);
        if (!shellInfo.exists() || !shellInfo.isExecutable()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    QStringLiteral("Shell path is not executable: %1").arg(validatedProfile.shellPath);
            }
            return false;
        }
        validatedProfile.shellPath = shellInfo.canonicalFilePath();
    } else {
        validatedProfile.shellPath.clear();
    }

    if (validatedProfile.workingDirectoryMode == WorkingDirectoryMode::CustomPath) {
        const QFileInfo workingDirectoryInfo(validatedProfile.customWorkingDirectory);
        if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Custom working directory does not exist: %1")
                                    .arg(validatedProfile.customWorkingDirectory);
            }
            return false;
        }
        validatedProfile.customWorkingDirectory = QDir::cleanPath(workingDirectoryInfo.canonicalFilePath());
    }

    storage::TerminalProfileStore profileStore(m_storageService);
    if (!profileStore.saveGlobalProfile(validatedProfile, errorMessage)) {
        return false;
    }

    m_globalProfile = validatedProfile;
    emit profilesChanged();

    qCInfo(qtcodeTerminal) << "Configured global terminal profile with shell"
                           << resolveShellPath() << "and working directory mode"
                           << workingDirectoryModeToString(m_globalProfile.workingDirectoryMode);
    return true;
}

bool TerminalManager::projectProfile(
    const QString &projectId,
    TerminalProfile *profile,
    bool *found,
    QString *errorMessage) const
{
    storage::TerminalProfileStore profileStore(m_storageService);
    return profileStore.loadProjectProfile(projectId, profile, found, errorMessage);
}

bool TerminalManager::setProjectProfile(
    const QString &projectId,
    const TerminalProfile &profile,
    QString *errorMessage)
{
    TerminalProfile validatedProfile = profile;

    if (!validatedProfile.shellPath.trimmed().isEmpty()) {
        const QFileInfo shellInfo(validatedProfile.shellPath);
        if (!shellInfo.exists() || !shellInfo.isExecutable()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    QStringLiteral("Shell path is not executable: %1").arg(validatedProfile.shellPath);
            }
            return false;
        }
        validatedProfile.shellPath = shellInfo.canonicalFilePath();
    }

    if (validatedProfile.workingDirectoryMode == WorkingDirectoryMode::CustomPath) {
        const QFileInfo workingDirectoryInfo(validatedProfile.customWorkingDirectory);
        if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Custom working directory does not exist: %1")
                                    .arg(validatedProfile.customWorkingDirectory);
            }
            return false;
        }
        validatedProfile.customWorkingDirectory = QDir::cleanPath(workingDirectoryInfo.canonicalFilePath());
    }

    storage::TerminalProfileStore profileStore(m_storageService);
    if (!profileStore.saveProjectProfile(projectId, validatedProfile, errorMessage)) {
        return false;
    }

    emit profilesChanged();

    qCInfo(qtcodeTerminal) << "Configured project terminal profile for" << projectId
                           << "with working directory mode"
                           << workingDirectoryModeToString(validatedProfile.workingDirectoryMode);
    return true;
}

TerminalProfile TerminalManager::effectiveProfile(const QString &projectId) const
{
    TerminalProfile effectiveProfile = m_globalProfile;

    if (projectId.isEmpty()) {
        return effectiveProfile;
    }

    storage::TerminalProfileStore profileStore(m_storageService);
    TerminalProfile projectProfile;
    bool found = false;
    QString errorMessage;
    if (!profileStore.loadProjectProfile(projectId, &projectProfile, &found, &errorMessage)) {
        qCWarning(qtcodeTerminal) << "Failed to load project terminal profile:" << errorMessage;
        return effectiveProfile;
    }

    if (found) {
        if (projectProfile.hasShellOverride()) {
            effectiveProfile.shellPath = projectProfile.shellPath;
        }
        effectiveProfile.workingDirectoryMode = projectProfile.workingDirectoryMode;
        effectiveProfile.customWorkingDirectory = projectProfile.customWorkingDirectory;
    }

    return effectiveProfile;
}

bool TerminalManager::resolveProjectWorkingDirectory(
    const QString &projectId,
    QString *workingDirectory,
    QString *projectName,
    QString *errorMessage) const
{
    if (workingDirectory != nullptr) {
        workingDirectory->clear();
    }
    if (projectName != nullptr) {
        projectName->clear();
    }

    if (projectId.isEmpty()) {
        if (workingDirectory != nullptr) {
            *workingDirectory = QDir::homePath();
        }
        return true;
    }

    storage::ProjectRepository projectRepository(m_storageService);
    settings::ProjectRecord project;
    bool found = false;
    if (!projectRepository.findProjectById(projectId, &project, &found, errorMessage)) {
        return false;
    }

    if (!found) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project '%1' was not found.").arg(projectId);
        }
        return false;
    }

    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(project.rootPath);

    if (workingDirectory != nullptr) {
        const QString resolvedPath =
            qtcode::settings::effectiveProjectPath(project, repoConfig);
        if (resolvedPath.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Project path is empty for project '%1'.").arg(projectId);
            }
            return false;
        }

        const QFileInfo rootInfo(resolvedPath);
        if (!rootInfo.exists() || !rootInfo.isDir()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Project path does not exist: %1").arg(resolvedPath);
            }
            return false;
        }
        *workingDirectory = QDir::cleanPath(rootInfo.canonicalFilePath());
    }
    if (projectName != nullptr) {
        *projectName = qtcode::settings::effectiveProjectDisplayName(project, repoConfig);
    }

    return true;
}

bool TerminalManager::resolveWorkingDirectory(
    const TerminalProfile &profile,
    const QString &projectId,
    QString *workingDirectory,
    QString *errorMessage) const
{
    if (workingDirectory == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Working directory output must not be null.");
        }
        return false;
    }

    switch (profile.workingDirectoryMode) {
    case WorkingDirectoryMode::Home:
        *workingDirectory = QDir::homePath();
        return true;
    case WorkingDirectoryMode::CustomPath: {
        const QFileInfo workingDirectoryInfo(profile.customWorkingDirectory);
        if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Custom working directory does not exist: %1")
                                    .arg(profile.customWorkingDirectory);
            }
            return false;
        }
        *workingDirectory = QDir::cleanPath(workingDirectoryInfo.canonicalFilePath());
        return true;
    }
    case WorkingDirectoryMode::ProjectRoot:
    default:
        return resolveProjectWorkingDirectory(projectId, workingDirectory, nullptr, errorMessage);
    }
}

bool TerminalManager::createTerminal(
    const QString &projectId,
    QWidget *parent,
    TerminalSession *session,
    QWidget **widget,
    QString *errorMessage)
{
    if (parent == nullptr || session == nullptr || widget == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal creation inputs must not be null.");
        }
        return false;
    }

    *widget = nullptr;

    TerminalSession newSession = buildSessionForProject(projectId, errorMessage);
    if (newSession.id.isEmpty()) {
        return false;
    }

    auto *terminalWidget = new QTermWidget(0, parent);
    if (!configureWidget(terminalWidget, newSession)) {
        delete terminalWidget;
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal shell did not start for %1.").arg(newSession.shellPath);
        }
        return false;
    }

    if (!persistSession(newSession, errorMessage)) {
        delete terminalWidget;
        return false;
    }

    m_sessions.append(newSession);
    emit sessionsChanged();

    *session = newSession;
    *widget = terminalWidget;

    terminalWidget->setProperty("qtcode_session_id", newSession.id);

    qCInfo(qtcodeTerminal) << "Created terminal session" << newSession.id << "in"
                           << newSession.workingDirectory << "with shell" << newSession.shellPath;
    return true;
}

bool TerminalManager::resolveSessionWorkingDirectory(
    TerminalSession *session,
    QString *errorMessage) const
{
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session output must not be null.");
        }
        return false;
    }

    TerminalProfile profile = TerminalProfile::defaults();
    if (!session->profileJson.trimmed().isEmpty()) {
        const QJsonDocument profileDocument = QJsonDocument::fromJson(session->profileJson.toUtf8());
        if (profileDocument.isObject()) {
            profile = TerminalProfile::fromJson(profileDocument.object());
        }
    } else if (!session->projectId.isEmpty()) {
        profile = effectiveProfile(session->projectId);
    } else {
        profile = m_globalProfile;
    }

    if (!session->projectId.isEmpty()) {
        QString projectName;
        if (!resolveProjectWorkingDirectory(
                session->projectId,
                &session->workingDirectory,
                &projectName,
                errorMessage)) {
            return false;
        }
        if (!projectName.isEmpty()) {
            session->title = projectName;
        }
    } else if (!resolveWorkingDirectory(profile, session->projectId, &session->workingDirectory, errorMessage)) {
        return false;
    }

    const QFileInfo workingDirectoryInfo(session->workingDirectory);
    if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Working directory does not exist: %1")
                                .arg(session->workingDirectory);
        }
        return false;
    }

    session->workingDirectory = QDir::cleanPath(workingDirectoryInfo.canonicalFilePath());
    return true;
}

bool TerminalManager::syncSessionsToActiveProject(
    const QString &projectId,
    QString *errorMessage)
{
    if (projectId.isEmpty()) {
        return true;
    }

    QString workingDirectory;
    QString projectName;
    if (!resolveProjectWorkingDirectory(projectId, &workingDirectory, &projectName, errorMessage)) {
        return false;
    }

    bool changed = false;
    for (TerminalSession &session : m_sessions) {
        if (session.projectId == projectId && session.workingDirectory == workingDirectory
            && (projectName.isEmpty() || session.title == projectName)) {
            continue;
        }

        session.projectId = projectId;
        session.workingDirectory = workingDirectory;
        if (!projectName.isEmpty()) {
            session.title = projectName;
        }
        session.updatedAt = currentTimestamp();
        if (!updatePersistedSession(session, errorMessage)) {
            return false;
        }
        changed = true;
    }

    if (changed) {
        emit sessionsChanged();
    }

    return true;
}

bool TerminalManager::applySessionToWidget(
    QWidget *widget,
    const TerminalSession &session,
    QString *errorMessage) const
{
    if (widget == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal widget must not be null.");
        }
        return false;
    }

    auto *terminalWidget = qobject_cast<QTermWidget *>(widget);
    if (terminalWidget == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Widget is not a QTermWidget instance.");
        }
        return false;
    }

    const QString appliedDirectory =
        normalizedWorkingDirectory(terminalWidget->property(kTerminalWorkingDirectoryProperty).toString());
    const QString targetDirectory = normalizedWorkingDirectory(session.workingDirectory);
    if (!targetDirectory.isEmpty() && appliedDirectory == targetDirectory) {
        return true;
    }

    return restartShellInDirectory(terminalWidget, targetDirectory, errorMessage);
}

bool TerminalManager::restoreTerminal(
    const TerminalSession &session,
    QWidget *parent,
    QWidget **widget,
    QString *errorMessage)
{
    if (parent == nullptr || widget == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal restore inputs must not be null.");
        }
        return false;
    }

    *widget = nullptr;

    TerminalSession resolvedSession = session;
    if (!resolveSessionWorkingDirectory(&resolvedSession, errorMessage)) {
        return false;
    }

    auto *terminalWidget = new QTermWidget(0, parent);
    if (!configureWidget(terminalWidget, resolvedSession)) {
        delete terminalWidget;
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal shell did not start for %1.").arg(session.shellPath);
        }
        return false;
    }

    *widget = terminalWidget;

    terminalWidget->setProperty("qtcode_session_id", session.id);

    qCInfo(qtcodeTerminal) << "Restored terminal session" << resolvedSession.id << "in"
                           << resolvedSession.workingDirectory;
    return true;
}

QList<TerminalSession> TerminalManager::sessions() const
{
    return m_sessions;
}

bool TerminalManager::closeSession(const QString &sessionId, QString *errorMessage)
{
    if (sessionId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session id must not be empty.");
        }
        return false;
    }

    const auto sessionIt = std::find_if(
        m_sessions.begin(),
        m_sessions.end(),
        [&sessionId](const TerminalSession &session) { return session.id == sessionId; });
    if (sessionIt == m_sessions.end()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    if (!removePersistedSession(sessionId, errorMessage)) {
        return false;
    }

    m_sessions.erase(sessionIt);
    emit sessionsChanged();

    qCInfo(qtcodeTerminal) << "Closed terminal session" << sessionId;
    return true;
}

bool TerminalManager::loadGlobalProfile(QString *errorMessage)
{
    storage::TerminalProfileStore profileStore(m_storageService);
    if (!profileStore.loadGlobalProfile(&m_globalProfile, errorMessage)) {
        return false;
    }

    emit profilesChanged();
    return true;
}

bool TerminalManager::loadSessions(QString *errorMessage)
{
    storage::TerminalSessionRepository repository(m_storageService);
    if (!repository.listSessions(&m_sessions, errorMessage)) {
        return false;
    }

    emit sessionsChanged();
    return true;
}

bool TerminalManager::persistSession(const TerminalSession &session, QString *errorMessage)
{
    storage::TerminalSessionRepository repository(m_storageService);
    return repository.insertSession(session, errorMessage);
}

bool TerminalManager::updatePersistedSession(const TerminalSession &session, QString *errorMessage)
{
    storage::TerminalSessionRepository repository(m_storageService);
    return repository.updateSession(session, errorMessage);
}

bool TerminalManager::removePersistedSession(const QString &sessionId, QString *errorMessage)
{
    storage::TerminalSessionRepository repository(m_storageService);
    return repository.deleteSession(sessionId, errorMessage);
}

TerminalSession TerminalManager::buildSessionForProject(
    const QString &projectId,
    QString *errorMessage) const
{
    TerminalSession session;
    session.id = createId();
    session.projectId = projectId;
    session.createdAt = currentTimestamp();
    session.updatedAt = session.createdAt;

    const TerminalProfile profile = effectiveProfile(projectId);
    session.shellPath = terminal::resolveShellPath(profile.shellPath);
    session.profileJson = profileSnapshotJson(profile);

    QString projectName;
    if (projectId.isEmpty()) {
        if (!resolveWorkingDirectory(profile, projectId, &session.workingDirectory, errorMessage)) {
            return {};
        }
    } else if (!resolveProjectWorkingDirectory(
                   projectId,
                   &session.workingDirectory,
                   &projectName,
                   errorMessage)) {
        return {};
    }

    if (!projectName.isEmpty()) {
        session.title = projectName;
    } else {
        session.title = QStringLiteral("Terminal");
    }

    const QFileInfo workingDirectoryInfo(session.workingDirectory);
    if (!workingDirectoryInfo.exists() || !workingDirectoryInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Working directory does not exist: %1")
                                .arg(session.workingDirectory);
        }
        return {};
    }

    return session;
}

bool TerminalManager::configureWidget(QTermWidget *widget, const TerminalSession &session) const
{
    if (widget == nullptr) {
        return false;
    }

    applyDefaultColorScheme(widget);
    installTerminalContextMenu(widget);
    widget->setShellProgram(session.shellPath);
    const QString workingDirectory = normalizedWorkingDirectory(session.workingDirectory);
    widget->setWorkingDirectory(workingDirectory);
    widget->setTerminalSizeHint(true);
    widget->setScrollBarPosition(QTermWidget::ScrollBarRight);
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->startShellProgram();

    if (widget->getShellPID() <= 0) {
        return false;
    }

    widget->setProperty(kTerminalWorkingDirectoryProperty, workingDirectory);
    return true;
}

QString TerminalManager::currentTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString TerminalManager::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString TerminalManager::profileSnapshotJson(const TerminalProfile &profile)
{
    return QString::fromUtf8(
        QJsonDocument(profile.toJson()).toJson(QJsonDocument::Compact));
}

} // namespace qtcode::terminal

#include "terminal/TerminalManager.h"

#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/TerminalProfileStore.h"
#include "storage/repositories/TerminalSessionRepository.h"
#include "storage/StorageService.h"
#include "terminal/TerminalProfile.h"

#include <qtermwidget.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

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

    if (workingDirectory != nullptr) {
        *workingDirectory = project.rootPath;
    }
    if (projectName != nullptr) {
        *projectName = project.name;
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

    auto *terminalWidget = new QTermWidget(0, parent);
    if (!configureWidget(terminalWidget, session)) {
        delete terminalWidget;
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal shell did not start for %1.").arg(session.shellPath);
        }
        return false;
    }

    *widget = terminalWidget;

    terminalWidget->setProperty("qtcode_session_id", session.id);

    qCInfo(qtcodeTerminal) << "Restored terminal session" << session.id << "in"
                           << session.workingDirectory;
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
    if (!resolveWorkingDirectory(profile, projectId, &session.workingDirectory, errorMessage)) {
        return {};
    }

    if (!resolveProjectWorkingDirectory(projectId, nullptr, &projectName, errorMessage)) {
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

    widget->setShellProgram(session.shellPath);
    widget->setWorkingDirectory(session.workingDirectory);
    widget->setTerminalSizeHint(true);
    widget->setScrollBarPosition(QTermWidget::ScrollBarRight);
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->startShellProgram();

    return widget->getShellPID() > 0;
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

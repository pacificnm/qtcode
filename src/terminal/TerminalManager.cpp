#include "terminal/TerminalManager.h"

#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/repositories/ProjectRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/repositories/TerminalSessionRepository.h"
#include "storage/StorageService.h"
#include "terminal/TerminalProfile.h"

#include <qtermwidget.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QUuid>

namespace qtcode::terminal {

TerminalManager::TerminalManager(storage::StorageService &storageService, QObject *parent)
    : QObject(parent)
    , m_storageService(storageService)
{
}

bool TerminalManager::restoreState(QString *errorMessage)
{
    if (!loadConfiguredShellPath(errorMessage)) {
        return false;
    }

    if (!loadSessions(errorMessage)) {
        return false;
    }

    qCInfo(qtcodeTerminal) << "Restored" << m_sessions.size() << "terminal session(s) with shell"
                           << resolveShellPath();
    return true;
}

bool TerminalManager::setDefaultShellPath(const QString &shellPath, QString *errorMessage)
{
    const QString trimmedPath = shellPath.trimmed();
    if (trimmedPath.isEmpty()) {
        m_configuredShellPath.clear();
    } else {
        const QFileInfo shellInfo(trimmedPath);
        if (!shellInfo.exists() || !shellInfo.isExecutable()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Shell path is not executable: %1").arg(trimmedPath);
            }
            return false;
        }
        m_configuredShellPath = shellInfo.canonicalFilePath();
    }

    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    json.insert(QStringLiteral("path"), m_configuredShellPath);
    if (!settingsRepository.upsertJson(kDefaultShellSettingKey, json, errorMessage)) {
        return false;
    }

    qCInfo(qtcodeTerminal) << "Configured default shell path"
                           << (m_configuredShellPath.isEmpty() ? resolveShellPath() : m_configuredShellPath);
    return true;
}

QString TerminalManager::defaultShellPath() const
{
    return m_configuredShellPath;
}

QString TerminalManager::resolveShellPath() const
{
    return terminal::resolveShellPath(m_configuredShellPath);
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

    qCInfo(qtcodeTerminal) << "Restored terminal session" << session.id << "in"
                           << session.workingDirectory;
    return true;
}

QList<TerminalSession> TerminalManager::sessions() const
{
    return m_sessions;
}

bool TerminalManager::loadConfiguredShellPath(QString *errorMessage)
{
    storage::SettingsRepository settingsRepository(m_storageService);
    QJsonObject json;
    bool found = false;
    if (!settingsRepository.loadJson(kDefaultShellSettingKey, &json, &found, errorMessage)) {
        return false;
    }

    if (found) {
        m_configuredShellPath = json.value(QStringLiteral("path")).toString();
    } else {
        m_configuredShellPath.clear();
    }

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

TerminalSession TerminalManager::buildSessionForProject(
    const QString &projectId,
    QString *errorMessage) const
{
    TerminalSession session;
    session.id = createId();
    session.projectId = projectId;
    session.shellPath = resolveShellPath();
    session.createdAt = currentTimestamp();
    session.updatedAt = session.createdAt;

    QString projectName;
    if (!resolveProjectWorkingDirectory(
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

} // namespace qtcode::terminal

#pragma once

#include "terminal/TerminalSession.h"

#include "terminal/TerminalProfile.h"

#include <QList>
#include <QString>

#include <QObject>

class QTermWidget;
class QWidget;

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::terminal {

class TerminalManager final : public QObject
{
    Q_OBJECT

public:
    explicit TerminalManager(storage::StorageService &storageService, QObject *parent = nullptr);

    [[nodiscard]] bool restoreState(QString *errorMessage = nullptr);
    [[nodiscard]] bool setDefaultShellPath(const QString &shellPath, QString *errorMessage = nullptr);
    [[nodiscard]] QString defaultShellPath() const;
    [[nodiscard]] QString resolveShellPath() const;

    [[nodiscard]] TerminalProfile globalProfile() const;
    [[nodiscard]] bool setGlobalProfile(
        const TerminalProfile &profile,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool projectProfile(
        const QString &projectId,
        TerminalProfile *profile,
        bool *found,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool setProjectProfile(
        const QString &projectId,
        const TerminalProfile &profile,
        QString *errorMessage = nullptr);
    [[nodiscard]] TerminalProfile effectiveProfile(const QString &projectId) const;

    [[nodiscard]] bool resolveProjectWorkingDirectory(
        const QString &projectId,
        QString *workingDirectory,
        QString *projectName,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool resolveWorkingDirectory(
        const TerminalProfile &profile,
        const QString &projectId,
        QString *workingDirectory,
        QString *errorMessage = nullptr) const;

    [[nodiscard]] bool createTerminal(
        const QString &projectId,
        QWidget *parent,
        TerminalSession *session,
        QWidget **widget,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool restoreTerminal(
        const TerminalSession &session,
        QWidget *parent,
        QWidget **widget,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool closeSession(const QString &sessionId, QString *errorMessage = nullptr);

    [[nodiscard]] QList<TerminalSession> sessions() const;

signals:
    void sessionsChanged();
    void profilesChanged();

private:
    [[nodiscard]] bool loadGlobalProfile(QString *errorMessage);
    [[nodiscard]] bool loadSessions(QString *errorMessage);
    [[nodiscard]] bool persistSession(const TerminalSession &session, QString *errorMessage);
    [[nodiscard]] bool removePersistedSession(const QString &sessionId, QString *errorMessage);
    [[nodiscard]] TerminalSession buildSessionForProject(
        const QString &projectId,
        QString *errorMessage) const;
    [[nodiscard]] bool configureWidget(QTermWidget *widget, const TerminalSession &session) const;
    [[nodiscard]] static QString currentTimestamp();
    [[nodiscard]] static QString createId();
    [[nodiscard]] static QString profileSnapshotJson(const TerminalProfile &profile);

    storage::StorageService &m_storageService;
    TerminalProfile m_globalProfile;
    QList<TerminalSession> m_sessions;
};

} // namespace qtcode::terminal

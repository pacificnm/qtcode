#pragma once

#include "terminal/TerminalSession.h"

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

    [[nodiscard]] bool resolveProjectWorkingDirectory(
        const QString &projectId,
        QString *workingDirectory,
        QString *projectName,
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

    [[nodiscard]] QList<TerminalSession> sessions() const;

signals:
    void sessionsChanged();

private:
    [[nodiscard]] bool loadConfiguredShellPath(QString *errorMessage);
    [[nodiscard]] bool loadSessions(QString *errorMessage);
    [[nodiscard]] bool persistSession(const TerminalSession &session, QString *errorMessage);
    [[nodiscard]] TerminalSession buildSessionForProject(
        const QString &projectId,
        QString *errorMessage) const;
    [[nodiscard]] bool configureWidget(QTermWidget *widget, const TerminalSession &session) const;
    [[nodiscard]] static QString currentTimestamp();
    [[nodiscard]] static QString createId();

    storage::StorageService &m_storageService;
    QString m_configuredShellPath;
    QList<TerminalSession> m_sessions;
};

} // namespace qtcode::terminal

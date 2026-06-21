#include "storage/repositories/TerminalSessionRepository.h"

#include "shared/Logging.h"
#include "storage/StorageService.h"

#include <QSqlError>
#include <QSqlQuery>

namespace qtcode::storage {

namespace {

terminal::TerminalSession sessionFromQuery(const QSqlQuery &query)
{
    terminal::TerminalSession session;
    session.id = query.value(QStringLiteral("id")).toString();
    session.projectId = query.value(QStringLiteral("project_id")).toString();
    session.title = query.value(QStringLiteral("title")).toString();
    session.shellPath = query.value(QStringLiteral("shell_path")).toString();
    session.workingDirectory = query.value(QStringLiteral("working_directory")).toString();
    session.profileJson = query.value(QStringLiteral("profile_json")).toString();
    session.lastCommand = query.value(QStringLiteral("last_command")).toString();
    session.createdAt = query.value(QStringLiteral("created_at")).toString();
    session.updatedAt = query.value(QStringLiteral("updated_at")).toString();
    return session;
}

} // namespace

TerminalSessionRepository::TerminalSessionRepository(StorageService &storageService)
    : m_storageService(storageService)
{
}

bool TerminalSessionRepository::insertSession(
    const terminal::TerminalSession &session,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "INSERT INTO terminal_sessions ("
        "id, project_id, title, shell_path, working_directory, profile_json, last_command, "
        "created_at, updated_at) "
        "VALUES ("
        ":id, :project_id, :title, :shell_path, :working_directory, :profile_json, :last_command, "
        ":created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":id"), session.id);
    query.bindValue(QStringLiteral(":project_id"), session.projectId);
    query.bindValue(QStringLiteral(":title"), session.title);
    query.bindValue(QStringLiteral(":shell_path"), session.shellPath);
    query.bindValue(QStringLiteral(":working_directory"), session.workingDirectory);
    query.bindValue(QStringLiteral(":profile_json"), session.profileJson);
    query.bindValue(QStringLiteral(":last_command"), session.lastCommand);
    query.bindValue(QStringLiteral(":created_at"), session.createdAt);
    query.bindValue(QStringLiteral(":updated_at"), session.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to insert terminal session: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to insert terminal session" << message;
        return false;
    }

    return true;
}

bool TerminalSessionRepository::updateSession(
    const terminal::TerminalSession &session,
    QString *errorMessage)
{
    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral(
        "UPDATE terminal_sessions SET "
        "project_id = :project_id, "
        "title = :title, "
        "shell_path = :shell_path, "
        "working_directory = :working_directory, "
        "profile_json = :profile_json, "
        "last_command = :last_command, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), session.id);
    query.bindValue(QStringLiteral(":project_id"), session.projectId);
    query.bindValue(QStringLiteral(":title"), session.title);
    query.bindValue(QStringLiteral(":shell_path"), session.shellPath);
    query.bindValue(QStringLiteral(":working_directory"), session.workingDirectory);
    query.bindValue(QStringLiteral(":profile_json"), session.profileJson);
    query.bindValue(QStringLiteral(":last_command"), session.lastCommand);
    query.bindValue(QStringLiteral(":updated_at"), session.updatedAt);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to update terminal session: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to update terminal session" << message;
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool TerminalSessionRepository::listSessions(
    QList<terminal::TerminalSession> *sessions,
    QString *errorMessage) const
{
    if (sessions == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session list output must not be null.");
        }
        return false;
    }

    sessions->clear();

    QSqlQuery query(m_storageService.database());
    if (!query.exec(QStringLiteral(
            "SELECT id, project_id, title, shell_path, working_directory, profile_json, "
            "last_command, created_at, updated_at "
            "FROM terminal_sessions "
            "ORDER BY created_at ASC"))) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to list terminal sessions: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to list terminal sessions" << message;
        return false;
    }

    while (query.next()) {
        sessions->append(sessionFromQuery(query));
    }

    return true;
}

bool TerminalSessionRepository::deleteSession(const QString &sessionId, QString *errorMessage)
{
    if (sessionId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session id must not be empty.");
        }
        return false;
    }

    QSqlQuery query(m_storageService.database());
    query.prepare(QStringLiteral("DELETE FROM terminal_sessions WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), sessionId);

    if (!query.exec()) {
        const QString message = query.lastError().text();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to delete terminal session: %1").arg(message);
        }
        qCWarning(qtcodeStorage) << "Failed to delete terminal session" << message;
        return false;
    }

    if (query.numRowsAffected() == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Terminal session '%1' was not found.").arg(sessionId);
        }
        return false;
    }

    return true;
}

} // namespace qtcode::storage

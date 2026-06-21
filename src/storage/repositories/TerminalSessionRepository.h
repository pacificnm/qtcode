#pragma once

#include "terminal/TerminalSession.h"

#include <QList>
#include <QString>

namespace qtcode::storage {

class StorageService;

class TerminalSessionRepository
{
public:
    explicit TerminalSessionRepository(StorageService &storageService);

    [[nodiscard]] bool insertSession(
        const terminal::TerminalSession &session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool updateSession(
        const terminal::TerminalSession &session,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool listSessions(
        QList<terminal::TerminalSession> *sessions,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool deleteSession(const QString &sessionId, QString *errorMessage = nullptr);

private:
    StorageService &m_storageService;
};

} // namespace qtcode::storage

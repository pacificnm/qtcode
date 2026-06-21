#pragma once

#include "commands/QtCommandModels.h"

#include <QObject>
#include <QHash>
#include <optional>

namespace qtcode::commands {

class CommandLibraryService final : public QObject
{
    Q_OBJECT

public:
    explicit CommandLibraryService(QObject *parent = nullptr);

    void setProjectRoot(const QString &projectRoot);
    [[nodiscard]] QString projectRoot() const;

    [[nodiscard]] bool refresh();
    [[nodiscard]] bool hasCommandsDirectory() const;

    [[nodiscard]] QList<QtCommandIndexEntry> commands() const;
    [[nodiscard]] QList<QtCommandIndexEntry> search(const QString &query) const;
    [[nodiscard]] std::optional<QtCommandDefinition> loadCommand(const QString &nameOrAlias) const;
    [[nodiscard]] std::optional<QtCommandDefinition> loadCommandFromPath(const QString &sourcePath) const;
    [[nodiscard]] QList<QtCommandDiagnostic> diagnostics() const;

    [[nodiscard]] static QString commandsDirectoryForRoot(const QString &projectRoot);

signals:
    void commandsChanged();

private:
    [[nodiscard]] static QString normalizedLookupKey(const QString &value);
    [[nodiscard]] static bool entryMatchesQuery(
        const QtCommandIndexEntry &entry,
        const QStringList &tokens);
    [[nodiscard]] static int entrySearchRank(
        const QtCommandIndexEntry &entry,
        const QString &query);

    QString m_projectRoot;
    QList<QtCommandIndexEntry> m_commands;
    QList<QtCommandDiagnostic> m_diagnostics;
    QHash<QString, QString> m_lookupKeys;
};

} // namespace qtcode::commands

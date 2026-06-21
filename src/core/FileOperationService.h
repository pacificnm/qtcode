#pragma once

#include <QString>

namespace qtcode::core {

class FileOperationService final
{
public:
    [[nodiscard]] static bool isPathInsideProjectRoot(
        const QString &candidatePath,
        const QString &projectRoot,
        QString *errorMessage = nullptr);

    [[nodiscard]] bool createFile(
        const QString &projectRoot,
        const QString &parentDirectory,
        const QString &fileName,
        QString *createdPath,
        QString *errorMessage = nullptr) const;

    [[nodiscard]] bool createFolder(
        const QString &projectRoot,
        const QString &parentDirectory,
        const QString &folderName,
        QString *createdPath,
        QString *errorMessage = nullptr) const;

    [[nodiscard]] bool renameEntry(
        const QString &projectRoot,
        const QString &existingPath,
        const QString &newName,
        QString *renamedPath,
        QString *errorMessage = nullptr) const;

    [[nodiscard]] bool deleteEntry(
        const QString &projectRoot,
        const QString &targetPath,
        QString *errorMessage = nullptr) const;

private:
    [[nodiscard]] static bool isValidEntryName(const QString &name, QString *errorMessage);
    [[nodiscard]] static QString absolutePath(const QString &path);
    [[nodiscard]] static bool ensureExistingPathInsideRoot(
        const QString &existingPath,
        const QString &projectRoot,
        QString *errorMessage);
    [[nodiscard]] static bool ensureParentInsideRoot(
        const QString &parentDirectory,
        const QString &projectRoot,
        QString *errorMessage);
};

} // namespace qtcode::core

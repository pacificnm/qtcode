#include "core/FileOperationService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace qtcode::core {

namespace {

constexpr QLatin1Char kPathSeparator('/');

bool hasParentReference(const QString &name)
{
    return name == QStringLiteral("..") || name.contains(QStringLiteral("..%1").arg(kPathSeparator))
        || name.contains(QStringLiteral("%1..").arg(kPathSeparator));
}

} // namespace

bool FileOperationService::isPathInsideProjectRoot(
    const QString &candidatePath,
    const QString &projectRoot,
    QString *errorMessage)
{
    const QString rootPath = absolutePath(projectRoot);
    if (rootPath.isEmpty() || !QDir(rootPath).exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project root is unavailable.");
        }
        return false;
    }

    const QString candidateAbsolutePath = absolutePath(candidatePath);
    if (candidateAbsolutePath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Path is empty.");
        }
        return false;
    }

    const QFileInfo candidateInfo(candidateAbsolutePath);
    const QString canonicalRoot = QDir(rootPath).canonicalPath();
    if (canonicalRoot.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project root could not be resolved.");
        }
        return false;
    }

    QString scopedPath = candidateInfo.exists()
        ? candidateInfo.canonicalFilePath()
        : QFileInfo(candidateInfo.absolutePath()).canonicalFilePath();
    if (scopedPath.isEmpty()) {
        scopedPath = candidateAbsolutePath;
    }

    if (scopedPath == canonicalRoot || scopedPath.startsWith(canonicalRoot + kPathSeparator)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Path is outside the active repository root.");
    }
    return false;
}

bool FileOperationService::createFile(
    const QString &projectRoot,
    const QString &parentDirectory,
    const QString &fileName,
    QString *createdPath,
    QString *errorMessage) const
{
    QString nameError;
    if (!isValidEntryName(fileName, &nameError)) {
        if (errorMessage != nullptr) {
            *errorMessage = nameError;
        }
        return false;
    }

    if (!ensureParentInsideRoot(parentDirectory, projectRoot, errorMessage)) {
        return false;
    }

    const QString parentPath = absolutePath(parentDirectory);
    const QString fullPath = absolutePath(parentPath + kPathSeparator + fileName);
    if (!isPathInsideProjectRoot(fullPath, projectRoot, errorMessage)) {
        return false;
    }

    if (QFileInfo::exists(fullPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("A file or folder named \"%1\" already exists.").arg(fileName);
        }
        return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create file \"%1\".").arg(fileName);
        }
        return false;
    }

    if (createdPath != nullptr) {
        *createdPath = fullPath;
    }
    return true;
}

bool FileOperationService::createFolder(
    const QString &projectRoot,
    const QString &parentDirectory,
    const QString &folderName,
    QString *createdPath,
    QString *errorMessage) const
{
    QString nameError;
    if (!isValidEntryName(folderName, &nameError)) {
        if (errorMessage != nullptr) {
            *errorMessage = nameError;
        }
        return false;
    }

    if (!ensureParentInsideRoot(parentDirectory, projectRoot, errorMessage)) {
        return false;
    }

    const QString parentPath = absolutePath(parentDirectory);
    const QString fullPath = absolutePath(parentPath + kPathSeparator + folderName);
    if (!isPathInsideProjectRoot(fullPath, projectRoot, errorMessage)) {
        return false;
    }

    if (QFileInfo::exists(fullPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("A file or folder named \"%1\" already exists.").arg(folderName);
        }
        return false;
    }

    QDir parentDir(parentPath);
    if (!parentDir.mkdir(folderName)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create folder \"%1\".").arg(folderName);
        }
        return false;
    }

    if (createdPath != nullptr) {
        *createdPath = fullPath;
    }
    return true;
}

bool FileOperationService::renameEntry(
    const QString &projectRoot,
    const QString &existingPath,
    const QString &newName,
    QString *renamedPath,
    QString *errorMessage) const
{
    QString nameError;
    if (!isValidEntryName(newName, &nameError)) {
        if (errorMessage != nullptr) {
            *errorMessage = nameError;
        }
        return false;
    }

    if (!ensureExistingPathInsideRoot(existingPath, projectRoot, errorMessage)) {
        return false;
    }

    const QFileInfo existingInfo(absolutePath(existingPath));
    if (!existingInfo.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected path no longer exists.");
        }
        return false;
    }

    const QString destinationPath =
        absolutePath(existingInfo.absolutePath() + kPathSeparator + newName);
    if (!isPathInsideProjectRoot(destinationPath, projectRoot, errorMessage)) {
        return false;
    }

    if (QFileInfo::exists(destinationPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("A file or folder named \"%1\" already exists.").arg(newName);
        }
        return false;
    }

    if (!QFile::rename(existingInfo.absoluteFilePath(), destinationPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not rename \"%1\".").arg(existingInfo.fileName());
        }
        return false;
    }

    if (renamedPath != nullptr) {
        *renamedPath = destinationPath;
    }
    return true;
}

bool FileOperationService::deleteEntry(
    const QString &projectRoot,
    const QString &targetPath,
    QString *errorMessage) const
{
    if (!ensureExistingPathInsideRoot(targetPath, projectRoot, errorMessage)) {
        return false;
    }

    const QFileInfo targetInfo(absolutePath(targetPath));
    if (!targetInfo.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected path no longer exists.");
        }
        return false;
    }

    if (targetInfo.isDir()) {
        QDir directory(targetInfo.absoluteFilePath());
        if (!directory.removeRecursively()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    QStringLiteral("Could not delete folder \"%1\".").arg(targetInfo.fileName());
            }
            return false;
        }
        return true;
    }

    if (!QFile::remove(targetInfo.absoluteFilePath())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not delete file \"%1\".").arg(targetInfo.fileName());
        }
        return false;
    }

    return true;
}

bool FileOperationService::isValidEntryName(const QString &name, QString *errorMessage)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Name cannot be empty.");
        }
        return false;
    }

    if (trimmed.contains(kPathSeparator) || trimmed.contains('\\')) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Name cannot contain path separators.");
        }
        return false;
    }

    if (hasParentReference(trimmed)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Name cannot contain parent-directory references.");
        }
        return false;
    }

    return true;
}

QString FileOperationService::absolutePath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

bool FileOperationService::ensureExistingPathInsideRoot(
    const QString &existingPath,
    const QString &projectRoot,
    QString *errorMessage)
{
    return isPathInsideProjectRoot(existingPath, projectRoot, errorMessage);
}

bool FileOperationService::ensureParentInsideRoot(
    const QString &parentDirectory,
    const QString &projectRoot,
    QString *errorMessage)
{
    const QString parentPath = absolutePath(parentDirectory);
    if (!QDir(parentPath).exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Parent folder is unavailable.");
        }
        return false;
    }

    return isPathInsideProjectRoot(parentPath, projectRoot, errorMessage);
}

} // namespace qtcode::core

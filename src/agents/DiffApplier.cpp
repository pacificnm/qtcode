#include "agents/DiffApplier.h"

#include "shared/Logging.h"
#include "shared/ProcessRunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>

namespace qtcode::agents {

namespace {

constexpr int kPatchTimeoutMs = 30000;

} // namespace

bool DiffApplier::applyArtifact(
    const QString &projectRoot,
    const AgentArtifact &artifact,
    QString *errorMessage)
{
    if (artifact.reviewState != ArtifactReviewState::Pending) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Only pending artifacts can be applied.");
        }
        return false;
    }

    const QString kind = artifact.kind.trimmed();
    if (kind == QStringLiteral("file_write")) {
        QString targetPath;
        if (!resolveTargetPath(projectRoot, artifact.filePath, &targetPath, errorMessage)) {
            return false;
        }

        return applyFileWrite(targetPath, artifact.content, errorMessage);
    }

    if (kind == QStringLiteral("unified_diff")) {
        return applyUnifiedDiff(projectRoot, artifact.content, errorMessage);
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Unsupported artifact kind: %1").arg(kind);
    }
    return false;
}

bool DiffApplier::resolveTargetPath(
    const QString &projectRoot,
    const QString &filePath,
    QString *resolvedPath,
    QString *errorMessage)
{
    if (resolvedPath == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Resolved path output must not be null.");
        }
        return false;
    }

    const QFileInfo projectInfo(projectRoot);
    if (!projectInfo.exists() || !projectInfo.isDir()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Project root does not exist: %1").arg(projectRoot);
        }
        return false;
    }

    const QString cleanedRelativePath = QDir::cleanPath(filePath.trimmed());
    if (cleanedRelativePath.isEmpty() || cleanedRelativePath == QStringLiteral(".")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact file path must not be empty.");
        }
        return false;
    }

    if (QDir::isAbsolutePath(cleanedRelativePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact file path must be project-relative.");
        }
        return false;
    }

    if (cleanedRelativePath.startsWith(QStringLiteral(".."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact file path escapes the project root.");
        }
        return false;
    }

    const QString canonicalProjectRoot = projectInfo.canonicalFilePath();
    const QString absolutePath = QDir(canonicalProjectRoot).filePath(cleanedRelativePath);
    const QString canonicalTargetPath = QFileInfo(absolutePath).canonicalFilePath();
    if (canonicalTargetPath.isEmpty()) {
        *resolvedPath = absolutePath;
        return true;
    }

    if (!canonicalTargetPath.startsWith(canonicalProjectRoot)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Artifact file path escapes the project root.");
        }
        return false;
    }

    *resolvedPath = absolutePath;
    return true;
}

bool DiffApplier::applyFileWrite(
    const QString &targetPath,
    const QString &content,
    QString *errorMessage)
{
    const QFileInfo targetInfo(targetPath);
    QDir parentDirectory = targetInfo.dir();
    if (!parentDirectory.exists() && !parentDirectory.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to create parent directory for %1")
                                .arg(targetPath);
        }
        return false;
    }

    QFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to open %1 for writing: %2")
                                .arg(targetPath, targetFile.errorString());
        }
        return false;
    }

    if (targetFile.write(content.toUtf8()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to write %1: %2")
                                .arg(targetPath, targetFile.errorString());
        }
        return false;
    }

    qCInfo(qtcodeAgents) << "Applied file write artifact to" << targetPath;
    return true;
}

bool DiffApplier::applyUnifiedDiff(
    const QString &projectRoot,
    const QString &patchContent,
    QString *errorMessage)
{
    if (patchContent.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unified diff content must not be empty.");
        }
        return false;
    }

    QTemporaryFile patchFile;
    patchFile.setAutoRemove(true);
    if (!patchFile.open()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to create temporary patch file.");
        }
        return false;
    }

    if (patchFile.write(patchContent.toUtf8()) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to write temporary patch file.");
        }
        return false;
    }
    patchFile.close();

    QProcess patchProcess;
    patchProcess.setWorkingDirectory(projectRoot);
    patchProcess.setProgram(QStringLiteral("patch"));
    patchProcess.setArguments(
        {QStringLiteral("-p1"),
         QStringLiteral("--forward"),
         QStringLiteral("-i"),
         patchFile.fileName()});
    patchProcess.start();

    if (!patchProcess.waitForStarted(kPatchTimeoutMs)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The patch command is unavailable on PATH.");
        }
        return false;
    }

    if (!patchProcess.waitForFinished(kPatchTimeoutMs)) {
        patchProcess.kill();
        patchProcess.waitForFinished(kPatchTimeoutMs);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The patch command timed out.");
        }
        return false;
    }

    if (patchProcess.exitCode() != 0) {
        const QString details = QString::fromUtf8(patchProcess.readAllStandardError()).trimmed();
        if (errorMessage != nullptr) {
            *errorMessage = details.isEmpty()
                ? QStringLiteral("patch exited with code %1.").arg(patchProcess.exitCode())
                : details;
        }
        return false;
    }

    qCInfo(qtcodeAgents) << "Applied unified diff artifact in" << projectRoot;
    return true;
}

} // namespace qtcode::agents

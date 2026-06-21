#include "core/FileOperationService.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class FileOperationServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsPathsInsideProjectRoot();
    void rejectsPathsOutsideProjectRoot();
    void createRenameAndDeleteStayInsideProjectRoot();
    void rejectsInvalidEntryNames();
    void rejectsDuplicateCreate();
    void rejectsOperationsOutsideProjectRoot();
};

void FileOperationServiceTest::acceptsPathsInsideProjectRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    const QString nestedDir = root + QStringLiteral("/src");
    QVERIFY(QDir().mkpath(nestedDir));

    QString errorMessage;
    QVERIFY(qtcode::core::FileOperationService::isPathInsideProjectRoot(root, root, &errorMessage));
    QVERIFY(qtcode::core::FileOperationService::isPathInsideProjectRoot(
        nestedDir + QStringLiteral("/main.cpp"),
        root,
        &errorMessage));
    QVERIFY(errorMessage.isEmpty());
}

void FileOperationServiceTest::rejectsPathsOutsideProjectRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    const QString outsidePath = tempDir.path() + QStringLiteral("-outside/file.txt");

    QString errorMessage;
    QVERIFY(!qtcode::core::FileOperationService::isPathInsideProjectRoot(outsidePath, root, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void FileOperationServiceTest::createRenameAndDeleteStayInsideProjectRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    qtcode::core::FileOperationService service;

    QString createdPath;
    QString errorMessage;
    QVERIFY(service.createFile(root, root, QStringLiteral("notes.txt"), &createdPath, &errorMessage));
    QVERIFY(QFileInfo::exists(createdPath));

    QString renamedPath;
    QVERIFY(service.renameEntry(
        root,
        createdPath,
        QStringLiteral("README.md"),
        &renamedPath,
        &errorMessage));
    QCOMPARE(renamedPath, root + QStringLiteral("/README.md"));
    QVERIFY(QFileInfo::exists(renamedPath));

    QVERIFY(service.deleteEntry(root, renamedPath, &errorMessage));
    QVERIFY(!QFileInfo::exists(renamedPath));

    QString folderPath;
    QVERIFY(service.createFolder(root, root, QStringLiteral("docs"), &folderPath, &errorMessage));
    QVERIFY(QDir(folderPath).exists());
    QVERIFY(service.deleteEntry(root, folderPath, &errorMessage));
    QVERIFY(!QDir(folderPath).exists());
}

void FileOperationServiceTest::rejectsInvalidEntryNames()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    qtcode::core::FileOperationService service;

    QString createdPath;
    QString errorMessage;

    QVERIFY(!service.createFile(root, root, QString(), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.createFile(root, root, QStringLiteral("   "), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.createFile(root, root, QStringLiteral("nested/file.txt"), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.createFolder(root, root, QStringLiteral(".."), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.renameEntry(
        root,
        root + QStringLiteral("/existing.txt"),
        QStringLiteral("../escape"),
        &createdPath,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void FileOperationServiceTest::rejectsDuplicateCreate()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    qtcode::core::FileOperationService service;

    QString createdPath;
    QString errorMessage;
    QVERIFY(service.createFile(root, root, QStringLiteral("notes.txt"), &createdPath, &errorMessage));

    errorMessage.clear();
    createdPath.clear();
    QVERIFY(!service.createFile(root, root, QStringLiteral("notes.txt"), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    createdPath.clear();
    QVERIFY(service.createFolder(root, root, QStringLiteral("docs"), &createdPath, &errorMessage));

    errorMessage.clear();
    createdPath.clear();
    QVERIFY(!service.createFolder(root, root, QStringLiteral("docs"), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void FileOperationServiceTest::rejectsOperationsOutsideProjectRoot()
{
    QTemporaryDir tempDir;
    QTemporaryDir outsideDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(outsideDir.isValid());

    const QString root = tempDir.path();
    const QString outsidePath = outsideDir.path();
    qtcode::core::FileOperationService service;

    QString createdPath;
    QString errorMessage;

    QVERIFY(!service.createFile(root, outsidePath, QStringLiteral("outside.txt"), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.createFolder(root, outsidePath, QStringLiteral("outside"), &createdPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.renameEntry(
        root,
        outsidePath + QStringLiteral("/missing.txt"),
        QStringLiteral("renamed.txt"),
        &createdPath,
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    errorMessage.clear();
    QVERIFY(!service.deleteEntry(root, outsidePath + QStringLiteral("/missing.txt"), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

QObject *buildFileOperationServiceTest()
{
    return new FileOperationServiceTest();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return QTest::qExec(buildFileOperationServiceTest(), argc, argv);
}

#include "FileOperationServiceTest.moc"

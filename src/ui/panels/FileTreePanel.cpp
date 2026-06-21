#include "ui/panels/FileTreePanel.h"

#include "core/FileOperationService.h"
#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QFileSystemModel>
#include <QHeaderView>

namespace qtcode::ui {

namespace {

constexpr int kTreeViewIndex = 0;
constexpr int kEmptyStateIndex = 1;

bool hasBinarySuffix(const QFileInfo &info)
{
    static const QStringList binarySuffixes = {
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("gif"),
        QStringLiteral("ico"),
        QStringLiteral("pdf"),
        QStringLiteral("zip"),
        QStringLiteral("gz"),
        QStringLiteral("so"),
        QStringLiteral("o"),
        QStringLiteral("a"),
        QStringLiteral("dll"),
        QStringLiteral("exe"),
        QStringLiteral("bin"),
        QStringLiteral("db"),
        QStringLiteral("sqlite"),
        QStringLiteral("woff"),
        QStringLiteral("woff2"),
        QStringLiteral("ttf"),
        QStringLiteral("eot"),
        QStringLiteral("mp3"),
        QStringLiteral("mp4"),
        QStringLiteral("avi"),
        QStringLiteral("mov"),
        QStringLiteral("webp"),
    };

    return binarySuffixes.contains(info.suffix().toLower());
}

} // namespace

FileTreePanel::FileTreePanel(
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QWidget(parent)
    , m_projectManager(projectManager)
    , m_statusService(statusService)
    , m_fileOperationService(std::make_unique<qtcode::core::FileOperationService>())
{
    configureLayout();

    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &FileTreePanel::syncActiveProject);
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::projectsChanged,
            this,
            &FileTreePanel::syncActiveProject);
    }

    syncActiveProject();
}

void FileTreePanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_contentStack = new QStackedWidget(this);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_fileModel->setRootPath(QString());

    m_treeView = new QTreeView(m_contentStack);
    m_treeView->setModel(m_fileModel);
    m_treeView->setHeaderHidden(true);
    for (int column = 1; column < m_fileModel->columnCount(); ++column) {
        m_treeView->setColumnHidden(column, true);
    }
    if (auto *header = m_treeView->header()) {
        header->setStretchLastSection(false);
        header->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::activated, this, &FileTreePanel::onTreeActivated);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FileTreePanel::showContextMenu);

    m_emptyStateLabel = new QLabel(m_contentStack);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_emptyStateLabel->setContentsMargins(12, 12, 12, 12);

    m_contentStack->addWidget(m_treeView);
    m_contentStack->addWidget(m_emptyStateLabel);

    layout->addWidget(m_contentStack, 1);
}

void FileTreePanel::refreshTree()
{
    if (m_rootPath.isEmpty()) {
        syncActiveProject();
        return;
    }

    if (!QDir(m_rootPath).exists()) {
        showEmptyState(i18n("Project path is not available: %1", m_rootPath));
        return;
    }

    const QModelIndex rootIndex = m_fileModel->index(m_rootPath);
    m_fileModel->setRootPath(m_rootPath);
    m_treeView->setRootIndex(rootIndex);
    showTree();
}

void FileTreePanel::syncActiveProject()
{
    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        m_rootPath.clear();
        showEmptyState(i18n("No active project. Add or select a repository from the Repository view."));
        return;
    }

    qtcode::settings::ProjectRecord project;
    if (!m_projectManager->activeProject(&project) || project.rootPath.isEmpty()) {
        m_rootPath.clear();
        showEmptyState(i18n("No active project. Add or select a repository from the Repository view."));
        return;
    }

    if (!QDir(project.rootPath).exists()) {
        m_rootPath = project.rootPath;
        showEmptyState(i18n("Project path is not available: %1", project.rootPath));
        return;
    }

    m_rootPath = project.rootPath;
    m_fileModel->setRootPath(m_rootPath);
    m_treeView->setRootIndex(m_fileModel->index(m_rootPath));
    showTree();
    qCInfo(qtcodeUi) << "File tree rooted at" << m_rootPath;
}

void FileTreePanel::showEmptyState(const QString &message)
{
    if (m_emptyStateLabel != nullptr) {
        m_emptyStateLabel->setText(message);
    }

    if (m_contentStack != nullptr) {
        m_contentStack->setCurrentIndex(kEmptyStateIndex);
    }
}

void FileTreePanel::showTree()
{
    if (m_contentStack != nullptr) {
        m_contentStack->setCurrentIndex(kTreeViewIndex);
    }
}

bool FileTreePanel::isOpenableTextFile(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        return false;
    }

    if (hasBinarySuffix(info)) {
        return false;
    }

    return true;
}

bool FileTreePanel::hasActiveProjectRoot(QString *projectRoot)
{
    if (m_rootPath.isEmpty()) {
        reportError(i18n("Select an active repository before changing files."));
        return false;
    }

    if (projectRoot != nullptr) {
        *projectRoot = m_rootPath;
    }
    return true;
}

QString FileTreePanel::selectedPath() const
{
    if (m_treeView == nullptr || m_fileModel == nullptr) {
        return {};
    }

    const QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid()) {
        return {};
    }

    return m_fileModel->filePath(index);
}

QString FileTreePanel::targetDirectoryPath() const
{
    const QString selected = selectedPath();
    if (selected.isEmpty()) {
        return m_rootPath;
    }

    const QFileInfo info(selected);
    if (info.isDir()) {
        return info.absoluteFilePath();
    }

    return info.absolutePath();
}

void FileTreePanel::reportError(const QString &message)
{
    if (m_statusService != nullptr) {
        m_statusService->showMessage(message, qtcode::core::StatusSeverity::Error);
    }
    qCWarning(qtcodeUi) << message;
}

void FileTreePanel::reportSuccess(const QString &message)
{
    if (m_statusService != nullptr) {
        m_statusService->showMessage(message);
    }
}

void FileTreePanel::selectPath(const QString &absolutePath)
{
    if (m_treeView == nullptr || m_fileModel == nullptr || absolutePath.isEmpty()) {
        return;
    }

    const QModelIndex index = m_fileModel->index(absolutePath);
    if (!index.isValid()) {
        refreshTree();
        const QModelIndex refreshedIndex = m_fileModel->index(absolutePath);
        if (!refreshedIndex.isValid()) {
            return;
        }
        m_treeView->setCurrentIndex(refreshedIndex);
        m_treeView->scrollTo(refreshedIndex);
        return;
    }

    m_treeView->setCurrentIndex(index);
    m_treeView->scrollTo(index);
}

void FileTreePanel::createNewFile()
{
    QString projectRoot;
    if (!hasActiveProjectRoot(&projectRoot) || !m_fileOperationService) {
        return;
    }

    const QString parentDirectory = targetDirectoryPath();
    bool ok = false;
    const QString fileName = QInputDialog::getText(
        this,
        i18n("New File"),
        i18n("File name:"),
        QLineEdit::Normal,
        {},
        &ok);
    if (!ok || fileName.trimmed().isEmpty()) {
        return;
    }

    QString createdPath;
    QString errorMessage;
    if (!m_fileOperationService->createFile(
            projectRoot,
            parentDirectory,
            fileName.trimmed(),
            &createdPath,
            &errorMessage)) {
        reportError(errorMessage.isEmpty() ? i18n("Could not create the file.") : errorMessage);
        QMessageBox::warning(
            this,
            i18n("Create File Failed"),
            errorMessage.isEmpty() ? i18n("Could not create the file.") : errorMessage);
        return;
    }

    refreshTree();
    selectPath(createdPath);
    reportSuccess(i18n("Created file %1.", QFileInfo(createdPath).fileName()));
    emit fileOpenRequested(createdPath);
}

void FileTreePanel::createNewFolder()
{
    QString projectRoot;
    if (!hasActiveProjectRoot(&projectRoot) || !m_fileOperationService) {
        return;
    }

    const QString parentDirectory = targetDirectoryPath();
    bool ok = false;
    const QString folderName = QInputDialog::getText(
        this,
        i18n("New Folder"),
        i18n("Folder name:"),
        QLineEdit::Normal,
        {},
        &ok);
    if (!ok || folderName.trimmed().isEmpty()) {
        return;
    }

    QString createdPath;
    QString errorMessage;
    if (!m_fileOperationService->createFolder(
            projectRoot,
            parentDirectory,
            folderName.trimmed(),
            &createdPath,
            &errorMessage)) {
        reportError(errorMessage.isEmpty() ? i18n("Could not create the folder.") : errorMessage);
        QMessageBox::warning(
            this,
            i18n("Create Folder Failed"),
            errorMessage.isEmpty() ? i18n("Could not create the folder.") : errorMessage);
        return;
    }

    refreshTree();
    selectPath(createdPath);
    reportSuccess(i18n("Created folder %1.", QFileInfo(createdPath).fileName()));
}

void FileTreePanel::renameSelectedEntry()
{
    QString projectRoot;
    if (!hasActiveProjectRoot(&projectRoot) || !m_fileOperationService) {
        return;
    }

    const QString selected = selectedPath();
    if (selected.isEmpty()) {
        reportError(i18n("Select a file or folder to rename."));
        return;
    }

    const QFileInfo selectedInfo(selected);
    if (QFileInfo(projectRoot).canonicalFilePath() == selectedInfo.canonicalFilePath()) {
        reportError(i18n("The repository root cannot be renamed."));
        return;
    }

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        i18n("Rename"),
        i18n("New name:"),
        QLineEdit::Normal,
        selectedInfo.fileName(),
        &ok);
    if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == selectedInfo.fileName()) {
        return;
    }

    QString renamedPath;
    QString errorMessage;
    if (!m_fileOperationService->renameEntry(
            projectRoot,
            selected,
            newName.trimmed(),
            &renamedPath,
            &errorMessage)) {
        reportError(errorMessage.isEmpty() ? i18n("Could not rename the selection.") : errorMessage);
        QMessageBox::warning(
            this,
            i18n("Rename Failed"),
            errorMessage.isEmpty() ? i18n("Could not rename the selection.") : errorMessage);
        return;
    }

    emit pathRenamed(selected, renamedPath);
    refreshTree();
    selectPath(renamedPath);
    reportSuccess(i18n("Renamed to %1.", QFileInfo(renamedPath).fileName()));
}

void FileTreePanel::deleteSelectedEntry()
{
    QString projectRoot;
    if (!hasActiveProjectRoot(&projectRoot) || !m_fileOperationService) {
        return;
    }

    const QString selected = selectedPath();
    if (selected.isEmpty()) {
        reportError(i18n("Select a file or folder to delete."));
        return;
    }

    const QFileInfo selectedInfo(selected);
    if (QFileInfo(projectRoot).canonicalFilePath() == selectedInfo.canonicalFilePath()) {
        reportError(i18n("The repository root cannot be deleted."));
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        i18n("Delete Selection"),
        selectedInfo.isDir()
            ? i18n("Delete folder \"%1\" and everything inside it?", selectedInfo.fileName())
            : i18n("Delete file \"%1\"?", selectedInfo.fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_fileOperationService->deleteEntry(projectRoot, selected, &errorMessage)) {
        reportError(errorMessage.isEmpty() ? i18n("Could not delete the selection.") : errorMessage);
        QMessageBox::warning(
            this,
            i18n("Delete Failed"),
            errorMessage.isEmpty() ? i18n("Could not delete the selection.") : errorMessage);
        return;
    }

    emit pathDeleted(selected, selectedInfo.isDir());
    refreshTree();
    reportSuccess(
        selectedInfo.isDir()
            ? i18n("Deleted folder %1.", selectedInfo.fileName())
            : i18n("Deleted file %1.", selectedInfo.fileName()));
}

void FileTreePanel::showContextMenu(const QPoint &position)
{
    if (m_treeView == nullptr || m_rootPath.isEmpty()) {
        return;
    }

    const QModelIndex index = m_treeView->indexAt(position);
    const QString contextPath =
        index.isValid() && m_fileModel != nullptr ? m_fileModel->filePath(index) : QString();
    const QFileInfo contextInfo(contextPath);
    const bool isContextFile = contextInfo.isFile();

    QMenu menu(this);
    if (isContextFile && isOpenableTextFile(contextPath)) {
        menu.addAction(
            QIcon::fromTheme(QStringLiteral("document-open")),
            i18n("Open"),
            this,
            [this, contextPath]() {
                emit fileOpenRequested(contextPath);
            });
        menu.addAction(
            QIcon::fromTheme(QStringLiteral("bookmark-new")),
            i18n("Add to Context"),
            this,
            [this, contextPath]() {
                emit fileContextRequested(contextPath);
            });
        menu.addSeparator();
    }

    menu.addAction(i18n("New File"), this, &FileTreePanel::createNewFile);
    menu.addAction(i18n("New Folder"), this, &FileTreePanel::createNewFolder);
    menu.addSeparator();

    const bool hasSelection = !selectedPath().isEmpty();
    QAction *renameAction = menu.addAction(i18n("Rename"), this, &FileTreePanel::renameSelectedEntry);
    renameAction->setEnabled(hasSelection);
    QAction *deleteAction = menu.addAction(i18n("Delete"), this, &FileTreePanel::deleteSelectedEntry);
    deleteAction->setEnabled(hasSelection);

    menu.exec(m_treeView->viewport()->mapToGlobal(position));
}

void FileTreePanel::onTreeActivated(const QModelIndex &index)
{
    if (!index.isValid() || m_fileModel == nullptr) {
        return;
    }

    const QString path = m_fileModel->filePath(index);
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    if (info.isDir()) {
        return;
    }

    if (!isOpenableTextFile(path)) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Cannot open %1 in the editor.", info.fileName()),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    emit fileOpenRequested(path);
}

} // namespace qtcode::ui

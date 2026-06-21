#include "ui/panels/FileTreePanel.h"

#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QFileSystemModel>

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
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setUniformRowHeights(true);
    connect(m_treeView, &QTreeView::activated, this, &FileTreePanel::onTreeActivated);

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

    m_fileModel->setRootPath(m_rootPath);
    m_treeView->setRootIndex(m_fileModel->index(m_rootPath));
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

#pragma once

#include <QWidget>

#include <memory>

class QFileSystemModel;
class QLabel;
class QStackedWidget;
class QTreeView;

#include "core/FileOperationService.h"
#include "core/ProjectManager.h"
#include "core/StatusService.h"

namespace qtcode::ui {

class FileTreePanel final : public QWidget
{
    Q_OBJECT

public:
    explicit FileTreePanel(
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

    void refreshTree();

public slots:
    void createNewFile();
    void createNewFolder();
    void renameSelectedEntry();
    void deleteSelectedEntry();

signals:
    void fileOpenRequested(const QString &absolutePath);
    void pathRenamed(const QString &oldPath, const QString &newPath);
    void pathDeleted(const QString &path, bool isDirectory);

private slots:
    void syncActiveProject();
    void onTreeActivated(const QModelIndex &index);
    void showContextMenu(const QPoint &position);

private:
    void configureLayout();
    void showEmptyState(const QString &message);
    void showTree();
    [[nodiscard]] bool isOpenableTextFile(const QString &path) const;
    [[nodiscard]] bool hasActiveProjectRoot(QString *projectRoot);
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] QString targetDirectoryPath() const;
    void reportError(const QString &message);
    void reportSuccess(const QString &message);
    void selectPath(const QString &absolutePath);

    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    std::unique_ptr<qtcode::core::FileOperationService> m_fileOperationService;
    QFileSystemModel *m_fileModel = nullptr;
    QTreeView *m_treeView = nullptr;
    QLabel *m_emptyStateLabel = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QString m_rootPath;
};

} // namespace qtcode::ui

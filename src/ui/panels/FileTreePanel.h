#pragma once

#include <QWidget>

class QFileSystemModel;
class QLabel;
class QStackedWidget;
class QTreeView;

namespace qtcode::core {
class ProjectManager;
class StatusService;
} // namespace qtcode::core

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

signals:
    void fileOpenRequested(const QString &absolutePath);

private slots:
    void syncActiveProject();
    void onTreeActivated(const QModelIndex &index);

private:
    void configureLayout();
    void showEmptyState(const QString &message);
    void showTree();
    [[nodiscard]] bool isOpenableTextFile(const QString &path) const;

    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::StatusService *m_statusService = nullptr;
    QFileSystemModel *m_fileModel = nullptr;
    QTreeView *m_treeView = nullptr;
    QLabel *m_emptyStateLabel = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QString m_rootPath;
};

} // namespace qtcode::ui
